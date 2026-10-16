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
    setWindowModality(Qt::ApplicationModal);
    setSizeGripEnabled(true);
    ui->bodyLayout->setStretch(0, 5);
    ui->bodyLayout->setStretch(1, 4);
    for (QLayout* layout : findChildren<QLayout*>()) layout->setSpacing(6);
    ui->verticalLayout_main->setSpacing(8);
    ui->pathFile1->setToolTip(tr("Signal file defaults to the open file. Use Open to choose another signal file."));
    ui->btnSaveResults->setEnabled(false);
    ui->txtResults->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ui->txtResults->setPlaceholderText(tr("The full calculation report will appear here."));
    ui->metricTable->setColumnCount(3);
    ui->metricTable->setHorizontalHeaderLabels(QStringList() << tr("Metric") << tr("Value") << tr("Unit"));
    ui->metricTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->metricTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->metricTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->metricTable->verticalHeader()->hide();
    ui->metricTable->verticalHeader()->setDefaultSectionSize(28);
    ui->metricTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->metricTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->metricTable->setAlternatingRowColors(true);
    ui->metricTable->setShowGrid(true);
    for (QLineEdit* edit : {ui->edTrMin, ui->edTrMax, ui->edTrStp})
        edit->setValidator(new QIntValidator(1, 2147483647, edit));
    resetResults();
    updateAvailableMetrics(); // 初始化一次
}

void MetricsDialog::resetResults()
{
    ui->resultHint->setText(tr("Results will appear here after calculation. The Details tab contains the full report."));
    const QStringList names = QStringList() << "MSE" << "RMSE" << "PSNR" << "SNR" << "SSIM" << "TPRS";
    ui->metricTable->setRowCount(names.size());
    for (int row = 0; row < names.size(); ++row) {
        ui->metricTable->setItem(row, 0, new QTableWidgetItem(names[row]));
        ui->metricTable->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8("\xe2\x80\x94")));
        ui->metricTable->setItem(row, 2, new QTableWidgetItem(row == 2 || row == 3 ? "dB" : ""));
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
    ui->btnCalculate->setIcon(QIcon(active ? ":/images/stop.png" : ":/images/Procp.png"));
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

MetricsDialog::~MetricsDialog()
{
    if (calculationThread) {
        emit stop();
        calculationThread->quit();
        calculationThread->wait();
    }
    if (referenceFile) {
        referenceFile->UnRegisterLink(this);
        delete referenceFile;
    }
    if(sf2)
    {
        sf2->UnRegisterLink(this);
        delete sf2;
    }
    delete ui;
}

void MetricsDialog::show()
{
    if (running || !sf || !sf->Active()) return;
    if (referenceFile) {
        referenceFile->UnRegisterLink(this);
        delete referenceFile;
        referenceFile = nullptr;
    }
    resetResults();
    ui->edMess->setText(tr("Ready"));
    ui->progressBar->setValue(0);
    QDialog::show();

    fname = QString::fromLocal8Bit(sf->Fname().c_str());
    fname = QDir::cleanPath(fname);
    ui->pathFile1->setText(fname);

    FillLimits();
    UpdateUIMode();
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
    bool isCalculation = ui->rbCalculation->isChecked();

    ui->label_file1->setText(isCalculation ? tr("Signal file:") : tr("Input file:"));
    ui->label_file2->setText(tr("Noisy file:"));
    ui->pathFile1->setEnabled(true);
    ui->label_file1->setEnabled(true);
    ui->btnOpenFile1->setEnabled(isCalculation);
    if (primaryFile()) {
        fname = QDir::cleanPath(QString::fromLocal8Bit(primaryFile()->Fname().c_str()));
        ui->pathFile1->setText(fname);
        FillLimits();
    }
    ui->modeHint->setText(isCalculation
        ? tr("Compare a signal file with a noisy file. Trace count, sample count and sample interval must match.")
        : tr("Classic metrics use the mean of selected traces as a reference. These values are not ground-truth recovery scores."));

    ui->pathFile2->setEnabled(isCalculation);
    ui->btnOpenFile2->setEnabled(isCalculation);
    ui->label_file2->setEnabled(isCalculation);
    ui->pathFile2->setVisible(isCalculation);
    ui->btnOpenFile2->setVisible(isCalculation);
    ui->label_file2->setVisible(isCalculation);

    // TPRS 仅用于估算模式
    ui->chkTPRS->setEnabled(!isCalculation);
    if (isCalculation) ui->chkTPRS->setChecked(false);

    updateAvailableMetrics();
    resetResults();
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
    SeisFile* primary = primaryFile();
    if(running)
    {
        stopRequested = true;
        ui->btnCalculate->setText(tr("Stopping..."));
        ui->btnCalculate->setEnabled(false);
        emit stop();
        return;
    }

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

        if(sf2)
        {
            sf2->UnRegisterLink(this);
            delete sf2;
        }

        sf2 = new SgyFile(this);
        sf2->setFname(fname2);
        sf2->setActive(true);

        if (!sf2->Active()) {
            showMessage( tr("Metrics"), tr("Could not open the comparison file."));
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

void MetricsDialog::on_btnOpenFile2_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Choose Noisy File",
        curDir,
        "SEG-Y file (*.sgy *.segy)");

    if(fileName.isEmpty()) return;

    ui->pathFile2->setText(fileName);
    fname2 = fileName;
}

void MetricsDialog::on_btnOpenFile1_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Choose Signal File",
        curDir,
        "SEG-Y file (*.sgy *.segy)");

    if(fileName.isEmpty()) return;

    SeisFile* selected = new SgyFile(this);
    selected->setFname(fileName);
    selected->setActive(true);
    if (!selected->Active()) {
        selected->UnRegisterLink(this);
        delete selected;
        showMessage( tr("Metrics"), tr("Could not open the reference file."));
        return;
    }
    if (referenceFile) {
        referenceFile->UnRegisterLink(this);
        delete referenceFile;
    }
    referenceFile = selected;

    ui->pathFile1->setText(fileName);
    fname = fileName;
    FillLimits();
}

SeisFile* MetricsDialog::primaryFile() const
{
    return ui->rbCalculation->isChecked() && referenceFile ? referenceFile : sf;
}

void MetricsDialog::on_pathFile1_textChanged(const QString &arg1)
{
    fname = arg1;
    ui->pathFile1->setToolTip(arg1);
    ui->pathFile1->setCursorPosition(0);
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
        *savDir,
        "Text File (*.txt)");

    if(fileName.isEmpty()) return;

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        showMessage( "Error", "Cannot create file!");
        return;
    }

    QTextStream out(&file);
    out << results;
    file.close();

    QFileInfo fi(fileName);
    *savDir = fi.dir().absolutePath();

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

void MetricsDialog::on_pathFile2_textChanged(const QString &arg1)
{
    fname2 = arg1;
    ui->pathFile2->setToolTip(arg1);
    ui->pathFile2->setCursorPosition(0);
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
        for (int row = 0; row < ui->metricTable->rowCount(); ++row) {
            if (ui->metricTable->item(row, 0)->text() == match.captured(1))
                ui->metricTable->item(row, 1)->setText(match.captured(2));
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
