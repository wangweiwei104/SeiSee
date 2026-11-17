#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gfxsrcseismemsegd.h"


#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QUrl>
#include <QWidget>

#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "furlib.h"
#include "gfxobj.h"
#include "util2.h"

// #include <vld.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4267 4100 4068)
#endif

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    busy = 0;

    WorkThread = NULL;
    ChkfThread = NULL;

    curDir = "";

    /*
        QStringList paths = QCoreApplication::libraryPaths();
        paths.append(".");
        paths.append("imageformats");
        paths.append("platforms");
        paths.append("sqldrivers");
        QCoreApplication::setLibraryPaths(paths);
    */
    segd = new TSegD();

    cs_changed = false;

    ui->setupUi(this);

#if QT_VERSION >= 0x50700
    //   printf("v=%0x\n", QT_VERSION); fflush(stdout);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    m_statMess = new QLabel("SegDSeeMp", this);
    m_statMess->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statTime = new QLabel("T=", this);
    m_statTime->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statChSet = new QLabel("Ch=", this);
    m_statChSet->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statChan = new QLabel("Chan=", this);
    m_statChan->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statFfid = new QLabel("FFID=", this);
    m_statFfid->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statSp = new QLabel("SP=", this);
    m_statSp->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statusRight = new QLabel(VERSION, this);
    m_statusRight->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    statusBar()->addPermanentWidget(m_statMess, 1);
    statusBar()->addPermanentWidget(m_statTime, 1);
    statusBar()->addPermanentWidget(m_statChSet, 1);
    statusBar()->addPermanentWidget(m_statChan, 1);
    statusBar()->addPermanentWidget(m_statFfid, 1);
    statusBar()->addPermanentWidget(m_statSp, 1);
    statusBar()->addPermanentWidget(m_statusRight, 2);

    progressBar = new QProgressBar;
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setTextVisible(true);
    progressBar->setFormat("xxxx%p%");
    progressBar->setTextVisible(false);
    this->statusBar()->addPermanentWidget(progressBar, 1);

    progressBar->setValue(0);
    progressBar->setMinimum(0);
    progressBar->setMaximum(100);

    chsSel.resize(1000);
    chsSel.fill(false);

    _f1 = 10;
    _f2 = 20;
    _f3 = 500;
    _f4 = 1000;

    _agcw = 1000;

    QVBoxLayout *vlayout;

    dirGrid.setRowCount(0);
    dirGrid.setColCount(3);
    dirGrid.verticalHeader()->setVisible(false);

    dirGrid.setColLabel(0, "Name");
    dirGrid.setColLabel(1, "FFID");
    dirGrid.setColLabel(2, "Type");
    dirGrid.setColWidth(1, 50);
    dirGrid.setColWidth(2, 50);

    vlayout = new QVBoxLayout;

    vlayout->setMargin(1);
    vlayout->setSpacing(1);

    vlayout->addWidget(&dirGrid);

    ui->dirFrame->setLayout(vlayout);

    chsGrid.setRowCount(0);
    chsGrid.setColCount(2);
    chsGrid.verticalHeader()->setVisible(false);

    chsGrid.setColLabel(0, "");
    chsGrid.setColLabel(1, "Channel Set #");
    chsGrid.setColWidth(0, 20);
    chsGrid.setColChkbx(0, true);

    vlayout = new QVBoxLayout;

    vlayout->setMargin(1);
    vlayout->setSpacing(1);

    vlayout->addWidget(&chsGrid);

    ui->chsGridFrame->setLayout(vlayout);

    hdrGrid.setRowCount(0);
    hdrGrid.setColCount(1);
    hdrGrid.verticalHeader()->setVisible(false);

    hdrGrid.setColLabel(0, "Header Name");

    vlayout = new QVBoxLayout;

    vlayout->setMargin(1);
    vlayout->setSpacing(1);

    vlayout->addWidget(&hdrGrid);

    ui->hdrFrame->setLayout(vlayout);

    seisView.setXs(1);
    seisView.setYs(1);

    hlist.append("ChSet#");
    hlist.append("Chan #");

    timeAxis.setView(&timeView);

    timeAxis.setYo(0);
    timeAxis.setXo(0);
    timeAxis.setX1(0);
    timeAxis.setX2(13);
    timeAxis.setY1(0);
    timeAxis.setY2(6);
    timeAxis.setYs(10);
    timeAxis.setTi(0.1);

    hdrsLab.setView(&hlabView);

    hlabView.setFixedHeight(42);

    hdrsLab.setTrim(false);

    hdrsScrl = new MyScrollArea(&hdrsView, 2);
    seisScrl = new MyScrollArea(&seisView, 3);
    timeScrl = new MyScrollArea(&timeView, 1);

    hdrsScrl->setMaximumHeight(42);
    hdrsScrl->setFixedHeight(42);
    timeScrl->setMaximumWidth(62);

    myEventCatcher *ef;

    ef = new myEventCatcher(1, this);
    ui->edGn->installEventFilter(ef);

    connect(ef, SIGNAL(whellEvent(int, QWheelEvent *)), this,
            SLOT(edWhellEvent(int, QWheelEvent *)));

    ef = new myEventCatcher(2, this);
    ui->edTm->installEventFilter(ef);

    connect(ef, SIGNAL(whellEvent(int, QWheelEvent *)), this,
            SLOT(edWhellEvent(int, QWheelEvent *)));

    ef = new myEventCatcher(3, this);
    ui->edTr->installEventFilter(ef);

    connect(ef, SIGNAL(whellEvent(int, QWheelEvent *)), this,
            SLOT(edWhellEvent(int, QWheelEvent *)));

    connect(seisScrl, SIGNAL(mouseEvent(QMouseEvent *)), this,
            SLOT(viewMouseEvent(QMouseEvent *)));

    connect(seisScrl, SIGNAL(zoomW(QRect)), this, SLOT(winZoom(QRect)));

    connect(seisScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoom(double, double, int, int)));

    connect(timeScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoomV(double, double, int, int)));

    connect(hdrsScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoomH(double, double, int, int)));

    connect(&dirGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(dirGridEvent(int, int)));

    connect(&hdrGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(hdrGridEvent(int, int)));

    connect(&chsGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(chsGridEvent(int, int)));

    connect(&ProcParmDlg, SIGNAL(changedEvent()), this, SLOT(procParmEvent()));

    hdrsScrl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hdrsScrl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeScrl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeScrl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QGridLayout *seisLayout;

    seisLayout = new QGridLayout;

    timeScrl->setFixedWidth(50);

    seisLayout->addWidget(&hlabView, 0, 0);
    seisLayout->addWidget(timeScrl, 1, 0);
    seisLayout->addWidget(hdrsScrl, 0, 1);
    seisLayout->addWidget(seisScrl, 1, 1);
    seisLayout->setMargin(0);
    seisLayout->setSpacing(0);

    hlabView.setFixedHeight(hdrsScrl->height());
    hlabView.setFixedWidth(timeScrl->width());

    ui->seisFrame->setLayout(seisLayout);
    ui->seisFrame->setMouseTracking(true);

    GetRegInfo();

    setWindowTitle(QString("SegDSeeMp (") + QString(__DATE__) + QString(")"));

    double v[3] = {-1, 0, 1};
    QRgb c[3];

    c[0] = QColor(0, 0, 255).rgb();
    c[1] = QColor(255, 255, 255).rgb();
    c[2] = QColor(255, 0, 0).rgb();

    pal = CreatePalette24(c, v, -1, 1, 3);

    Clear();
}

