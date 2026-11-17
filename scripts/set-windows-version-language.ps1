param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ExecutablePath
)

$ErrorActionPreference = "Stop"

$versionResourceType = "SeiSeeVersionResourceLanguage" -as [type]
if ($null -eq $versionResourceType) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class SeiSeeVersionResourceLanguage
{
    private static readonly IntPtr ResourceTypeVersion = new IntPtr(16);
    private static readonly IntPtr ResourceId = new IntPtr(1);
    private const ushort NeutralLanguage = 0;
    private const ushort EnglishUnitedStates = 0x0409;
    private const uint LoadLibraryAsDataFile = 0x00000002;

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryEx(string fileName, IntPtr file, uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr FindResourceEx(IntPtr module, IntPtr type, IntPtr name, ushort language);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr LoadResource(IntPtr module, IntPtr resource);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr LockResource(IntPtr resource);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint SizeofResource(IntPtr module, IntPtr resource);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr module);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr BeginUpdateResource(string fileName, bool deleteExistingResources);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool UpdateResource(IntPtr update, IntPtr type, IntPtr name, ushort language, byte[] data, uint size);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool EndUpdateResource(IntPtr update, bool discard);

    private sealed class VersionBlock
    {
        public int Start;
        public int Length;
        public int KeyOffset;
        public int ValueOffset;
        public int ValueLength;
        public int ChildrenOffset;
        public string Key;
    }

    public static void SetEnglishLanguage(string fileName)
    {
        byte[] data = ReadVersionResource(fileName);
        VersionBlock root = ReadBlock(data, 0, data.Length);
        if (root.Key != "VS_VERSION_INFO")
            throw new InvalidOperationException("The executable has no valid version-info resource.");

        VersionBlock stringFileInfo = FindChild(data, root, "StringFileInfo");
        VersionBlock varFileInfo = FindChild(data, root, "VarFileInfo");
        if (stringFileInfo == null || varFileInfo == null)
            throw new InvalidOperationException("The version-info resource is missing string or translation data.");

        VersionBlock stringTable = FindFirstChild(data, stringFileInfo);
        VersionBlock translation = FindChild(data, varFileInfo, "Translation");
        if (stringTable == null || stringTable.Key.Length != 8)
            throw new InvalidOperationException("The version-info resource has no valid string table.");
        if (translation == null || translation.ValueLength < 4 || translation.ValueLength % 4 != 0)
            throw new InvalidOperationException("The version-info resource has no valid language translation.");

        // Inno Setup emits a neutral table; update both language identifiers.
        byte[] englishLanguage = Encoding.Unicode.GetBytes(EnglishUnitedStates.ToString("X4"));
        Buffer.BlockCopy(englishLanguage, 0, data, stringTable.KeyOffset, englishLanguage.Length);
        for (int offset = 0; offset < translation.ValueLength; offset += 4)
            WriteUInt16(data, translation.ValueOffset + offset, EnglishUnitedStates);

        UpdateVersionResource(fileName, data);
    }

    private static byte[] ReadVersionResource(string fileName)
    {
        IntPtr module = LoadLibraryEx(fileName, IntPtr.Zero, LoadLibraryAsDataFile);
        if (module == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not load the installer version resource.");

        try {
            IntPtr resource = FindResourceEx(module, ResourceTypeVersion, ResourceId, NeutralLanguage);
            if (resource == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not find the installer version resource.");
            IntPtr loaded = LoadResource(module, resource);
            uint size = SizeofResource(module, resource);
            IntPtr locked = LockResource(loaded);
            if (loaded == IntPtr.Zero || locked == IntPtr.Zero || size == 0)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read the installer version resource.");

            byte[] data = new byte[size];
            Marshal.Copy(locked, data, 0, data.Length);
            return data;
        }
        finally {
            FreeLibrary(module);
        }
    }

    private static void UpdateVersionResource(string fileName, byte[] data)
    {
        IntPtr update = BeginUpdateResource(fileName, false);
        if (update == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not open the installer for version-resource updates.");

        try {
            if (!UpdateResource(update, ResourceTypeVersion, ResourceId, NeutralLanguage, data, (uint)data.Length))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not set the installer version-resource language.");
            if (!EndUpdateResource(update, false)) {
                update = IntPtr.Zero;
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not save the installer version-resource language.");
            }
            update = IntPtr.Zero;
        }
        finally {
            if (update != IntPtr.Zero)
                EndUpdateResource(update, true);
        }
    }

    private static VersionBlock FindChild(byte[] data, VersionBlock parent, string key)
    {
        int offset = parent.ChildrenOffset;
        int end = parent.Start + parent.Length;
        while (offset + 6 <= end) {
            VersionBlock child = ReadBlock(data, offset, end);
            if (child.Key == key)
                return child;
            int next = Align4(child.Start + child.Length);
            if (next <= offset)
                break;
            offset = next;
        }
        return null;
    }

    private static VersionBlock FindFirstChild(byte[] data, VersionBlock parent)
    {
        int end = parent.Start + parent.Length;
        if (parent.ChildrenOffset + 6 > end)
            return null;
        return ReadBlock(data, parent.ChildrenOffset, end);
    }

    private static VersionBlock ReadBlock(byte[] data, int offset, int parentEnd)
    {
        if (offset < 0 || offset + 6 > parentEnd)
            throw new InvalidOperationException("The installer version resource is malformed.");

        int length = ReadUInt16(data, offset);
        int blockEnd = offset + length;
        if (length < 6 || blockEnd > parentEnd)
            throw new InvalidOperationException("The installer version resource is malformed.");

        int keyOffset = offset + 6;
        int keyEnd = keyOffset;
        while (keyEnd + 1 < blockEnd && ReadUInt16(data, keyEnd) != 0)
            keyEnd += 2;
        if (keyEnd + 1 >= blockEnd)
            throw new InvalidOperationException("The installer version resource has an unterminated key.");

        int valueOffset = Align4(keyEnd + 2);
        int valueLength = ReadUInt16(data, offset + 2);
        int type = ReadUInt16(data, offset + 4);
        int valueSize = type == 1 ? valueLength * 2 : valueLength;
        if (valueOffset + valueSize > blockEnd)
            throw new InvalidOperationException("The installer version resource has an invalid value.");

        return new VersionBlock {
            Start = offset,
            Length = length,
            KeyOffset = keyOffset,
            ValueOffset = valueOffset,
            ValueLength = valueSize,
            ChildrenOffset = Align4(valueOffset + valueSize),
            Key = Encoding.Unicode.GetString(data, keyOffset, keyEnd - keyOffset)
        };
    }

    private static int ReadUInt16(byte[] data, int offset)
    {
        return data[offset] | (data[offset + 1] << 8);
    }

    private static void WriteUInt16(byte[] data, int offset, ushort value)
    {
        data[offset] = (byte)(value & 0xFF);
        data[offset + 1] = (byte)(value >> 8);
    }

    private static int Align4(int value)
    {
        return (value + 3) & ~3;
    }
}
'@ | Out-Null
}

$resolvedExecutablePath = (Resolve-Path -LiteralPath $ExecutablePath).Path
[SeiSeeVersionResourceLanguage]::SetEnglishLanguage($resolvedExecutablePath)
