#ifndef WORKTHREAD_H
#define WORKTHREAD_H

#include <atomic>
#include <vector>
#include <functional>
#include <QThread>
#include <QVector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(                                                               \
    disable                                                                    \
    : 4100 4244 4267 4305) // implicit conversion, possible loss of data
#else
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <QDir>
#include <QFile>
#include <QImage>
#include <QObject>
#include <QTextStream>

#include "seisfile.h"
#include "sgyfile.h"
#include "gfxobjseissect.h"

//---------------------------------------------------------------------------------------------

// Frequency rows (positive frequencies, excluding the even-length Nyquist bin),
// shifted wavenumber columns. Same convention as fk_spectrum_fixed.py/python.
struct FkSpectrum {
    QVector<float> amplitude;
    int nf = 0, nk = 0;
    double df = 0, dk = 0, k0 = 0;
};
Q_DECLARE_METATYPE(FkSpectrum)
FkSpectrum calculateFk(const QVector<float>& traces, int nx, int nt,
                       double dt, double dx, bool preprocess,
                       const std::function<bool()>& cancelled = {});

class FkWorker : public QThread {
    Q_OBJECT
public:
    QString path;
    int fileType = 1, first = 0, last = 0, sampleFirst = 0, sampleLast = 0;
    double dx = 1;
    bool preprocess = true;
    explicit FkWorker(QObject* parent = nullptr) : QThread(parent) {}
signals:
    void spectrumReady(FkSpectrum result);
    void failed(QString message);
    void progress(int percent);
protected:
    void run() override;
};

class DirItem
{
public:
    QString fname;
    int ftype; // 0 - dir, 1 - Sgd, 2 - ltod, 3 - file in ltod
    int ffid;
    int type;    // Sgd type
    qint64 lpos; // position in ltod
    qint64 llen; // file length in ltod
};

//---------------------------------------------------------------------------------------------

class Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject *parent = 0);

protected:
    std::atomic_bool m_stop;

    virtual void xprint(QString str)
    {
        emit eprint(str);
    }

    virtual void xprogr(int pers, QString str = "")
    {
        emit eprogr(pers, str);
    }

    virtual void xpers(int pers, QString str = "")
    {
        QString progr = QString::number(pers) + "%";
        emit eprogr(pers, str);
    }

    virtual int xprintf(const char *fmt, ...);

public slots:
    virtual void process()
    {
        emit efin("");
        emit finished();
    }

    void stop();

signals:
    void finished();
    void eprint(QString str);
    void eprogr(int pers, QString str);
    void efin(QString rc);
    void elckt(QString tape, QString info, bool lock);
};
//---------------------------------------------------------------------------------------------

typedef QList<DirItem> DirList;

class DirScanWorker : public Worker
{
    Q_OBJECT

public:
    explicit DirScanWorker(QString dirName, QObject *parent = 0);

protected:
    QString m_dirName;
    int m_type; // 1 - dir, 2 - ltod, 3 - tape

    DirList dlist;

public slots:
    virtual void process();

signals:
    void edir(DirList dist);
};
//---------------------------------------------------------------------------------------------

class TraceFindWorker : public Worker
{
    Q_OBJECT

public:
    explicit TraceFindWorker(SeisFile *sf, long long cidx, QString sidx,
                             double sval, QString dir, QString sign,
                             QObject *parent = 0);

protected:
    SeisFile *m_sf;
    QString m_sidx;
    double m_sval;
    QString m_dir; // direction
    QString m_sign;
    long long m_cidx;

public slots:
    virtual void process();

signals:
    void efind(int tridx);
};
//---------------------------------------------------------------------------------------------

class DelayScanWorker : public Worker
{
    Q_OBJECT

public:
    explicit DelayScanWorker(SeisFile *sf, QObject *parent = 0);

protected:
    SeisFile *m_sf;

public slots:
    virtual void process();

signals:
    void edelay(int dlymin, int dlymax);
};
//---------------------------------------------------------------------------------------------

class ChangeThExprWorker : public Worker
{
    Q_OBJECT

public:
    explicit ChangeThExprWorker(SeisFile *sf, QMap<QString, SeisTrExpr> elist,
                                QString undofn, int mode, QObject *parent = 0);

