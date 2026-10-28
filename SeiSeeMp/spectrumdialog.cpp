#include "spectrumdialog.h"
#include <QImage>
#include <QPainter>

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
