#include "spectrumdialog.h"
#include <QImage>
#include <QPainter>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLayout>
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

void SpectrumDialog::updateResponsiveGroups(
    QFrame *frame, const QList<QFrame *> &hideOrder)
{
    auto layout = qobject_cast<QHBoxLayout *>(frame->layout());
    if (!layout)
        return;

    for (QFrame *group : hideOrder)
        group->show();

    const auto updateSeparators = [layout]() {
        for (int i = 0; i < layout->count(); ++i) {
            auto separator = qobject_cast<QFrame *>(
                layout->itemAt(i)->widget());
            if (!separator ||
                !separator->objectName().endsWith("Separator")) {
                continue;
            }

            QWidget *previous = nullptr;
            QWidget *next = nullptr;
            for (int j = i - 1; j >= 0; --j) {
                QWidget *widget = layout->itemAt(j)->widget();
                if (widget &&
                    !widget->objectName().endsWith("Separator")) {
                    previous = widget;
                    break;
                }
            }
            for (int j = i + 1; j < layout->count(); ++j) {
                QWidget *widget = layout->itemAt(j)->widget();
                if (widget &&
                    !widget->objectName().endsWith("Separator")) {
                    next = widget;
                    break;
                }
            }
            separator->setVisible(previous && next &&
                                  !previous->isHidden() &&
                                  !next->isHidden());
        }
    };
    updateSeparators();

    const QMargins margins = layout->contentsMargins();
    const int availableWidth =
        frame->contentsRect().width() - margins.left() - margins.right();
    const auto minimumWidth = [layout]() {
        int width = 0;
        int visibleGroups = 0;
        for (int i = 0; i < layout->count(); ++i) {
            QWidget *widget = layout->itemAt(i)->widget();
            if (!widget || widget->isHidden())
                continue;
            width += qMax(widget->minimumWidth(),
                          widget->minimumSizeHint().width());
            ++visibleGroups;
        }
        if (visibleGroups > 1)
            width += layout->spacing() * (visibleGroups - 1);
        return width;
    };

    for (QFrame *group : hideOrder) {
        if (minimumWidth() <= availableWidth)
            break;
        group->hide();
        updateSeparators();
    }

    layout->invalidate();
    layout->activate();
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
