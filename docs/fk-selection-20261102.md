# FK 框选交互修复

开启 FK 框选后，切换文件会保留所选道号与时间范围，并按新文件坐标重新定位选框。新文件较短或道数较少时仅显示重叠部分，原输入参数不变；没有重叠时隐藏，切回后恢复。取消框选或关闭 FK 窗口后清除选框。

修改起止道号和起止时间，按 Enter 或移开输入焦点确认数值后，主图选框同步改变。鼠标拖动和调整选框仍会更新输入参数。文件切换与范围修改不会自动计算频谱。

`FkSpectrumDialog::inputRangeChanged()` 通知范围变化，`MainWindow::syncFkSelectionRectangle()` 使用当前文件与主图坐标变换重建选框。`MyScrollArea::setSelectionRectangle()` 裁剪显示范围，不回发鼠标选区信号，避免像素取整反复改写参数。

## 验证

`tests/fk_regression.cpp` 使用合成 SEG-Y，覆盖键盘输入四个范围参数、小数毫秒、同尺寸文件切换、不同道数和采样间隔、无重叠文件、切回恢复、鼠标移动后的跨文件保留、取消框选以及不触发自动计算。

先按普通 `GxApps.pro` 构建依赖；在应用构建目录执行 qmake 时添加 `CONFIG+=fk_regression`，生成测试程序，运行时通过 `SEISEE_TEST_OUT` 指定测试输出目录。可用 `QT_QPA_PLATFORM=offscreen` 和 `QT_SCALE_FACTOR=1.5` 检查不同缩放。测试设置和合成文件保存在输出目录，不修改用户数据。

Windows x64 Release / Qt 5.15.2 / MSVC 2022 完整应用编译通过。100% 和 150% 缩放的专项回归均为 3 项通过、0 失败（含初始化和清理）。验证使用合成 SEG-Y 与 Qt 离屏界面，未覆盖真实超大文件或多显示器手工操作。
