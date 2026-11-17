#include <limits.h>
#include <math.h>

#include "gfx.h"

#include <QApplication>

#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>
#include <QPen>

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define Vo 128
#define Va 0
#define Vb 255

int Round(double x)
{
    if (x == 0)
        return 0;

    if (x > INT_MAX)
        return INT_MAX;
    if (x < -INT_MAX)
        return -INT_MAX;

    if (x > 0)
        return (int)(x + 0.5);

    return (int)(x - 0.5);
}

inline int Clip(int v, int clip)
{
    if (clip <= 0)
        clip = 1;

    if (v < -clip)
        return -clip;
    if (v > clip)
        return clip;
    return v;
}

int Bresenham(int x0, int y0, int x1, int y1, int *x, int *y)
{
    int n = 0;
    int xo;

    int dy = y1 - y0;
    int dx = x1 - x0;
    int stepx, stepy;

    if (dy < 0) {
        dy = -dy;
        stepy = -1;
    } else {
        stepy = 1;
    }
    if (dx < 0) {
        dx = -dx;
        stepx = -1;
    } else {
        stepx = 1;
    }
    dy <<= 1; // dy is now 2*dy
    dx <<= 1; // dx is now 2*dx

    //  SetPixel(1,x0,y0);
    x[n] = x0;
    y[n++] = y0;

    xo = x0;

    if (dx > dy) {
        int fraction = dy - (dx >> 1); // same as 2*dy - dx
        while (x0 != x1) {
            if (fraction >= 0) {
                y0 += stepy;
                fraction -= dx; // same as fraction -= 2*dx
            }
            x0 += stepx;
            fraction += dy; // same as fraction -= 2*dy
                            //  SetPixel(1,x0,y0);
            x[n] = x0;
            y[n++] = y0;
        }
        return n;
    } else {
        int fraction = dx - (dy >> 1);
        while (y0 != y1) {
            if (fraction >= 0) {
                x0 += stepx;
                fraction -= dy;
            }
            y0 += stepy;
            fraction += dx;
            //  SetPixel(1,x0,y0);
            if (xo != x0) {
                x[n] = x0;
                y[n++] = y0;
                xo = x0;
            }
        }

        x[n - 1] = x0;
        y[n - 1] = y0;

        return n;
    }
}

inline void Swapi(int &a, int &b)
{
    int t = a;
    a = b;
    b = t;
}

inline void Swapf(float &a, float &b)
{
    float t = a;
    a = b;
    b = t;
}

void iRamp(int X0, int X1, int V0, int V1, int *X, int *V, int &NX)
{
    int xo, vo;
    int x0 = (X0);
    int x1 = (X1);
    int v0 = (V0);
    int v1 = (V1);
    if (x0 > x1) {
        Swapi(x0, x1);
        Swapi(v0, v1);
    }

    v0 <<= 16;
    v1 <<= 16;

    int dv = v1 - v0;
    int dx = x1 - x0;
    int stepv;
    int nx = 0;

    stepv = dv / dx;

    for (vo = v0, xo = x0; xo <= x1; xo++, vo += stepv) {
        if (X)
            X[nx] = xo;
        if (V)
            V[nx] = vo >> 16;
        nx++;
    }
    NX = nx;
}

inline void Ramp(int X0, int X1, int V0, int V1, int *X, int *V, int &NX)
{
    int xo, vo;
    int x0 = (X0);
    int x1 = (X1);
    int v0 = (V0);
    int v1 = (V1);

    if (x0 == x1) {
        NX = 1;
        X[0] = x0;
        V[0] = v0;
        return;
    }

    if (x0 > x1) {
        Swapi(x0, x1);
        Swapi(v0, v1);
    }

    if ((x1 - x0) == 1) {
        NX = 2;
        X[0] = x0;
        V[0] = v0;
        X[1] = x1;
        V[1] = v1;
        return;
    }

    if (v1 == v0) {
        /*int dx = x1 - x0; */
        int nx = 0;
        for (xo = x0; xo <= x1; xo++) {
            {
                X[nx] = xo;
                V[nx] = v1;
                nx++;
            }
        }
        NX = nx;
        return;
    }

    {
        int nx = 0;
        int dx = x1 - x0;
        v0 <<= 16;
        v1 <<= 16;

        int dv = v1 - v0;
        int stepv = dv / dx;

        for (vo = v0, xo = x0; xo <= x1; xo++, vo += stepv) {
            X[nx] = xo;
            V[nx] = vo >> 16;
            nx++;
        }
        NX = nx;
    }
}

int RangeIntersect(int x11, int x12, int x21, int x22, int &x1o, int &x2o)
{
    if (x11 > x12)
        Swapi(x11, x12);
    if (x21 > x22)
        Swapi(x21, x22);

    x1o = max(x11, x21);
    x2o = min(x12, x22);

    if (x1o <= x2o)
        return 1;
    else
        return 0;
}

//---------------------------------------------------------------------------

Gfx::Gfx()
{
    m_x = 0;
    m_y = 0;
    m_w = 0;
    m_h = 0;
    m_t = 0;

    z = 0;

    m_size = GfxStyle::LabelFontPointSize;
    m_dpiX = GfxStyle::ReferenceDpi;
    m_dpiY = GfxStyle::ReferenceDpi;
    m_devicePixelRatio = 1.0;
    _img = QImage(1, 1, QImage::Format_RGB32);
    SetDpi(m_dpiX, m_dpiY);

    _font = QFont(GfxStyle::LabelFontFamily);
    _font.setStyleHint(GfxStyle::LabelFontStyleHint);
    ApplyFontPointSize(m_size);

    _vpainter = NULL;
    _trim = false;

    int n;

    for (n = 0; n < 256; n++) {
        _pal[n] = QColor(n, n, n).rgb();
    }
}

