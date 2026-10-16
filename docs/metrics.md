# Metrics 指标分析（Windows / Linux）

## 新增功能

Windows 和 Linux 使用同一套指标界面与计算源码。先打开地震数据，再选择 **Processing → Metrics**。

- **Reference comparison（双文件）**：选择参考 SEG-Y 与比较 SEG-Y；道数、采样点数和采样间隔必须一致。
- **Self-assessment（单文件）**：使用当前文件；经典指标以所选道的平均道为参考。此模式的分数不能直接解释为真实恢复精度。
- 支持 MSE、RMSE、PSNR、SNR、SSIM；单文件模式支持 TPRS。
- 可指定道范围、道步长、时间范围（ms，相对记录起点），查看概览及详细报告。
- **Calculate / Stop** 执行或停止计算；停止后丢弃未完成报告。**Save Results** 导出 TXT。
- 错误信息和保存路径显示在窗口下方状态栏；计算过程中锁定输入，避免文件或范围发生变化。

保留本地指标公式。SSIM 使用逐道的一维窗口算法；TPRS 沿用本地实现，本次未重新论证其科学适用性。双文件完全一致时，原实现使用 `1e30` 表示 PSNR/SNR 的零误差情形。

## 两平台构建与发布

主工程 `GxApps.pro` 已包含新增源文件；原 Windows 安装包、Linux AppImage 打包路径继续使用。分别参照 [打包说明](../scripts/README.md) 和 [Linux Docker 说明](../docker/README.md)。

本次已构建并验证 **Windows x64 / Qt 5.15.2 / MSVC 2022** 便携版本。**本次没有构建或运行 Linux 二进制**；Linux 需要在原有 Linux 构建环境中重新打包并验证，不能将此前发布的 AppImage 视为已包含 Metrics。

回归测试使用合成 SEG-Y，覆盖相同文件的 MSE=0、SSIM=1；半幅信号的 MSE 与 SNR=6.020599913 dB；指定道/时间范围；参考文件切换；单文件均值参考及 TPRS；报告导出；停止状态；采样间隔不一致校验。Windows 100% / 150% 缩放和便携运行库环境均为 **5 项通过、0 失败**（含初始化与清理）。已检查离屏界面图，尚未全面验证真实大体积数据、原生文件对话框及多显示器。

## 开发者回归测试

先在仓库外构建完整主工程，再将测试程序建到该构建目录的 `MetricsRegression` 子目录，以复用兄弟目录中的静态库：

```bash
# Linux 示例，Qt 5 开发环境（需要 QtTest / QtSvg）
repo="$(pwd)"
mkdir -p ../seisee-build
cd ../seisee-build
qmake "$repo/GxApps.pro" CONFIG+=release
make -j2
mkdir -p MetricsRegression
cd MetricsRegression
qmake "$repo/SeiSeeMp/MetricsRegression.pro" CONFIG+=release
make -j2
mkdir -p results
QT_QPA_PLATFORM=offscreen SEISEE_TEST_OUT="$PWD/results" ./metrics_regression
```

Windows 在 MSVC 开发者终端中使用同样的目录结构、qmake 和 nmake，测试程序位于 `MetricsRegression/release/metrics_regression.exe`。Qt DLL、平台插件和可用字体必须可被找到；缩放回归分别设置 `QT_SCALE_FACTOR=1` 和 `1.5`。上述 Linux 命令为构建指引，尚未在本次环境执行。

## 集成范围

新增 `SeiSeeMp/metricsdialog.*`、`metricsworker.*`、菜单入口及 QtTest 回归。停止标志改为原子变量，待计算线程结束后恢复界面；主窗口在指标计算期间拒绝关闭，析构时等待线程。修复原 Difference 对话框文件指针未初始化导致的退出崩溃，补充 Windows 注册表函数所需的 `advapi32` 链接。
