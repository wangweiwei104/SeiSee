#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QThread>

#include "gfxstyle.h"
#include "saveasdialog.h"
#include "ui_saveasdialog.h"

SaveAsDialog::SaveAsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SaveAsDialog)
{
    ui->setupUi(this);
    resize(GfxStyle::SaveAsDialogWidth, GfxStyle::SaveAsDialogHeight);

    QFont labelFont = QApplication::font();
    labelFont.setPointSizeF(GfxStyle::SaveAsLabelFontPointSize);
    ui->label_2->setFont(labelFont);
    ui->label_3->setFont(labelFont);
    ui->label_4->setFont(labelFont);
    ui->label_5->setFont(labelFont);
    ui->label_6->setFont(labelFont);
    ui->label_7->setFont(labelFont);
    ui->label_8->setFont(labelFont);

    hdrListGrid.setRowCount(0);
    hdrListGrid.setColCount(2);
    hdrListGrid.verticalHeader()->setVisible(false);
    hdrListGrid.setColLabel(0, "Name");
    hdrListGrid.setColLabel(1, "Description");
    hdrListGrid.setColWidthInCharacters(
        0, GfxStyle::HeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    hdrListGrid.setColWidth(1, GfxStyle::HiddenTableColumnWidth);

    QVBoxLayout *hdrListCkLayout = new QVBoxLayout;
    hdrListCkLayout->setMargin(GfxStyle::CompactLayoutMargin);
    hdrListCkLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    hdrListCkLayout->addWidget(&hdrListGrid);
    ui->hdrBox->setLayout(hdrListCkLayout);

    running = false;

    connect(&hdrListGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(hdrListEvent(int, int)));
}

SaveAsDialog::~SaveAsDialog()
{
    delete ui;
}

void SaveAsDialog::on_btnSave_clicked()
{
    if (running) {
        emit stop();
        return;
    }

    if (!sf || !sf->Active())
        return;

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

    const QFileInfo sourceFile(
        QString::fromLocal8Bit(sf->Fname().c_str()));
    QString defaultFileName = sourceFile.completeBaseName() + "-copy";
    if (!sourceFile.suffix().isEmpty())
        defaultFileName += "." + sourceFile.suffix();

    QString fileName = QFileDialog::getSaveFileName(
        this, "Save File As", QDir(curDir).absoluteFilePath(defaultFileName),
        "SEG-Y File (*.sgy *.segy)");
    if (fileName == "") {
        return;
    }

    bool rev = ui->ckRev->isChecked();
    bool isProc = ui->ckProc->isChecked();

    QFileInfo fi(fileName);
    *savDir = fi.dir().absolutePath();

    ui->btnSave->setIcon(QIcon(":/images/stop.png"));

    running = true;

    QThread *thread = new QThread;

    SaveAsWorker *worker =
        new SaveAsWorker(sf, fileName, selh, trmin, trmax, trstp, tmmin, tmmax,
                         frmt, rev, isProc);

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

void SaveAsDialog::ShowProgress(QString mes, int pers)
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

void SaveAsDialog::x_fin(QString mess)
{
    running = false;
    ui->btnSave->setIcon(QIcon(":/images/FileSave.png"));
    ShowProgress(mess, -1);
    ui->edMess->setText(mess);
}

void SaveAsDialog::x_progr(int pers, QString mess)
{
    ShowProgress(mess, pers);
}

void SaveAsDialog::on_btnClose_clicked()
{
    close();
}

void SaveAsDialog::FillHdrListGrid()
{
    int n;
    int l;

    TrHdrDefList *List = TraceHdrsSet.List();

    l = List->count();

    hdrListGrid.setRowCount(l - 1);

    for (n = 0; n < l; n++) {
        TrHdrDef *h = List->item(n);

        hdrListGrid.setCell(n, 0, h->name);
        hdrListGrid.setCell(n, 1, h->desc);
    }
}

void SaveAsDialog::FillLimits()
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

int SaveAsDialog::ReadTxt(QLineEdit *edt, bool &ok)
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

void SaveAsDialog::show()
{
    QDialog::show();
    FillHdrListGrid();
    FillLimits();
}

void SaveAsDialog::hdrListEvent(int row, int mode)
{
    if (mode == 1) {
        if (ui->tabNumExp->currentIndex() == 0) {
            ui->edIdx->setText(hdrListGrid.Cell(row, 0));
        }
    }
}

void SaveAsDialog::on_ckTrAll_clicked()
{
    FillLimits();
}

void SaveAsDialog::on_ckTmAll_clicked()
{
    FillLimits();
}

void SaveAsDialog::on_btnTrMin_clicked()
{
    ui->edTrMin->setText("1");
}

void SaveAsDialog::on_btnTrMax_clicked()
{
    int tr2 = sf->Nt();
    ui->edTrMax->setText(QString::number(tr2));
}

void SaveAsDialog::on_btnTrStp_clicked()
{
    ui->edTrStp->setText("0");
}

void SaveAsDialog::on_btnTmMin_clicked()
{
    int tm1 = sf->Tmin() * 1000;
    ui->edTmMin->setText(QString::number(tm1));
}

void SaveAsDialog::on_btnTmMax_clicked()
{
    int tm2 = sf->Tmax() * 1000;
    ui->edTmMax->setText(QString::number(tm2));
}