void Gfx::SetViewPort(QPainter *p, QRect *r, int dpiX, int dpiY,
                      qreal devicePixelRatio)
{
    if (devicePixelRatio <= 0)
        devicePixelRatio = 1.0;

    _vpainter = p;
    if (dpiX <= 0)
        dpiX = p && p->device() ? p->device()->logicalDpiX()
                                : GfxStyle::ReferenceDpi;
    if (dpiY <= 0)
        dpiY = p && p->device() ? p->device()->logicalDpiY()
                                : GfxStyle::ReferenceDpi;
    if (_ipainter.isActive())
        _ipainter.end();

    m_x = r->left();
    m_y = r->top();
    m_w = qMax(1, r->width());
    m_h = qMax(1, r->height());
    m_devicePixelRatio = devicePixelRatio;

    const int imageWidth =
        qMax(1, static_cast<int>(ceil(m_w * devicePixelRatio)));
    const int imageHeight =
        qMax(1, static_cast<int>(ceil(m_h * devicePixelRatio)));
    if (_img.width() != imageWidth || _img.height() != imageHeight) {
        _img = QImage(imageWidth, imageHeight, QImage::Format_RGB32);
    }
    SetDpi(dpiX, dpiY);
    _img.fill(QColor(255, 255, 255).rgb());
    _ipainter.begin(&_img);
    _ipainter.scale(devicePixelRatio, devicePixelRatio);

    _ipainter.setFont(_font);
}

void Gfx::SetDpi(int dpiX, int dpiY)
{
    m_dpiX = dpiX > 0 ? dpiX : GfxStyle::ReferenceDpi;
    m_dpiY = dpiY > 0 ? dpiY : GfxStyle::ReferenceDpi;
    _img.setDotsPerMeterX(Round(m_dpiX * GfxStyle::MillimetersPerMeter /
                                GfxStyle::MillimetersPerInch));
    _img.setDotsPerMeterY(Round(m_dpiY * GfxStyle::MillimetersPerMeter /
                                GfxStyle::MillimetersPerInch));
}

int Gfx::ScaleByDpiX(double logicalValue) const
{
    return Round(logicalValue * m_dpiX / GfxStyle::ReferenceDpi);
}

int Gfx::ScaleByDpiY(double logicalValue) const
{
    return Round(logicalValue * m_dpiY / GfxStyle::ReferenceDpi);
}

int Gfx::ScaleByDpi(double logicalValue) const
{
    return Round(logicalValue * (m_dpiX + m_dpiY) /
                 (2.0 * GfxStyle::ReferenceDpi));
}

int Gfx::StrokeToPixel(int logicalWidth) const
{
    if (logicalWidth <= 1)
        return GfxStyle::AxisLineWidthPixels;

    int width = ScaleByDpi(logicalWidth);
    if (width < 1)
        width = GfxStyle::AxisLineWidthPixels;
    return width;
}

void Gfx::ApplyFontPointSize(double pointSize)
{
    if (pointSize <= 0)
        pointSize = m_size;

    _font.setPointSizeF(pointSize);
}

void Gfx::mSetPixel(int c, int x, int y)
{
    const bool inBounds =
        m_h > 0 && x >= 0 && x < m_w && y >= 0 && y < m_h;
    const bool inTrim = !_trim ||
                        (x >= m_tx1 && x <= m_tx2 && y >= m_ty1 &&
                         y <= m_ty2);
    if (!inBounds || !inTrim)
        return;

    const int x1 = qMax(0, Round(x * m_devicePixelRatio));
    const int x2 = qMin(
        _img.width(),
        qMax(x1 + 1, Round((x + 1) * m_devicePixelRatio)));
    const int y1 = qMax(0, Round(y * m_devicePixelRatio));
    const int y2 = qMin(
        _img.height(),
        qMax(y1 + 1, Round((y + 1) * m_devicePixelRatio)));

    for (int py = y1; py < y2; ++py) {
        QRgb *line = reinterpret_cast<QRgb *>(_img.scanLine(py));
        for (int px = x1; px < x2; ++px)
            line[px] = c;
    }
}

void Gfx::Paint()
{
    if (_vpainter == NULL)
        return;

    _vpainter->drawImage(QRect(m_x, m_y, m_w, m_h), _img);
}

void Gfx::DrawLine(int x0, int y0, int x1, int y1, int cidx)
{
    int dy = y1 - y0;
    int dx = x1 - x0;
    int stepx, stepy;

    x0 -= m_x;
    y0 -= m_y;
    x1 -= m_x;
    y1 -= m_y;

    if (x0 < 0 && x1 < 0)
        return;
    if (x0 > m_w && x1 > m_w)
        return;

    if (y0 < 0 && y1 < 0)
        return;
    if (y0 > m_h && y1 > m_h)
        return;

    if (dy < 0) {
        dy = -dy;
        stepy = -1;
    } else {
        stepy = 1;
    }
    if (dx < 0) {
        dx = -dx;
        stepx = -1;
    } else {
        stepx = 1;
    }
    dy <<= 1; // dy is now 2*dy
    dx <<= 1; // dx is now 2*dx

    mSetPixel(cidx, x0, y0);

    if (dx == 0 && dy == 0)
        return;

    if (dx > dy) {
        int fraction = dy - (dx >> 1); // same as 2*dy - dx
        while (x0 != x1) {
            if (fraction >= 0) {
                y0 += stepy;
                fraction -= dx; // same as fraction -= 2*dx
            }
            x0 += stepx;
            fraction += dy; // same as fraction -= 2*dy
            mSetPixel(cidx, x0, y0);
        }
    } else {
        int fraction = dx - (dy >> 1);
        while (y0 != y1) {
            if (fraction >= 0) {
                x0 += stepx;
                fraction -= dy;
            }
            y0 += stepy;
            fraction += dx;
            mSetPixel(cidx, x0, y0);
        }
    }
}

