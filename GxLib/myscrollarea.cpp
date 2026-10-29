#include <QApplication>
#include <QDebug>
#include <QWheelEvent>
#include <QWidget>

#include "myscrollarea.h"
#include "gfxselectionband.h"

MyScrollArea::MyScrollArea(QWidget *plt, int mrb)
{
    plot = plt;
    setWidget(plot);
    mv = 0;
    mvrb = mrb;
    setFrameStyle(0);
    rubberBand = NULL;
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    ignore = false;

    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this,
            SLOT(on_hScroll(int)));

    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this,
            SLOT(on_vScroll(int)));
}

void MyScrollArea::wheelEvent(QWheelEvent *event)
{
    if (selectionMode) {
        event->accept();
        return;
    }
    double d = event->delta();
    double z = 1 + 0.002 * abs(d);
    if (d < 0)
        z = 1 / z;

    int xm = event->pos().x();
    int ym = event->pos().y();

    double zx = z;
    double zy = z;

    if ((mvrb & 1) == 0)
        zy = 1;
    if ((mvrb & 2) == 0)
        zx = 1;

    emitZoomChanged(zx, zy, xm, ym);
}

void MyScrollArea::mousePressEvent(QMouseEvent *event)
{
    if (selectionMode) {
        if (event->button() != Qt::LeftButton)
            return;
        origin = event->pos();
        selecting = true;
        selectionEdges = 0;
        selectionStart = selectionBand->geometry();
        if (selectionBand->isVisible() &&
            selectionStart.adjusted(-6, -6, 6, 6).contains(origin)) {
            if (qAbs(origin.x() - selectionStart.left()) <= 6)
                selectionEdges |= 1;
            if (qAbs(origin.x() - selectionStart.right()) <= 6)
                selectionEdges |= 2;
            if (qAbs(origin.y() - selectionStart.top()) <= 6)
                selectionEdges |= 4;
            if (qAbs(origin.y() - selectionStart.bottom()) <= 6)
                selectionEdges |= 8;
            if (!selectionEdges)
                selectionEdges = 16; // Move existing window.
        } else {
            selectionBand->setGeometry(QRect(origin, QSize()));
            selectionBand->show();
        }
        return;
    }
    if (rubberBand) {
        if (event->button() == Qt::RightButton) {
            setCursor(Qt::ArrowCursor);

            rubberBand->hide();
            delete rubberBand;
            rubberBand = NULL;
        } else {
            origin = event->pos();

            rubberBand->setGeometry(QRect(origin, QSize()));
            rubberBand->show();
        }

        return;
    } else if (mvrb && event->button() == Qt::RightButton) {
        xo = event->x();
        yo = event->y();
        mv = 1;
        setCursor(Qt::OpenHandCursor);
    }
    emit mouseEvent(event);
}

void MyScrollArea::mouseReleaseEvent(QMouseEvent *event)
{
    if (selectionMode) {
        if (selecting && event->button() == Qt::LeftButton) {
            selecting = false;
            if (selectionBand->width() > 1 && selectionBand->height() > 1)
                emit selectionChanged(selectionBand->geometry());
        }
        return;
    }
    if (rubberBand) {
        setCursor(Qt::ArrowCursor);

        QRect r = rubberBand->geometry();

        rubberBand->hide();
        delete rubberBand;
        rubberBand = NULL;

        emit zoomW(r);
    } else if (event->button() == Qt::RightButton) {
        setCursor(Qt::ArrowCursor);
        mv = 0;
    }

    emit mouseEvent(event);
}

