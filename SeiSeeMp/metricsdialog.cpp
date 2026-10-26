#include "metricsdialog.h"
#include "ui_metricsdialog.h"
#include "workthread.h"

#include <QFileDialog>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QHeaderView>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QIntValidator>
#include <QSignalBlocker>
#include <QFileInfo>

MetricsDialog::MetricsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MetricsDialog)
{
    ui->setupUi(this);
    running = false;
    stopRequested = false;
    sf = nullptr;
    savDir = nullptr;
    sf2 = nullptr;
    setWindowModality(Qt::NonModal);
    setModal(false);
    setSizeGripEnabled(true);
    ui->bodyLayout->setStretch(0, 5);
    ui->bodyLayout->setStretch(1, 4);
    for (QLayout* layout : findChildren<QLayout*>()) layout->setSpacing(6);
    ui->verticalLayout_main->setSpacing(8);
    ui->pathFile1->setToolTip(tr("Signal file is fixed to the file captured from the main window."));
    ui->btnSaveResults->setEnabled(false);
    ui->txtResults->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ui->ckSNR->setToolTip(tr("Estimation: 10*log10(TPRS/(1-TPRS)); TPRS is computed automatically. Calculation: reference signal energy / error energy."));
    ui->txtResults->setPlaceholderText(tr("The full calculation report will appear here."));
    ui->metricTable->setColCount(3);
    ui->metricTable->setColLabel(0, tr("Metric"));
    ui->metricTable->setColLabel(1, tr("Value"));
    ui->metricTable->setColLabel(2, tr("Unit"));
    ui->metricTable->setColWidthInCharacters(0, 8);
    ui->metricTable->setColWidthToStretch(1);
    ui->metricTable->setColWidthInCharacters(2, 5);
    ui->metricTable->verticalHeader()->hide();
    ui->metricTable->verticalHeader()->setDefaultSectionSize(28);
    ui->metricTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->metricTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->metricTable->setAlternatingRowColors(true);
    ui->metricTable->setShowGrid(true);
    for (QLineEdit* edit : {ui->edTrMin, ui->edTrMax, ui->edTrStp})
        edit->setValidator(new QIntValidator(1, 2147483647, edit));
    connect(ui->pathFile1, &QLineEdit::editingFinished, this, [this]() {
        if (!running && !ui->rbCalculation->isChecked() && openNoisyFile()) FillLimits();
    });
    connect(ui->pathFile2, &QLineEdit::editingFinished, this, [this]() {
        if (!running && openNoisyFile()) FillLimits();
    });
    resetResults();
    updateAvailableMetrics(); // 初始化一次
}

void MetricsDialog::resetResults()
{
    ui->resultHint->setText(tr("Results will appear here after calculation. The Details tab contains the full report."));
    const QStringList names = QStringList() << "MSE" << "RMSE" << "PSNR" << "SNR" << "SSIM" << "TPRS";
    ui->metricTable->setRowCount(names.size());
    for (int row = 0; row < names.size(); ++row) {
        ui->metricTable->setCell(row, 0, names[row]);
        ui->metricTable->setCell(row, 1, QString::fromUtf8("\xe2\x80\x94"));
        ui->metricTable->setCell(row, 2, row == 2 || row == 3 ? "dB" : "");
    }
    ui->txtResults->clear();
    ui->btnSaveResults->setEnabled(false);
}

void MetricsDialog::setRunning(bool active)
{
    running = active;
    for (QWidget* group : {ui->groupBox_mode, ui->groupBox_files, ui->groupBox_trace,
                          ui->groupBox_time, ui->groupBox_metrics}) group->setEnabled(!active);
    ui->btnClose->setEnabled(!active);
    ui->btnCalculate->setEnabled(true);
    ui->btnCalculate->setText(active ? tr("Stop") :
        (ui->rbCalculation->isChecked() ? tr("Calculate") : tr("Estimate")));
    ui->btnCalculate->setIcon(QIcon(active ? ":/images/stop.png" : ":/images/Run.png"));
    ui->btnSaveResults->setEnabled(!active && !ui->txtResults->toPlainText().isEmpty());
}

