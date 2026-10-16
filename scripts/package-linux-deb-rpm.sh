#!/usr/bin/env bash
# 将 publish-linux-release.sh 生成的 Linux AppImage 转换为 deb 和 rpm 安装包。
#
# 工作方式:
#   1. 从 SeiSeeMp/mainwindow.h 的 VERSION 宏读取版本号；
#   2. 定位 dist/linux/SeiSee-<VERSION>-x86_64.AppImage；
#   3. 生成 config_package.json 包配置；
#   4. 克隆 appimage2debrpm-converter 转换器并调用其 main.py
#      完成 AppImage -> deb/rpm 转换。
#
# 用法:
#   scripts/package-linux-deb-rpm.sh                 # 转换当前版本默认 AppImage
#   scripts/package-linux-deb-rpm.sh /path/AppImage  # 转换指定 AppImage
#
# 选项:
#   -h, --help      显示本帮助
#   --keep-temp     保留转换器临时文件，便于调试
#
# 环境变量:
#   OUTPUT_DIR             输出目录 (默认 dist/linux)
#   MAINTAINER             包维护者 (默认 "SeiSee <seisee@example.com>")
#   HOMEPAGE               项目主页 (默认 https://github.com/tuoyuangui/SeiSee)
#   LICENSE                软件许可证 (默认 MIT)
#   ARCHITECTURE           目标架构 (默认 amd64)
#   CONVERTER_REPO_SSH     转换器仓库 SSH 地址
#   CONVERTER_REPO_HTTPS   转换器仓库 HTTPS 地址 (SSH 失败时回退)
#   CONVERTER_REPO_REF     转换器仓库分支或标签 (默认仓库默认分支)
#   APPIMAGE2DEBRPM_VERSION 包版本号覆盖 (默认取 VERSION 宏)

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/package-linux-deb-rpm.sh [AppImage] [--keep-temp]

Convert the Linux AppImage produced by scripts/publish-linux-release.sh
into deb and rpm packages using the appimage2debrpm-converter tool.

With no argument the script uses dist/linux/SeiSee-<VERSION>-x86_64.AppImage,
where <VERSION> is read from the VERSION macro in SeiSeeMp/mainwindow.h.
Pass an explicit AppImage path to convert a different file.

Output packages are written to dist/linux/ (see OUTPUT_DIR).

Options:
  -h, --help     Show this help.
  --keep-temp    Keep the converter's temporary files for debugging.

Environment variables:
  OUTPUT_DIR              Output directory (default: dist/linux)
  MAINTAINER              Package maintainer (default: SeiSee <seisee@example.com>)
  HOMEPAGE                Project home page (default: https://github.com/tuoyuangui/SeiSee)
  LICENSE                 Package license (default: MIT)
  ARCHITECTURE            Target architecture (default: amd64)
  CONVERTER_REPO_SSH      SSH URL of the converter repository
  CONVERTER_REPO_HTTPS    HTTPS URL used when the SSH clone fails
  CONVERTER_REPO_REF      Branch or tag of the converter repository to check out
  APPIMAGE2DEBRPM_VERSION Package version override

Requires python3, git, unsquashfs (squashfs-tools); dpkg-deb for deb
packages (ar+tar+gzip are used as a fallback) and rpmbuild for rpm packages.
EOF
}

CONVERTER_REPO_SSH="${CONVERTER_REPO_SSH:-git@github.com:tuoyuangui/appimage2debrpm-converter.git}"
CONVERTER_REPO_HTTPS="${CONVERTER_REPO_HTTPS:-https://github.com/tuoyuangui/appimage2debrpm-converter.git}"
CONVERTER_REPO_REF="${CONVERTER_REPO_REF:-}"
MAINTAINER="${MAINTAINER:-SeiSee <seisee@example.com>}"
HOMEPAGE="${HOMEPAGE:-https://github.com/tuoyuangui/SeiSee}"
LICENSE="${LICENSE:-MIT}"
ARCHITECTURE="${ARCHITECTURE:-amd64}"
OUTPUT_DIR="${OUTPUT_DIR:-}"
APPIMAGE2DEBRPM_VERSION="${APPIMAGE2DEBRPM_VERSION:-}"

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
version_header="$repo_root/SeiSeeMp/mainwindow.h"

