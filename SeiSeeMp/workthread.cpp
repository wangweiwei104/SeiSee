#include "workthread.h"
#include "cwp.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileInfoList>
#include <QImageWriter>
#include <QObject>
#include <QPainter>
#include <QSaveFile>
#include <QSettings>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "gfxutil.h"
#include "util2.h"
#include "util2qt.h"

#include <math.h>
#include <algorithm>
#include <limits>
#include <cmath>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

//---------------------------------------------------------------------------------------------

Worker::Worker(QObject *parent)
    : QObject(parent)
{
    m_stop = false;
}

void Worker::stop()
{
    m_stop = true;
}

int Worker::xprintf(const char *fmt, ...)
{
    char buffer[2048];
    va_list argptr;
    int cnt;

    va_start(argptr, fmt);
    cnt = vsprintf(buffer, fmt, argptr);
    va_end(argptr);

    xprint(buffer);

    return cnt;
}

//---------------------------------------------------------------------------------------------

DirScanWorker::DirScanWorker(QString dirName, QObject *parent)
    : Worker(parent)
{
    m_dirName = dirName;
}

void DirScanWorker::process()
{
    DirItem ditem;
    int nr, n, l, pers;
    int cgg, junk, swap;

    QDir dir(m_dirName);
    dlist.clear();

    QFileInfoList list = dir.entryInfoList();

    l = list.size();

    dlist.clear();

    for (nr = n = 0; n < l; n++) {
        QFileInfo fileInfo = list.at(n);

        if (!fileInfo.isDir())
            continue;
        if (fileInfo.fileName() == ".")
            continue;

        ditem.ftype = 0;
        ditem.fname = fileInfo.fileName();

        dlist.append(ditem);
        nr++;
        pers = (int)((100. * nr) / l);
        xpers(pers, "Scanning directory...");
    }

    for (n = 0; n < l; n++) {
        QFileInfo fileInfo = list.at(n);

        if (fileInfo.isDir())
            continue;

        pers = (int)((100. * nr) / l);
        xpers(pers, "Scanning directory...");

        QByteArray qfn = fileInfo.filePath().toLocal8Bit();
        const char *fname = qfn.data();
        // int   type;

        if (m_stop)
            break;

        if (is_cst_f(fname)) {
            // continue;
            ditem.ftype = 10;
            ditem.fname = fileInfo.fileName();
            ditem.type = 3;
            ditem.ffid = 0;
            dlist.append(ditem);
            nr++;
        } else if (is_segy_f(fname, swap, cgg, junk) == 1) {
            ditem.ftype = 10;
            ditem.fname = fileInfo.fileName();
            ditem.type = 1;
            ditem.ffid = 0;
            dlist.append(ditem);
            nr++;
        } else if (is_su_f(fname, swap)) {
            // continue;
            ditem.ftype = 10;
            ditem.fname = fileInfo.fileName();
            ditem.type = 2;
            ditem.ffid = 0;
            dlist.append(ditem);
            nr++;
        }
    }

    if (m_stop) {
        dlist.clear();
    } else {
        emit edir(dlist);
    }

    emit efin("");
    emit finished(); //!!!!
}

//---------------------------------------------------------------------------------------------

DelayScanWorker::DelayScanWorker(SeisFile *sf, QObject *parent)
    : Worker(parent)
{
    m_sf = sf;
}

void DelayScanWorker::process()
{
    qint64 nt = m_sf->Nt();
    qint64 n;

    int dly;
    int dly_min;
    int dly_max;

    int pers, pp = -1;

    for (n = 0; n < nt; n++) {
        if (m_stop)
            break;

        pers = (n) * 100. / nt;

        if (pp != pers) {
            xpers(pers, "Scanning delay time");
            pp = pers;
        }

        dly = m_sf->Rh(n, "DELRECT");

        if (n == 0) {
            dly_min = dly;
            dly_max = dly;
        } else {
            if (dly_min > dly)
                dly_min = dly;
            if (dly_max < dly)
                dly_max = dly;
        }
    }

    if (!m_stop)
        emit edelay(dly_min, dly_max);

    QString mess;

    emit efin(mess);
    emit finished(); //!!!!
}

//---------------------------------------------------------------------------------------------

TraceFindWorker::TraceFindWorker(SeisFile *sf, long long cidx, QString sidx,
                                 double sval, QString dir, QString sign,
                                 QObject *parent)
    : Worker(parent)
{
    m_sf = sf;
    m_sidx = sidx;
    m_sval = sval;
    m_dir = dir;
    m_sign = sign;
    m_cidx = cidx;
    if (m_cidx < 0)
        m_cidx = 0;
}