void MetricsDialog::reject()
{
    if (!running) QDialog::reject();
}

void MetricsDialog::closeEvent(QCloseEvent* event)
{
    if (running) event->ignore();
    else QDialog::closeEvent(event);
}

void MetricsDialog::shutdown()
{
    if (calculationThread) {
        stopRequested = true;
        emit stop();
        calculationThread->quit();
        calculationThread->wait();
        calculationThread = nullptr;
    }
    setRunning(false);
    close();
}

MetricsDialog::~MetricsDialog()
{
    shutdown();
    delete signalFile;
    delete sf2;
    delete ui;
}

void MetricsDialog::show()
{
    if (isVisible() || running) { raise(); activateWindow(); return; }
    if (!sf || !sf->Active()) return;
    const QString path = QString::fromLocal8Bit(sf->Fname().c_str());
    SeisFile* selected = sf->typeIdx() == 2 ? static_cast<SeisFile*>(new SuFile(this))
        : sf->typeIdx() == 3 ? static_cast<SeisFile*>(new CstFile(this))
        : static_cast<SeisFile*>(new SgyFile(this));
    selected->setFname(path);
    selected->setActive(true);
    if (!selected->Active()) {
        delete selected;
        showMessage(tr("Metrics"), tr("Could not open the signal file."));
        return;
    }
    delete signalFile;
    signalFile = selected;
    fname = QDir::cleanPath(path);
    sf = nullptr; // The main window may replace/delete its file while we are open.
    stopRequested = false;
    resetResults();
    ui->edMess->setText(tr("Ready"));
    ui->progressBar->setValue(0);
    UpdateUIMode();
    QDialog::show();
}

void MetricsDialog::on_rbCalculation_toggled(bool checked)
{
    Q_UNUSED(checked);
    UpdateUIMode();
}

void MetricsDialog::on_rbEstimation_toggled(bool checked)
{
    Q_UNUSED(checked);
    UpdateUIMode();
}

void MetricsDialog::UpdateUIMode()
{
    if (running) return;
    const bool isCalculation = ui->rbCalculation->isChecked();
    if (!isCalculation && fname2.isEmpty()) fname2 = fname;
    if (!isCalculation) openNoisyFile();
    const QSignalBlocker block1(ui->pathFile1), block2(ui->pathFile2);
    ui->label_file1->setText(isCalculation ? tr("Signal file:") : tr("Noisy file:"));
    ui->label_file2->setText(tr("Noisy file:"));
    ui->pathFile1->setText(isCalculation ? fname : fname2);
    ui->pathFile1->setToolTip(isCalculation ? fname : fname2);
    ui->pathFile1->setReadOnly(isCalculation);
    ui->pathFile1->setEnabled(!isCalculation);
    ui->btnOpenFile1->setEnabled(!isCalculation);
    ui->pathFile2->setText(fname2);
    ui->pathFile2->setToolTip(fname2);
    ui->pathFile2->setReadOnly(false);
    ui->pathFile2->setVisible(isCalculation);
    ui->btnOpenFile2->setVisible(isCalculation);
    ui->label_file2->setVisible(isCalculation);
    ui->modeHint->setText(isCalculation
        ? tr("Signal file is fixed to the main-window file. Choose or paste the noisy file path.")
        : tr("Choose or paste a noisy file path. Classic metrics use the mean trace as a reference, not ground truth."));
    updateAvailableMetrics();
    resetResults();
    FillLimits();
    setRunning(false);
}

void MetricsDialog::updateAvailableMetrics()
{
    const bool isCalculation = ui->rbCalculation->isChecked();
    ui->chkTPRS->setEnabled(!isCalculation);
    if (isCalculation) ui->chkTPRS->setChecked(false);
}

