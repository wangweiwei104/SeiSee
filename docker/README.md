# Docker 构建 Linux AppImage

本目录中的 `linux.Dockerfile` 用于构建 SeiSee 的 Linux AppImage。构建环境基于 manylinux2014（CentOS 7 / glibc 2.17），目标是让产物可在 CentOS 7 及更新版本、Ubuntu 20.04 及更新版本的 x86_64 系统上运行。

## 1. 在 Ubuntu 主机安装 Docker

```bash
sudo apt-get update
sudo apt-get install -y docker.io
sudo systemctl enable --now docker
```

确认 Docker 服务已运行：

```bash
sudo systemctl status docker --no-pager
sudo docker version
```

## 2. 配置 Docker 权限

将当前用户加入 `docker` 组：

```bash
sudo usermod -aG docker "$USER"
```

Docker 组成员可以通过 Docker 容器获得主机 root 级别的访问权限。只应将可信用户加入该组。

执行后需要**完全注销并重新登录**，或重启 VS Code Remote / SSH 会话，使新的组权限生效。然后检查 `docker` 是否出现在用户组列表中，并确认可以连接 daemon：

```bash
id -nG
docker info
```

如果仍提示无法连接 `/var/run/docker.sock`，请确认 Docker 服务正在运行，并重新建立登录会话。也可以临时通过 `sudo docker ...` 执行 Docker 命令；保持 `--user "$(id -u):$(id -g)"` 参数可避免输出文件归 root 所有。

## 3. 准备 Qt SDK

构建需要 x86_64 Linux 版 Qt 5.15.2 SDK，默认位置为：

```text
/home/ww/Qt/5.15.2/gcc_64
```

确认该目录下存在 `bin/qmake`、`lib` 和 `plugins`。Docker 运行时会将 `/home/ww/Qt` 以只读方式挂载到容器同一路径，不会把 Qt SDK 打入镜像。

如果 Qt 安装在其他目录，请相应修改 `docker/linux.Dockerfile` 中的 `QT_ROOT`，并修改运行命令中的 Qt 卷挂载路径。

## 4. 首次构建 Docker 镜像

在仓库根目录运行一次：

```bash
docker build -f docker/linux.Dockerfile -t seisee-linux-builder .
mkdir -p dist/linux
```

镜像准备好后，每次打包都在仓库根目录运行：

```bash
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -e JOBS="$(nproc)" \
  -v /home/ww/Qt:/home/ww/Qt:ro \
  -v "$PWD:/workspace" \
  seisee-linux-builder
```

打包脚本会从 `SeiSeeMp/mainwindow.h` 读取 `#define VERSION`，安装包写入宿主机的 `dist/linux`。命令会把当前工作区挂载到容器，因此使用最新源码。为兼容旧命令，也可以继续通过 `-e VERSION="$VERSION"` 显式传入版本；本机直接运行脚本时同样可用 `VERSION=...` 覆盖头文件中的版本。Docker 构建的 Makefile、目标文件、界面头文件、静态库和可执行文件写入 `build/docker-release`，每次打包先在该目录执行 qmake `distclean` 再完整重建，不会清理本机 VS Code 构建目录。本机直接运行打包脚本则使用 `build/linux-package-release`。可通过修改 `JOBS` 调整并行构建任务数。只有修改 `docker/linux.Dockerfile` 或需要更新容器内工具时，才需要重新运行 `docker build`。首次构建镜像需要网络访问基础镜像仓库和 Dockerfile 中使用的 AppImage 工具下载地址。

CentOS 7 已停止维护，Dockerfile 从 Tsinghua 镜像的 CentOS 7.9.2009 归档仓库安装构建依赖；若该镜像无法访问，可在 `docker/linux.Dockerfile` 中替换对应仓库 URL。

AppImage 是供桌面 Linux 主机运行的安装包，不是 GUI Docker 容器；启动应用仍需要图形桌面及系统提供的图形、字体等运行环境。该 x86_64 构建以 glibc 2.17 为兼容基线，但不同发行版仍可能需要安装宿主机缺少的运行库。若 CentOS 7 上 AppImage 无法挂载，可尝试：

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./dist/linux/SeiSee-4.0.0-alpha.1-x86_64.AppImage
```