void Gfx::DrawLine(int x0, int y0, int x1, int y1, int cidx, int w, int wc)
{
    w = StrokeToPixel(w);

    if (w < 2)
        return DrawLine(x0, y0, x1, y1, cidx);

    int dx = abs(x0 - x1);
    int dy = abs(y0 - y1);

    int w2 = w >> 1;

    if (dx < dy) {
        int xa = -w2;
        int xb = w2;
        int x;

        if (wc != -1) {
            DrawLine(x0 + xa - 1, y0, x1 + xa - 1, y1, wc);
            DrawLine(x0 + xb + 1, y0, x1 + xb + 1, y1, wc);
        }

        for (x = xa; x <= xb; x++)
            DrawLine(x0 + x, y0, x1 + x, y1, cidx);
    } else {
        int ya = -w2;
        int yb = w2;
        int y;

        if (wc != -1) {
            DrawLine(x0, y0 + ya - 1, x1, y1 + ya - 1, wc);
            DrawLine(x0, y0 + yb + 1, x1, y1 + yb + 1, wc);
        }

        for (y = ya; y <= yb; y++)
            DrawLine(x0, y0 + y, x1, y1 + y, cidx);
    }
}

void Gfx::DrawRampLine(int xa, int ya, int xb, int yb, int ca, int cb)
{
    //  int t;

    int cidx = 0;

    int stepx, stepy;
    //  int stepc;

    xa -= m_x;
    ya -= m_y;
    xb -= m_x;
    yb -= m_y;

    int x0 = xa;
    int y0 = ya;
    int x1 = xb;
    int y1 = yb;

    int dy = y1 - y0;
    int dx = x1 - x0;

    if (x0 < 0 && x1 < 0)
        return;
    if (x0 > m_w && x1 > m_w)
        return;

    if (y0 < 0 && y1 < 0)
        return;
    if (y0 > m_h && y1 > m_h)
        return;

    if (dy < 0) {
        dy = -dy;
        stepy = -1;
    } else {
        stepy = 1;
    }
    if (dx < 0) {
        dx = -dx;
        stepx = -1;
    } else {
        stepx = 1;
    }
    dy <<= 1; // dy is now 2*dy
    dx <<= 1; // dx is now 2*dx

    SetPixel(ca, xa, ya);

    if (dx == 0 && dy == 0)
        return;

    if (dx > dy) {
        int *c = new int[dx + 1];
        int nc;

        iRamp(xa, xb, ca, cb, NULL, c, nc);

        int fraction = dy - (dx >> 1); // same as 2*dy - dx
        while (x0 != x1) {
            if (fraction >= 0) {
                y0 += stepy;
                fraction -= dx; // same as fraction -= 2*dx
            }
            x0 += stepx;
            fraction += dy; // same as fraction -= 2*dy
            cidx = c[abs(x0 - xa)];
            SetPixel(cidx, x0, y0);
        }
        delete[] c;
    } else {
        int *c = new int[dy + 1];
        int nc;

        iRamp(ya, yb, ca, cb, NULL, c, nc);

        int fraction = dx - (dy >> 1);
        while (y0 != y1) {
            if (fraction >= 0) {
                x0 += stepx;
                fraction -= dy;
            }
            y0 += stepy;
            fraction += dx;
            cidx = c[abs(y0 - ya)];
            SetPixel(cidx, x0, y0);
        }
        delete[] c;
    }
}

void Gfx::DrawRampLine(int x0, int y0, int x1, int y1, int c0, int c1, int w)
{
    if (w < 2)
        DrawRampLine(x0, y0, x1, y1, c0, c1);

    int dx = abs(x0 - x1);
    int dy = abs(y0 - y1);

    int w2 = w >> 1;

    if (dx < dy) {
        int xa = -w2;
        int xb = w2;
        int x;

        for (x = xa; x <= xb; x++)
            DrawRampLine(x0 + x, y0, x1 + x, y1, c0, c1);
    } else {
        int ya = -w2;
        int yb = w2;
        int y;

        for (y = ya; y <= yb; y++)
            DrawRampLine(x0, y0 + y, x1, y1 + y, c0, c1);
    }
}

void Gfx::DrawRect(int x1, int y1, int x2, int y2, int cidx)
{
    DrawHLine(y1, x1, x2, 1, cidx);
    DrawHLine(y2, x1, x2, 1, cidx);
    DrawVLine(x1, y1, y2, 1, cidx);
    DrawVLine(x2, y1, y2, 1, cidx);
}

void Gfx::FillRect(int x1, int y1, int x2, int y2, int cidx)
{
    int y;

    if (y1 > y2) {
        int t = y2;
        y2 = y1;
        y1 = t;
    }

    for (y = y1; y <= y2; y++) {
        DrawHLine(y, x1, x2, 1, cidx);
    }
}

void Gfx::DrawHLine(int y, int x0, int x1, int w, int cidx)
{
    w = StrokeToPixel(w);

    int x, y1, y2;
    int w2 = w >> 1;

    if (x1 < x0) {
        x = x0;
        x0 = x1;
        x1 = x;
    }

    y1 = y - w2 - m_y;
    y2 = y + w2 - m_y;

    x0 -= m_x;
    x1 -= m_x;
    y -= m_y;

    if (x0 < 0 && x1 < 0)
        return;
    if (x0 > m_w && x1 > m_w)
        return;
    if (y < 0 || y > m_h)
        return;

    for (y = y1; y <= y2; y++)
        for (x = x0; x <= x1; x++)
            mSetPixel(cidx, x, y);
}

void Gfx::DrawVLine(int x, int y0, int y1, int w, int cidx)
{
    w = StrokeToPixel(w);

    int y, x1, x2;
    int w2 = w >> 1;

    if (y1 < y0) {
        y = y0;
        y0 = y1;
        y1 = y;
    }

    x1 = x - w2 - m_x;
    x2 = x + w2 - m_x;

    y0 -= m_y;
    y1 -= m_y;
    x -= m_x;

    if (y0 < 0 && y1 < 0)
        return;
    if (y0 > m_h && y1 > m_h)
        return;
    if (x < 0 || x > m_w)
        return;

    for (x = x1; x <= x2; x++)
        for (y = y0; y <= y1; y++)
            mSetPixel(cidx, x, y);
}

