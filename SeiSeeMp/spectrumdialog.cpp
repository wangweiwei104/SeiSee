#include "spectrumdialog.h"
#include <QImage>
#include <QPainter>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QPushButton>
#include "Toast.h"

void SpectrumDialog::setExportDpi(int dpi)
{
    m_exportDpi = (dpi == 400 || dpi == 600) ? dpi : 300;
    if (auto button = findChild<QPushButton *>("fkExport"))
        button->setToolTip(
            tr("Export high-resolution image (%1 DPI)").arg(m_exportDpi));
}
void SpectrumDialog::setCaptureRange(const QString &source, int first, int last,
                                     double startSeconds, double endSeconds)
{
    captureSource = source;
    captureFirst = first;
    captureLast = last;
    captureStart = startSeconds;
    captureEnd = endSeconds;
}
QString SpectrumDialog::nextImagePath(const QString &details, int dpi) const
{
    const QFileInfo source(captureSource);
    QString name = QString("%1_%2_%3_%4_%5_%6")
                       .arg(source.baseName())
                       .arg(captureFirst)
                       .arg(captureLast)
                       .arg(QString::number(captureStart, 'g', 8))
                       .arg(QString::number(captureEnd, 'g', 8))
                       .arg(details);
    if (dpi > 0)
        name += QString("_%1").arg(dpi);
    const QDir output(source.absolutePath());
    QString filename = output.filePath(name + ".png");
    for (int i = 1; QFile::exists(filename); ++i)
        filename = output.filePath(QString("%1_%2.png").arg(name).arg(i));
    return filename;
}
void SpectrumDialog::imageExportNotice(bool success, const QString &filename)
{
    auto notice = new Toast(this);
    notice->setDuration(5000);
    notice->setTitle(success ? tr("Image saved") : tr("Image export failed"));
    notice->setText(
        success
            ? QDir::toNativeSeparators(filename)
            : tr("Could not save %1. Check the directory or use a lower DPI.")
                  .arg(filename));
    notice->setMaximumWidth(qMax(300, width() / 2));
    notice->applyPreset(success ? ToastPreset::SUCCESS
                                : ToastPreset::ERROR_NOTIFICATION);
    notice->show();
}

bool SpectrumDialog::savePlotImage(QWidget *plot, const QString &filename,
                                   int dpi)
{
    if (dpi <= 0)
        return plot->grab().save(filename, "PNG");
    const double factor = dpi / double(plot->logicalDpiX());
    const QSize size = plot->size() * factor;
    if (size.isEmpty() || qint64(size.width()) * size.height() > 64000000)
        return false;
    QImage image(size, QImage::Format_RGB32);
    if (image.isNull())
        return false;
    image.fill(Qt::white);
    image.setDotsPerMeterX(qRound(dpi / .0254));
    image.setDotsPerMeterY(qRound(dpi / .0254));
    QPainter painter(&image);
    painter.scale(factor, factor);
    plot->render(&painter);
    painter.end();
    return image.save(filename, "PNG");
}
