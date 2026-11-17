#include "myhugetable.h"

#include <QApplication>
#include <QDebug>
#include <QFontMetrics>
#include <QKeyEvent>

MyHugeTable::MyHugeTable(QWidget *parent)
    : QWidget(parent)
    , m_interfaceScale(1.0)
{
    vScrollBar = new QScrollBar(Qt::Vertical, this);
    hScrollBar = new QScrollBar(Qt::Horizontal, this);

    cWidget = new MyHcWidget(this);
    //  QPalette Pal2(palette());
    //  Pal2.setColor(QPalette::Background, Qt::green);
    cWidget->setAutoFillBackground(true);
    //  cWidget->setPalette(Pal2);

    //    QPalette Pal1(palette());
    //    Pal1.setColor(QPalette::Background, Qt::yellow);

    tWidget = new MyHtWidget(this);

    txWidget = new QWidget(this);
    txWidget->setAutoFillBackground(true);
    //  txWidget->setPalette(Pal1);

    cxWidget = new QWidget(this);
    cxWidget->setFixedWidth(17);
    cxWidget->setAutoFillBackground(true);
    //  cxWidget->setPalette(Pal1);

    colHeaderFrame = new QWidget(this);
    //  colhFrame->setAutoFillBackground(true);
    //  colhFrame->setPalette(Pal1);

    QVBoxLayout *topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    QFrame *gridFrame = new QFrame(this);

    topLayout->addWidget(colHeaderFrame);
    topLayout->addWidget(gridFrame);

    QGridLayout *colhLayout = new QGridLayout(colHeaderFrame);
    colhLayout->setContentsMargins(0, 0, 0, 0);
    colhLayout->setSpacing(0);

    colhLayout->addWidget(cWidget, 0, 0);
    colhLayout->addWidget(cxWidget, 0, 1);

    QGridLayout *gridLayout = new QGridLayout(gridFrame);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(0);

    gridLayout->addWidget(tWidget, 0, 0);
    gridLayout->addWidget(vScrollBar, 0, 1);
    gridLayout->addWidget(hScrollBar, 1, 0);
    gridLayout->addWidget(txWidget, 1, 1);

    connect(vScrollBar, SIGNAL(valueChanged(int)), this, SLOT(vChanged(int)));

    connect(hScrollBar, SIGNAL(valueChanged(int)), this, SLOT(hChanged(int)));

    connect(cWidget, SIGNAL(colEvent(int)), this, SLOT(hcCol(int)));

    updateHeaderHeight();

    setRowCount(0);

    //  tWidget->setFocusPolicy(Qt::TabFocus);
    //  tWidget->setFocus();

    m_crow = -1;
    m_ccol = -1;
}

void MyHugeTable::setInterfaceScale(qreal scale)
{
    m_interfaceScale = qMax<qreal>(0.1, scale);
    updateCharacterColumnWidths();
    updateHeaderHeight();
    resizeEvent(nullptr);
    updateGeometry();
    update();
    cWidget->update();
    tWidget->update();
}

int MyHugeTable::columnWidth(int n) const
{
    return qRound(m_cols[n].width * m_interfaceScale);
}

int MyHugeTable::rowHeight() const
{
    return qMax(1, QFontMetrics(tWidget->font()).height() +
                       qRound(4 * m_interfaceScale));
}

int MyHugeTable::headerHeight() const
{
    return qMax(1, QFontMetrics(cWidget->font()).height() +
                       qRound(4 * m_interfaceScale));
}

void MyHugeTable::updateHeaderHeight()
{
    const int height = headerHeight();
    colHeaderFrame->setFixedHeight(height);
    cWidget->setFixedHeight(height);
    updateGeometry();
}

void MyHugeTable::updateCharacterColumnWidths()
{
    const QFontMetrics metrics(tWidget->font());
    const int padding = qRound(4 * m_interfaceScale);

    for (int column = 0; column < colCount(); ++column) {
        const int characterCount = m_cols[column].characterCount;
        if (characterCount < 1)
            continue;

        const int pixelWidth =
            metrics.horizontalAdvance(
                QString(characterCount, QLatin1Char('0'))) +
            padding;
        m_cols[column].width =
            qMax(1, qRound(pixelWidth / m_interfaceScale));
    }

    cWidget->update();
    resizeEvent(nullptr);
}

void MyHugeTable::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() != QEvent::FontChange)
        return;

    updateCharacterColumnWidths();
    updateHeaderHeight();
    tWidget->update();
    cWidget->update();
}