void MetricsDialog::on_btnCalculate_clicked()
{
    if(running)
    {
        stopRequested = true;
        ui->btnCalculate->setText(tr("Stopping..."));
        ui->btnCalculate->setEnabled(false);
        emit stop();
        return;
    }

    if (!openNoisyFile()) return;
    SeisFile* primary = primaryFile();
    if(!primary || !primary->Active())
    {
        showMessage( "Error", "Primary file is not active!");
        return;
    }

    bool isCalculation = ui->rbCalculation->isChecked();

    // 计算模式：需要第二个文件
    if(isCalculation)
    {
        fname2 = QDir::cleanPath(fname2);

        if(fname2.isEmpty())
        {
            showMessage( "Error", "Please select noisy file!");
            return;
        }

        if(!QFile::exists(fname2))
        {
            showMessage( "Error", "Noisy file does not exist!");
            return;
        }

        if(fname == fname2)
        {
            showMessage( "Error",  "Signal file and noisy file cannot be the same!");
            return;
        }

        if (!qFuzzyCompare(primary->Si(), sf2->Si())) {
            showMessage( tr("Metrics"), tr("The two files must have the same sample interval."));
            return;
        }

        if(primary->Ns() != sf2->Ns())
        {
            showMessage( "Error",
                QString("Samples must be same! Primary: %1, Secondary: %2")
                .arg(primary->Ns()).arg(sf2->Ns()));
            return;
        }
        if(primary->Nt() != sf2->Nt())
        {
            showMessage( "Error",
                QString("Traces must be same! Primary: %1, Secondary: %2")
                .arg(primary->Nt()).arg(sf2->Nt()));
            return;
        }
    }

    bool calcPSNR = ui->ckPSNR->isChecked();
    bool calcSSIM = ui->ckSSIM->isChecked();
    bool calcMSE  = ui->ckMSE->isChecked();
    bool calcSNR  = ui->ckSNR->isChecked();
    bool calcTPRS = ui->chkTPRS->isChecked();

    if(!calcPSNR && !calcSSIM && !calcMSE && !calcSNR && !calcTPRS)
    {
        showMessage( "Warning", "Please select at least one metric!");
        return;
    }

    bool ok = true;
    int tmmin = ReadTxt(ui->edTmMin, ok);
    int tmmax = ReadTxt(ui->edTmMax, ok);
    int trmin = ReadTxt(ui->edTrMin, ok) - 1;
    int trmax = ReadTxt(ui->edTrMax, ok);
    int trstp = ReadTxt(ui->edTrStp, ok);

    if(!ok || trmin < 0 || trmax > primary->Nt() || trmax <= trmin || trstp < 1
       || tmmin > tmmax || tmmin < int(primary->Tmin()*1000) || tmmax > int(primary->Tmax()*1000))
    {
        ui->edMess->setText(tr("Invalid range. Use valid trace/time limits and a trace step of at least 1."));
        return;
    }

    resetResults();
    stopRequested = false;
    setRunning(true);
    ui->edMess->setText(tr("Calculating..."));

    ui->progressBar->setMaximum(100);
    ui->progressBar->setValue(0);

    QThread* thread = new QThread;
    calculationThread = thread;
    completionMessage.clear();
    MetricsWorker* worker = new MetricsWorker(
        primary, sf2, isCalculation,
        trmin, trmax, trstp,
        tmmin, tmmax,
        calcPSNR, calcSSIM, calcMSE, calcSNR, calcTPRS);

    worker->moveToThread(thread);

    connect(thread, SIGNAL(started()),          worker, SLOT(process()));
    connect(worker, SIGNAL(finished()),         thread, SLOT(quit()), Qt::DirectConnection);
    connect(worker, SIGNAL(finished()),         worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()),         thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(emetrics(QString)),  this,   SLOT(x_metrics(QString)));
    connect(worker, SIGNAL(eprogr(int,QString)),this,   SLOT(x_progr(int,QString)));
    connect(worker, &MetricsWorker::efin, this, [this](const QString& message) {
        completionMessage = message;
    });
    connect(thread, &QThread::finished, this, [this]() {
        calculationThread = nullptr;
        x_fin(completionMessage);
    });
    connect(this,   SIGNAL(stop()),             worker, SLOT(stop()), Qt::DirectConnection);

    thread->start();
}