void TraceFindWorker::process()
{
    bool find = false;
    long long n;

    if (m_sf->Active()) {
        int thidx = m_sf->ThIdx(m_sidx);

        double cval = m_sf->Th(m_cidx, thidx);
        double sv;

        if (m_sign == "=")
            sv = m_sval;
        else if (m_sign == "+")
            sv = cval + m_sval;
        else if (m_sign == "-")
            sv = cval - m_sval;
        else
            sv = m_sval;

        long long l = m_sf->Nt();
        int pers, pp = -1;

        if (m_dir == ">") {
            for (n = m_cidx; n < l; n++) {
                if (m_stop)
                    break;

                if (m_sf->Rh(n, thidx) == sv) {
                    find = true;
                    break;
                }

                pers = (100. * n) / l;
                if (pp != pers) {
                    xpers(pers, "Searching...");
                    pp = pers;
                }
            }
        } else if (m_dir == "<") {
            for (n = m_cidx; n >= 0; n--) {
                if (m_stop)
                    break;

                if (m_sf->Rh(n, thidx) == sv) {
                    find = true;
                    break;
                }

                pers = (100. * n) / l;
                if (pp != pers) {
                    xpers(pers, "Searching...");
                    pp = pers;
                }
            }
        } else if (m_dir == "=") {
            int tmid;
            int tfst = 0;
            int tlst = l - 1;

            double vf;
            double vl;
            double vm;

            vf = m_sf->Rh(tfst, thidx);
            vl = m_sf->Rh(tlst, thidx);

            for (;;) {
                tmid = (tfst + tlst) / 2;

                vm = m_sf->Rh(tmid, thidx);

                if (sv == vm) {
                    n = tmid;
                    find = true;
                    break;
                }

                if (tfst >= tlst)
                    break;
                if (m_stop)
                    break;

                if (vl > vf) {
                    if (sv > vm)
                        tfst = tmid + 1;
                    else
                        tlst = tmid - 1;
                } else {
                    if (sv < vm)
                        tfst = tmid + 1;
                    else
                        tlst = tmid - 1;
                }

                pers = (tmid) * 100. / l;

                if (pp != pers) {
                    xpers(pers, "Searching...");
                    pp = pers;
                }
            }
        }
    }

    //  int    cnt;

    QString mess;

    if (find)
        emit efind(n);
    else
        mess = "Trace Not Found";

    emit efin(mess);
    emit finished(); //!!!!
}
//---------------------------------------------------------------------------------------------

ChangeThExprWorker::ChangeThExprWorker(SeisFile *sf,
                                       QMap<QString, SeisTrExpr> elist,
                                       QString undofn, int mode,
                                       QObject *parent)
    : Worker(parent)
{
    m_sf = sf;
    m_elist = elist;
    m_undofn = undofn;
    m_mode = mode;
}

ChangeThExprWorker::~ChangeThExprWorker()
{
}

//---------------------------------------------------------------------------------------------

QString ChangeThExprWorker::undo()
{
    QString mess;
    long long n;

    long long l = m_sf->Nt();
    int i;
    int cnt;

    int uf = open_q(m_undofn, O_RDONLY | O_BINARY);

    QList<QString> hlist = m_elist.keys();
    int nh = hlist.count();

    if (uf < 0 || nh < 1) {
        return "Undo error";
    }

    long long fsz = getfilesize64(uf);

    int pers, pp = -1;

    int wsz = nh * sizeof(double);

    long long lu = fsz / wsz;

    double *backv = new double[nh];

    QMap<QString, double> vlist;

    for (n = 0; n < lu; n++) {
        /*
        if(m_stop)
        {
            mess="Terminated";
            break;
        }
        */

        cnt = read(uf, backv, wsz);

        if (cnt != wsz)
            break;

        for (i = 0; i < nh; i++) {
            vlist[hlist[i]] = backv[i];
        }

        m_sf->Wvlist(n, &vlist);

        pers = (n) * 100. / lu;

        if (pp != pers) {
            xpers(pers, "Undoing...");
            pp = pers;
        }
    }

    close(uf);

    delete[] backv;

    unlink_q(m_undofn);

    return "Undone";
}

QString ChangeThExprWorker::update()
{
    QString mess;
    long long n;

    long long l = m_sf->Nt();
    int i;

    int uf = open_q(m_undofn, O_CREAT | O_TRUNC | O_RDWR | O_BINARY);

    int pers, pp = -1;

    QList<QString> hlist = m_elist.keys();

    int nh = hlist.count();

    double *backv = new double[nh];

    for (n = 0; n < l; n++) {
        if (m_stop) {
            mess = "Terminated";
            break;
        }
        m_sf->Welist(n, &m_elist);

        for (i = 0; i < nh; i++) {
            backv[i] = m_elist[hlist[i]].V;
        }

        write(uf, backv, sizeof(double) * nh);

        pers = (n) * 100. / l;

        if (pp != pers) {
            xpers(pers, "Updating...");
            pp = pers;
        }
    }

    close(uf);

    delete[] backv;

    if (m_stop)
        mess = undo();

    return mess;
}

void ChangeThExprWorker::process()
{
    QString mess;

    if (m_sf->Active()) {
        if (m_mode == 0)
            mess = update();
        else
            mess = undo();
    }

    m_sf->Reset();

    emit efin(mess);
    emit finished(); //!!!!
}

//---------------------------------------------------------------------------------------------