void MainWindow::edWhellEvent(int tag, QWheelEvent *event)
{
    double xs, ys;
    int xo, yo;
    double d = event->delta();
    double z;

    if (tag == 1) {
        z = 1 + 0.01 * fabs(d);
        if (d < 0)
            z = 1 / z;
        double g = sctList.Gw() * z;
        sctList.setGc(g);
        sctList.setGw(g);
        FillEdits();
    } else if (tag == 2) {
        GetZoom(xs, ys, xo, yo);
        z = 1 + 0.002 * fabs(d);
        if (d < 0)
            z = 1 / z;
        ys = ys * z;
        SetZoomC(xs, ys, 0, 0);
    } else if (tag == 3) {
        GetZoom(xs, ys, xo, yo);
        z = 1 + 0.002 * fabs(d);
        if (d < 0)
            z = 1 / z;
        xs = xs * z;
        SetZoomC(xs, ys, 0, 0);
    }
}

int MainWindow::Filter(float *inps, float f1, float f2, float f3, float f4,
                       double si, int ns)
{
    if (Fi.ok < 1 || Fi.dt != si || Fi.nt != ns || Fi.npoly != 4 ||
        Fi.f[0] != f1 || Fi.f[1] != f2 || Fi.f[2] != f3 || Fi.f[3] != f4) {
        //      float fa[4];
        float ff[4];

        ff[0] = f1;
        ff[1] = f2;
        ff[2] = f3;
        ff[3] = f4;

        Fi.Init(ns, si, 4, ff, 0, NULL);
    }

    if (Fi.ok > 0)
        Fi.Apply(inps);

    return Fi.ok;
}

void MainWindow::sendSmp(int ncs, int nch, float *smp, char &rc)
{
    int n;
    float maxa;

    rc = 0;

    if (ncs < 0 || ncs >= segd->get_ncs())
        return; // 0;

    TCs *cs = segd->get_csi(ncs);

    if (nch < 0 || nch >= cs->nch)
        return; // 0;

    TTr *tr = cs->get_trc()->get_tr(nch);
    int ns = cs->get_trc()->get_tr(nch)->ns; ///!!! Can be different ns ????

    tr->get_smps(smp);

    double si = cs->si / 1000; // segd->Si/1000.;

    if (ui->ckFilt->isChecked())
        Filter(smp, _f1, _f2, _f3, _f4, si, ns);
    //      BandPassFilter(smp, smp,  _f1, _f2, _f3, _f4, si, ns);

    if (ui->ckAgc->isChecked()) {
        try {
            n = round(_agcw * 1e-3 / si);
            if (n > ns)
                n = ns;
        } catch (...) {
            n = -1;
        }

        if (n > 0) {
            maxa = MaxAmp(smp, ns);
            Normalize(smp, maxa, ns);
            Agc(smp, n, ns);
            maxa = MaxAmp(smp, ns);
            Normalize(smp, maxa, ns);
        }
    } else if (ui->ckNorm->isChecked()) {
        maxa = MaxAmp(smp, ns);
        Normalize(smp, maxa, ns);
    }

    rc = 1;
}

MainWindow::~MainWindow()
{
    StopThreads();

    delete segd;
    delete ui;
}

void MainWindow::SetZoomC(double xs, double ys, int xc, int yc)
{
    double xsp, ysp;
    int xo, yo;

    GetZoom(xsp, ysp, xo, yo);

    int x = xc + xo;
    int y = yc + yo;

    double xa = x / xsp;
    double ya = y / ysp;

    x = xa * xs;
    y = ya * ys;

    xo = x - xc;
    yo = y - yc;

    SetZoom(xs, ys, xo, yo);
}

void MainWindow::sclZoomV(double, double zy, int xc, int yc)
{
    sclZoom(1, zy, xc, yc);
}

void MainWindow::sclZoomH(double zx, double, int xc, int yc)
{
    sclZoom(zx, 1, xc, yc);
}

void MainWindow::winZoom(QRect r)
{
    int x1 = r.left();
    int y1 = r.top();

    double xsp, ysp;
    int xo, yo;

    GetZoom(xsp, ysp, xo, yo);

    int x = x1 + xo;
    int y = y1 + yo;

    double xa = x / xsp;
    double ya = y / ysp;

    double ws = seisScrl->width();
    double hs = seisScrl->height();

    double wr = r.width();
    double hr = r.height();

    double xz = ws / wr;
    double yz = hs / hr;

    double xs = xsp * xz;
    double ys = ysp * yz;

    x = xa * xs;
    y = ya * ys;

    xo = x;
    yo = y;

    SetZoom(xs, ys, xo, yo);
}

void MainWindow::sclZoom(double zx, double zy, int xc, int yc)
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    if (zx == 0 && zy == 0) {
        if (xc < 0)
            xc = xo;
        if (yc < 0)
            yc = yo;
        SetZoom(xs, ys, xc, yc);
    } else {
        SetZoomC(xs * zx, ys * zy, xc, yc);
    }
}

void MainWindow::ChangeDataDir(QString Dname, QString Find)
{
    StopThreads();

    if (curDir == "") {
        QList<int> sz;
        sz << dirw << ui->splitter->width();
        ui->splitter->setSizes(sz);

        ui->splitter->setStretchFactor(0, 0);
        ui->splitter->setStretchFactor(1, 1);
    }

    QDir dir(Dname);

    if (dir.count() < 1) {
        m_statMess->setStyleSheet(
            "QLabel { background-color : red; color : black; }");
        m_statMess->setText("Access Denied");
        return;
    }

    curDir = Dname;

    find = Find;

    ShowProgress("Scanning Directry ...", 0);

    DirScanThread *DirScaner = new DirScanThread();

    WorkThread = DirScaner;

    DirScaner->Read(Dname, this);
}

void MainWindow::StopThreads()
{
    if (WorkThread) {
        if (WorkThread->isRunning()) {
            WorkThread->Stop();
            WorkThread->wait();
        }
        delete WorkThread;
        WorkThread = NULL;
    }

    if (ChkfThread) {
        if (ChkfThread->isRunning()) {
            ChkfThread->Stop();
            ChkfThread->wait();
        }
        delete ChkfThread;
        ChkfThread = NULL;
    }
}

void MainWindow::ScanLtod()
{
    StopThreads();

    ShowProgress("Scanning LTOD tape image ...", 0);

    LtdScanThread *LtdScaner = new LtdScanThread();

    ChkfThread = LtdScaner;

    LtdScaner->Read(curFile, this);
}

void MainWindow::CheckFiles(QString Dname, QList<DirItem> Dlist)
{
    if (ChkfThread) {
        if (ChkfThread->isRunning()) {
            ChkfThread->Stop();
            ChkfThread->wait();
        }
        delete ChkfThread;
    }

    ShowProgress("Checking file types ...", 0);

    FileCheckThread *FileScaner = new FileCheckThread();

    ChkfThread = FileScaner;

    FileScaner->Read(Dname, Dlist, this);
}

