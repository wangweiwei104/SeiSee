#include "gfxobj.h"
#include "gfxsrc.h"
#include "gfxview.h"



GfxObj::GfxObj(QObject *parent)
    : QObject(parent)
{
    m_view = NULL;
    m_src = NULL;

    m_Xo = 0;
    m_Yo = 0;

    m_X1 = 0;
    m_X2 = 0;
    m_Y1 = 0;
    m_Y2 = 0;

    m_Xs = 1;
    m_Ys = 1;

    m_Layer = 0;

    m_src = NULL;

    m_trim = true;

    _preset = true;
}

GfxObj::~GfxObj()
{
    if (m_view)
        m_view->UnRegisterLink(this);
}

void GfxObj::setLayer(int v)
{
    m_Layer = v;
    Preset();
}

void GfxObj::setXo(double v)
{
    m_Xo = v;
    Preset();
}

void GfxObj::setYo(double v)
{
    m_Yo = v;
    Preset();
}

void GfxObj::setX1(double v)
{
    m_X1 = v;
    Preset();
}

void GfxObj::setX2(double v)
{
    m_X2 = v;
    Preset();
}

void GfxObj::setXs(double v)
{
    m_Xs = v;
    Preset();
}

void GfxObj::setY1(double v)
{
    m_Y1 = v;
    Preset();
}

void GfxObj::setY2(double v)
{
    m_Y2 = v;
    Preset();
}

void GfxObj::setYs(double v)
{
    m_Ys = v;
    Preset();
}

void GfxObj::setView(GfxView *v)
{
    if (m_view)
        m_view->UnRegisterLink(this);

    m_view = v;

    if (m_view) {
        m_view->RegisterLink(this);
        Preset();
    }
}

void GfxObj::setTrim(bool v)
{
    m_trim = v;
    Preset();
}

void GfxObj::setSrc(GfxSrc *v)
{
    if (m_src)
        m_src->UnRegisterLink(this);

    m_src = v;

    if (m_src)
        m_src->RegisterLink(this);

    Preset();
}

void GfxObj::Update()
{
    if (m_view)
        m_view->update();
}

void GfxObj::Preset()
{
    _preset = true;
    if (m_view) {
        xpmm = m_view->Xpmm() * m_view->Xs();
        ypmm = m_view->Ypmm() * m_view->Ys();
        m_view->update();
    }
}

void GfxObj::Draw()
{
    if (!m_view)
        return;

    DrawTo(m_view->getGfx(), m_view->Xpmm() * m_view->Xs(),
           m_view->Ypmm() * m_view->Ys());
}

void GfxObj::DrawTo(Gfx *target, double xPixelsPerMillimeter,
                    double yPixelsPerMillimeter)
{
    if (!target)
        return;

    xpmm = xPixelsPerMillimeter;
    ypmm = yPixelsPerMillimeter;
    gfx = target;

    L = gfx->Xo();
    T = gfx->Yo();
    W = gfx->W();
    H = gfx->H();
    R = L + W;
    B = T + H;

    int x1 = x2pix(X1());
    int x2 = x2pix(X2());
    int y1 = y2pix(Y1());
    int y2 = y2pix(Y2());

    if (_preset)
        DoPreset();

    if (m_trim)
        gfx->SetTrim(x1, x2, y1, y2);

    DoDraw();

    gfx->ClrTrim();
}

void GfxObj::DoDraw()
{
    /*
        int x1 = x2pix(m_X1);
        int x2 = x2pix(m_X2);
        int y1 = y2pix(m_Y1);
        int y2 = y2pix(m_Y2);

        gfx->DrawLine(x1,y1,x2,y2,0);
    */
}

double GfxObj::WidthMM()
{
    return fabs(m_X2 - m_X1) * m_Xs;
}

double GfxObj::HeightMM()
{
    return fabs(m_Y2 - m_Y1) * m_Ys;
}

double GfxObj::WidthPix()
{
    if (!m_view)
        return 0;

    double d = fabs(m_X2 - m_X1);

    //  qDebug() << "x2" << m_X2 << "x1" << m_X1 << "d" << d;

    return m_view->Xpmm() * d * m_Xs;
}

double GfxObj::HeightPix()
{
    if (!m_view)
        return 0;

    return m_view->Ypmm() * fabs(m_Y2 - m_Y1) * m_Ys;
}

int GfxObj::WidthView()
{
    if (!m_view)
        return 0;

    return m_view->Xpmm() * m_view->Xs() * fabs(m_X2 - m_X1) * m_Xs;
}

int GfxObj::HeightView()
{
    if (!m_view)
        return 0;

    return m_view->Ypmm() * m_view->Ys() * fabs(m_Y2 - m_Y1) * m_Ys;
}
