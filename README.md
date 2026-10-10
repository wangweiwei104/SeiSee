<div align="center">
  <h1 align="center">地震数据查看器SeiSee</h1>
</div>

<div align="center">一个支持多平台的地震数据查看工具,源代码开发者为：https://mail.dmng.ru/freeware/</div>
<br>
<p align="center">
  <a href="https://github.com/wangweiwei104/SeiSee/releases/latest">
    <img src="https://img.shields.io/github/v/release/wangweiwei104/SeiSee" />
  </a>
  <a href="https://github.com/wangweiwei104/SeiSee/releases/latest">
    <img src="https://img.shields.io/github/downloads/wangweiwei104/SeiSee/total" />
  </a>
  <a href="https://github.com/wangweiwei104/SeiSee/fork">
    <img src="https://img.shields.io/github/forks/wangweiwei104/SeiSee" />
  </a>
  <a href="https://github.com/wangweiwei104/SeiSee/star">
    <img src="https://img.shields.io/github/stars/wangweiwei104/SeiSee" />
  </a>
</p>

## 界面

![主界面](screenshots/home.png)
主界面

![截图功能](screenshots/capture.png)
截图功能

![difference](screenshots/difference.png)
difference

![axissetup](screenshots/axissetup.png)
axis setup

![process](screenshots/process.png)
process

## 功能

- 增加地震数据的右侧和底部坐标轴，形成四周对称式坐标，符合国人审美和行业通用标准
- 增加地震数据截图功能，并使用带圆角提示框显示操作结果。
- 增加图件导出功能，提供300 400 600DPI图件导出。
- 增加打开文件入口。
- 增加 Difference功能，用于计算两个地震数据的差值。
- 增加AGC的Statistical Type，包含ABS/RMS。
- 增加文件目录浏览界面的右键菜单，可以获取文件名、文件路径、打开文件所在目录
- 改进高 DPI、多显示器环境下的界面布局和显示效果。
- 统一界面字体、字体大小及坐标轴等显示样式的设置方式。
- 调整 Axis Setup 和多个控件的布局与间距。
- 改进图标在不同 DPI 屏幕上的显示，并支持窗口在不同 DPI 屏幕间移动。
- 补全道头字段（至240，兼容GeoEast）显示。
- 实现 Save as 界面 apply process的实际功能
- 精心设计的高DPI图标。

## 安装方法

### Windows

从 [GitHub Releases](https://github.com/wangweiwei104/SeiSee/releases/latest) 下载最新 Windows 安装程序，运行安装程序并按提示完成安装。安装完成后可从开始菜单启动 SeiSee。

### Linux

从 [GitHub Releases](https://github.com/wangweiwei104/SeiSee/releases/latest) 下载适用于 x86_64 的 `.AppImage` 文件，在终端中进入下载目录后运行：

```bash
chmod +x SeiSee-*.AppImage
./SeiSee-*.AppImage
```

Linux 版本以 glibc 2.17 为兼容基线，目标支持 CentOS 7 及以上、Ubuntu 20.04 及以上的 x86_64 系统。运行需要桌面环境及宿主系统所需的图形运行库。若 CentOS 7 上无法挂载 AppImage，可尝试：

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./SeiSee-*.AppImage
```

如果提示 `error loading libfuse.so.2`，可用上述 `APPIMAGE_EXTRACT_AND_RUN=1` 方式启动，或在系统中安装 FUSE 2 兼容运行库后再正常启动。

开发者构建安装包、配置编译环境或参与项目开发，请参阅[打包与开发说明](./scripts/README.md)和 [Docker 构建说明](./docker/README.md)。

## 用户设置保存位置

程序使用 Qt `QSettings` 保存窗口布局、数据目录和显示选项等用户设置：

- **Windows：**注册表 `HKEY_CURRENT_USER\Software\WW\SeiSeeMp`
- **Linux：**配置文件 `~/.config/WW/SeiSeeMp.conf`（`~` 表示当前用户的主目录）

如果 Linux 设置了 `XDG_CONFIG_HOME` 环境变量，配置文件会位于 `$XDG_CONFIG_HOME/PSI/SeiSeeMp.conf`。

## 更新日志

[更新日志](./CHANGELOG.md)

## 赞赏

<div>暂不需要~</div>


## 关注

不用


## 免责声明

本项目仅供学习交流用途，开发者不负任何责任

## 许可证

[MIT](./LICENSE) License &copy; 2025-PRESENT [wangweiwei104](https://github.com/wangweiwei104)