void MainWindow::FillDirGrid(QList<DirItem> list, int mode)
{
    // mode 0 - dir before file type checking
    //        - ltod
    //
    int ni, nr;

    dlist = list;

    ni = dlist.count();

    int nrp = 0;
    int nrc = dirGrid.curRow();

    if (mode == -11)
        nrp = dirGrid.RowCount();

    dirGrid.setRowCount(ni);

    for (nr = nrp; nr < ni; nr++) {
        DirItem di = dlist[nr];

        FillDirRow(nr, di);
    }

    if (mode) {
        if (nrc >= 0) {
            busy = true;
            dirGrid.setCurCell(nrc);
            busy = false;
        }
        if (mode == 1)
            dirGrid.setCurCell(0);

        return;
    }

    int nf = 0;

    if (find != "") {
        for (nr = 0; nr < ni; nr++) {
            QString name = dirGrid.Cell(nr, 0);
            if (name == find) {
                nf = nr;
                break;
            }
        }
    }

    dirGrid.setCurCell(0);
    dirGrid.setCurCell(nf);

    if (mode == 0)
        CheckFiles(curDir, list);
}

int MainWindow::tabcsi(int nr)
{
    int n = chsGrid.Tag(nr, 1);
    if (n < 0)
        return n;
    TCs *cs = segd->get_csi(n);
    int csi = cs->csn;

    return csi;
}

void MainWindow::SaveCsSel(bool clear)
{
    if (clear)
        chsSel.fill(false);

    int nr;

    for (nr = 0; nr < chsGrid.RowCount(); nr++) {
        int csi = tabcsi(nr);
        chsSel[csi] = chsGrid.Check(nr, 0);
    }
}

void MainWindow::viewMouseEvent(QMouseEvent *event)
{
    if (event->type() == QEvent::MouseMove) {
        int y = event->y() - seisView.y();
        int x = event->x() - seisView.x();

        if (sctList.count() > 0) {
            double t = round(sctList[0]->pix2y(y) * 1000);
            double p = round(sctList[0]->pix2x(x));
            m_statTime->setText(Tprintf("T=%g", t));
            //          qDebug() << "x=" << x;
        }
    } else if (event->type() == QEvent::MouseButtonPress &&
               event->button() == Qt::LeftButton) {
        int x = event->x() - seisView.x();
        ;

        int xa = 0;
        int xb;
        int n;

        for (n = 0; n < sctList.count(); n++) {
            GfxObjSeisSect *Sct = sctList[n];
            GfxSrcSeisMemSegd *Src = (GfxSrcSeisMemSegd *)srcList[n];
            xb = xa + Sct->WidthView();

            if (x >= xa && x <= xb) {
                int it = round(Sct->pix2x(x));

                if (it >= 0 && it < Src->Nt()) {
                    int chn = Src->Th(it, 1);
                    int csn = Src->Th(it, 0);

                    m_statChSet->setText(Tprintf("Ch.Set=%d", csn));
                    m_statChan->setText(Tprintf("Chan=%d", chn));

                    Sct->setSLcolor(QColor(0, 0, 255).rgb());
                    Sct->setSelTr(it);
                }
            } else {
                Sct->setSelTr(-1);
            }

            xa = xb + 1;
        }
    }
}

void MainWindow::chsGridEvent(int row, int mode)
{
    if (ui->ckAll->isChecked()) {
        chsGrid.setCheck(row, 0, 2);
        return;
    }

    if (ui->ckSng->isChecked()) {
        chsGrid.setColCheck(0, 0);
        chsGrid.setCheck(row, 0, 2);
        SaveCsSel(true);
    } else if (mode != 4) {
        return;
    } else {
        SaveCsSel();
    }

    SelCsets();
}

void MainWindow::dirGridEvent(int row, int mode)
{
    if (busy)
        return;

    QString name = dirGrid.Cell(row, 0);
    int tag = dirGrid.Tag(row, 0);
    int frm = dirGrid.Tag(row, 1);

    if (mode == 1 && tag == -1) {
        DirItem di = FileTypeCheck(curDir, name);
        FillDirRow(row, di);
        tag = dirGrid.Tag(row, 0);
        frm = dirGrid.Tag(row, 1);
    }

    m_statChSet->setText("Ch.Set=");
    m_statChan->setText("Chan=");

    m_statMess->setStyleSheet("");
    m_statMess->setText("");

    if (mode == 2) {
        m_statFfid->setText("FFID=");
        m_statSp->setText("SP=");

        curFn = "";

        setWindowTitle("");
        if (tag == 0) {
            if (name == "..") {
                QDir dir(curDir);

                QString name = dir.dirName();

                dir.cdUp();

                QString dirname = dir.absolutePath();

                ChangeDataDir(dirname, name);
            } else if (name == ".") {
                QDir dir(curDir);

                QString name = QFileInfo(curFile).fileName();

                QString dirname = dir.absolutePath();

                ChangeDataDir(dirname, name);
            } else {
                ChangeDataDir(curDir + "/" + name);
            }
        } else if (tag == 2) {
            curFile = curDir + "/" + name;
            ScanLtod();
        } else if (tag == 4) {
            QUrl url = QUrl::fromLocalFile(curDir + "/" + name);
            QDesktopServices::openUrl(url);
        }
    } else if (mode == 1 && tag == 1) {
        setWindowTitle("");
        curFile = curDir + "/" + name;
        ReadSegdFile();
    } else if (mode == 1 && tag == 3 && frm > 0) {
        setWindowTitle("");
        ReadSegdInLtod(row);
    } else if (mode == 1) {
        m_statFfid->setText("FFID=");
        m_statSp->setText("SP=");

        curFn = "";

        setWindowTitle(QString("Dir: ") + curDir);
        Clear();
    }
}

#pragma GCC diagnostic ignored "-Wunused-parameter"

void MainWindow::hdrGridEvent(int row, int mode)
{
    curHdr = hdrGrid.Cell(row, 0);
    int tag = hdrGrid.Tag(row, 0);

    TSegDObj *obj = segd->get_obj(tag);

    ui->logText->setPlainText(obj->get_inf().c_str());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    SaveRegInfo();
}

#pragma GCC diagnostic warning "-Wunused-parameter"

void MainWindow::ShowProgress(QString mes, int pers)
{
    if (pers >= 0) {
        progressBar->setValue(pers);
        m_statMess->setStyleSheet(
            "QLabel { background-color : lime; color : black; }");
        m_statMess->setText(mes);
    } else if (pers == -1) {
        progressBar->setValue(0);
        m_statMess->setStyleSheet("");
        m_statMess->setText("");
    }
}

int MainWindow::ReadSegdFile()
{
    if (WorkThread) {
        if (WorkThread->isRunning()) {
            WorkThread->Stop();
            WorkThread->wait();
        }
        delete WorkThread;
    }

    segd->Clear();

    SgdReadThread *SgdReader = new SgdReadThread();

    WorkThread = SgdReader;

    SgdReader->Read(curFile, (QObject *)this);
    ShowProgress("", -1);

    curFn = "";

    return 1;
}

int MainWindow::ReadSegdInLtod(int row)
{
    if (WorkThread) {
        if (WorkThread->isRunning()) {
            WorkThread->Stop();
            WorkThread->wait();
        }
        delete WorkThread;
    }

    segd->Clear();

    DirItem di = dlist[row];

    long long pos = di.lpos;
    long long len = di.llen;
    curFn = di.fname;

    LtdReadThread *LtdReader = new LtdReadThread();

    WorkThread = LtdReader;

    LtdReader->Read(curFile, pos, len, (QObject *)this);
    ShowProgress("", -1);

    return 1;
}

