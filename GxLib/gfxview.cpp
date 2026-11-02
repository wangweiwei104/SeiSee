#include <QDebug>
#include <QEvent>
#include <QApplication>
#include <QTimer>

#include "gfx.h"
#include "gfxview.h"
#include "gfxstyle.h"

GfxView::GfxView(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(5000, 5000);
    setAutoFillBackground(true);
    QPalette plotPalette = palette();
    plotPalette.setColor(QPalette::Window, Qt::white);
    plotPalette.setColor(QPalette::Base, Qt::white);
    setPalette(plotPalette);
    m_trackedWindow = nullptr;
    m_trackedScreen = nullptr;

    QScreen *currentScreen = screen();
    if (!currentScreen)
        currentScreen = QApplication::primaryScreen();
    int dpix = GfxStyle::ScreenDpiX(currentScreen);
    int dpiy = GfxStyle::ScreenDpiY(currentScreen);
    m_dpiX = dpix;
    m_dpiY = dpiy;
    m_devicePixelRatio = 1.0;
    m_captureScale = 0;
    m_suppressCapturePaint = false;
    m_Xpmm = dpix / GfxStyle::MillimetersPerInch;
    m_Ypmm = dpiy / GfxStyle::MillimetersPerInch;
    m_gfx.SetDpi(dpix, dpiy);

    m_Xs = 1;
    m_Ys = 1;
    setMouseTracking(true);
}

GfxView::~GfxView()
{
    int n;

    for (n = 0; n < m_links.count(); n++)
        m_links[n]->setView(NULL);
}
// ----------------------------------------------------------------------

void GfxView::refreshScreenDpi()
{
    TrackWindowScreen();
}

bool GfxView::event(QEvent *event)
{
    bool result = QWidget::event(event);

    if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange ||
        event->type() == QEvent::ScreenChangeInternal) {
        QTimer::singleShot(0, this, &GfxView::TrackWindowScreen);
    }

    return result;
}

void GfxView::TrackWindowScreen()
{
    QWidget *topLevelWidget = window();
    QWindow *window = topLevelWidget ? topLevelWidget->windowHandle() : nullptr;
    if (window != m_trackedWindow) {
        if (m_trackedWindow)
            disconnect(m_trackedWindow, nullptr, this, nullptr);

        m_trackedWindow = window;
        if (m_trackedWindow) {
            connect(m_trackedWindow, &QWindow::screenChanged, this,
                    [this](QScreen *screen) {
                        TrackScreen(screen);
                        RefreshScreenDpi();
                    });
        }
    }

    if (m_trackedWindow)
        TrackScreen(m_trackedWindow->screen());
    else
        TrackScreen(screen());

    RefreshScreenDpi();
}

void GfxView::TrackScreen(QScreen *screen)
{
    if (screen == m_trackedScreen)
        return;

    disconnect(m_screenDpiConnection);

    m_trackedScreen = screen;
    if (m_trackedScreen) {
        m_screenDpiConnection =
            connect(m_trackedScreen, &QScreen::logicalDotsPerInchChanged, this,
                    [this](qreal) { RefreshScreenDpi(); });
    }
}

void GfxView::RefreshScreenDpi()
{
    QScreen *currentScreen = m_trackedScreen ? m_trackedScreen.data() : screen();
    if (!currentScreen)
        currentScreen = QApplication::primaryScreen();

    int dpiX = GfxStyle::ScreenDpiX(currentScreen);
    int dpiY = GfxStyle::ScreenDpiY(currentScreen);
    ApplyDpi(dpiX, dpiY, devicePixelRatioF());
}

void GfxView::ApplyDpi(int dpiX, int dpiY, qreal devicePixelRatio)
{
    if (dpiX <= 0)
        dpiX = GfxStyle::ReferenceDpi;
    if (dpiY <= 0)
        dpiY = GfxStyle::ReferenceDpi;
    if (devicePixelRatio <= 0)
        devicePixelRatio = 1.0;

    if (m_dpiX == dpiX && m_dpiY == dpiY &&
        qFuzzyCompare(m_devicePixelRatio, devicePixelRatio))
        return;

    m_dpiX = dpiX;
    m_dpiY = dpiY;
    m_devicePixelRatio = devicePixelRatio;
    m_Xpmm = dpiX / GfxStyle::MillimetersPerInch;
    m_Ypmm = dpiY / GfxStyle::MillimetersPerInch;
    m_gfx.SetDpi(dpiX, dpiY);
    Preset();
    emit dpiChanged();
}

