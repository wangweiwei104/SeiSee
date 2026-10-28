#ifndef GFXSELECTIONBAND_H
#define GFXSELECTIONBAND_H

#include <QPainter>
#include <QWidget>

// Shared analysis selection: the main-window ZoomW appearance, independent of
// platform QRubberBand masks and whether the modeless window has focus.
class GfxSelectionBand : public QWidget
{
public:
    explicit GfxSelectionBand(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName("analysisSelectionBand");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(QRect());
        hide(); // Child widgets otherwise appear when the plot first shows.
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const QColor border =
            palette().color(QPalette::Active, QPalette::Highlight);
        QColor fill = border;
        fill.setAlpha(64);
        QPainter painter(this);
        painter.fillRect(rect(), fill);
        QPen pen(border);
        pen.setWidth(
            0); // One physical pixel, matching the main selection edge.
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

#endif
