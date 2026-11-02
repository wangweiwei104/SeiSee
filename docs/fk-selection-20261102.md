# FK 框选交互修复（2026-11-02）

在 FK 窗口开启框选后，切换主窗口数据文件会保留所选道号和时间范围，并在新文件完成显示布局后重新定位选框。同尺寸文件的选框位置保持一致；新文件道数或记录时长较小时，仅显示选区与文件的重叠部分，原输入参数不变。没有重叠时暂时隐藏选框，切回有重叠的文件后恢复。取消框选或关闭 FK 窗口后不再恢复。

修改 First / Last trace、Start / End time 后，按 Enter 或离开输入框确认数值，主图选框同步移动或改变大小。拖动和调整选框仍会反向更新这些参数。道间距和频谱显示范围不控制主图输入选框。切换文件和修改范围不自动计算频谱，仍需点击 Calculate。

实现位置：`SpectrumDialog::inputRangeChanged()` 提供输入变化通知；`MainWindow::syncSpectrumSelectionRectangle()` 使用当前文件的道坐标和主图坐标变换重建选框；`MyScrollArea::setSelectionRectangle()` 裁剪到可见区域，不回发鼠标选区信号，避免像素取整反复改写参数。合并版中 FK 和频谱窗口共用此接口。

合并版的验证范围及结果见 [频谱与速度拾取合并说明](combined-spectra-velocity-20261102-r2.md)。回归使用合成 SEG-Y，覆盖实际键盘输入四个范围参数、小数毫秒、同尺寸文件切换、不同道数/采样间隔、无重叠文件、切回恢复、鼠标移动后的跨文件保留、取消框选以及不触发区域自动计算。

测试入口为应用 qmake 配置 `CONFIG+=fk_regression`，运行前设置 `SEISEE_TEST_OUT` 指定测试输出目录。区域计算仍需点击 Calculate。