void MetricsDialog::on_btnClose_clicked()
{
    close();
}

bool MetricsDialog::openNoisyFile()
{
    const QString path = fname2.trimmed();
    if (path.isEmpty() || !QFileInfo(path).isFile()) {
        showMessage(tr("Metrics"), tr("Please select a readable noisy file."));
        return false;
    }
    if (sf2 && sf2->Active() && QDir::cleanPath(QString::fromLocal8Bit(sf2->Fname().c_str())) == QDir::cleanPath(path))
        return true;
    const int type = signalFile && QFileInfo(path).canonicalFilePath() == QFileInfo(fname).canonicalFilePath()
        ? signalFile->typeIdx() : 1;
    SeisFile* selected = type == 2 ? static_cast<SeisFile*>(new SuFile(this))
        : type == 3 ? static_cast<SeisFile*>(new CstFile(this))
        : static_cast<SeisFile*>(new SgyFile(this));
    selected->setFname(path);
    selected->setActive(true);
    if (!selected->Active()) {
        delete selected;
        showMessage(tr("Metrics"), tr("Could not open the noisy file."));
        return false;
    }
    delete sf2;
    sf2 = selected;
    fname2 = QDir::cleanPath(path);
    FillLimits();
    return true;
}

void MetricsDialog::chooseNoisyFile()
{
    if (running) return;
    const QString chosen = QFileDialog::getOpenFileName(
        this, tr("Choose Noisy File"), curDir, tr("SEG-Y file (*.sgy *.segy)"));
    if (chosen.isEmpty()) return;
    const QString previous = fname2;
    fname2 = chosen;
    if (!openNoisyFile()) { fname2 = previous; return; }
    UpdateUIMode();
}

void MetricsDialog::on_btnOpenFile2_clicked() { chooseNoisyFile(); }

void MetricsDialog::on_btnOpenFile1_clicked()
{
    if (!ui->rbCalculation->isChecked()) chooseNoisyFile();
}

SeisFile* MetricsDialog::primaryFile() const
{
    return ui->rbCalculation->isChecked() ? signalFile : sf2;
}

void MetricsDialog::on_pathFile1_textChanged(const QString &path)
{
    ui->pathFile1->setToolTip(path);
    if (running || ui->rbCalculation->isChecked()) return;
    fname2 = path;
    resetResults();
}

QString MetricsDialog::defaultReportPath() const
{
    const QString base = QFileInfo(fname2).completeBaseName();
    return QDir(curDir.isEmpty() ? QDir::currentPath() : curDir)
        .absoluteFilePath((base.isEmpty() ? QString("noisy") : base) + "-metrics.txt");
}

void MetricsDialog::on_btnSaveResults_clicked()
{
    QString results = ui->txtResults->toPlainText();
    if(results.isEmpty())
    {
        showMessage( "Info", "No results to save!");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, "Save Results",
        defaultReportPath(),
        "Text File (*.txt)");

    if(fileName.isEmpty()) return;

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        showMessage( "Error", "Cannot create file!");
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << results;
    file.close();

    QFileInfo fi(fileName);
    if (savDir) *savDir = fi.dir().absolutePath();

    ui->edMess->setText(tr("Results saved: %1").arg(fileName));
}

void MetricsDialog::on_ckTrAll_clicked()
{
    FillLimits();
}

void MetricsDialog::on_ckTmAll_clicked()
{
    FillLimits();
}

