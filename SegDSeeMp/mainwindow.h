#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>

#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QStack>
#include <QThread>
#include <QWidget>

#include "gfxobjhsrslab.h"
#include "gfxobjseishdrs.h"
#include "gfxobjseissect.h"
#include "gfxobjtaxis.h"
#include "gfxsctlist.h"
#include "gfxsrcseismemsegd.h"
#include "gfxsrcseistest.h"
#include "gfxview.h"
#include "myobjptrlist.h"
#include "myscrollarea.h"
#include "mystringtable.h"

#include "aboutdialog.h"
#include "procparmdialog.h"

#include "readerthread.h"

#include "cwp.h"
#include "libSegD.h"

#define VERSION "4.0.2"

namespace Ui {
    class MainWindow;
}

class myEventCatcher : public QObject
{
    Q_OBJECT
protected:
    int _tag;

public:
    myEventCatcher(int tag, QObject *parent)
        : QObject(parent)
    {
        _tag = tag;
    }
    ~myEventCatcher()
    {
    }

    bool eventFilter(QObject *object, QEvent *event)
    {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *e = dynamic_cast<QWheelEvent *>(event);
            emit whellEvent(_tag, e);
            return true;
        }

        // standard event processing
        return QObject::eventFilter(object, event);
    }

signals:
    void whellEvent(int tag, QWheelEvent *event);

public slots:
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:

    void sendSmp(int ncs, int nch, float *smp, char &rc);

    void sclZoom(double zx, double zy, int xc, int yc);
    void sclZoomV(double zx, double zy, int xc, int yc);
    void sclZoomH(double zx, double zy, int xc, int yc);

    void winZoom(QRect r);

    void FillChsGrid();

    virtual void closeEvent(QCloseEvent *event);

private slots:

    void dirGridEvent(int row, int mode);
    void hdrGridEvent(int row, int mode);
    void chsGridEvent(int row, int mode);

    void viewMouseEvent(QMouseEvent *event);

    void procParmEvent();

    void edWhellEvent(int tag, QWheelEvent *event);

    void ResetSrc();

    void on_ckAgc_toggled(bool checked);

    void on_ckFilt_toggled(bool checked);

    void on_ckNorm_toggled(bool checked);

    void on_ckWiggle_toggled(bool checked);

    void on_ckGray_toggled(bool checked);

    void on_ckColor_toggled(bool checked);

    void on_rbNon_toggled(bool checked);

    void on_rbPos_toggled(bool checked);

    void on_rbNeg_toggled(bool checked);

    void on_btnCkNon_pressed();

    void on_btnCkAll_pressed();

    void on_ckAll_toggled(bool checked);

    void on_ckSng_toggled(bool checked);

    void on_zoomAllBtn_pressed();

    void on_zoomVallBtn_pressed();

    void on_zoomHallBtn_pressed();

    void on_edTr_editingFinished();

    void on_edTm_editingFinished();

    void on_edGn_editingFinished();

    void on_zoomPreBtn_pressed();

    void on_zoomOutBtn_pressed();

    void on_zoomInBtn_pressed();

    void on_selDirBtn_pressed();

    void on_refreshBtn_pressed();

    void on_procParmBtn_pressed();

    void on_zoomWinBtn_pressed();

    void on_actionOpen_Directory_triggered();

    void on_actionAbout_triggered();

    void on_TrSlider_valueChanged(int p);

    void on_TrSlider_sliderReleased();

    void on_TrSlider_sliderPressed();

    void on_TmSlider_sliderPressed();

    void on_TmSlider_sliderReleased();

    void on_TmSlider_valueChanged(int p);

    void on_GnSlider_sliderPressed();

    void on_GnSlider_sliderReleased();

    void on_GnSlider_valueChanged(int p);

    void on_aGainBtn_pressed();

    void on_splitter_splitterMoved(int pos, int index);

    void on_hdrsTopBtn_clicked();

    void on_hdrsBtmBtn_clicked();

    void on_actionUser_Manual_triggered();

    void on_actionParameters_triggered();

    void on_actionUser_s_Manual_Russian_triggered();

private:
    Ui::MainWindow *ui;

    ReadThread *WorkThread;
    ReadThread *ChkfThread;

    QProgressBar *progressBar;

    QLabel *m_statMess;
    QLabel *m_statTime;
    QLabel *m_statChSet;
    QLabel *m_statChan;
    QLabel *m_statFfid;
    QLabel *m_statSp;
    QLabel *m_statusRight;

    ProcParmDialog ProcParmDlg;
    AboutDialog AboutDlg;

    QList<DirItem> dlist;
    QList<QString> hlist;
    QStack<zdata> zstack;

    QList<QRgb> pal;

    QString find;

    int dirw;

    double _f1, _f2, _f3, _f4;
    double _agcw;

    float _smax;
    bool cs_changed;

    bool busy;

    double xss, yss, gss; // sliders' values

    QVector<bool> chsSel;

    GfxView timeView;
    GfxView hdrsView;
    GfxView hlabView;
    GfxView seisView;

    MyScrollArea *hdrsScrl;
    MyScrollArea *timeScrl;
    MyScrollArea *seisScrl;

    GfxObjTAxis timeAxis;
    GfxObjHsrsLab hdrsLab;

    MyStringTable dirGrid;
    MyStringTable chsGrid;
    MyStringTable hdrGrid;

    GfxSctList sctList;
    MyObjPtrList srcList;
    MyObjPtrList haxList;

    TSegD *segd;

    QString curDir;
    QString curFile;
    QString curHdr;
    QString curFn; // file number in ltod

    int tabcsi(int nr);

    double allSectWidth();

    void FillDirRow(int nr, DirItem di);
    void FillDirGrid(QList<DirItem> list, int mode);

    void StopThreads();

    void ChangeDataDir(QString Dname, QString Find = "");
    void ScanLtod();
    void CheckFiles(QString Dname, QList<DirItem> Dlist);

    void SetZoomC(double xs, double ys, int xc, int yc);
    void SetZoom(double xs, double ys, int xo, int yo, bool push = true);
    void GetZoom(double &xs, double &ys, int &xo, int &yo);

    int Parse(byte *buf, int len);
    void Clear();

    CwpFilter Fi;

    int Filter(float *inps, float f1, float f2, float f3, float f4, double si,
               int ns);

    void ShowProgress(QString mes, int pers);

    int ReadSegdFile();
    int ReadSegdInLtod(int row);

    void SelCsets();

    void ArrangeSections();

    void FillEdits();
    void FillControls();

    void GetRegInfo();
    void SaveRegInfo();

    void SaveCsSel(bool clear = false);

    void NewFileReady(TSegD *newsegd);

    bool event(QEvent *evt);
};

#endif // MAINWINDOW_H