SaveAsWorker::SaveAsWorker(SeisFile *sf, QString outfn, QString selh, int trmin,
                           int trmax, int trstp, int tmmin, int tmmax, int frmt,
                           bool rev, bool isProc, QObject *parent)
    : Worker(parent)
{
    m_sf = sf;
    m_outfn = outfn;
    m_selh = selh;
    m_tmmin =
        int((double)tmmin / (m_sf->Si() * 1000)); // unit: ms->sample index
    m_tmmax =
        int((double)tmmax / (m_sf->Si() * 1000)) + 1; // unit: ms->sample index
    m_trmin = trmin;
    m_trmax = trmax;
    m_trstp = trstp;
    m_frmt = frmt;
    m_rev = rev;
    m_isProc = isProc;

    m_selhi = sf->ThIdx(m_selh);
}

void SaveAsWorker::process()
{
    QString mess;
    char buf[3200];
    char ebuf[3200];
    int n, i, j;
    int cnt;
    int out_Ns = m_tmmax - m_tmmin;

    QByteArray qtxt;
    QString txthed;

    BinHed binhed;

    txthed = m_sf->TxtHed();

    qtxt = txthed.toLocal8Bit();
    char *s = qtxt.data();

    //   if(m_sf->Active())
    //   {
    //    }

    int outf = open_q(m_outfn, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0664);

    if (outf < 0) {
        mess = "Cannot create output file: " + m_outfn;
        goto END;
    }

    memset(buf, ' ', 3200);

    for (i = j = n = 0; n < strlen(s); n++) {
        char c = s[n];
        if (c == '\n') {
            i++;
            j = 0;
            continue;
        } else {
            if (j < 80 && i < 40) {
                buf[i * 80 + j] = c;
                j++;
            }
        }
    }

    asebdn(ebuf, buf, 3200);

    cnt = write(outf, ebuf, 3200);

    binhed.set(1, m_frmt, m_sf->binHed());
    binhed.setVal(7, out_Ns);

    cnt = write(outf, binhed.buf(), 400);

    if (m_selhi == 0)
        CopyTrcByIdx(outf);
    else
        CopyTrcByHdr(outf);

    close(outf);

END:
    emit efin(mess);
    emit finished(); //!!!!
}

void SaveAsWorker::CopyTrcByIdx(int outf)
{
    long long tidx;
    long long tcount = m_sf->Nt();
    int cnt;
    byte *trbuf;
    int pp = -1;
    int pers;

    int inc = m_trstp;
    if (inc < 1)
        inc = 1;

    long long trccount = (m_trmax - m_trmin);

    if (m_trstp < 1)

        for (tidx = m_trmin; tidx < m_trmax; tidx += inc) {
            long long ti = tidx;
            if (m_rev)
                ti = tcount - tidx - 1;

            trbuf =
                m_sf->MakeTrace(cnt, ti, 1, m_frmt, m_tmmin, m_tmmax, m_isProc);
            cnt = write(outf, trbuf, cnt);
            pers = (100. * (tidx - m_trmin)) / trccount;
            if (pp != pers) {
                xpers(pers, "Saving...");
                pp = pers;
            }

            delete[] trbuf;
        }
}

void SaveAsWorker::CopyTrcByHdr(int outf)
{
    long long tidx;
    long long tcount = m_sf->Nt();
    int cnt;
    byte *trbuf;
    int pp = -1;
    int pers;

    for (tidx = 0; tidx < tcount; tidx++) {
        long long ti = tidx;
        if (m_rev)
            ti = tcount - tidx - 1;

        int h = m_sf->Rh(ti, m_selhi);

        if (h < m_trmin || h > m_trmax) {
            continue;
        }

        if (m_trstp > 1 && h % m_trstp)
            continue;

        trbuf = m_sf->MakeTrace(cnt, ti, 1, m_frmt, m_tmmin, m_tmmax, m_isProc);
        cnt = write(outf, trbuf, cnt);
        pers = (100. * tidx) / tcount;
        if (pp != pers) {
            xpers(pers, "Saving...");
            pp = pers;
        }

        delete[] trbuf;
    }
}

//---------------------------------------------------------------------------------------------

DiffWorker::DiffWorker(SeisFile *sf1, SeisFile *sf2, QString outfn,
                       QString selh, int trmin, int trmax, int trstp, int tmmin,
                       int tmmax, int frmt,
                       //                      bool    rev,
                       //                      bool    isProc,
                       QObject *parent)
    : Worker(parent)
{
    m_sf1 = sf1;
    m_sf2 = sf2;
    m_outfn = outfn;
    m_selh = selh;
    m_tmmin =
        int((double)tmmin / (m_sf1->Si() * 1000)); // unit: ms->sample index
    m_tmmax =
        int((double)tmmax / (m_sf1->Si() * 1000)) + 1; // unit: ms->sample index
    m_trmin = trmin;
    m_trmax = trmax;
    m_trstp = trstp;
    m_frmt = frmt;
    //    m_rev   = rev;
    //    m_isProc = isProc;

    m_selhi = sf1->ThIdx(m_selh);
}