void MainWindow::Clear()
{
    segd->Clear();
    chsGrid.setRowCount(0);

    ui->seisFrame->setVisible(false);
    ui->logText->clear();
    hdrGrid.setRowCount(0);
}

void MainWindow::FillChsGrid()
{
    TCs *cs;
    int csi;
    int n;
    int nr = 0;

    for (n = 0; n < segd->get_ncs(); n++) {
        cs = segd->get_csi(n);
        if (cs->nch > 0)
            nr++;
    }

    chsGrid.setRowCount(nr);

    for (nr = 0, n = 0; n < segd->get_ncs(); n++) {
        cs = segd->get_csi(n);
        csi = cs->csn;

        if (cs->nch > 0) {
            chsGrid.setCell(nr, 1, Tprintf("%02d-%s", cs->csn, cs->typnm));
            chsGrid.setTag(nr, 1, n);

            int chk;

            if (chsSel[csi])
                chk = 2;
            else
                chk = 0;

            if (ui->ckAll->isChecked())
                chk = 2;

            chsGrid.setCheck(nr, 0, chk);

            nr++;
        }
    }

    SelCsets();
}

void MainWindow::NewFileReady(TSegD *newsegd)
{
    int n;
    int i;
    int ngh = 0;

    delete segd;

    segd = newsegd;

    n = segd->get_nobj();
    ngh = segd->get_nobj(0);

    n = n - ngh + 1;

    if (hdrGrid.RowCount() != n) {
        hdrGrid.setRowCount(n);
    }

    for (ngh = n = i = 0; n < segd->get_nobj(); n++) {
        TSegDObj *obj = segd->get_obj(n);

        if (obj->type == 0) {
            if (ngh == 0) {
                TGenHedAll *agh = segd->agh;
                hdrGrid.setCell(i, 0, agh->name.c_str());
                hdrGrid.setTag(i, 0, -1);
                i++;
            }
            ngh++;
        } else {
            hdrGrid.setCell(i, 0, obj->name.c_str());
            hdrGrid.setTag(i, 0, n);
            i++;
        }
    }

    if (curHdr != "") {
        for (n = 0; n < hdrGrid.RowCount(); n++) {
            QString name = hdrGrid.Cell(n, 0);
            if (name == curHdr) {
                hdrGridEvent(n, 0);
                hdrGrid.setCurCell(n);
            }
        }
    }

    FillChsGrid();

    if (curFn == "") {
        setWindowTitle(QString("File: ") + curFile);
        m_statFfid->setText(Tprintf("FFID=%d", segd->F));
        m_statSp->setText(Tprintf("SP=%d", segd->SP));
    } else {
        setWindowTitle(QString("File: ") + curFile + " : " + curFn);
        m_statFfid->setText(Tprintf("FFID=%d", segd->F));
        m_statSp->setText(Tprintf("SP=%d", segd->SP));
    }

    return;
}

int MainWindow::Parse(byte *buf, int len)
{
    int n;

    segd->Parse(buf, len, false);

    n = segd->get_nobj();

    if (hdrGrid.RowCount() != n) {
        hdrGrid.setRowCount(n);
    }

    for (n = 0; n < segd->get_nobj(); n++) {
        TSegDObj *obj = segd->get_obj(n);
        hdrGrid.setCell(n, 0, obj->name.c_str());
        hdrGrid.setTag(n, 0, n);
    }

    if (curHdr != "") {
        for (n = 0; n < hdrGrid.RowCount(); n++) {
            QString name = hdrGrid.Cell(n, 0);
            if (name == curHdr) {
                hdrGridEvent(n, 0);
                hdrGrid.setCurCell(n);
            }
        }
    }

    FillChsGrid();

    return 0;
}

void MainWindow::SelCsets()
{

    int nt;
    int ns;
    int it;
    int r;

    double taa = 0, ta = 0;
    double tbb = 0, tb = 0;

    TCs *cs;
    int ics;

    srcList.clear();
    sctList.clear();
    haxList.clear();

    nt = 0;
    ns = 0;

    for (r = 0; r < chsGrid.RowCount(); r++) {
        if (!chsGrid.Check(r, 0))
            continue;

        ics = chsGrid.Tag(r, 1);
        cs = segd->get_csi(ics);

        if (cs == NULL)
            continue;

        if (cs->nch && cs->get_ntr()) {
            TTr *tr = cs->get_trc()->get_tr(0);

            if (tr->ns > ns)
                ns = tr->ns;

            ta = cs->mto / 1000.;
            tb = cs->mtn / 1000.;

            if (nt == 0) {
                taa = ta;
                tbb = tb;
                nt++;
            } else {
                if (ta < taa)
                    taa = ta;
                if (tb > tbb)
                    tbb = tb;
            }
        }
    }

    nt = 0;

    for (r = 0; r < chsGrid.RowCount(); r++) {
        if (!chsGrid.Check(r, 0))
            continue;

        ics = chsGrid.Tag(r, 1);
        cs = segd->get_csi(ics);

        if (cs == NULL)
            continue;

        if (cs->nch && cs->get_ntr()) {
            TTr *tr = cs->get_trc()->get_tr(0);

            if (tr->ns > ns)
                ns = tr->ns;

            /*
                        ta = cs->to*2/1000.;
                        tb = cs->tn*2/1000.;
            */
            ta = cs->mto / 1000.;
            tb = cs->mtn / 1000.;

            GfxSrcSeisMemSegd *Src = new GfxSrcSeisMemSegd();
            srcList.append(Src);
            GfxObjSeisSect *Sct = new GfxObjSeisSect();
            sctList.append(Sct);
            GfxObjSeisHdrs *Hax = new GfxObjSeisHdrs();
            haxList.append(Hax);

            connect(Src, SIGNAL(smpRequest(int, int, float *, char &)), this,
                    SLOT(sendSmp(int, int, float *, char &)));

            Src->setTo(ta);
            //          Src->setNh(2);
            Src->setHdrList(hlist);

            // Sct->setLayer(2);
            Sct->setSrc(Src);
            Sct->setView(&seisView);
            Sct->setXs(1);
            Sct->setYs(10);

            // Hax->setLayer=2;
            Hax->setXo(0);
            Hax->setYo(0);

            Hax->setSrc(Src);
            Hax->setView(&hdrsView);
            Hax->setXs(1);
            Hax->setHdrList(hlist);

            hdrsLab.setHdrList(hlist);

            Src->setNt(cs->get_ntr());
            Src->setNs(tr->ns);
            Src->setSi(segd->Si / (cs->nsscan) / 1000.);
            Src->setTi(1);

            Sct->setX1(-1);
            Sct->setX2(cs->get_ntr());
            Sct->setY1(taa);
            Sct->setY2(tbb);

            Sct->setTrim(true);

            Hax->setX1(-1);
            Hax->setX2(cs->get_ntr());
            Hax->setYs(1);
            Hax->setY1(0);
            Hax->setY2(12);

            Hax->setTrim(true);

            TTrList *tlist = cs->get_trc();

            for (it = 0; it < tlist->getCount(); it++) {
                TTr *tr = cs->get_trc()->get_tr(it);

                if (tr == NULL)
                    continue;

                if (tr->get_err() != "")
                    break;

                Src->Set(it, ics, it);

                Src->setTh(it, 1, tr->tn);
                Src->setTh(it, 0, cs->csn);
            }

            nt += cs->nch;
        }
    }

    if (nt < 1) {
        return;
    }

    timeAxis.setY1(taa);
    timeAxis.setY2(tbb);

    sctList.setDispCol(ui->ckGray->isChecked() || ui->ckColor->isChecked());
    sctList.setDispWig(ui->ckWiggle->isChecked());

    if (ui->rbNon->isChecked())
        sctList.setWFill(0);
    if (ui->rbNeg->isChecked())
        sctList.setWFill(-1);
    if (ui->rbPos->isChecked())
        sctList.setWFill(1);

    if (ui->ckColor->isChecked())
        sctList.setPal(pal);
    else
        sctList.setPal();

    sctList.setWFcolor(Qt::black);
    sctList.setWLcolor(Qt::black);

    ArrangeSections();

    _smax = 0;

    for (nt = 0, r = 0; r < chsGrid.RowCount(); r++) {
        if (!chsGrid.Check(r, 0))
            continue;

        ics = chsGrid.Tag(r, 1);
        cs = segd->get_csi(ics);

        TTrList *tlist = cs->get_trc();

        for (it = 0; it < tlist->getCount(); it++, nt++) {
            TTr *tr = cs->get_trc()->get_tr(it);

            if (tr == NULL)
                continue;

            if (tr->get_err() != "")
                break;
        }
    }

    cs_changed = false;
}

