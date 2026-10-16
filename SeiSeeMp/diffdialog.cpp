#include <QFileDialog>
#include <QThread>
#include <QMessageBox>

#include "diffdialog.h"
#include "ui_diffdialog.h"

DiffDialog::DiffDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DiffDialog)
{
    ui->setupUi(this);

    sf = nullptr;
    sf2 = nullptr;
    savDir = nullptr;
    running = false;
}

DiffDialog::~DiffDialog()
{
    if (sf2) {
        sf2->UnRegisterLink(this);
    }
    delete ui;
}

void DiffDialog::show()
{
    QDialog::show();
    fname = QString::fromLocal8Bit(sf->Fname().c_str());
    fname = QDir::cleanPath(fname);
    ui->pathFile->setText(fname);
    UpdateDefaultOutputPath();
    FillLimits();
}

// Difference 对话框选定减数文件后，会自动填入结果文件名，格式为：
// [当前左侧目录]/diff_[被减数文件名]_and_[减数文件名].[减数文件后缀]
// 例如：
// 如果当前左侧目录为 /home/user/data， 被减数文件名为 primary.sgy，减数文件名为 secondary.sgy，
// 则默认结果文件名为：
// /home/user/data/diff_primary_and_secondary.sgy
void DiffDialog::UpdateDefaultOutputPath()
{
    const QString secondaryFile = ui->pathFile2->text().trimmed();
    if (fname.isEmpty() || secondaryFile.isEmpty())
        return;

    const QFileInfo primaryInfo(fname);
    const QFileInfo secondaryInfo(secondaryFile);
    QString outputName =
        QString("diff_%1_and_%2")
            .arg(primaryInfo.completeBaseName(),
                 secondaryInfo.completeBaseName());
    if (!secondaryInfo.suffix().isEmpty())
        outputName += "." + secondaryInfo.suffix();

    ui->pathFileOutput->setText(
        QDir(curDir).absoluteFilePath(outputName));
}

void DiffDialog::on_btnSave_clicked()
{
    if (running) {
        emit stop();
        return;
    }

    if (!sf || !sf->Active())
        return;

    // 统一路径格式
    fname2 = QDir::cleanPath(fname2);
    //    fname = QDir::cleanPath(fname);
    fnameOut = QDir::cleanPath(fnameOut);

    if (fname2 == "")
        return;

    if (!QFile::exists(fname2)) {
        QMessageBox::critical(this, "Error", "Secondary File not exits!",
                              QMessageBox::Discard);
        return;
    }

    if (fnameOut == "")
        return;

    sf2 = new SgyFile(this);

    sf2->setFname(fname2);
    sf2->setActive(true);

    if (fname2 == fnameOut) {
        QMessageBox::critical(
            this, "Error",
            "File cannot be same! Secondary and Output file is same!",
            QMessageBox::Discard);
        return;
    }

    if (fname2 == fname) {
        QMessageBox::critical(
            this, "Error",
            "File cannot be same! Primary and Secondary file is same!",
            QMessageBox::Discard);
        return;
    }

    if (fname == fnameOut) {
        QMessageBox::critical(
            this, "Error",
            "File cannot be same! Primary and Output file is same!",
            QMessageBox::Discard);
        return;
    }

    if (sf->Ns() != sf2->Ns()) {
        QMessageBox::critical(this, "Error",
                              "Samples must be same! Primary file is " +
                                  QString::number(sf->Ns()) +
                                  ", but Secondary file is " +
                                  QString::number(sf2->Ns()),
                              QMessageBox::Discard);
        return;
    }

    if (sf->Nt() != sf2->Nt()) {
        QMessageBox::critical(this, "Error",
                              "Traces must be same! Primary file is " +
                                  QString::number(sf->Nt()) +
                                  ", but Secondary file is " +
                                  QString::number(sf2->Nt()),
                              QMessageBox::Discard);
        return;
    }

    //    qDebug() << "# Traces            : " << sf->Nt() << endl;
    //    qDebug() << "# Samples           :  " << sf->Ns() << endl;
    //    qDebug() << "Sample Format       :  "<< sf->Format() << endl ;
    //    qDebug() << "Format name : "<< sf->FormatName() << endl;
    //    qDebug() << "Sample Interval (μs): %d\n"<< int(sf->Si()*1000000) <<
    //    endl;

    //    qDebug() << "# Traces            : " << sf2->Nt() << endl;
    //    qDebug() << "# Samples           :  " << sf2->Ns() << endl;
    //    qDebug() << "Sample Format       :  "<< sf2->Format() << endl ;
    //    qDebug() << "Format name : "<< sf2->FormatName() << endl;
    //    qDebug() << "Sample Interval (μs): %d\n"<< int(sf2->Si()*1000000) <<
    //    endl;

    bool ok = true;

    int tmmin = ReadTxt(ui->edTmMin, ok);
    int tmmax = ReadTxt(ui->edTmMax, ok);
    int trmin = ReadTxt(ui->edTrMin, ok) - 1;
    int trmax = ReadTxt(ui->edTrMax, ok);
    int trstp = ReadTxt(ui->edTrStp, ok);

    int frmt = 5;

    if (ui->cbFormat->currentIndex() > 0)
        frmt = 1;

    if (!ok) {
        ui->edMess->setText("Parameter error");
        return;
    } else {
        ui->edMess->setText("");
    }

    QString selh = ui->edIdx->text();

    //    QString fileName = QFileDialog::getSaveFileName(this, "Save File As",
    //                                                    *savDir,
    //                                                    "SEG-Y File (*.sgy)");
    //    if(fileName=="")
    //    {
    //        return;
    //    }

    ////    bool rev = ui->ckRev->isChecked();
    ////    bool isProc = ui->ckProc->isChecked();

    //    QFileInfo fi(fileName);
    //    *savDir  = fi.dir().absolutePath();

    ui->btnSave->setIcon(QIcon(":/images/stop.png"));

    running = true;

    QThread *thread = new QThread;

    DiffWorker *worker = new DiffWorker(sf, sf2, fnameOut, selh, trmin, trmax,
                                        trstp, tmmin, tmmax, frmt);

    worker->moveToThread(thread);
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(eprogr(int, QString)), this,
            SLOT(x_progr(int, QString)));
    connect(worker, SIGNAL(efin(QString)), this, SLOT(x_fin(QString)));

    connect(this, SIGNAL(stop()), worker, SLOT(stop()), Qt::DirectConnection);

    thread->start();
}