    ~ChangeThExprWorker();

protected:
    SeisFile *m_sf;
    QMap<QString, SeisTrExpr> m_elist;

    QString m_undofn;
    int m_mode; // 0 - update, 1 - undo

    QString update();
    QString undo();

public slots:
    virtual void process();
};
//---------------------------------------------------------------------------------------------

class SaveAsWorker : public Worker
{
    Q_OBJECT

public:
    explicit SaveAsWorker(SeisFile *sf, QString outfn, QString selh, int trmin,
                          int trmax, int trstp, int tmmin, int tmmax, int frmt,
                          bool rev, bool isProc, QObject *parent = 0);

protected:
    SeisFile *m_sf;
    QString m_outfn;

    QString m_selh;
    int m_selhi;
    int m_trmin;
    int m_trmax;
    int m_trstp;
    int m_tmmin;
    int m_tmmax;
    int m_frmt;
    bool m_rev;
    bool m_isProc;

    QString save();

    void CopyTrcByIdx(int outf);
    void CopyTrcByHdr(int outf);

public slots:
    virtual void process();
};
//---------------------------------------------------------------------------------------------

class DiffWorker : public Worker
{
    Q_OBJECT

public:
    explicit DiffWorker(SeisFile *sf1, SeisFile *sf2, QString outfn,
                        QString selh, int trmin, int trmax, int trstp,
                        int tmmin, int tmmax, int frmt, QObject *parent = 0);

protected:
    SeisFile *m_sf1;
    SeisFile *m_sf2;
    QString m_outfn;

    QString m_selh;
    int m_selhi;
    int m_trmin;
    int m_trmax;
    int m_trstp;
    int m_tmmin;
    int m_tmmax;
    int m_frmt;
    //    bool                      m_rev;
    //    bool                      m_isProc;

    QString save();

    void DiffTrcByIdx(int outf);
    //    void CopyTrcByHdr(int outf);

public slots:
    virtual void process();
};
//---------------------------------------------------------------------------------------------

class ImageExportWorker : public Worker
{
    Q_OBJECT

public:
    explicit ImageExportWorker(const QImage &image,
                               const SeisSectionCaptureSnapshot &snapshot,
                               const QString &outputPath, QObject *parent = 0);

    void requestCancel();

signals:
    void exportProgress(QString message);
    void exportFinished(bool canceled, bool succeeded, QString error);

protected:
    QImage m_image;
    SeisSectionCaptureSnapshot m_snapshot;
    QString m_outputPath;
    std::atomic<bool> m_cancelRequested;
    bool m_wasCanceled;
    bool m_succeeded;
    QString m_errorString;

public slots:
    void process() override;
};
//---------------------------------------------------------------------------------------------

// Built-in seismic quality metrics.
class MetricsWorker : public Worker
{
    Q_OBJECT

public:
    MetricsWorker(SeisFile* sf1,
                  SeisFile* sf2,
                  bool    isCalculation,
                  int     trmin,
                  int     trmax,
                  int     trstp,
                  int     tmmin,
                  int     tmmax,
                  bool    calcPSNR,
                  bool    calcSSIM,
                  bool    calcMSE,
                  bool    calcSNR,
                  bool    calcTPRS,
                  QObject *parent = nullptr);

public slots:
    void process();

signals:
    void emetrics(QString results);
    void eprogr(int pers, QString mess);
    void efin(QString mess);

private:
    SeisFile* m_sf1;         // 主文件
    SeisFile* m_sf2;         // 副文件（计算模式）
    bool      m_isCalculation; // 模式：true=计算，false=估算
    int       m_trmin;       // 道范围
    int       m_trmax;
    int       m_trstp;
    int       m_tmmin;       // 时间范围（采样点索引）
    int       m_tmmax;
    bool      m_calcPSNR;
    bool      m_calcSSIM;
    bool      m_calcMSE;
    bool      m_calcSNR;
    bool      m_calcTPRS;


    float computeTPRS_realMode(int pStart = 75, int pEnd = 99); // 仅估算模式用

    // FFT & shift helpers
    int  estimateShift(const float* a, const float* b, int n, int maxLag);
    static void applyShift(const float* src, float* dst, int n, int shift);


    // 辅助函数
    double safe_log10(double x);
    double ssim1d(const float* x, const float* y, int n, int win = 11);
};





#endif // WORKTHREAD_H