double MainWindow::allSectWidth()
{
    int n;
    double w = 0;

    for (n = 0; n < sctList.count(); n++) {
        GfxObjSeisSect *Sct = sctList[n];
        w += Sct->WidthPix();
    }

    return w;
}

void MainWindow::ArrangeSections()
{
    ui->seisFrame->setVisible(true);

    int hh = timeAxis.HeightView();

    if (hh == 0)
        hh = 1;

    hlabView.setFixedHeight(hdrsScrl->height());
    hlabView.setFixedWidth(timeScrl->width());

    seisView.setFixedHeight(hh);
    timeView.setFixedHeight(hh + 100);

    int n;
    double x = 0;
    double w = 0;

    for (n = 0; n < sctList.count(); n++) {
        GfxObjSeisSect *Sct = sctList[n];
        GfxObjSeisHdrs *Hax = (GfxObjSeisHdrs *)haxList[n];
        Sct->setXo(x);
        Hax->setXo(x);
        x += (Sct->X2() - Sct->X1());
        w += Sct->WidthView(); //+1;
    }

    double wa = allSectWidth() * seisView.Xs();

    //  qDebug() << "wa" << wa << "wo" << w;

    seisView.setFixedWidth(wa);
    hdrsView.setFixedWidth(wa);
}

void MainWindow::ResetSrc()
{
    int n;

    for (n = 0; n < srcList.count(); n++) {
        GfxSrcSeisMemSegd *Src = (GfxSrcSeisMemSegd *)(srcList[n]);
        Src->Reset();
    }
}

void MainWindow::on_ckAgc_toggled(bool checked)
{
    if (checked && ui->ckNorm->isChecked())
        ui->ckNorm->setChecked(false);

    ResetSrc();
}

void MainWindow::on_ckNorm_toggled(bool checked)
{
    if (checked && ui->ckAgc->isChecked())
        ui->ckAgc->setChecked(false);

    ResetSrc();
}

void MainWindow::on_ckFilt_toggled(bool)
{
    ResetSrc();
}

void MainWindow::on_ckWiggle_toggled(bool checked)
{
    sctList.setDispWig(checked);
}

void MainWindow::on_ckGray_toggled(bool checked)
{
    if (checked && ui->ckColor->isChecked())
        ui->ckColor->setChecked(false);

    sctList.setDispCol(checked);

    if (ui->ckColor->isChecked())
        sctList.setPal(pal);
    else
        sctList.setPal();
}

void MainWindow::on_ckColor_toggled(bool checked)
{
    if (checked && ui->ckGray->isChecked())
        ui->ckGray->setChecked(false);

    sctList.setDispCol(checked);

    if (ui->ckColor->isChecked())
        sctList.setPal(pal);
    else
        sctList.setPal();
}

void MainWindow::on_rbNon_toggled(bool checked)
{
    if (checked)
        sctList.setWFill(0);
}

void MainWindow::on_rbPos_toggled(bool checked)
{
    if (checked)
        sctList.setWFill(1);
}

void MainWindow::on_rbNeg_toggled(bool checked)
{
    if (checked)
        sctList.setWFill(-1);
}

void MainWindow::on_btnCkNon_pressed()
{
    if (ui->ckSng->isChecked())
        return;
    if (ui->ckAll->isChecked())
        return;

    chsGrid.setColCheck(0, 0);
    SaveCsSel(true);
    SelCsets();

    FillChsGrid();
}

void MainWindow::on_btnCkAll_pressed()
{
    if (ui->ckSng->isChecked())
        return;
    if (ui->ckAll->isChecked())
        return;

    chsGrid.setColCheck(0, 2);

    SaveCsSel();
    SelCsets();

    FillChsGrid();
}

void MainWindow::FillEdits()
{
    ui->edTr->setText(Tprintf("%g", seisView.Xs()));
    ui->edTm->setText(Tprintf("%g", seisView.Ys()));
    ui->edGn->setText(Tprintf("%g", sctList.Gw()));
}

void MainWindow::FillControls()
{
    ui->ckWiggle->setChecked(sctList.DispWig());

    if (sctList.DispCol()) {
        if (sctList.Pal()->count() > 0) {
            ui->ckGray->setChecked(false);
            ui->ckColor->setChecked(true);
        } else {
            ui->ckGray->setChecked(true);
            ui->ckColor->setChecked(false);
        }
    } else {
        ui->ckGray->setChecked(false);
        ui->ckColor->setChecked(false);
    }

    int v = sctList.WFill();

    ui->rbNeg->setChecked((v == -1));
    ui->rbNon->setChecked((v == 0));
    ui->rbPos->setChecked((v == 1));
}

void MainWindow::GetRegInfo()
{
    QSettings settings("WW", "SegDSeeMp");

    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

    dirw = settings.value("DirWidth", 250).toInt();
    if (dirw < 100)
        dirw = 100;

    int color_mode = settings.value("DispCol", 1).toInt();
    bool wigle_mode = settings.value("DispWig", false).toBool();

    if (color_mode) {
        sctList.setDispCol(true);
        if (color_mode == 1)
            sctList.setPal();
        else
            sctList.setPal(pal);
    } else {
        if (!wigle_mode) {
            sctList.setDispCol(true);
            sctList.setPal();
        } else {
            sctList.setDispCol(false);
        }
    }

    sctList.setDispWig(wigle_mode);

    sctList.setWFill(settings.value("WFill", -1).toInt());

    sctList.setGc(settings.value("Gc", 1).toDouble());
    sctList.setGw(settings.value("Gw", 1).toDouble());

    ui->ckFilt->setChecked(settings.value("Filt", false).toBool());
    ui->ckAgc->setChecked(settings.value("Agc", false).toBool());
    ui->ckNorm->setChecked(settings.value("Norm", true).toBool());

    if (ui->ckNorm->isChecked() && ui->ckAgc->isChecked())
        ui->ckAgc->setChecked(false);

    _f1 = settings.value("F1", 10).toInt();
    _f2 = settings.value("F2", 20).toInt();
    _f3 = settings.value("F3", 500).toInt();
    _f4 = settings.value("F4", 1000).toInt();

    _agcw = settings.value("AgcW", 1000).toInt();

    ui->ckAll->setChecked(settings.value("CkAll", false).toBool());
    ui->ckSng->setChecked(settings.value("CkSng", false).toBool());

    if (ui->ckAll->isChecked() && ui->ckSng->isChecked())
        ui->ckSng->setChecked(false);

    ui->SeisTab->setCurrentIndex(settings.value("SeisTab", 1).toInt());

    double xs = settings.value("Xs", 1.0).toDouble();
    double ys = settings.value("Ys", 1.0).toDouble();

    seisView.setXs(xs);
    seisView.setYs(ys);
    hdrsView.setXs(xs);
    timeView.setYs(ys);

    FillEdits();
    FillControls();

    int i, n;

    int size = settings.beginReadArray("ChsSel");
    for (n = 0; n < size; n++) {
        settings.setArrayIndex(n);
        i = settings.value("Cs#").toInt();
        chsSel[i] = true;
    }
    settings.endArray();

    QString dir = settings.value("DataDir").toString();
    ChangeDataDir(dir);
}