void DiffDialog::on_btnClose_clicked()
{
    close();
}

void DiffDialog::on_ckTrAll_clicked()
{
    FillLimits();
}

void DiffDialog::on_ckTmAll_clicked()
{
    FillLimits();
}

void DiffDialog::on_btnTrMin_clicked()
{
    ui->edTrMin->setText("1");
}

void DiffDialog::on_btnTrMax_clicked()
{
    int tr2 = sf->Nt();
    ui->edTrMax->setText(QString::number(tr2));
}

void DiffDialog::on_btnTrStp_clicked()
{
    ui->edTrStp->setText("0");
}

void DiffDialog::on_btnTmMin_clicked()
{
    int tm1 = sf->Tmin() * 1000;
    ui->edTmMin->setText(QString::number(tm1));
}

void DiffDialog::on_btnTmMax_clicked()
{
    int tm2 = sf->Tmax() * 1000;
    ui->edTmMax->setText(QString::number(tm2));
}

void DiffDialog::FillLimits()
{
    int tm1 = sf->Tmin() * 1000;
    int tm2 = sf->Tmax() * 1000;

    int tr1 = 1;
    int tr2 = sf->Nt();

    if (ui->edIdx->text() == "") {
        ui->edIdx->setText(hdrListGrid.Cell(0, 0));
    }

    if (ui->ckTmAll->isChecked()) {
        ui->edTmMin->setText(QString::number(tm1));
        ui->edTmMax->setText(QString::number(tm2));
        ui->edTmMin->setEnabled(false);
        ui->edTmMax->setEnabled(false);
        ui->btnTmMax->setEnabled(false);
        ui->btnTmMin->setEnabled(false);
    } else {
        ui->edTmMin->setEnabled(true);
        ui->edTmMax->setEnabled(true);
        ui->btnTmMax->setEnabled(true);
        ui->btnTmMin->setEnabled(true);
    }

    if (ui->ckTrAll->isChecked()) {
        ui->edTrMin->setText(QString::number(tr1));
        ui->edTrMax->setText(QString::number(tr2));
        ui->edTrStp->setText("0");
        ui->edTrMin->setEnabled(false);
        ui->edTrMax->setEnabled(false);
        ui->edTrStp->setEnabled(false);
        ui->btnTrMin->setEnabled(false);
        ui->btnTrMax->setEnabled(false);
        ui->btnTrStp->setEnabled(false);
    } else {
        ui->edTrMin->setEnabled(true);
        ui->edTrMax->setEnabled(true);
        ui->edTrStp->setEnabled(true);
        ui->btnTrMin->setEnabled(true);
        ui->btnTrMax->setEnabled(true);
        ui->btnTrStp->setEnabled(true);
    }
}