void GfxView::RegisterLink(GfxObj *v)
{
    m_links.append(v);
    _nlinks = m_links.count();
    update();
}

void GfxView::UnRegisterLink(GfxObj *v)
{
    _nlinks = m_links.count();
    m_links.removeAll(v);
    _nlinks = m_links.count();
    update();
}
// ----------------------------------------------------------------------

void GfxView::Preset()
{
    int n;
    int nobj = m_links.count();

    for (n = 0; n < nobj; n++) {
        m_links[n]->Preset();
    }
}

// ----------------------------------------------------------------------

static bool myLessThan(GfxObj *a, GfxObj *b)
{
    return a->Layer() < b->Layer();
}

void GfxView::paintEvent(QPaintEvent *pe)
{
    if (m_suppressCapturePaint) {
        QPainter backgroundPainter(this);
        backgroundPainter.fillRect(pe->rect(),
                                   palette().color(QPalette::Window));
        return;
    }

    int n, nobj;

    QPainter painter;

    QRect r = pe->rect();

    QPaintEngine *engine;

    engine = painter.paintEngine();

    painter.begin(this);

    painter.setFont(font());
    painter.setBackgroundMode(Qt::TransparentMode);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    engine = painter.paintEngine();

    QScreen *currentScreen = m_trackedScreen ? m_trackedScreen.data() : screen();
    if (!currentScreen)
        currentScreen = QApplication::primaryScreen();
    int screenDpiX = currentScreen
                         ? GfxStyle::ScreenDpiX(currentScreen)
                         : engine->paintDevice()->logicalDpiX();
    int screenDpiY = currentScreen
                         ? GfxStyle::ScreenDpiY(currentScreen)
                         : engine->paintDevice()->logicalDpiY();
    int dpix = screenDpiX;
    int dpiy = screenDpiY;
    const qreal devicePixelRatio = this->devicePixelRatioF();

    bool dpiWasChanged =
        dpix != m_dpiX || dpiy != m_dpiY ||
        !qFuzzyCompare(m_devicePixelRatio, devicePixelRatio);
    if (dpiWasChanged)
        ApplyDpi(dpix, dpiy, devicePixelRatio);

    //   qDebug() << "m_Ypmm:" << m_Ypmm;

    const qreal renderScale =
        m_captureScale > 0 ? m_captureScale : devicePixelRatio;
    m_gfx.SetViewPort(&painter, &r, dpix, dpiy, renderScale);

    int rc = receivers(SIGNAL(OnPrevDraw(GfxView *)));
    if (rc > 0) {
        emit OnPrevDraw(this);
    }

    nobj = m_links.count();

    if (m_links.count() > 0) {
        qSort(m_links.begin(), m_links.end(), myLessThan);
    }

    for (n = 0; n < nobj; n++) {
        m_links[n]->Draw();
    }

    rc = receivers(SIGNAL(OnPostDraw(GfxView *)));
    if (rc > 0) {
        emit OnPostDraw(this);
    }

    m_gfx.Paint();
    emit overlayPaint(&painter);

    //   painter.drawRect(r);
    //   painter.drawLine(r.left(),r.top(),r.right(),r.bottom());
}

/*
GfxObj* GfxView::ObjHit(int x, int y)
{
    GfxObj*  obj_hit = NULL;
    //int      layer   = -1;
    //int      n;

    if(m_links.count()>0)
    {
      for(n=0;n<m_links.count();n++)
      {
          GfxObj* obj = m_links[n];

          if(obj->Hit(x,y))
          {
            //if(obj->gxLayer>=layer)
            //{
                obj_hit = obj;
            //    layer   = obj->gxLayer;
            //}
          }
      }
    }
    return obj_hit;
}
*/
void GfxView::mousePressEvent(QMouseEvent *event)
{
    emit mouseEvent(event);
    QWidget::mousePressEvent(event);
}

void GfxView::wheelEvent(QWheelEvent *event)
{
    /*
        double d = event->delta();
        double z = 1+0.002*abs(d); if(d<0) z = 1/z;

        int xm = event->pos().x();
        int ym = event->pos().y();
    */
    emit wheel_Event(event);

    QWidget::wheelEvent(event);
}