void MainWindow::SaveRegInfo()
{
    QSettings settings("WW", "SegDSeeMp");
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());

    settings.setValue("DataDir", curDir);

    if (sctList.DispCol()) {
        int nc = sctList.Pal()->count();
        if (nc)
            settings.setValue("DispCol", 2);
        else
            settings.setValue("DispCol", 1);
    } else {
        settings.setValue("DispCol", 0);
    }

    settings.setValue("DispWig", sctList.DispWig());

    settings.setValue("WFill", sctList.WFill());

    settings.setValue("Gc", sctList.Gc());
    settings.setValue("Gw", sctList.Gw());

    settings.setValue("Filt", ui->ckFilt->isChecked());
    settings.setValue("Agc", ui->ckAgc->isChecked());
    settings.setValue("Norm", ui->ckNorm->isChecked());

    settings.setValue("F1", _f1);
    settings.setValue("F2", _f2);
    settings.setValue("F3", _f3);
    settings.setValue("F4", _f4);

    settings.setValue("AgcW", _agcw);

    settings.setValue("Xs", seisView.Xs());
    settings.setValue("Ys", seisView.Ys());

    settings.setValue("CkAll", ui->ckAll->isChecked());
    settings.setValue("CkSng", ui->ckSng->isChecked());

    settings.setValue("SeisTab", ui->SeisTab->currentIndex());

    int i, n;

    settings.beginWriteArray("ChsSel");
    for (i = n = 0; i < chsSel.count(); ++i) {
        if (chsSel[i]) {
            settings.setArrayIndex(n);
            settings.setValue("Cs#", i);
            n++;
        }
    }
    settings.endArray();

    int dirw = ui->dirGroup->width();
    settings.setValue("DirWidth", dirw);
}

void MainWindow::on_ckAll_toggled(bool checked)
{
    if (checked)
        ui->ckSng->setChecked(false);

    FillChsGrid();
}

void MainWindow::on_ckSng_toggled(bool checked)
{
    if (checked)
        ui->ckAll->setChecked(false);
    SelCsets();
}

void MainWindow::on_zoomAllBtn_pressed()
{
    double ws = seisScrl->width() - 2;
    double wv = allSectWidth();
    double hs = seisScrl->height() - 2;
    double hv = timeAxis.HeightPix();

    if (wv == 0)
        return;
    if (hv == 0)
        return;

    double xs = ws / wv;
    double ys = hs / hv;

    SetZoom(xs, ys, 0, 0);
}

void MainWindow::on_zoomVallBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    double hs = seisScrl->viewport()->height() - 2;
    double hv = timeAxis.HeightPix();

    if (hv == 0)
        return;

    ys = hs / hv;

    SetZoom(xs, ys, xo, 0);
}

void MainWindow::on_zoomHallBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    double ws = seisScrl->viewport()->width(); // - 2;
    double wv = allSectWidth();

    if (wv == 0)
        return;

    xs = ws / wv;

    SetZoom(xs, ys, 0, yo);
}

void MainWindow::GetZoom(double &xs, double &ys, int &xo, int &yo)
{
    xs = seisView.Xs();
    ys = seisView.Ys();

    xo = seisScrl->Hscroll(); // horizontalScrollBar()->value();
    yo = seisScrl->Vscroll(); // verticalScrollBar  ()->value();
}

void MainWindow::SetZoom(double xs, double ys, int xo, int yo, bool push)
{
    double xsp, ysp;
    int xop, yop;

    GetZoom(xsp, ysp, xop, yop);

    if (xs == 0) {
        xs = xsp;
        xo = xop;
    }

    if (ys == 0) {
        ys = ysp;
        yo = yop;
    }

    if (push)
        zstack.push(zdata(xsp, ysp, xop, yop));

    seisView.setXs(xs);
    seisView.setYs(ys);
    hdrsView.setXs(xs);
    timeView.setYs(ys);

    seisScrl->setHscroll(xo);
    seisScrl->setVscroll(yo);
    hdrsScrl->setHscroll(xo);
    timeScrl->setVscroll(yo);

    ArrangeSections();

    FillEdits();
}

void MainWindow::on_edTr_editingFinished()
{
    double xs, ys;
    int xo, yo;
    bool ok;

    GetZoom(xs, ys, xo, yo);

    QString txt = ui->edTr->text();

    xs = txt.toFloat(&ok);
    if (!ok)
        return;

    SetZoomC(xs, ys, 0, 0);
}

void MainWindow::on_edTm_editingFinished()
{
    double xs, ys;
    int xo, yo;
    bool ok;

    GetZoom(xs, ys, xo, yo);

    QString txt = ui->edTm->text();

    ys = txt.toFloat(&ok);
    if (!ok)
        return;

    SetZoomC(xs, ys, 0, 0);
}

void MainWindow::on_edGn_editingFinished()
{
    bool ok;
    double g;

    g = ui->edGn->text().toFloat(&ok);
    if (!ok)
        return;

    sctList.setGc(g);
    sctList.setGw(g);

    FillEdits();
}

void MainWindow::on_zoomPreBtn_pressed()
{
    zdata z;

    if (zstack.isEmpty())
        return;

    z = zstack.pop();

    SetZoom(z.xs, z.ys, z.xo, z.yo, false);
}

void MainWindow::on_zoomOutBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    SetZoomC(xs / 1.5, ys / 1.5, 0, 0);
}

void MainWindow::on_zoomInBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    SetZoomC(xs * 1.5, ys * 1.5, 0, 0);
}

void MainWindow::on_selDirBtn_pressed()
{
    QFileDialog dirDialog(this, "Select Directory", curDir, "");
    dirDialog.setFileMode(QFileDialog::Directory);
    dirDialog.setOptions(QFileDialog::ShowDirsOnly);

    if (!dirDialog.exec())
        return;

    QDir d = dirDialog.directory();

    QString dir = d.absolutePath();
    ChangeDataDir(dir);
}

void MainWindow::on_refreshBtn_pressed()
{
    int row = dirGrid.curRow();

    if (row >= 0) {
        QString name = dirGrid.Cell(row, 0);
        int tag = dirGrid.Tag(row, 0);
        //      int       frm  = dirGrid.Tag (row,1);

        if (name == "." || tag == 3) {
            ScanLtod();
            find = name;
        } else {
            ChangeDataDir(curDir, name);
        }
    }
}