void MetricsDialog::on_pathFile2_textChanged(const QString &path)
{
    ui->pathFile2->setToolTip(path);
    if (running) return;
    fname2 = path;
    resetResults();
}

void MetricsDialog::FillLimits()
{
    SeisFile* primary = primaryFile();
    if(!primary || !primary->Active()) return;

    int tm1 = primary->Tmin() * 1000;
    int tm2 = primary->Tmax() * 1000;
    int tr1 = 1;
    int tr2 = primary->Nt();

    if(ui->ckTmAll->isChecked())
    {
        ui->edTmMin->setText(QString::number(tm1));
        ui->edTmMax->setText(QString::number(tm2));
        ui->edTmMin->setEnabled(false);
        ui->edTmMax->setEnabled(false);
    }
    else
    {
        ui->edTmMin->setEnabled(true);
        ui->edTmMax->setEnabled(true);
    }

    if(ui->ckTrAll->isChecked())
    {
        ui->edTrMin->setText(QString::number(tr1));
        ui->edTrMax->setText(QString::number(tr2));
        ui->edTrStp->setText("1");
        ui->edTrMin->setEnabled(false);
        ui->edTrMax->setEnabled(false);
        ui->edTrStp->setEnabled(false);
    }
    else
    {
        ui->edTrMin->setEnabled(true);
        ui->edTrMax->setEnabled(true);
        ui->edTrStp->setEnabled(true);
    }
}

void MetricsDialog::x_fin(QString mess)
{
    if (stopRequested) {
        resetResults();
        ui->edMess->setText(ui->rbCalculation->isChecked()
            ? tr("Calculation stopped. No complete result was saved.")
            : tr("Estimation stopped. No complete result was saved."));
        ui->progressBar->setValue(0);
    } else if (!ui->txtResults->toPlainText().isEmpty()) {
        ui->edMess->setText(ui->rbCalculation->isChecked()
            ? tr("Calculation complete. Results are ready to export.")
            : tr("Estimation complete. Results are ready to export."));
        ui->progressBar->setValue(100);
    } else {
        ui->edMess->setText(mess);
        ui->progressBar->setValue(0);
    }
    setRunning(false);
}

void MetricsDialog::x_progr(int pers, QString mess)
{
    ShowProgress(mess,pers);
}

void MetricsDialog::ShowProgress(QString mes, int pers)
{
    if(pers<0)
    {
        ui->progressBar->setValue(0);
        ui->edMess->setStyleSheet("");
        ui->edMess->setText(mes);
    }
    else
    {
        ui->progressBar->setValue(pers);
        ui->edMess->setText(mes);
    }
}

void MetricsDialog::x_metrics(QString results)
{
    if (stopRequested) return;
    ui->txtResults->setPlainText(results);
    const QRegularExpression metricLine("^(MSE|RMSE|PSNR|SNR|SSIM|TPRS)\\s*=\\s*(\\S+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator matches = metricLine.globalMatch(results);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        for (int row = 0; row < ui->metricTable->RowCount(); ++row) {
            if (ui->metricTable->Cell(row, 0) == match.captured(1))
                ui->metricTable->setCell(row, 1, match.captured(2));
        }
    }
    ui->resultTabs->setCurrentIndex(0);
    ui->resultHint->setText(tr("Only selected metrics are calculated. See Details for the reference and sample range."));
}

int MetricsDialog::ReadTxt(QLineEdit* edt, bool& ok)
{
    bool Ok;
    int v = edt->text().toInt(&Ok);

    QPalette palette;
    if(!Ok)
    {
        ok = false;
        palette.setColor(QPalette::Text, Qt::red);
    }
    else
    {
        palette.setColor(QPalette::Text, Qt::black);
    }
    edt->setPalette(palette);

    return v;
}

void MetricsDialog::showMessage(const QString& title, const QString& message)
{
    ui->edMess->setText(title + ": " + message);
    ui->edMess->setToolTip(message);
}