appimage_path=""
keep_temp=false
while (($#)); do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        --keep-temp)
            keep_temp=true
            ;;
        -*)
            echo "Error: unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            if [[ -n "$appimage_path" ]]; then
                echo "Error: unexpected extra argument: $1" >&2
                usage >&2
                exit 2
            fi
            appimage_path="$1"
            ;;
    esac
    shift
done

# 版本号：与 publish-linux-release.sh 相同，从 SeiSeeMp/mainwindow.h 读取
mapfile -t versions < <(
    sed -nE 's/^[[:space:]]*#[[:space:]]*define[[:space:]]+VERSION[[:space:]]+"([^"]+)"[[:space:]]*$/\1/p' "$version_header"
)
if ((${#versions[@]} != 1)); then
    echo "Error: expected exactly one #define VERSION \"value\" in $version_header; found ${#versions[@]}." >&2
    exit 1
fi
header_version="${versions[0]}"

if [[ -z "$appimage_path" ]]; then
    appimage_path="$repo_root/dist/linux/SeiSee-$header_version-x86_64.AppImage"
fi
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="$repo_root/dist/linux"
fi

[[ -f "$appimage_path" ]] || {
    echo "Error: Linux AppImage not found: $appimage_path" >&2
    echo "Build it first with scripts/package-linux.sh or pass an existing AppImage path." >&2
    exit 1
}

abs_appimage="$(cd -- "$(dirname -- "$appimage_path")" && pwd)/$(basename -- "$appimage_path")"
mkdir -p "$OUTPUT_DIR"
abs_output="$(cd -- "$OUTPUT_DIR" && pwd)"

# 包版本号优先级：APPIMAGE2DEBRPM_VERSION > VERSION 宏 > 文件名解析(转换器内完成)
package_version="$APPIMAGE2DEBRPM_VERSION"
if [[ -z "$package_version" && "$appimage_path" == "$repo_root/dist/linux/SeiSee-$header_version-x86_64.AppImage" ]]; then
    package_version="$header_version"
fi
if [[ -n "$package_version" ]]; then
    printf 'Package version: %s\n' "$package_version"
else
    printf 'Package version: parsed from AppImage file name by the converter\n'
fi

# 检查必需工具
missing=()
for tool in python3 git unsquashfs; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
done
if ((${#missing[@]})); then
    echo "Error: required tools not found: ${missing[*]}" >&2
    echo "Hint: install python3, git and squashfs-tools (provides unsquashfs)." >&2
    exit 1
fi
if ! command -v dpkg-deb >/dev/null 2>&1; then
    for tool in ar tar gzip; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done
    if ((${#missing[@]})); then
        echo "Error: dpkg-deb not found and fallback tools missing: ${missing[*]}" >&2
        echo "Hint: install dpkg-dev (Debian/Ubuntu)." >&2
        exit 1
    fi
    printf 'Note: dpkg-deb not found; deb will be built with ar+tar+gzip fallback.\n'
fi
command -v rpmbuild >/dev/null 2>&1 || {
    echo "Error: rpmbuild not found; cannot build rpm packages." >&2
    echo "Hint: install rpm-build (Debian/Ubuntu, Fedora/RHEL)." >&2
    exit 1
}

# 工作目录与转换器仓库
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/seisee-deb-rpm.XXXXXX")"
converter_dir="$work_dir/appimage2debrpm-converter"
cleanup() { rm -rf -- "$work_dir"; }
if [[ "$keep_temp" == true ]]; then
    cleanup() { printf 'Temporary files kept at: %s\n' "$work_dir"; }
fi
trap cleanup EXIT

clone_converter() {
    local clone_args=(--depth 1)
    if [[ -n "$CONVERTER_REPO_REF" ]]; then
        clone_args=(--branch "$CONVERTER_REPO_REF")
    fi
    local git_ssh="${GIT_SSH_COMMAND:-ssh -o BatchMode=yes -o ConnectTimeout=10}"
    echo "Cloning converter repository: $CONVERTER_REPO_SSH"
    if GIT_SSH_COMMAND="$git_ssh" git clone "${clone_args[@]}" "$CONVERTER_REPO_SSH" "$converter_dir" 2>"$work_dir/clone.err"; then
        return 0
    fi
    echo "Warning: SSH clone failed ($(tail -n 1 "$work_dir/clone.err")). Falling back to HTTPS." >&2
    git clone "${clone_args[@]}" "$CONVERTER_REPO_HTTPS" "$converter_dir"
}
clone_converter

# 生成 config_package.json 包配置
config_file="$work_dir/config_package.json"
python3 - "$config_file" "$MAINTAINER" "$HOMEPAGE" "$LICENSE" "$ARCHITECTURE" <<'PYEOF'
import json
import sys

config_path, maintainer, homepage, license_name, architecture = sys.argv[1:6]
config = {
    "maintainer": maintainer,
    "license": license_name,
    "homepage": homepage,
    "architecture": architecture,
    "deb": {
        "section": "science",
        "priority": "optional",
        # AppImage 已捆绑 Qt 运行库，仅声明基础系统依赖
        "depends": ["libc6", "libgcc-s1"],
    },
    "rpm": {
        "section": "Applications/Engineering",
        "depends": [],
        "requires": ["glibc", "libgcc"],
    },
}
with open(config_path, "w", encoding="utf-8") as f:
    json.dump(config, f, ensure_ascii=False, indent=4)
print(f"Package config written to: {config_path}")
PYEOF

# 转换器上游有两个问题需要在临时克隆中打最小补丁（其余逻辑完全使用上游代码）：
#   1. 不解析 AppImage 真实版本号（默认恒为 1.0.0）
#      -> 优先读取 APPIMAGE2DEBRPM_VERSION 环境变量，否则从 AppImage 文件名解析；
#   2. unsquashfs 提取时遇到 security.selinux 等 xattr 会以非零码失败
#      -> 追加 -no-xattrs（xattr 对打包无意义）。
patch_converter() {
    local appimage_py="$converter_dir/core/appimage.py"
    python3 - "$appimage_py" <<'PYEOF'
import sys

path = sys.argv[1]
with open(path, encoding="utf-8") as f:
    src = f.read()

# --- Patch 1: resolve the real package version ---
anchor = "        return self.info"
if src.count(anchor) != 1:
    sys.exit(f"patch anchor not found exactly once in {path}: version patch")
version_patch = '''        # Patch (SeiSee scripts): upstream keeps the default "1.0.0"; resolve the
        # real version from the APPIMAGE2DEBRPM_VERSION environment variable or
        # from the AppImage file name, e.g. SeiSee-4.0.0-alpha.2-x86_64.AppImage.
        env_version = os.environ.get("APPIMAGE2DEBRPM_VERSION", "").strip()
        if env_version:
            self.info.version = env_version
        else:
            import re
            m = re.search(
                r"[-_](\\d+(?:\\.\\d+)+(?:[.+~_-](?:alpha|beta|rc|pre|dev)\\d*(?:\\.\\d+)*)*)"
                r"(?:[-_].*)?\\.AppImage$",
                os.path.basename(self.appimage_path),
            )
            if m:
                self.info.version = m.group(1)
'''
src = src.replace(anchor, version_patch + anchor, 1)

# --- Patch 2: unsquashfs must not fail on SELinux xattrs ---
anchor2 = "'unsquashfs', '-d', self.extracted_dir, self.appimage_path"
if src.count(anchor2) != 1:
    sys.exit(f"patch anchor not found exactly once in {path}: unsquashfs patch")
src = src.replace(
    anchor2,
    "'unsquashfs', '-no-xattrs', '-d', self.extracted_dir, self.appimage_path",
    1,
)

with open(path, "w", encoding="utf-8") as f:
    f.write(src)
PYEOF
}
patch_converter
echo "Patched converter (package version resolution, unsquashfs -no-xattrs)."

# 调用转换器：--format both 同时生成 deb 和 rpm
marker="$work_dir/packages.marker"
touch "$marker"
echo "Running converter: python3 main.py $abs_appimage --format both --output $abs_output --config $config_file"
(
    export APPIMAGE2DEBRPM_VERSION="$package_version"
    python3 "$converter_dir/main.py" "$abs_appimage" \
        --format both \
        --output "$abs_output" \
        --config "$config_file" \
        --verbose
)

# 核对产物（转换器在 both 模式下个别失败时仍可能以 0 退出，必须独立检查）
packages=()
while IFS= read -r -d '' p; do
    packages+=("$p")
done < <(find "$abs_output" -maxdepth 1 -type f \( -name '*.deb' -o -name '*.rpm' \) -newer "$marker" -print0)
if ((${#packages[@]} == 0)); then
    echo "Error: no deb/rpm package was generated in $abs_output" >&2
    exit 1
fi

deb_found=false
rpm_found=false
printf '\nGenerated packages:\n'
for p in "${packages[@]}"; do
    printf '  %s (%s)\n' "$p" "$(du -h "$p" | cut -f1)"
    case "$p" in
        *.deb) deb_found=true ;;
        *.rpm) rpm_found=true ;;
    esac
done
if [[ "$deb_found" != true ]]; then
    echo "Error: no .deb package was generated." >&2
    exit 1
fi
if [[ "$rpm_found" != true ]]; then
    echo "Error: no .rpm package was generated." >&2
    exit 1
fi
printf 'Done: deb and rpm packages written to %s\n' "$abs_output"