void MainWindow::on_procParmBtn_pressed()
{
    ProcParmDlg.f1 = _f1;
    ProcParmDlg.f2 = _f2;
    ProcParmDlg.f3 = _f3;
    ProcParmDlg.f4 = _f4;
    ProcParmDlg.agcw = _agcw;
    ProcParmDlg.agc = ui->ckAgc->isChecked();
    ProcParmDlg.norm = ui->ckNorm->isChecked();
    ProcParmDlg.filt = ui->ckFilt->isChecked();

    ProcParmDlg.show();
}

void MainWindow::procParmEvent()
{
    _f1 = ProcParmDlg.f1;
    _f2 = ProcParmDlg.f2;
    _f3 = ProcParmDlg.f3;
    _f4 = ProcParmDlg.f4;
    _agcw = ProcParmDlg.agcw;

    ui->ckAgc->setChecked(ProcParmDlg.agc);
    ui->ckNorm->setChecked(ProcParmDlg.norm);
    ui->ckFilt->setChecked(ProcParmDlg.filt);

    ResetSrc();
}

void MainWindow::FillDirRow(int nr, DirItem di)
{
    if (di.ftype == 0) // DIR
    {
        dirGrid.setCell(nr, 0, di.fname);
        dirGrid.setImgName(nr, 0, ":/images/Dir.png");
        dirGrid.setCell(nr, 1, "");
        dirGrid.setCell(nr, 2, "DIR");
        dirGrid.setTag(nr, 0, 0);
    } else if (di.ftype == -1) // File - Before type recognizing
    {
        dirGrid.setCell(nr, 0, di.fname);
        dirGrid.setImgName(nr, 0, ":/images/Wait1.png");
        dirGrid.setCell(nr, 2, "...");
        dirGrid.setTag(nr, 0, -1);
    } else if (di.ftype == 2) // LTOD
    {
        dirGrid.setCell(nr, 0, di.fname);
        dirGrid.setImgName(nr, 0, ":/images/Tape.png");
        dirGrid.setTag(nr, 0, 2);

        int type = di.type;

        if (type > 9999)
            type = type - 10000;

        dirGrid.setCell(nr, 1, "LTOD");

        if (type == 1)
            dirGrid.setCell(nr, 2, "SEG-Y");
        else if (type == 200)
            dirGrid.setCell(nr, 2, "SEG-B");
        else if (is_segd_format(type))
            dirGrid.setCell(nr, 2, Tprintf("%04d", type));
        else
            dirGrid.setCell(nr, 2, "???");

        dirGrid.setTag(nr, 1, type);
    } else if (di.ftype == 1) // SEG-D or SEG-B
    {
        int frmt = di.type;

        int ffid = di.ffid;

        QString fn = di.fname;

        dirGrid.setTag(nr, 0, 1);
        dirGrid.setTag(nr, 1, frmt);

        dirGrid.setCell(nr, 0, fn);

        if (frmt > 0) {
            dirGrid.setCell(nr, 1, Tprintf("%04d", ffid));
            dirGrid.setCell(nr, 2, Tprintf("%04d", frmt));
        } else {
            dirGrid.setCell(nr, 1, "");
            dirGrid.setCell(nr, 2, "???");
        }

        if (frmt == 200)
            dirGrid.setImgName(nr, 0, ":/images/Segb1.png");
        else if (frmt > 0)
            dirGrid.setImgName(nr, 0, ":/images/Segd1.png");
        else
            dirGrid.setImgName(nr, 0, ":/images/Unk1.png");
    } else if (di.ftype == 3) // File in LTOD
    {
        int frmt = di.type;

        int ffid = di.ffid;

        QString fn = di.fname;

        dirGrid.setTag(nr, 0, 3);
        dirGrid.setTag(nr, 1, frmt);

        dirGrid.setCell(nr, 0, fn);

        if (frmt > 0) {
            dirGrid.setCell(nr, 1, Tprintf("%04d", ffid));
            dirGrid.setCell(nr, 2, Tprintf("%04d", frmt));
        } else {
            dirGrid.setCell(nr, 1, "");
            dirGrid.setCell(nr, 2, "???");
        }

        if (frmt == 200)
            dirGrid.setImgName(nr, 0, ":/images/Segb2.png");
        else if (frmt > 0)
            dirGrid.setImgName(nr, 0, ":/images/Segd2.png");
        else
            dirGrid.setImgName(nr, 0, ":/images/Unk2.png");
    } else if (di.ftype == 4) // Unknown
    {
        QString fn = di.fname;

        dirGrid.setTag(nr, 0, 4);
        dirGrid.setTag(nr, 1, 0);

        dirGrid.setCell(nr, 0, fn);

        dirGrid.setCell(nr, 1, "");
        dirGrid.setCell(nr, 2, "???");

        dirGrid.setImgName(nr, 0, ":/images/Unk1.png");
    }
}

