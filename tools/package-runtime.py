"""Complete windeployqt output with imports required by the Conda Qt build.

Usage: python tools/package-runtime.py dist/SeiSee-FK-20261028 QT_BIN
Copies redistributable dependencies from QT_BIN or the installed MSVC redist.
Windows system DLLs are checked for availability and are never copied.
"""
from pathlib import Path
import shutil
import struct
import sys


def imports(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count, optional_size = struct.unpack_from('<H12xH', data, pe + 6)
    optional = pe + 24
    directory = optional + (112 if struct.unpack_from('<H', data, optional)[0] == 0x20b else 96)
    import_rva = struct.unpack_from('<I', data, directory + 8)[0]
    sections = []
    for index in range(count):
        offset = optional + optional_size + 40 * index
        virtual_size, address, raw_size, raw_offset = struct.unpack_from('<IIII', data, offset + 8)
        sections.append((address, max(virtual_size, raw_size), raw_offset))

    def offset(rva):
        for address, size, raw in sections:
            if address <= rva < address + size:
                return raw + rva - address
        raise ValueError(f'Invalid RVA {rva:x} in {path}')

    if not import_rva:
        return []
    names = []
    cursor = offset(import_rva)
    while any(struct.unpack_from('<IIIII', data, cursor)):
        name_rva = struct.unpack_from('<I', data, cursor + 12)[0]
        start = offset(name_rva)
        names.append(data[start:data.index(b'\0', start)].decode('ascii'))
        cursor += 20
    return names


def complete(destination, runtime):
    redist = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC')
    sources = [runtime] + sorted(redist.glob('*/x64/Microsoft.VC143.CRT'), reverse=True)
    available = {p.name.lower(): p for folder in sources for p in folder.glob('*.dll')}
    system = Path(r'C:\Windows\System32')
    queue = [p for p in destination.rglob('*') if p.suffix.lower() in ('.exe', '.dll')]
    checked = set()
    while queue:
        item = queue.pop()
        if item in checked:
            continue
        checked.add(item)
        for name in imports(item):
            if name.lower().startswith(('api-ms-', 'ext-ms-')):
                continue
            target = destination / name
            if target.exists():
                queue.append(target)
                continue
            source = available.get(name.lower())
            if source:
                shutil.copy2(source, target)
                print(f'Copied {name}')
                queue.append(target)
            elif not (system / name).exists():
                raise SystemExit(f'Missing {name}, imported by {item}')
    print(f'Checked {len(checked)} PE files')


if __name__ == '__main__':
    complete(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
