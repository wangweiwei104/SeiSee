#ifndef Gm_XVIEW_H
#define Gm_XVIEW_H

#include <QList>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QPainter>
#include <QMetaObject>
#include <QPointer>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#include <math.h>

#include "gfx.h"
#include "gfxobj.h"

class GfxView : public QWidget
{
    Q_OBJECT
protected:
    Gfx m_gfx;

    QList<GfxObj *> m_links;

    double m_Xpmm;
    double m_Ypmm;
    double m_Xs;
    double m_Ys;
    int m_dpiX;
    int m_dpiY;
    qreal m_devicePixelRatio;
    qreal m_captureScale;
    bool m_suppressCapturePaint;
    QPointer<QWindow> m_trackedWindow;
    QPointer<QScreen> m_trackedScreen;
    QMetaObject::Connection m_screenDpiConnection;

    int _preset;
    int _nlinks;

    virtual void paintEvent(QPaintEvent *pe);

    virtual void mousePressEvent(QMouseEvent *event);

    virtual void wheelEvent(QWheelEvent *event);
    virtual bool event(QEvent *event);

    void Preset(void);
    void TrackWindowScreen();
    void TrackScreen(QScreen *screen);
    void RefreshScreenDpi();
    void ApplyDpi(int dpiX, int dpiY, qreal devicePixelRatio);

    //  GfxObj* ObjHit(int x, int y);

public:
    explicit GfxView(QWidget *parent = 0);

    ~GfxView();

    double Xpmm()
    {
        return m_Xpmm;
    }
    double Ypmm()
    {
        return m_Ypmm;
    }

    int dpiY() const
    {
        return m_dpiY;
    }

    qreal devicePixelRatio() const
    {
        return m_devicePixelRatio;
    }

    int dpiX() const
    {
        return m_dpiX;
    }

    void setCaptureScale(qreal scale)
    {
        m_captureScale = scale > 0 ? scale : 0;
    }

    bool isHighResolutionCapture() const
    {
        return m_captureScale > 0;
    }

    void setCapturePaintSuppressed(bool suppressed)
    {
        m_suppressCapturePaint = suppressed;
    }

    double Xs()
    {
        return m_Xs;
    }
    double Ys()
    {
        return m_Ys;
    }

    void refreshScreenDpi();

    void setXs(double v)
    {
        m_Xs = v;
        _preset = true;
        Preset();
        update();
    }

    void setYs(double v)
    {
        m_Ys = v;
        _preset = true;
        Preset();
        update();
    }

    void RegisterLink(GfxObj *v);

    void UnRegisterLink(GfxObj *v);

    Gfx *getGfx()
    {
        return &m_gfx;
    }

signals:
    void dpiChanged();
    void OnPrevDraw(GfxView *view);
    void OnPostDraw(GfxView *view);
    void overlayPaint(QPainter *painter);
    void mouseEvent(QMouseEvent *event);
    void wheel_Event(QWheelEvent *event);
    void timeSetEvent(double time);

public slots:
};

#endif // Gm_XVIEW_H