bool MainWindow::event(QEvent *evt)
{
    if (evt->type() == QEvent::User + 1) {
        SgdReadEvent *e = (SgdReadEvent *)evt;

        if (e->pers >= 0) {
            ShowProgress("Loading SEG-D...", e->pers);
        } else {
            if (e->segd)
                NewFileReady(e->segd);

            if (ChkfThread && ChkfThread->isRunning()) {
                ShowProgress(ChkfThread->sstr, 0);
            } else {
                ShowProgress("", -1);
            }
        }

        return true;
    } else if (evt->type() == QEvent::User + 2) {
        DirScanEvent *e = (DirScanEvent *)evt;

        if (e->pers >= 0) {
            ShowProgress("Scanning Directory...", e->pers);
        } else if (e->pers == -1) {
            FillDirGrid(WorkThread->dlist, 0);
            ShowProgress("", -1);
        } else if (e->pers == -2) {
            ShowProgress("", -1);
        }

        return true;
    } else if (evt->type() == QEvent::User + 3) {
        LtdScanEvent *e = (LtdScanEvent *)evt;

        if (e->pers >= 0) {
            ShowProgress("Scanning LTOD image...", e->pers);
        } else if (e->pers == -1) {
            FillDirGrid(ChkfThread->dlist, 3);
            ShowProgress("", -1);
        } else if (e->pers == -10) {
            FillDirGrid(ChkfThread->dlist, 1);
        } else if (e->pers == -11) {
            FillDirGrid(ChkfThread->dlist, 2);
        } else if (e->pers == -2) {
            ShowProgress("", -1);
        }

        return true;
    } else if (evt->type() == QEvent::User + 4) {
        FileCheckEvent *e = (FileCheckEvent *)evt;

        if (e->nrow >= 0) {
            int nr = e->nrow;
            DirItem di = e->di;

            if (di.ftype == 2) // LTOD
            {
                dirGrid.setCell(nr, 0, di.fname);
                dirGrid.setImgName(nr, 0, ":/images/Tape.png");
                dirGrid.setTag(nr, 0, 2);

                int type = di.type;

                if (type > 9999)
                    type = type - 10000;

                dirGrid.setCell(nr, 1, "LTOD");

                if (type == 1)
                    dirGrid.setCell(nr, 2, "SEG-Y");
                else if (type == 200)
                    dirGrid.setCell(nr, 2, "SEG-B");
                else if (is_segd_format(type))
                    dirGrid.setCell(nr, 2, Tprintf("%04d", type));
                else
                    dirGrid.setCell(nr, 2, "???");

                dirGrid.setTag(nr, 1, type);
            } else if (di.ftype == 1) //
            {
                int frmt = di.type;

                int ffid = di.ffid;

                QString fn = di.fname;

                dirGrid.setTag(nr, 0, 1);
                dirGrid.setTag(nr, 1, frmt);

                dirGrid.setCell(nr, 0, fn);

                if (frmt > 0) {
                    dirGrid.setCell(nr, 1, Tprintf("%04d", ffid));
                    dirGrid.setCell(nr, 2, Tprintf("%04d", frmt));
                } else {
                    dirGrid.setCell(nr, 1, "");
                    dirGrid.setCell(nr, 2, "???");
                }

                if (frmt == 200)
                    dirGrid.setImgName(nr, 0, ":/images/Segb1.png");
                else if (frmt > 0)
                    dirGrid.setImgName(nr, 0, ":/images/Segd1.png");
                else
                    dirGrid.setImgName(nr, 0, ":/images/Unk1.png");
            } else if (di.ftype == 4) // Unknown
            {
                QString fn = di.fname;

                dirGrid.setTag(nr, 0, 4);
                dirGrid.setTag(nr, 1, 0);

                dirGrid.setCell(nr, 0, fn);

                dirGrid.setCell(nr, 1, "");
                dirGrid.setCell(nr, 2, "???");

                QFileInfo fi(curDir + "/" + di.fname);
                QFileIconProvider iconprov;
                QIcon icon = iconprov.icon(fi);

                // dirGrid.setImgName(nr,0,":/images/Unk1.png");
                dirGrid.setImgIcon(nr, 0, icon);
            } else if (di.ftype == 5) // Missing (file disappeared)
            {
                QString fn = di.fname;

                dirGrid.setTag(nr, 0, 4);
                dirGrid.setTag(nr, 1, 0);

                dirGrid.setCell(nr, 0, fn);

                dirGrid.setCell(nr, 1, "");
                dirGrid.setCell(nr, 2, "Missing");

                dirGrid.setImgName(nr, 0, ":/images/Misf.png");
            } else if (di.ftype == 6) // Cannot open
            {
                QString fn = di.fname;

                dirGrid.setTag(nr, 0, 4);
                dirGrid.setTag(nr, 1, 0);

                dirGrid.setCell(nr, 0, fn);

                dirGrid.setCell(nr, 1, "");
                dirGrid.setCell(nr, 2, "Denied");

                dirGrid.setImgName(nr, 0, ":/images/Disf.png");
            }

            ShowProgress("Checking file types...", e->pers);
        } else if (e->nrow == -1) {
            // FillDirGrid(WorkThread->dlist);
            ShowProgress("", -1);
        } else if (e->nrow == -2) {
            ShowProgress("", -1);
        }

        return true;
    } else if (evt->type() == QEvent::Polish) {

        // QList<int> sizes = ui->splitter->sizes();
        // int        w     = ui->splitter->width();
        // sizes[0]=dirw;
        // sizes[1]=w-dirw;

        // ui->splitter->setStretchFactor(0,50 );
        // ui->splitter->setStretchFactor(1,50);

        // ui->splitter->setSizes(sizes);

        // ui->dirGroup->resize(dirw, ui->dirGroup->height());

        // QWidget* w = ui->splitter->widget(0);

        // w->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

        return true;
    }

    return QMainWindow::event(evt);
}

void MainWindow::on_zoomWinBtn_pressed()
{
    seisScrl->setZoomW(1);
}

void MainWindow::on_actionOpen_Directory_triggered()
{
    on_selDirBtn_pressed();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDlg.show();
}

void MainWindow::on_TrSlider_valueChanged(int p)
{
    double r;

    if (p > 0) {
        r = 1 + p / 100.;
        SetZoomC(xss * r, yss, 0, 0);
    } else {
        r = 1 - p / 100.;
        SetZoomC(xss / r, yss, 0, 0);
    }
}

void MainWindow::on_TrSlider_sliderReleased()
{
    int xo, yo;
    GetZoom(xss, yss, xo, yo);
    ui->TrSlider->setValue(0);
}

void MainWindow::on_TrSlider_sliderPressed()
{
    int xo, yo;

    GetZoom(xss, yss, xo, yo);
}

void MainWindow::on_TmSlider_sliderPressed()
{
    int xo, yo;

    GetZoom(xss, yss, xo, yo);
}

void MainWindow::on_TmSlider_sliderReleased()
{
    int xo, yo;
    GetZoom(xss, yss, xo, yo);
    ui->TmSlider->setValue(0);
}

void MainWindow::on_TmSlider_valueChanged(int p)
{
    double r;

    if (p > 0) {
        r = 1 + p / 100.;
        SetZoomC(xss, yss * r, 0, 0);
    } else {
        r = 1 - p / 100.;
        SetZoomC(xss, yss / r, 0, 0);
    }
}

void MainWindow::on_GnSlider_sliderPressed()
{
    gss = sctList.Gw();
}

void MainWindow::on_GnSlider_sliderReleased()
{
    gss = sctList.Gw();
    ui->GnSlider->setValue(0);
}

void MainWindow::on_GnSlider_valueChanged(int p)
{
    double r;

    if (p > 0) {
        r = 1 + p / 100.;
        sctList.setGc(gss * r);
        sctList.setGw(gss * r);
    } else {
        r = 1 - p / 100.;
        sctList.setGc(gss / r);
        sctList.setGw(gss / r);
    }
    FillEdits();
}

void MainWindow::on_aGainBtn_pressed()
{
    if (ui->ckNorm->isChecked() || ui->ckAgc->isChecked()) {
        sctList.setGc(2);
        sctList.setGw(2);
    } else {

        int n;

        for (n = 0; n < sctList.count(); n++) {
            GfxObjSeisSect *Sct = sctList[n];
            if (Sct->selTr() >= 0) {
                float a = Sct->getSelMaxAmp();
                if (a != 0) {
                    double g = 2 / a;
                    sctList.setGc(g);
                    sctList.setGw(g);
                }
                break;
            }
        }
    }

    FillEdits();
}

#pragma GCC diagnostic ignored "-Wunused-parameter"

void MainWindow::on_splitter_splitterMoved(int pos, int index)
{
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void MainWindow::on_hdrsTopBtn_clicked()
{
    hdrGrid.setCurCell(0);
}

void MainWindow::on_hdrsBtmBtn_clicked()
{
    int nr = hdrGrid.RowCount();
    hdrGrid.setCurCell(nr - 1);
}

void MainWindow::on_actionParameters_triggered()
{
    on_procParmBtn_pressed();
}

void MainWindow::on_actionUser_Manual_triggered()
{
    QString dir = QApplication::applicationDirPath();

    QFileInfo dirfi(dir);

    QString link = dirfi.absoluteFilePath() + "/html/SegDSeeMp.Eng.htm";

    QUrl url = QUrl::fromLocalFile(link);

    QDesktopServices::openUrl(url);
}

void MainWindow::on_actionUser_s_Manual_Russian_triggered()
{
    QString dir = QApplication::applicationDirPath();

    QFileInfo dirfi(dir);

    QString link = dirfi.absoluteFilePath() + "/html/SegDSeeMp.Rus.htm";

    QUrl url = QUrl::fromLocalFile(link);

    QDesktopServices::openUrl(url);
}