QString MyHugeTable::getCellData(int nrow, int ncol)
{
    int rc = receivers(SIGNAL(cellDataRequest(int, int, QString &)));
    if (rc < 1)
        return "";

    QString data;

    emit cellDataRequest(nrow, ncol, data);

    return data;
}

void MyHugeTable::sendRowEvent(qint64 nrow, int mode)
{
    emit rowEvent(nrow, mode);
}

void MyHugeTable::setCurCell(qint64 row, int col)
{
    m_ccol = col;
    setCurRow(row);
}

void MyHugeTable::setCurRow(qint64 row)
{
    if (m_crow == row)
        return;

    if (row < 0)
        row = 0;
    if (row >= rowCount())
        row = rowCount() - 1;

    m_crow = row;

    tWidget->scrollToCurRow();

    tWidget->update();

    emit rowEvent(row, 1);
}

void MyHugeTable::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    int stp;
    int hmax;
    int wmax;

    qint64 nr = rowCount();

    int h = tWidget->height();
    int w = tWidget->width();

    stp = h / rowHeight();
    hmax = int(nr - stp);
    if (hmax <= 0)
        hmax = 0;

    vScrollBar->setMaximum(hmax);
    vScrollBar->setPageStep(stp);

    stp = w - 1;
    wmax = colsW() - stp;
    if (wmax <= 0)
        wmax = 0;

    hScrollBar->setMaximum(wmax);
    hScrollBar->setPageStep(stp);

    if (wmax <= 0) {
        hScrollBar->setVisible(false);
    } else {
        hScrollBar->setVisible(true);
    }

    if (hmax <= 0) {
        vScrollBar->setVisible(false);
    } else {
        vScrollBar->setVisible(true);
    }

    if (wmax <= 0 || hmax <= 0) {
        txWidget->setVisible(false);
    } else {
        txWidget->setVisible(true);
    }
}

void MyHugeTable::setRowCount(qint64 n)
{
    m_nrows = n;

    resizeEvent(NULL);
    tWidget->update();
}

void MyHugeTable::setColCount(int n)
{
    //  int w = tWidget->width();

    m_cols.resize(n);

    resizeEvent(NULL);

    tWidget->update();
    cWidget->update();
}

int MyHugeTable::colCount()
{
    return m_cols.count();
}

void MyHugeTable::vChanged(int val)
{
    Q_UNUSED(val);

    tWidget->update();
}

void MyHugeTable::hChanged(int val)
{
    Q_UNUSED(val);

    tWidget->update();
    cWidget->update();
}

void MyHugeTable::hcCol(int col)
{
    // qDebug() << "c=" << col;
    emit headerEvent(col);
}

void MyHugeTable::setColLabel(int n, QString v)
{
    if (n < 0 || n >= colCount())
        return;

    m_cols[n].hdr = v;

    cWidget->update();
}

QString MyHugeTable::colLabel(int n)
{
    if (n < 0 || n >= colCount())
        return "";

    return m_cols[n].hdr;
}

void MyHugeTable::setColHdrBgc(int n, QColor v)
{
    int l = colCount();

    if (n < 0) {
        int i;
        for (i = 0; i < l; i++) {
            m_cols[i].hbgc = Qt::white;
        }
        cWidget->update();
        return;
    }

    if (n < 0 || n >= l)
        return;

    m_cols[n].hbgc = v;
    cWidget->update();
}

void MyHugeTable::setColWidth(int n, int v)
{
    int l = colCount();
    if (n < 0 || n >= l)
        return;

    m_cols[n].width = v;
    m_cols[n].characterCount = 0;
    cWidget->update();
    resizeEvent(NULL);
}

void MyHugeTable::setColWidthInCharacters(int n, int characterCount)
{
    if (n < 0 || n >= colCount() || characterCount < 1)
        return;

    m_cols[n].characterCount = characterCount;
    updateCharacterColumnWidths();
}

void MyHugeTable::setColTag(int n, int v)
{
    int l = colCount();
    if (n < 0 || n >= l)
        return;

    m_cols[n].tag = v;
}

int MyHugeTable::colTag(int n)
{
    int l = colCount();
    if (n < 0 || n >= l)
        return 0;

    return m_cols[n].tag;
}