void DiffDialog::on_btnOpenFileIn2_clicked()
{
    QString fileName =
        QFileDialog::getOpenFileName(this, "Choose Secondary Segy File", curDir,
                                     "SEG-Y file (*.sgy *.segy)");
    if (fileName == "") {
        return;
    }

    QFileInfo fi(fileName);

    QByteArray qfn = fi.filePath().toLocal8Bit();
    const char *fname = qfn.data();

    int frm = 2;
    int cgg, junk, swap;

    if (is_cst_f(fname)) {
        frm = 3;
    } else if (is_su_f(fname, swap)) {
        frm = 2;
    } else if (is_segy_f(fname, swap, cgg, junk)) {
        frm = 1;
    } else {
        QMessageBox::critical(
            this, "Error", "Unknown format for seismic file: " + QString(fname),
            QMessageBox::Discard);
        return;
    }

    ui->pathFile2->setText(fileName);
}

void DiffDialog::x_fin(QString mess)
{
    running = false;
    ui->btnSave->setIcon(QIcon(":/images/FileSave.png"));
    ShowProgress(mess, -1);
    ui->edMess->setText(mess);
}

void DiffDialog::x_progr(int pers, QString mess)
{
    ShowProgress(mess, pers);
}

void DiffDialog::ShowProgress(QString mes, int pers)
{
    if (pers < 0) {
        ui->progressBar->setValue(0);
        ui->edMess->setStyleSheet("");
        ui->edMess->setText(mes);
    } else {
        ui->progressBar->setValue(pers);

        if (mes != "")
            ui->edMess->setStyleSheet(
                "QLabel { background-color : lime; color : black; }");
        else
            ui->edMess->setStyleSheet("");

        ui->edMess->setText(mes);
    }
}

int DiffDialog::ReadTxt(QLineEdit *edt, bool &ok)
{
    bool Ok;
    int v = edt->text().toInt(&Ok);

    if (!Ok) {
        ok = false;
        QPalette *palette = new QPalette();
        palette->setColor(QPalette::Text, Qt::red);
        edt->setPalette(*palette);
    } else {
        QPalette *palette = new QPalette();
        palette->setColor(QPalette::Text, Qt::black);
        edt->setPalette(*palette);
    }

    return v;
}

void DiffDialog::on_pathFile2_editingFinished()
{
    fname2 = ui->pathFile2->text();
}

// void DiffDialog::on_pathFile2_returnPressed()
//{
//     fname2 = ui->pathFile2->text();
// }

void DiffDialog::on_pathFileOutput_editingFinished()
{
    fnameOut = ui->pathFileOutput->text();
}

// void DiffDialog::on_pathFileOutput_returnPressed()
//{
//     fnameOut = ui->pathFileOutput->text();
// }

void DiffDialog::on_btnOpenFileOut_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Save File As", curDir, "SEG-Y file (*.sgy *.segy)");
    if (fileName == "") {
        return;
    }

    //    bool rev = ui->ckRev->isChecked();
    //    bool isProc = ui->ckProc->isChecked();

    //    QFileInfo fi(fileName);
    //    *savDir  = fi.dir().absolutePath();
    fnameOut = fileName;
    ui->pathFileOutput->setText(fileName);
}

void DiffDialog::on_pathFile2_textChanged(const QString &arg1)
{
    fname2 = arg1;
    UpdateDefaultOutputPath();
}

void DiffDialog::on_pathFileOutput_textChanged(const QString &arg1)
{
    fnameOut = arg1;
}