void MyScrollArea::mouseMoveEvent(QMouseEvent *event)
{
    if (selectionMode) {
        if (!selecting)
            return;
        const QRect bounds =
            viewport()->rect().intersected(QRect(plot->pos(), plot->size()));
        const QPoint current(
            qBound(bounds.left(), event->pos().x(), bounds.right()),
            qBound(bounds.top(), event->pos().y(), bounds.bottom()));
        QRect r = selectionStart;
        if (!selectionEdges)
            r = QRect(origin, current).normalized();
        else if (selectionEdges == 16) {
            r.translate(current - origin);
            r.moveLeft(
                qBound(bounds.left(), r.left(),
                       qMax(bounds.left(), bounds.right() - r.width() + 1)));
            r.moveTop(
                qBound(bounds.top(), r.top(),
                       qMax(bounds.top(), bounds.bottom() - r.height() + 1)));
        } else {
            if (selectionEdges & 1)
                r.setLeft(current.x());
            if (selectionEdges & 2)
                r.setRight(current.x());
            if (selectionEdges & 4)
                r.setTop(current.y());
            if (selectionEdges & 8)
                r.setBottom(current.y());
            r = r.normalized();
        }
        selectionBand->setGeometry(r.intersected(bounds));
        return;
    }
    int x = event->x();
    int y = event->y();

    if (rubberBand) {
        rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
        return;
    } else if (mv) {
        int dx = xo - x;
        int dy = yo - y;

        if ((mvrb & 1) == 0)
            dy = 0;
        if ((mvrb & 2) == 0)
            dx = 0;

        setHscroll(Hscroll() + dx);
        setVscroll(Vscroll() + dy);

        xo = x;
        yo = y;

        //      qDebug() << "Move x=" << x << "y=" << y << "dx=" << dx << "dy="
        //      << dy;

        x = Hscroll();
        y = Vscroll();

        if ((mvrb & 1) == 0)
            y = -1;
        if ((mvrb & 2) == 0)
            x = -1;

        emitZoomChanged(0, 0, x, y);
    }

    emit mouseEvent(event);
}

void MyScrollArea::on_vScroll(int)
{
    if (ignore)
        return;

    int y = verticalScrollBar()->value();

    emitZoomChanged(0, 0, -1, y);
}

void MyScrollArea::on_hScroll(int)
{
    if (ignore)
        return;

    int x = horizontalScrollBar()->value();

    emitZoomChanged(0, 0, x, -1);
}

void MyScrollArea::keyPressEvent(QKeyEvent *event)
{
    if (selectionMode) {
        event->accept();
        return;
    }
    emit keyEvent(event);
    QWidget::keyPressEvent(event);
}

void MyScrollArea::scrollContentsBy(int dx, int dy)
{
    QScrollArea::scrollContentsBy(dx, dy);
}

void MyScrollArea::setZoomW(int on)
{
    if (selectionMode)
        return;
    if (on && !rubberBand) {
        rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

        QPixmap a = QPixmap(":/images/ZoomWCurs.xpm");
        setCursor(QCursor(a, 10, 8));
    } else if (!on && rubberBand) {
        rubberBand->hide();
        delete rubberBand;
        rubberBand = NULL;
    }
}

void MyScrollArea::setSelectionMode(bool enabled)
{
    if (selectionMode == enabled)
        return;
    setZoomW(0);
    mv = 0;
    selecting = false;
    selectionMode = enabled;
    if (enabled) {
        if (!selectionBand)
            selectionBand = new GfxSelectionBand(viewport());
        setCursor(QCursor(QPixmap(":/images/SelectionBlackCurs.xpm"), 8, 8));
    } else {
        clearSelectionRectangle();
        unsetCursor();
    }
}

void MyScrollArea::clearSelectionRectangle()
{
    selecting = false;
    if (selectionBand)
        selectionBand->hide();
}

void MyScrollArea::emitZoomChanged(double zx, double zy, int x, int y)
{
    if (selectionMode)
        return;
    // zx==0 && zy==0 - scrolling to xc, yc;
    // else           - central zoom

    emit zoomChanged(zx, zy, x, y);
}

void MyScrollArea::setVscroll(int v)
{
    ignore = true;

    if ((mvrb & 1) == 0)
        v = 0;

    verticalScrollBar()->setValue(v);

    ignore = false;
}

void MyScrollArea::setHscroll(int v)
{
    ignore = true;

    if ((mvrb & 2) == 0)
        v = 0;

    horizontalScrollBar()->setValue(v);

    ignore = false;
}