int MyHugeTable::colsW()
{
    int W = 0;

    int ww = 0;
    int nw = 0;

    int n;
    int nc = colCount();

    for (n = 0; n < nc; n++) {
        int wc = columnWidth(n);

        if (wc > 0) {
            ww = ww + wc;
            nw++;
        }
    }

    nw = colCount() - nw;

    int w = tWidget->width() - 1;

    if (nw > 0) {
        ww = (w - ww) / (nw);
    }

    for (n = 0; n < nc; n++) {
        int wc = columnWidth(n);
        if (wc < 1)
            wc = ww;
        if (wc < qRound(30 * m_interfaceScale))
            wc = qRound(30 * m_interfaceScale);

        W = W + wc;
    }

    return W;
}

//-----------------------------------------------------------------------

MyHtWidget::MyHtWidget(MyHugeTable *parent)
{
    p = parent;
    lEdit = new QLineEdit(this);
    lEdit->hide();

    connect(lEdit, SIGNAL(editingFinished()), this,
            SLOT(on_lEdit_editingFinished()));
}

void MyHtWidget::scrollToCurRow()
{
    int h = height();
    int ro = p->vScrollBar->value();

    qint64 nr1 = ro;
    qint64 nr2 = ro + (h / p->rowHeight());
    qint64 nr;

    int cr = p->curRow();

    if (cr < nr1)
        nr1 = cr;
    if (cr >= nr2)
        nr1 = cr - h / p->rowHeight() + 1;

    if (nr1 < 0)
        nr1 = 0;

    p->vScrollBar->setValue(nr1);

    p->update();
}

void MyHtWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    int w = this->width() - 1;
    int h = this->height();

    QPainter painter(this);
    const QFontMetrics fontMetrics(painter.font());
    const int textBaselineOffset =
        (p->rowHeight() - fontMetrics.height()) / 2 + fontMetrics.ascent();

    qint64 nr;
    int ro = p->vScrollBar->value();
    int xo = p->hScrollBar->value();

    if (ro < 0)
        ro = 0;

    qint64 nr1 = ro;
    qint64 nr2 = ro + (h / p->rowHeight());

    int nc;
    int ww = 0;
    int nw = 0;

    for (nc = 0; nc < p->colCount(); nc++) {
        int wc = p->columnWidth(nc);

        if (wc > 0) {
            ww = ww + wc;
            nw++;
        }
    }

    nw = p->colCount() - nw;

    if (nw > 0) {
        ww = (w - ww) / (nw);
    }

    //    if(ww<=0) ww=10;

    for (nr = nr1; nr < p->rowCount() && nr <= nr2; nr++) {
        int x = 0;
        int wc = 0;

        for (nc = 0; nc < p->colCount(); nc++) {
            wc = p->columnWidth(nc);
            if (wc < 1)
                wc = ww;
            if (wc < qRound(30 * p->m_interfaceScale))
                wc = qRound(30 * p->m_interfaceScale);

            if (nr == p->curRow()) {
                painter.fillRect(QRect(x - xo, (nr - ro) * p->rowHeight(), wc,
                                       p->rowHeight()),
                                 Qt::lightGray);
            }

            QString data = p->getCellData(nr, nc);

            painter.drawText(x - xo + qRound(2 * p->m_interfaceScale),
                             (nr - ro) * p->rowHeight() + textBaselineOffset,
                             data);
            painter.drawRect(QRect(x - xo, (nr - ro) * p->rowHeight(), wc,
                                   p->rowHeight()));
            x += wc;
        }
    }

    //    painter.drawPixmap(dx,dy,pw,ph,p);
}

int MyHtWidget::xy2cr(int x, int y, int &c, qint64 &r)
{
    int ro = p->vScrollBar->value();
    int co = p->hScrollBar->value();

    y = y / p->rowHeight();

    r = ro + y;

    x = x + co;

    int nc;
    int ww, nw, wc;

    nw = 0;
    ww = 0;

    c = -1;

    for (nc = 0; nc < p->colCount(); nc++) {
        wc = p->columnWidth(nc);

        if (wc > 0) {
            //          qDebug() << "nc=" << nc << "x=" << x << " ww=" << ww;
            if (x >= ww && x < ww + wc) {
                c = nc;
                break;
            }
            ww = ww + wc;
            nw++;
        }
    }

    if (c < 0)
        return 0;

    //  qDebug()<< "c=" << c << " r=" << r;

    return 1;
}

void MyHtWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    //    int     ro = p->vScrollBar->value();
    //    int     co = p->hScrollBar->value();

    int cx = 0;
    qint64 ry = 0;

    xy2cr(event->x(), event->y(), cx, ry);

    /*
        int y = event->y() / p->rowHeight();

        int     x=   event->x();

        int r = ro+y;

        int nc;
        int ww,nw,wc;

        nw=0;
        ww=0;

        int c = -1;

        for(nc=0;nc<p->colCount();nc++)
        {
            wc = p->columnWidth(nc);

            if(wc>0)
            {
    //          qDebug() << "nc=" << nc << "x=" << x << " ww=" << ww;
                if(x>=ww && x<ww+wc)
                {
                  c=nc;
                  break;
                }
                ww=ww+wc;
                nw++;
            }
        }

        if(c<0) return;


        // qDebug() << "ro=" << ro << "r=" << r;

        QString data = p->getCellData(r,c);

        p->setCurRow(r);

        lEdit->setFocus();
        lEdit->move(ww, (r - ro) * p->rowHeight());
        lEdit->resize(wc, p->rowHeight());
        lEdit->setText(data);
        lEdit->show();
    */
}

void MyHtWidget::mousePressEvent(QMouseEvent *event)
{
    int c = 0;
    qint64 r = 0;

    xy2cr(event->x(), event->y(), c, r);
    /*
        int     ro = p->vScrollBar->value();
        int y = event->y() / p->rowHeight();

        int r = ro+y;

    //  qDebug() << "r=" << r << "y=" << y;
    */
    setFocus();

    p->setCurCell(r, c);
    p->setCurRow(r);
}

void MyHtWidget::wheelEvent(QWheelEvent *event)
{
    int crow = p->curRow();

    double d = event->delta() / 120;

    p->setCurRow(crow - d);

    //  qDebug() << "d=" << d;
}

void MyHtWidget::keyPressEvent(QKeyEvent *event)
{
    //  qDebug() << "key event in board:" << event->key();
    // emit keyCaught(event);

    setFocus();

    int vpos = p->vScrollBar->value();
    int vstp = p->vScrollBar->pageStep();
    int vmax = p->vScrollBar->maximum();
    int hpos = p->hScrollBar->value();
    int hstp = p->hScrollBar->pageStep();
    int hmax = p->hScrollBar->maximum();

    int crow = p->curRow();

    int key = event->key();

    if (key == Qt::Key_Down) {
        p->setCurRow(crow + 1);
    } else if (key == Qt::Key_Up) {
        p->setCurRow(crow - 1);
    } else if (key == Qt::Key_PageDown) {
        p->setCurRow(crow + vstp);
    } else if (key == Qt::Key_PageUp) {
        p->setCurRow(crow - vstp);
    } else if (key == Qt::Key_Right) {
        hpos = hpos + 10;
    } else if (key == Qt::Key_Left) {
        hpos = hpos - 10;
    } else if (key == Qt::Key_Home) {
        if (QApplication::keyboardModifiers() &
            Qt::ControlModifier) { // Controll also is pressed
            p->setCurRow(0);
        } else {
            hpos = 0;
        }
    } else if (key == Qt::Key_End) {
        if (QApplication::keyboardModifiers() &
            Qt::ControlModifier) { // Controll also is pressed
            p->setCurRow(p->rowCount() - 1);
        } else {
            hpos = hmax;
        }
    }

    if (vpos >= vmax)
        vpos = vmax;
    if (vpos < 0)
        vpos = 0;

    if (hpos >= vmax)
        hpos = hmax;
    if (hpos < 0)
        hpos = 0;
    /*
      if(vpos!=p->vScrollBar->value() ||
         hpos!=p->hScrollBar->value()  )
      {
        p->vScrollBar->setValue(vpos);
        p->hScrollBar->setValue(hpos);
        update();
      }
    */
}

//-----------------------------------------------------------------------

MyHcWidget::MyHcWidget(MyHugeTable *parent)
{
    p = parent;

    //  setFocusPolicy(Qt::ClickFocus);
    //  setFocusPolicy(Qt::StrongFocus);
}

void MyHcWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange && p->cWidget == this)
        p->updateHeaderHeight();
}

void MyHcWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    int w = this->width() - 1;
    //  int h = this->height();

    QPainter painter(this);
    const QFontMetrics fontMetrics(painter.font());
    const int textBaselineOffset =
        (p->headerHeight() - fontMetrics.height()) / 2 +
        fontMetrics.ascent();

    int ro = p->vScrollBar->value();
    int xo = p->hScrollBar->value();

    if (ro < 0)
        ro = 0;

    int nc;
    int ww = 0;
    int nw = 0;

    for (nc = 0; nc < p->colCount(); nc++) {
        int wc = p->columnWidth(nc);

        if (wc > 0) {
            ww = ww + wc;
            nw++;
        }
    }

    nw = p->colCount() - nw;

    if (nw > 0) {
        ww = (w - ww) / (nw);
    }

    int x = 0;
    int wc = 0;
    QColor hc;

    for (nc = 0; nc < p->colCount(); nc++) {
        wc = p->columnWidth(nc);
        hc = p->m_cols[nc].hbgc;
        if (wc < 1)
            wc = ww;
        if (wc < 30)
            wc = 30;

        QString data = p->colLabel(nc);

        QBrush b = QBrush(hc);
        painter.fillRect(QRect(x - xo, 0, wc, p->headerHeight()), b);

        painter.drawText(x - xo + qRound(2 * p->m_interfaceScale),
                         textBaselineOffset, data);
        painter.drawRect(QRect(x - xo, 0, wc, p->headerHeight()));
        x += wc;
    }
}

int MyHcWidget::x2c(int x)
{
    int nc, c;
    int ww, nw, wc;

    nw = 0;
    ww = 0;

    c = -1;

    for (nc = 0; nc < p->colCount(); nc++) {
        wc = p->columnWidth(nc);

        if (wc > 0) {
            //          qDebug() << "nc=" << nc << "x=" << x << " ww=" << ww;
            if (x >= ww && x < ww + wc) {
                c = nc;
                break;
            }
            ww = ww + wc;
            nw++;
        }
    }

    //  qDebug()<< "c=" << c << " r=" << r;

    return c;
}

void MyHcWidget::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);

    int col = x2c(event->x());

    emit colEvent(col);

    //  xy2cr(event->x(),event->y(),c,r);

    /*
        int     ro = p->vScrollBar->value();
        int     y=   event->y()/18;

        int r = ro+y;

    //  qDebug() << "r=" << r << "y=" << y;

        setFocus();

        p->setCurRow(r);
    */
}

void MyHcWidget::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event);
    /*
        int crow = p->curRow();

        double d = event->delta()/120;

        p->setCurRow(crow-d);

    //  qDebug() << "d=" << d;
    */
}

void MyHcWidget::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
    //  qDebug() << "key event in board:" << event->key();
    // emit keyCaught(event);

    setFocus();
    /*
      int vpos=p->vScrollBar->value();
      int vstp=p->vScrollBar->pageStep();
      int vmax=p->vScrollBar->maximum();
      int hpos=p->hScrollBar->value();
      int hstp=p->hScrollBar->pageStep();
      int hmax=p->hScrollBar->maximum();

      int crow = p->curRow();

      int key = event->key();

      if(key==Qt::Key_Down)
      {
          p->setCurRow(crow+1);
      }
      else if(key==Qt::Key_Up)
      {
          p->setCurRow(crow-1);
      }
      else if(key==Qt::Key_PageDown)
      {
          p->setCurRow(crow+vstp);
      }
      else if(key==Qt::Key_PageUp)
      {
          p->setCurRow(crow-vstp);
      }
      else if(key==Qt::Key_Right)
      {
          hpos = hpos+10;
      }
      else if(key==Qt::Key_Left)
      {
          hpos = hpos-10;
      }
      else if(key==Qt::Key_Home)
      {
          if(QApplication::keyboardModifiers() & Qt::ControlModifier)
          {// Controll also is pressed
              p->setCurRow(0);
          }
          else
          {
              hpos=0;
          }
      }
      else if(key==Qt::Key_End)
      {
          if(QApplication::keyboardModifiers() & Qt::ControlModifier)
          {// Controll also is pressed
              p->setCurRow(p->rowCount()-1);
          }
          else
          {
              hpos=hmax;
          }
      }

      if(vpos>=vmax) vpos=vmax;
      if(vpos<0) vpos=0;

      if(hpos>=vmax) hpos=hmax;
      if(hpos<0) hpos=0;
    */
    /*
      if(vpos!=p->vScrollBar->value() ||
         hpos!=p->hScrollBar->value()  )
      {
        p->vScrollBar->setValue(vpos);
        p->hScrollBar->setValue(hpos);
        update();
      }
    */
}