void DiffWorker::process()
{
    QString mess;
    char buf[3200];
    char ebuf[3200];
    int n, i, j;
    int cnt;
    int out_Ns = m_tmmax - m_tmmin;

    QByteArray qtxt;
    QString txthed;

    BinHed binhed;

    txthed = m_sf1->TxtHed();

    qtxt = txthed.toLocal8Bit();
    char *s = qtxt.data();

    //   if(m_sf->Active())
    //   {
    //    }

    int outf = open_q(m_outfn, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0664);

    if (outf < 0) {
        mess = "Cannot create output file: " + m_outfn;
        goto END;
    }

    memset(buf, ' ', 3200);

    for (i = j = n = 0; n < strlen(s); n++) {
        char c = s[n];
        if (c == '\n') {
            i++;
            j = 0;
            continue;
        } else {
            if (j < 80 && i < 40) {
                buf[i * 80 + j] = c;
                j++;
            }
        }
    }

    asebdn(ebuf, buf, 3200);

    cnt = write(outf, ebuf, 3200);

    binhed.set(1, m_frmt, m_sf1->binHed());
    binhed.setVal(7, out_Ns);

    cnt = write(outf, binhed.buf(), 400);

    DiffTrcByIdx(outf);

    close(outf);

END:
    emit efin(mess);
    emit finished(); //!!!!
}

void DiffWorker::DiffTrcByIdx(int outf)
{
    long long tidx;
    long long tcount = m_sf1->Nt();
    int cnt;
    int cnt2;
    byte *trbuf;
    float *data2;
    int pp = -1;
    int pers;

    int inc = m_trstp;
    if (inc < 1)
        inc = 1;

    long long trccount = (m_trmax - m_trmin);

    if (m_trstp < 1)

        for (tidx = m_trmin; tidx < m_trmax; tidx += inc) {
            long long ti = tidx;
            //        if(m_rev)  ti = tcount-tidx-1;
            data2 = m_sf2->GetTraceSample(cnt, ti, 1, m_frmt, m_tmmin,
                                          m_tmmax); // get data2

            trbuf = m_sf1->MakeTraceDiff(cnt, ti, 1, m_frmt, m_tmmin, m_tmmax,
                                         data2); // data1 - data2

            cnt = write(outf, trbuf, cnt);
            pers = (100. * (tidx - m_trmin)) / trccount;
            if (pp != pers) {
                xpers(pers, "Saving...");
                pp = pers;
            }

            delete[] trbuf;
        }
    //    xpers(1.,"Saving...");
}

ImageExportWorker::ImageExportWorker(const QImage &image,
                                     const SeisSectionCaptureSnapshot &snapshot,
                                     const QString &outputPath,
                                     QObject *parent)
    : Worker(parent)
    , m_image(image)
    , m_snapshot(snapshot)
    , m_outputPath(outputPath)
    , m_cancelRequested(false)
    , m_wasCanceled(false)
    , m_succeeded(false)
{
}

void ImageExportWorker::requestCancel()
{
    m_cancelRequested.store(true);
}

void ImageExportWorker::process()
{
    const auto finish = [this]() {
        emit exportFinished(m_wasCanceled, m_succeeded, m_errorString);
        emit finished();
    };

    if (m_cancelRequested.load()) {
        m_wasCanceled = true;
        finish();
        return;
    }

    emit exportProgress(tr("Rendering high-resolution seismic section..."));
    QImage sectionImage =
        GfxObjSeisSect::renderCaptureSnapshot(m_snapshot);
    if (sectionImage.isNull()) {
        m_errorString = tr("Could not render the seismic section.");
        finish();
        return;
    }
    if (m_cancelRequested.load()) {
        m_wasCanceled = true;
        finish();
        return;
    }

    QPainter imagePainter(&m_image);
    const QPoint sectionPosition(
        qRound(m_snapshot.targetPosition.x() *
               m_snapshot.captureScaleX),
        qRound(m_snapshot.targetPosition.y() *
               m_snapshot.captureScaleY));
    imagePainter.drawImage(sectionPosition, sectionImage);
    imagePainter.end();

    emit exportProgress(tr("Encoding and saving PNG..."));
    QSaveFile outputFile(m_outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        m_errorString = outputFile.errorString();
        finish();
        return;
    }

    QImageWriter writer(&outputFile, "png");
    if (!writer.write(m_image)) {
        if (m_cancelRequested.load())
            m_wasCanceled = true;
        else
            m_errorString = writer.errorString();
        outputFile.cancelWriting();
        finish();
        return;
    }

    if (m_cancelRequested.load()) {
        m_wasCanceled = true;
        outputFile.cancelWriting();
        finish();
        return;
    }

    if (!outputFile.commit()) {
        m_errorString = outputFile.errorString();
        finish();
        return;
    }

    m_succeeded = true;
    finish();
}