void Gfx::DrawWiggleTraceTB(int x, int *yi, int *yj, int nny, Ttr tr, int fil,
                            double g, int clp, int fc, int wc)
{
    int v1, v2, cy;
    int y1, y2;
    int s1, s2;

    if (nny < 1)
        return;

    if (x - clp - m_x < 0 && x + clp - m_x < 0)
        return;
    if (x - clp - m_x > m_w && x + clp - m_x > m_w)
        return;

    int *y = new int[SZ];
    int *v = new int[SZ];

    s1 = yi[nny - 3];

    for (cy = 0; cy < nny - 2; cy++) {
        s1 = yi[cy];
        s2 = yi[cy + 1];

        v1 = Round(tr[s1] * g);
        v2 = Round(tr[s2] * g);

        int l = tr._nsmp;

        if (s1 > l) {
            int v = v1;
        }

        v1 = Clip(v1, clp);
        v2 = Clip(v2, clp);

        y1 = yj[cy];
        y2 = yj[cy + 1];

        if (y1 - m_y < 0 && y2 - m_y < 0)
            continue;
        if (y1 - m_y > m_h && y2 - m_y > m_h)
            continue;

        int ny1;
        Ramp(y1, y2, v1, v2, y, v, ny1);

        int n;

        if (ny1 < 2)
            DrawHLine(y1, x + v1, x + v2, 1, wc);

        for (n = 0; n < ny1 - 1; n++) {
            if (fil < 0 && v[n] < 0) {
                DrawHLine(y[n], x, x + v[n], 1, fc);
            } else if (fil > 0 && v[n] > 0) {
                DrawHLine(y[n], x, x + v[n], 1, fc);
            }

            DrawLine(x + v[n], y[n], x + v[n + 1], y[n + 1], wc);
        }
    }

    //  DrawLine(0,0,2000,2000,0);

    delete[] v;
    delete[] y;
}