MetricsWorker::MetricsWorker(SeisFile* sf1,
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
                             QObject *parent)
    : Worker(parent)
{
    m_sf1 = sf1;
    m_sf2 = sf2;
    m_isCalculation = isCalculation;

    // DiffWorker 里是 ms -> sample index
    // tmmin/tmmax 输入单位是 ms，这里转换为采样点下标
    // 注意：tmmax 我也做成“右开区间”，方便循环
    m_tmmin = int((double)tmmin / (m_sf1->Si() * 1000.0));
    m_tmmax = int((double)tmmax / (m_sf1->Si() * 1000.0)) + 1;

    m_trmin = trmin;
    m_trmax = trmax;
    m_trstp = trstp;
    m_calcPSNR = calcPSNR;
    m_calcSSIM = calcSSIM;
    m_calcMSE  = calcMSE;
    m_calcSNR  = calcSNR;
    m_calcTPRS = calcTPRS;

}

double MetricsWorker::safe_log10(double x)
{
    // 避免 log10(0) / log10(负数)
    if (x <= 0) return -1e30;
    return std::log10(x);
}

double MetricsWorker::snrFromTprs(double tprs)
{
    if (!std::isfinite(tprs) || tprs < 0.0 || tprs > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (tprs == 0.0) return -std::numeric_limits<double>::infinity();
    if (tprs == 1.0) return std::numeric_limits<double>::infinity();
    return 10.0 * (std::log10(tprs) - std::log10(1.0 - tprs));
}

// 1D SSIM：用滑窗(默认 win=11)在 1D 信号上做 SSIM，并取平均
// 公式：SSIM = ((2uxuy + C1)(2cov + C2))/((ux^2+uy^2 + C1)(vx+vy + C2))
// 这里 vx/vy 用局部方差，cov 为局部协方差
double MetricsWorker::ssim1d(const float* x, const float* y, int n, int win)
{
    if (!x || !y || n <= 1) return 0.0;
    if (win < 3) win = 3;
    if (win % 2 == 0) win += 1;
    if (win > n) win = (n % 2 == 1) ? n : n - 1;
    if (win < 3) return 0.0;

    int half = win / 2;

    // 估计动态范围 L（用 max-min）
    float xmin = x[0], xmax = x[0];
    float ymin = y[0], ymax = y[0];
    for (int i = 1; i < n; ++i)
    {
        xmin = std::min(xmin, x[i]); xmax = std::max(xmax, x[i]);
        ymin = std::min(ymin, y[i]); ymax = std::max(ymax, y[i]);
    }
    double L = std::max((double)(xmax - xmin), (double)(ymax - ymin));
    if (L <= 0) L = 1.0;

    // SSIM 常数（标准做法）
    double C1 = (0.01 * L) * (0.01 * L);
    double C2 = (0.03 * L) * (0.03 * L);

    // 逐点滑窗计算并平均
    double ssim_sum = 0.0;
    int    ssim_cnt = 0;

    for (int c = 0; c < n; ++c)
    {
        int l = std::max(0, c - half);
        int r = std::min(n - 1, c + half);
        int m = r - l + 1;

        double mux = 0.0, muy = 0.0;
        for (int i = l; i <= r; ++i)
        {
            mux += x[i];
            muy += y[i];
        }
        mux /= m;
        muy /= m;

        double vx = 0.0, vy = 0.0, cov = 0.0;
        for (int i = l; i <= r; ++i)
        {
            double dx = x[i] - mux;
            double dy = y[i] - muy;
            vx  += dx * dx;
            vy  += dy * dy;
            cov += dx * dy;
        }
        vx  /= m;
        vy  /= m;
        cov /= m;

        double num = (2.0 * mux * muy + C1) * (2.0 * cov + C2);
        double den = (mux * mux + muy * muy + C1) * (vx + vy + C2);

        double s = (den != 0.0) ? (num / den) : 0.0;
        ssim_sum += s;
        ssim_cnt++;
    }

    if (ssim_cnt < 1) return 0.0;
    return ssim_sum / ssim_cnt;
}

void MetricsWorker::process()
{
    QString mess;

    if (!m_sf1 || !m_sf1->Active())
    {
        mess = "Primary file is not active.";
        emit efin(mess);
        emit finished();
        return;
    }

    if (m_isCalculation)
    {
        if (!m_sf2 || !m_sf2->Active())
        {
            mess = "Secondary file is not active (calculation mode requires two files).";
            emit efin(mess);
            emit finished();
            return;
        }

        if (m_sf1->Nt() != m_sf2->Nt())
        {
            mess = "Traces must be same for synthetic metrics.";
            emit efin(mess);
            emit finished();
            return;
        }
        if (m_sf1->Ns() != m_sf2->Ns())
        {
            mess = "Samples must be same for synthetic metrics.";
            emit efin(mess);
            emit finished();
            return;
        }
    }

    int Ns = m_sf1->Ns();
    m_tmmin = std::max(0, m_tmmin);
    m_tmmax = std::min(Ns, m_tmmax);
    if (m_tmmax <= m_tmmin)
    {
        mess = "Time window is invalid.";
        emit efin(mess);
        emit finished();
        return;
    }

    if (m_calcTPRS && m_tmmax - m_tmmin > 2 * PFA_MAX) {
        emit efin("TPRS time window exceeds the libCwp FFT length limit. Select a shorter time range.");
        emit finished();
        return;
    }
    long long Nt = m_sf1->Nt();
    m_trmin = std::max(0, m_trmin);
    m_trmax = std::min((int)Nt, m_trmax);
    if (m_trmax <= m_trmin)
    {
        mess = "Trace range is invalid.";
        emit efin(mess);
        emit finished();
        return;
    }

    int inc = m_trstp;
    if (inc < 1) inc = 1;

    int win = 11;

    // ---- synthetic: sf1 vs sf2 ----
    if (m_isCalculation)
    {
        double mse_sum = 0.0;
        double sig_sum = 0.0;
        double ssim_sum = 0.0;
        double peak = 0.0;
        long long total_samples = 0;
        long long total_traces  = 0;

        long long trccount = (m_trmax - m_trmin);
        int pp = -1;

        for (long long tidx = m_trmin; tidx < m_trmax; tidx += inc)
        {
            float* clean  = m_sf1->Rs(tidx);
            float* noisy  = m_sf2->Rs(tidx);
            if (!clean || !noisy) continue;
            int n = (m_tmmax - m_tmmin);

            // 【改】peak 使用干净信号
            for (int i = m_tmmin; i < m_tmmax; ++i)
                   peak = std::max(peak, (double)std::fabs(clean[i]));

            // 【改】MSE 和信号功率
            for (int i = m_tmmin; i < m_tmmax; ++i)
               {
                   double noise = (double)noisy[i] - (double)clean[i];
                   mse_sum += noise * noise;
                   sig_sum += (double)clean[i] * (double)clean[i];
               }


            ssim_sum += ssim1d(clean + m_tmmin, noisy + m_tmmin, n, win);

            total_samples += n;
            total_traces++;

            int pers = (int)(100.0 * (double)(tidx - m_trmin) / (double)trccount);

            if (tidx % 10 == 0) // 每 10 个 trace 输出一次
                {
                    qDebug() << "tidx=" << tidx
                             << "m_trmin=" << m_trmin
                             << "trccount=" << trccount
                             << "pers=" << pers;
                }

                if (pp != pers)
                {
                    qDebug() << "Calling xpers:" << pers;  // ← 添加这行
                    emit eprogr(pers, "Computing metrics...");
                    pp = pers;
                }



            if (pp != pers)
            {
                emit eprogr(pers, "Computing metrics...");
            }

            if (m_stop) break;
        }

        if (total_samples <= 0 || total_traces <= 0)
        {
            mess = "No valid traces to compute.";
            emit efin(mess);
            emit finished();
            return;
        }

        double mse  = mse_sum / (double)total_samples;
        double rmse = std::sqrt(std::max(0.0, mse));
        double psnr = 0.0;
        if (mse <= 0.0) psnr = 1e30;
        else            psnr = 10.0 * safe_log10((peak * peak) / mse);

        double snr = 0.0;
        if (mse_sum <= 0.0) snr = 1e30;
        else                snr = 10.0 * safe_log10(sig_sum / mse_sum);

        double mean_ssim = ssim_sum / (double)total_traces;

        QString out;
        out += "Mode: Calculation (signal vs noisy)\n";
        out += QString("Signal file: %1\nNoisy file: %2\n")
            .arg(QString::fromLocal8Bit(m_sf1->Fname().c_str()))
            .arg(QString::fromLocal8Bit(m_sf2->Fname().c_str()));
        out += QString("Trace range: [%1, %2), step=%3\n").arg(m_trmin).arg(m_trmax).arg(inc);
        out += QString("Time samples: [%1, %2)\n").arg(m_tmmin).arg(m_tmmax);
        out += QString("Total traces used: %1\n").arg(total_traces);
        out += QString("Total samples used: %1\n\n").arg((qlonglong)total_samples);

        if(m_calcMSE)
        {
            out += QString("MSE  = %1\n").arg(mse, 0, 'g', 10);
            out += QString("RMSE = %1\n").arg(rmse, 0, 'g', 10);
        }

        if(m_calcPSNR)
        {
            out += QString("PSNR = %1 dB\n").arg(psnr, 0, 'g', 10);
        }

        if(m_calcSNR)
        {
            out += QString("SNR  = %1 dB\n").arg(snr,  0, 'g', 10);
        }

        if(m_calcSSIM)
        {
            out += QString("SSIM = %1\n").arg(mean_ssim, 0, 'g', 10);
        }
        if (m_calcTPRS) {
            out += "TPRS: N/A in Calculation mode (only for Estimation mode)\n";
        }

        emit emetrics(out);
        emit efin(out);
        emit finished();
        return;
    }

    // ---- real: only sf1 ----
    {
        int n = (m_tmmax - m_tmmin);
        std::vector<double> mean(n, 0.0);

        long long Nt = m_sf1->Nt();
        int inc = m_trstp; if (inc < 1) inc = 1;
        long long trccount = (m_trmax - m_trmin);

        // SNR depends on TPRS; only the other metrics need a mean trace.
        bool anyClassic = (m_calcPSNR || m_calcSSIM || m_calcMSE);
        auto appendTprsMetrics = [&](QString& out, double tprs, bool valid) {
            if (m_calcSNR) {
                double snr = valid ? snrFromTprs(tprs) : std::numeric_limits<double>::quiet_NaN();
                QString value = std::isnan(snr) ? QString("N/A") :
                    std::isinf(snr) ? QString(snr > 0 ? "inf" : "-inf") : QString::number(snr, 'g', 10);
                out += QString("SNR  = %1 dB\n").arg(value);
                out += QString("SNR method: 10*log10(TPRS/(1-TPRS)); TPRS = %1\n").arg(tprs, 0, 'g', 10);
                if (!valid) out += "SNR unavailable: TPRS needs at least two valid traces, eight samples and finite nonzero total power.\n";
            }
            if (m_calcTPRS) out += QString("TPRS = %1\n").arg(tprs, 0, 'g', 10);
        };
        if ((m_calcTPRS || m_calcSNR) && !anyClassic)
        {
            bool valid = false;
            double tprs = computeTPRS_realMode(0, 100, &valid);
            if (m_stop) { emit efin("Estimation stopped."); emit finished(); return; }
            QString out;
            out += "Mode: Estimation (one file)\n";
            out += QString("Noisy file: %1\n").arg(QString::fromLocal8Bit(m_sf1->Fname().c_str()));
            out += QString("Trace range: [%1, %2), step=%3\n").arg(m_trmin).arg(m_trmax).arg(inc);
            out += QString("Time samples: [%1, %2)\n\n").arg(m_tmmin).arg(m_tmmax);
            appendTprsMetrics(out, tprs, valid);
            emit emetrics(out);
            emit efin(out);
            emit finished();
            return;
        }

        // Mean-reference metrics first; compute TPRS once for TPRS and/or SNR at the end.
        long long used_tr = 0;
        int pp = -1;

        for (long long tidx = m_trmin; tidx < m_trmax; tidx += inc)
        {
            float* a = m_sf1->Rs(tidx);
            if (!a) continue;

            for (int i = 0; i < n; ++i)
                mean[i] += a[m_tmmin + i];

            used_tr++;

            int pers = (int)(50.0 * (double)(tidx - m_trmin) / (double)trccount);
            if (pp != pers)
            {
                emit eprogr(pers, "Building mean trace...");
                pp = pers;
            }
            if (m_stop) break;
        }

        if (used_tr <= 0)
        {
            QString mess = "No valid traces to compute (estimation mode).";
            emit efin(mess);
            emit finished();
            return;
        }

        for (int i = 0; i < n; ++i) mean[i] /= (double)used_tr;

        // 2) Mean-reference metrics (MSE/PSNR/SSIM).
        double mse_sum = 0.0;
        double ssim_sum = 0.0;
        double peak = 0.0;
        long long total_samples = 0;
        long long total_traces  = 0;

        pp = -1;
        int win = 11;
        for (int i = 0; i < n; ++i) peak = std::max(peak, std::fabs(mean[i]));

        for (long long tidx = m_trmin; tidx < m_trmax; tidx += inc)
        {
            float* a = m_sf1->Rs(tidx);
            if (!a) continue;

            for (int i = 0; i < n; ++i)
            {
                double d = (double)a[m_tmmin + i] - mean[i];
                mse_sum += d * d;
            }

            std::vector<float> meanf(n);
            for (int i = 0; i < n; ++i) meanf[i] = (float)mean[i];

            if (m_calcSSIM) ssim_sum += ssim1d(a + m_tmmin, meanf.data(), n, win);

            total_samples += n;
            total_traces++;

            int pers = 50 + (int)(25.0 * (double)(tidx - m_trmin) / (double)trccount); // 50→75
            if (pp != pers)
            {
                emit eprogr(pers, "Computing classic metrics...");
                pp = pers;
            }
            if (m_stop) break;
        }

        double mse  = (total_samples > 0) ? (mse_sum / (double)total_samples) : 0.0;
        double rmse = std::sqrt(std::max(0.0, mse));
        double psnr = (mse <= 0.0) ? 1e30 : (10.0 * safe_log10((peak * peak) / mse));
        double mean_ssim = (total_traces > 0) ? (ssim_sum / (double)total_traces) : 0.0;

        QString out;
        out += "Mode: Estimation (one file)\n";
        out += QString("Noisy file: %1\n").arg(QString::fromLocal8Bit(m_sf1->Fname().c_str()));
        out += "MSE/PSNR/SSIM reference: Mean trace of selected traces\n";
        out += QString("Trace range: [%1, %2), step=%3\n").arg(m_trmin).arg(m_trmax).arg(inc);
        out += QString("Time samples: [%1, %2)\n").arg(m_tmmin).arg(m_tmmax);
        out += QString("Total traces used: %1\n").arg(total_traces);
        out += QString("Total samples used: %1\n\n").arg((qlonglong)total_samples);

        if(m_calcMSE)  { out += QString("MSE  = %1\n").arg(mse, 0, 'g', 10);
                         out += QString("RMSE = %1\n").arg(rmse, 0, 'g', 10); }
        if(m_calcPSNR) { out += QString("PSNR = %1 dB\n").arg(psnr, 0, 'g', 10); }
        if(m_calcSSIM) { out += QString("SSIM = %1\n").arg(mean_ssim, 0, 'g', 10); }

        // 3) TPRS and its SNR conversion share one computation.
        if (m_calcTPRS || m_calcSNR)
        {
            bool valid = false;
            double tprs = computeTPRS_realMode(75, 99, &valid);
            if (m_stop) { emit efin("Estimation stopped."); emit finished(); return; }
            appendTprsMetrics(out, tprs, valid);
        }

        emit emetrics(out);
        emit efin(out);
        emit finished();
        return;
    }

}

// 估计 b 相对 a 的整数时移：max_{lag in [-maxLag,maxLag]} sum a[i]*b[i+lag]
int MetricsWorker::estimateShift(const float* a, const float* b, int n, int maxLag)
{
    if (!a || !b || n <= 1) return 0;
    maxLag = std::max(0, std::min(maxLag, n - 1));

    float best = -std::numeric_limits<float>::max();
    int bestLag = 0;

    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        if (m_stop) return 0;
        float s = 0.0f;
        int i0 = std::max(0, -lag);
        int i1 = std::min(n - 1, n - 1 - lag);
        for (int i = i0; i <= i1; ++i)
        {
            s += a[i] * b[i + lag];
        }
        if (s > best)
        {
            best = s;
            bestLag = lag;
        }
    }
    return bestLag;
}