void Gfx::DrawWiggleTraceHiRes(double x, Ttr tr, int firstSample,
                               int lastSample, double firstSampleY,
                               double sampleYStep, int fil, double g,
                               int clp, int fc, int wc)
{
    if (lastSample <= firstSample || !tr._buf)
        return;

    const double clip = qMax(1, qAbs(clp));
    QPainterPath tracePath;
    QPainterPath fillPath;
    const double firstY = firstSampleY + firstSample * sampleYStep;
    tracePath.moveTo(x - m_x, firstY - m_y);

    for (int sample = firstSample; sample < lastSample; ++sample) {
        const double y0 = firstSampleY + sample * sampleYStep;
        const double y1 = y0 + sampleYStep;
        const double value0 = qBound(-clip, tr[sample] * g, clip);
        const double value1 =
            qBound(-clip, tr[sample + 1] * g, clip);
        const double x1 = x + value1;

        tracePath.lineTo(x1 - m_x, y1 - m_y);

        if (fil == 0)
            continue;

        const auto addFillSegment = [this, &fillPath, x, y0, y1, value0,
                                     value1, fil](double t0, double t1) {
            const double midValue =
                value0 + (value1 - value0) * ((t0 + t1) / 2.0);
            if ((fil > 0 && midValue <= 0) || (fil < 0 && midValue >= 0))
                return;

            const double segmentY0 = y0 + (y1 - y0) * t0;
            const double segmentY1 = y0 + (y1 - y0) * t1;
            const double segmentValue0 =
                value0 + (value1 - value0) * t0;
            const double segmentValue1 =
                value0 + (value1 - value0) * t1;
            QPainterPath segment;
            segment.moveTo(x - m_x, segmentY0 - m_y);
            segment.lineTo(x + segmentValue0 - m_x, segmentY0 - m_y);
            segment.lineTo(x + segmentValue1 - m_x, segmentY1 - m_y);
            segment.lineTo(x - m_x, segmentY1 - m_y);
            segment.closeSubpath();
            fillPath.addPath(segment);
        };

        if ((value0 < 0 && value1 > 0) ||
            (value0 > 0 && value1 < 0)) {
            const double crossing = -value0 / (value1 - value0);
            addFillSegment(0, crossing);
            addFillSegment(crossing, 1);
        } else {
            addFillSegment(0, 1);
        }
    }

    _ipainter.save();
    _ipainter.setRenderHint(QPainter::Antialiasing, true);
    if (!fillPath.isEmpty()) {
        _ipainter.setPen(Qt::NoPen);
        _ipainter.setBrush(QColor::fromRgb(fc));
        _ipainter.drawPath(fillPath);
    }
    _ipainter.setBrush(Qt::NoBrush);
    _ipainter.setPen(QPen(QColor::fromRgb(wc), 1.0,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    _ipainter.drawPath(tracePath);
    _ipainter.restore();
}

void Gfx::DrawColorTraceHiRes(double x1, double x2, Ttr tr1, Ttr tr2,
                              int firstSample, int lastSample,
                              double firstSampleY, double sampleYStep,
                              double g)
{
    if (!tr1._buf || !tr2._buf || lastSample < firstSample ||
        sampleYStep == 0 || qFuzzyCompare(x1, x2) || m_devicePixelRatio <= 0)
        return;

    const double xLeft = qMax(qMin(x1, x2), static_cast<double>(m_x));
    const double xRight =
        qMin(qMax(x1, x2), static_cast<double>(m_x + m_w));
    const double sampleY1 = firstSampleY + firstSample * sampleYStep;
    const double sampleY2 =
        firstSampleY + (lastSample + 1) * sampleYStep;
    const double yTop = qMax(qMin(sampleY1, sampleY2),
                             static_cast<double>(m_y));
    const double yBottom =
        qMin(qMax(sampleY1, sampleY2), static_cast<double>(m_y + m_h));
    if (xRight <= xLeft || yBottom <= yTop)
        return;

    const int imageWidth = qMax(
        1, static_cast<int>(ceil((xRight - xLeft) * m_devicePixelRatio)));
    const int imageHeight = qMax(
        1, static_cast<int>(ceil((yBottom - yTop) * m_devicePixelRatio)));
    QImage colorImage(imageWidth, imageHeight, QImage::Format_RGB32);
    if (colorImage.isNull())
        return;

    const auto colorIndexAt = [g](Ttr &trace, double samplePosition) {
        const int lowerSample = static_cast<int>(floor(samplePosition));
        const int upperSample = lowerSample + 1;
        const double sampleFraction = samplePosition - lowerSample;
        const int lowerColor = qBound(
            Va, Round(trace[lowerSample] * g) + Vo, Vb);
        const int upperColor = qBound(
            Va, Round(trace[upperSample] * g) + Vo, Vb);
        return lowerColor + (upperColor - lowerColor) * sampleFraction;
    };

    for (int row = 0; row < imageHeight; ++row) {
        const double y =
            yTop + (row + 0.5) / m_devicePixelRatio;
        const double samplePosition = qBound(
            static_cast<double>(firstSample),
            (y - firstSampleY) / sampleYStep,
            static_cast<double>(lastSample));
        QRgb *line = reinterpret_cast<QRgb *>(colorImage.scanLine(row));
        for (int column = 0; column < imageWidth; ++column) {
            const double x =
                xLeft + (column + 0.5) / m_devicePixelRatio;
            const double traceFraction =
                qBound(0.0, (x - x1) / (x2 - x1), 1.0);
            const double colorIndex =
                colorIndexAt(tr1, samplePosition) +
                (colorIndexAt(tr2, samplePosition) -
                 colorIndexAt(tr1, samplePosition)) *
                    traceFraction;
            const int paletteIndex =
                qBound(Va, Round(colorIndex), Vb);
            line[column] = GetPalColor(paletteIndex);
        }
    }

    _ipainter.save();
    _ipainter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    _ipainter.drawImage(
        QRectF(xLeft - m_x, yTop - m_y, xRight - xLeft, yBottom - yTop),
        colorImage);
    _ipainter.restore();
}

void Gfx::DrawWiggleTraceTB(int x, int *yi, int *yj, int nny, float *s, int fil,
                            double g, int clp, int fc, int wc)
{
    int v1, v2, cy;
    int y1, y2;
    int s1, s2;

    if (nny < 1)
        return;

    if (x - clp - m_x < 0 && x + clp - m_x < 0)
        return;
    if (x - clp - m_x > m_w && x + clp - m_x > m_w)
        return;

    int *y = new int[SZ];
    int *v = new int[SZ];

    for (cy = 0; cy < nny - 2; cy++) {
        s1 = yi[cy];
        s2 = yi[cy + 1];

        v1 = Round(s[s1] * g);
        v2 = Round(s[s2] * g);

        v1 = Clip(v1, clp);
        v2 = Clip(v2, clp);

        y1 = yj[cy];
        y2 = yj[cy + 1];

        if (y1 - m_y < 0 && y2 - m_y < 0)
            continue;
        if (y1 - m_y > m_h && y2 - m_y > m_h)
            continue;

        int ny1;
        Ramp(y1, y2, v1, v2, y, v, ny1);

        int n;

        if (ny1 < 2)
            DrawHLine(y1, x + v1, x + v2, 1, wc);

        for (n = 0; n < ny1 - 1; n++) {
            if (fil < 0 && v[n] < 0) {
                DrawHLine(y[n], x, x + v[n], 1, fc);
            } else if (fil > 0 && v[n] > 0) {
                DrawHLine(y[n], x, x + v[n], 1, fc);
            }

            DrawLine(x + v[n], y[n], x + v[n + 1], y[n + 1], wc);
        }
    }
    delete[] v;
    delete[] y;
}

void Gfx::Draw2ColorTraceTB(int x1, int *yi1, int *yj1, int nny1, float *s1,
                            int x2, int *yi2, int *yj2, int nny2, float *s2,
                            int lx, int rx, double g, int)
{
    x1 -= m_x;
    x2 -= m_x;

    if (nny1 < 1)
        return;
    if (nny2 < 1)
        return;

    if (x1 > x2) {
        int tmpx = x1;
        x1 = x2;
        x2 = tmpx;
        float *tmps = s1;
        s1 = s2;
        s2 = tmps;
        int *tmpyi = yi1;
        yi1 = yi2;
        yi2 = tmpyi;
        int *tmpyj = yj1;
        yj1 = yj2;
        yj2 = tmpyj;
        int tmpnny = nny1;
        nny1 = nny2;
        nny2 = tmpnny;
    }

    int y11 = yj1[0];
    int y12 = yj1[nny1 - 1];
    int y21 = yj2[0];
    int y22 = yj2[nny2 - 1];

    int ix, y1o, y2o;

    int ya, yb;

    ix = RangeIntersect(y11, y12, y21, y22, y1o, y2o);
    if (ix == 0)
        return;
    ix = RangeIntersect(y1o, y2o, m_y, m_y + m_h, ya, yb);
    if (ix == 0)
        return;

    int nab = yb - ya + 1;

    int *y1 = new int[nab + 1];
    memset(y1, 0, nab + 1);
    int *v1 = new int[nab + 1];
    int *y2 = new int[nab + 1];
    memset(y2, 0, nab + 1);
    int *v2 = new int[nab + 1];

    PrepareColorTrace(yi1, yj1, nny1, s1, g, ya, yb, y1, v1);
    PrepareColorTrace(yi2, yj2, nny2, s2, g, ya, yb, y2, v2);

    int ny;

    for (ny = 0; ny < nab; ny++) {
        DrawHRamp2(y1[ny] - m_y, x1, x2 - 1, v1[ny], v2[ny], lx, rx, 0);
    }

    delete[] y1;
    delete[] v1;
    delete[] y2;
    delete[] v2;
}

void Gfx::Draw2ColorTraceTB(int x1, int *yi1, int *yj1, int nny1, Ttr tr1,
                            int x2, int *yi2, int *yj2, int nny2, Ttr tr2,
                            int lx, int rx, double g, int mode)
{
    x1 -= m_x;
    x2 -= m_x;

    if (nny1 < 1)
        return;
    if (nny2 < 1)
        return;

    if (x1 > x2) {
        int tmpx = x1;
        x1 = x2;
        x2 = tmpx;
        // float* tmps   = s1;   s1   = s2;   s2   = tmps;
        Ttr tmpt = tr1;
        tr1 = tr2;
        tr2 = tmpt;
        int *tmpyi = yi1;
        yi1 = yi2;
        yi2 = tmpyi;
        int *tmpyj = yj1;
        yj1 = yj2;
        yj2 = tmpyj;
        int tmpnny = nny1;
        nny1 = nny2;
        nny2 = tmpnny;
    }

    int y11 = yj1[0];
    int y12 = yj1[nny1 - 1];
    int y21 = yj2[0];
    int y22 = yj2[nny2 - 1];

    int ix, y1o, y2o;

    int ya, yb;

    ix = RangeIntersect(y11, y12, y21, y22, y1o, y2o);
    if (ix == 0)
        return;
    ix = RangeIntersect(y1o, y2o, m_y, m_y + m_h, ya, yb);
    if (ix == 0)
        return;

    int nab = yb - ya + 1;

    int *y1 = new int[nab + 1];
    memset(y1, 0, nab + 1);
    int *v1 = new int[nab + 1];
    int *y2 = new int[nab + 1];
    memset(y2, 0, nab + 1);
    int *v2 = new int[nab + 1];

    PrepareColorTrace(yi1, yj1, nny1, tr1, g, ya, yb, y1, v1);
    PrepareColorTrace(yi2, yj2, nny2, tr2, g, ya, yb, y2, v2);

    int ny;

    for (ny = 0; ny < nab; ny++) {
        DrawHRamp2(y1[ny] - m_y, x1, x2 - 1, v1[ny], v2[ny], lx, rx, 0);
    }

    delete[] y1;
    delete[] v1;
    delete[] y2;
    delete[] v2;
}

inline void Gfx::PrepareColorTrace(int *yi, int *yj, int nny, float *s,
                                   double g, int ya, int yb, int *yp, int *vp)
{
    int v1, v2, cy;
    int y1, y2;
    int s1, s2;

    int n, k = 0;

    for (cy = 0; cy < nny - 1; cy++) {
        y1 = yj[cy];
        y2 = yj[cy + 1];

        if ((y1 < ya && y2 < ya))
            continue;
        if ((y1 > yb && y2 > yb))
            continue;

        s1 = yi[cy];
        s2 = yi[cy + 1];

        v1 = Round(s[s1] * g) + Vo;
        if (v1 < Va)
            v1 = Va;
        if (v1 > Vb)
            v1 = Vb;

        v2 = Round(s[s2] * g) + Vo;
        if (v2 < Va)
            v2 = Va;
        if (v2 > Vb)
            v2 = Vb;

        int ny1 = 0;

        Ramp(y1, y2, v1, v2, _cy, _cv, ny1);
        //      Ramp1(s1,s2,v1,v2,_h+1, vv+ny,so,nr);

        if (ny1 < 2) {
            vp[0] = v1;
            yp[0] = y1;
            k++;
        }

        for (n = 0; n < ny1; n++) {
            if (_cy[n] >= ya && _cy[n] <= yb) {
                int yy = _cy[n] - ya;
                vp[yy] = _cv[n];
                yp[yy] = _cy[n];
                k++;
            }
        }
    }

    //    delete [] v;
    //    delete [] y;
}

inline void Gfx::PrepareColorTrace(int *yi, int *yj, int nny, Ttr tr, double g,
                                   int ya, int yb, int *yp, int *vp)
{
    int v1, v2, cy;
    int y1, y2;
    int s1, s2;

    int n, k = 0;

    for (cy = 0; cy < nny - 1; cy++) {
        y1 = yj[cy];
        y2 = yj[cy + 1];

        if ((y1 < ya && y2 < ya))
            continue;
        if ((y1 > yb && y2 > yb))
            continue;

        s1 = yi[cy];
        s2 = yi[cy + 1];

        v1 = Round(tr[s1] * g) + Vo;
        if (v1 < Va)
            v1 = Va;
        if (v1 > Vb)
            v1 = Vb;

        v2 = Round(tr[s2] * g) + Vo;
        if (v2 < Va)
            v2 = Va;
        if (v2 > Vb)
            v2 = Vb;

        int ny1 = 0;

        Ramp(y1, y2, v1, v2, _cy, _cv, ny1);
        //      Ramp1(s1,s2,v1,v2,_h+1, vv+ny,so,nr);

        if (ny1 < 2) {
            vp[0] = v1;
            yp[0] = y1;
            k++;
        }

        for (n = 0; n < ny1; n++) {
            if (_cy[n] >= ya && _cy[n] <= yb) {
                int yy = _cy[n] - ya;
                vp[yy] = _cv[n];
                yp[yy] = _cy[n];
                k++;
            }
        }
    }

    //    delete [] v;
    //    delete [] y;
}

void Ramp1A(int XA, int XB, int X0, int X1, int V0, int V1, int *V, int &NX)
{
    int xo, vo;
    int x0 = (X0);
    int x1 = (X1);
    int v0 = (V0);
    int v1 = (V1);
    if (x0 > x1) {
        Swapi(x0, x1);
        Swapi(v0, v1);
    }

    v0 <<= 16;
    v1 <<= 16;

    int dv = v1 - v0;
    int dx = x1 - x0;
    int stepv;
    int nx = 0;

    if (dx == 0)
        stepv = 0;
    else
        stepv = dv / dx;

    for (vo = v0, xo = x0; xo <= x1; xo++, vo += stepv) {
        if (xo >= XA && xo <= XB) {
            V[xo - XA] = vo >> 16;
            nx++;
        }
    }
    NX = nx;
}

void Ramp1F(int XA, int XB, int X0, int X1, float V0, float V1, float *V,
            int &NX)
{
    int xo;
    float vo;
    int x0 = (X0);
    int x1 = (X1);
    float v0 = (V0);
    float v1 = (V1);
    if (x0 > x1) {
        Swapi(x0, x1);
        Swapf(v0, v1);
    }

    float dv = v1 - v0;
    int dx = x1 - x0;
    float stepv;
    int nx = 0;

    if (dx == 0)
        stepv = 0;
    else
        stepv = dv / dx;

    for (vo = v0, xo = x0; xo <= x1; xo++, vo += stepv) {
        if (xo >= XA && xo <= XB) {
            V[xo - XA] = vo;
            nx++;
        }
    }
    NX = nx;
}

void Gfx::PrepareVelTrace(int ya, int yb, int ll, int *yy, float *vv, float *v)
{
    int n;

    int l = yb - ya + 1;

    memset(v, -1, l * sizeof(int));

    int yp, yc;
    float vp, vc;
    int nx;

    for (n = 0; n < ll; n++) {
        yc = yy[n];
        vc = vv[n];

        if (n > 0) {
            Ramp1F(ya, yb, yp, yc, vp, vc, v, nx);
        }
        yp = yc;
        vp = vc;
    }
}

void Gfx::Draw2VelTraceTB(int xa, int ll1, int *yy1, float *vv1, int xb,
                          int ll2, int *yy2, float *vv2, int xl, int xr, int ya,
                          int yb)
{

    //    DrawVLine(xa, ya, yb, 1,1);
    //    DrawVLine(xb, ya, yb, 1,1);
    //    FillRect(xa, ya, xb, yb, RGB(100,100,100));
    //    DrawRect(xa, ya, xb, yb, 0);

    int n, y;
    int l = yb - ya + 1;

    float *v1 = new float[l];
    float *v2 = new float[l];

    PrepareVelTrace(ya, yb, ll1, yy1, vv1, v1);
    PrepareVelTrace(ya, yb, ll2, yy2, vv2, v2);

    for (n = 0, y = ya; n < l; n++, y++) {
        int yn = y - ya;
        if (v1[yn] >= 0 && v2[yn] >= 0) {
            DrawHRamp2F(y - m_y, xa - m_x, xb - m_x, v1[yn], v2[yn], xl - m_x,
                        xr - m_x, 0);
        }
    }

    delete[] v2;
    delete[] v1;
}

void Gfx::DrawHRamp1(int y, int x0, int x1, int v0, int v1, int)
{
    int xo, vo;

    if (x0 < 0 && x1 < 0)
        return;
    if (x0 > m_w && x1 > m_w)
        return;
    if (y < 0 || y > m_h)
        return;

    if (x0 > x1) {
        Swapi(x0, x1);
        Swapi(v0, v1);
    }

    if (x0 == x1) {
        SetPixel(vo, xo, y);
        return;
    }

    v0 <<= 16;
    v1 <<= 16;

    int dv = v1 - v0;
    int dx = x1 - x0;
    int stepv;

    stepv = dv / dx;

    for (vo = v0, xo = x0; xo <= x1; xo++, vo += stepv)
        SetPixel(vo >> 16, xo, y);

    return;
}

void Gfx::DrawHRamp2(int y, int x0, int x1, int v0, int v1, int lx, int rx, int)
{
    int xo, vo;

    if (x0 < 0 && x1 < 0)
        return;
    if (x0 > m_w && x1 > m_w)
        return;
    if (y < 0 || y > m_h)
        return;

    if (x0 == x1) {
        SetPixel(v0, x0, y);
        return;
    }

    if (x0 > x1) {
        Swapi(x0, x1);
        Swapi(v0, v1);
    }

    v0 <<= 16;
    v1 <<= 16;

    int dv = v1 - v0;
    int dx = x1 - x0;
    int stepv;

    stepv = dv / dx;

    for (vo = v0, xo = x0; xo <= x1; xo++, vo += stepv)
        if (xo >= lx && xo <= rx)
            SetPixel(vo >> 16, xo, y);

    return;
}

void Gfx::DrawHRamp2F(int y, int x0, int x1, float v0, float v1, int lx, int rx,
                      int)
{
    int xo;
    float vo;

    if (x0 < 0 && x1 < 0)
        return;
    if (x0 > m_w && x1 > m_w)
        return;
    if (y < 0 || y > m_h)
        return;

    //    vo=2;
    //    v1=6;

    if (x0 == x1) {
        SetPixel(v0, x0, y);
        return;
    }

    if (x0 > x1) {
        Swapi(x0, x1);
        Swapf(v0, v1);
    }

    //  v0<<=16;  v1<<=16;

    float dv = v1 - v0;
    float dx = x1 - x0;
    float stepv;

    stepv = dv / dx;

    for (vo = v0, xo = x0; xo <= x1; xo++, vo += stepv) {
        if (xo >= lx && xo <= rx) {
            //            qDebug() << xo << vo;
            SetPixel(vo, xo, y);
        }
    }

    return;
}

void Gfx::SetPixel(int v, int x, int y)
{
    QRgb c;

    if (v < 0)
        v = 0;
    if (v > 255)
        v = 255;

    c = _pal[v];

    mSetPixel(c, x, y);
}

void Gfx::DrawText(int x, int y, QString str, int color, double rotate,
                   int size, bool fill_bg)
{
    // QFont font;
    if (size < 0)
        ApplyFontPointSize(m_size);
    else
        ApplyFontPointSize(size);
    // font.setStyleHint(QFont::Courier);

    y -= m_y;
    x -= m_x;

    _ipainter.save();
    _ipainter.setRenderHint(QPainter::TextAntialiasing);
    _ipainter.setFont(_font);
    _ipainter.translate(x, y);

    QPen penHText(color);
    _ipainter.setPen(penHText);

    QFontMetrics fm = _ipainter.fontMetrics();
    int h = fm.height();
    int w = fm.width(str);
    _ipainter.rotate(-rotate);
    if (fill_bg)
        _ipainter.fillRect(0, -h, w, h, QColor(255, 255, 255));
    _ipainter.drawText(0, 0, str);
    _ipainter.restore();
}

int Gfx::GetTextWidth(QString str)
{
    QFontMetrics fm(_font, &_img);
    return fm.width(QString(str));
}

int Gfx::GetTextWidth(QString str, int size)
{
    // QFont font;
    ApplyFontPointSize(size);
    // font.setStyleHint(QFont::Courier);

    QFontMetrics fm(_font, &_img);
    return fm.width(str);
}

int Gfx::GetTextHeight()
{
    QFontMetrics fm(_font, &_img);
    return fm.height();
}

int Gfx::GetTextHeight(int size)
{
    ApplyFontPointSize(size);

    QFontMetrics fm(_font, &_img);
    return fm.height();
}

QFontMetrics Gfx::GetFontMetrics()
{
    return QFontMetrics(_font, &_img);
}

void Gfx::SetTrim(int x1, int x2, int y1, int y2)
{
    if (x1 > x2)
        Swapi(x1, x2);
    if (y1 > y2)
        Swapi(y1, y2);

    _trim = true;
    m_tx1 = x1 - m_x;
    m_tx2 = x2 - m_x;
    m_ty1 = y1 - m_y;
    m_ty2 = y2 - m_y;

    int x = m_tx1;
    int y = m_ty1;
    int w = m_tx2 - m_tx1 + 1;
    int h = m_ty2 - m_ty1 + 1;

    _ipainter.setClipRect(x, y, w, h);
    _ipainter.setClipping(_trim);
}

void Gfx::ClrTrim()
{
    _trim = false;
}

void Gfx::SetPalette(QList<QRgb> pal)
{
    int n;

    for (n = 0; n < pal.count() && n < 256; n++) {
        _pal[n] = pal[n];
    }

    for (; n < 256; n++) {
        _pal[n] = QColor(n, n, n).rgb();
    }
}

void Gfx::SetFontSize(int size)
{
    m_size = size;
    ApplyFontPointSize(m_size);
}

int Gfx::ScaleX(int logicalValue) const
{
    return ScaleByDpiX(logicalValue);
}

int Gfx::ScaleY(int logicalValue) const
{
    return ScaleByDpiY(logicalValue);
}

int Gfx::Scale(int logicalValue) const
{
    return ScaleByDpi(logicalValue);
}

void Gfx::setFont(const QFont &font)
{
    _font = font;
}

int Gfx::GetPalColor(int cidx)
{
    if (cidx < 0 || cidx > 255)
        return _pal[0];

    return _pal[cidx];
}

void Gfx::DrawPLine(int *x, int *y, int n, int cidx, int w)
{
    int i;

    for (i = 0; i < n - 1; i++) {
        DrawLine(x[i], y[i], x[i + 1], y[i + 1], cidx, w);
    }
}

void Gfx::DrawPLine(int *x, int *y, int *cidx, int n, int w)
{
    int i;

    for (i = 0; i < n - 1; i++) {
        DrawLine(x[i], y[i], x[i + 1], y[i + 1], cidx[i], w);
    }
}

void Gfx::DrawPLine(int *x, int *y, int *cidx, int *w, int n, int wc)
{
    int i;

    for (i = 0; i < n - 1; i++) {
        DrawLine(x[i], y[i], x[i + 1], y[i + 1], cidx[i], w[i], wc);
    }
}

void Gfx::DrawCircle(int xCenter, int yCenter, int r, int cidx, int w, int wc)
{
    w = StrokeToPixel(w);

    int rr;
    int ww = w >> 1;

    if (wc != -1) {
        DrawCircle(xCenter, yCenter, r - ww - 1, wc);
        DrawCircle(xCenter, yCenter, r + ww + 1, wc);
    }

    if (w < 2)
        DrawCircle(xCenter, yCenter, r, cidx);
    else
        for (rr = r - ww; rr <= r + ww; rr++)
            DrawCircle(xCenter, yCenter, rr, cidx);
}

void Gfx::DrawCircle(int xCenter, int yCenter, int r, int cidx)
{
    yCenter -= m_y;
    xCenter -= m_x;

    if (xCenter + r < 0)
        return;
    if (xCenter - r > m_w)
        return;
    if (yCenter + r < 0)
        return;
    if (yCenter - r > m_h)
        return;

    if (r == 1) {
        mSetPixel(cidx, xCenter, yCenter);
        return;
    }

    int x = 0, y = r;
    int d = 3 - (2 * r);

    while (x <= y) {

        mSetPixel(cidx, xCenter + x, yCenter + y);
        mSetPixel(cidx, xCenter + y, yCenter + x);
        mSetPixel(cidx, xCenter - x, yCenter + y);
        mSetPixel(cidx, xCenter + y, yCenter - x);
        mSetPixel(cidx, xCenter - x, yCenter - y);
        mSetPixel(cidx, xCenter - y, yCenter - x);
        mSetPixel(cidx, xCenter + x, yCenter - y);
        mSetPixel(cidx, xCenter - y, yCenter + x);

        if (d < 0)
            d += (4 * x) + 6;
        else {
            d += (4 * (x - y)) + 10;
            y -= 1;
        }
        x++;
    }
}

void Gfx::FillCircle(int xCenter, int yCenter, int r, int cidx)
{
    //    yCenter  -= _y;
    //    xCenter  -= _x;

    if (xCenter + r - m_x < 0)
        return;
    if (xCenter - r - m_x > m_w)
        return;
    if (yCenter + r - m_y < 0)
        return;
    if (yCenter - r - m_y > m_h)
        return;

    int x = 0, y = r;
    int d = 3 - (2 * r);

    while (x <= y) {
        DrawHLine(yCenter + y, xCenter + x, xCenter - x, 1, cidx);

        DrawHLine(yCenter + x, xCenter + y, xCenter - y, 1, cidx);

        DrawHLine(yCenter - x, xCenter + y, xCenter - y, 1, cidx);

        DrawHLine(yCenter - y, xCenter - x, xCenter + x, 1, cidx);

        if (d < 0)
            d += (4 * x) + 6;
        else {
            d += (4 * (x - y)) + 10;
            y -= 1;
        }
        x++;
    }
}