void MetricsWorker::applyShift(const float* src, float* dst, int n, int shift)
{
    for (int i = 0; i < n; ++i)
    {
        int j = i + shift;
        dst[i] = (j >= 0 && j < n) ? src[j] : 0.0f;
    }
}

namespace {
// Backend adapter: reuse libCwp's float real FFT, with zero padding only.
void realTraceSpectrum(const float* samples, int count, int nfft,
                       std::vector<float>& time, std::vector<complex>& spectrum)
{
    std::fill(time.begin(), time.end(), 0.0f);
    std::copy(samples, samples + count, time.begin());
    pfarc(1, nfft, time.data(), spectrum.data());
}
}

float MetricsWorker::computeTPRS_realMode(int pStart, int pEnd, bool* valid)
{
    if (valid) *valid = false;
    auto progress = [&](int percent, const char* message) {
        emit eprogr(pStart + (pEnd - pStart) * percent / 100, message);
    };
    const int count = m_tmmax - m_tmmin;
    const int step = std::max(1, m_trstp);
    std::vector<std::vector<float>> traces;
    for (int t = m_trmin; t < m_trmax; t += step) {
        if (m_stop) return 0.0f;
        const float* samples = m_sf1->Rs(t);
        if (samples) traces.emplace_back(samples + m_tmmin, samples + m_tmmax);
    }
    if (traces.size() < 2 || count < 8) {
        progress(100, "TPRS: not enough data");
        return 0.0f;
    }
    const int nfft = npfar(count);
    const int bins = nfft / 2; // Keep the previous convention: omit Nyquist.
    std::vector<float> time(nfft), px(bins, 0.0f), ps(bins, 0.0f);
    std::vector<complex> spectrum(bins + 1), first(bins + 1), second(bins + 1);
    for (int t = 0; t < int(traces.size()); ++t) {
        if (m_stop) return 0.0f;
        realTraceSpectrum(traces[t].data(), count, nfft, time, spectrum);
        for (int k = 0; k < bins; ++k)
            px[k] += spectrum[k].r * spectrum[k].r + spectrum[k].i * spectrum[k].i;
        progress(40 * t / int(traces.size()), "TPRS: computing Px...");
    }
    const int pairs = int(traces.size()) - 1;
    const int maxLag = std::max(1, std::min(200, count / 2));
    std::vector<float> shifted(count);
    for (int t = 0; t < pairs; ++t) {
        if (m_stop) return 0.0f;
        const int lag = estimateShift(traces[t].data(), traces[t+1].data(), count, maxLag);
        applyShift(traces[t+1].data(), shifted.data(), count, lag);
        realTraceSpectrum(traces[t].data(), count, nfft, time, first);
        realTraceSpectrum(shifted.data(), count, nfft, time, second);
        for (int k = 0; k < bins; ++k)
            ps[k] += std::fabs(first[k].r * second[k].r + first[k].i * second[k].i);
        progress(40 + 60 * t / pairs, "TPRS: computing Ps...");
    }
    float signalPower = 0.0f, totalPower = 0.0f;
    for (int k = 0; k < bins; ++k) {
        signalPower += ps[k] / pairs;
        totalPower += px[k] / float(traces.size());
    }
    progress(100, "TPRS: done");
    if (!(totalPower > 0.0f) || !std::isfinite(totalPower) || !std::isfinite(signalPower))
        return 0.0f;
    if (valid) *valid = true;
    return std::max(0.0f, std::min(1.0f, signalPower / totalPower));
}
