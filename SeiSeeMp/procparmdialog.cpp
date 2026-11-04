#include "procparmdialog.h"
#include "gfxstyle.h"
#include "ui_procparmdialog.h"

#include <QSvgRenderer>
#include <QSvgWidget>

#include "util2.h"

ProcParmDialog::ProcParmDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ProcParmDialog)
{
    ui->setupUi(this);
    const int diagramIndex = ui->horizontalLayout_14->indexOf(ui->label);
    ui->horizontalLayout_14->removeWidget(ui->label);
    delete ui->label;
    ui->label = nullptr;
    QSvgWidget *diagram =
        new QSvgWidget(QStringLiteral(":/images/Filt.svg"), ui->groupBox);
    diagram->renderer()->setAspectRatioMode(Qt::KeepAspectRatio);
    ui->horizontalLayout_14->insertWidget(diagramIndex, diagram);
}

ProcParmDialog::~ProcParmDialog()
{
    delete ui;
}

void ProcParmDialog::on_closeButton_pressed()
{
    close();
}

void ProcParmDialog::on_okButton_pressed()
{
    if (ReadForm()) {
        emit changedEvent();
        close();
    }
}

void ProcParmDialog::on_applyButton_pressed()
{
    if (ReadForm()) {
        emit changedEvent();
    }
}

void ProcParmDialog::show()
{
    FillForm();

    QDialog::show();
}

void ProcParmDialog::FillForm()
{
    if (agc && norm)
        norm = false;

    ui->ckAgc->setChecked(agc);
    ui->ckNorm->setChecked(norm);
    ui->ckFilt->setChecked(filt);

    ui->edF1->setText(Tprintf("%g", f1));
    ui->edF2->setText(Tprintf("%g", f2));
    ui->edF3->setText(Tprintf("%g", f3));
    ui->edF4->setText(Tprintf("%g", f4));

    ui->edAgcw->setText(Tprintf("%g", agcw));
    ui->cbAgcType->setCurrentIndex(
        qBound(0, agcType, ui->cbAgcType->count() - 1));
}

bool ProcParmDialog::ReadForm()
{
    bool ok = true;
    bool rc = true;

    f1 = ui->edF1->text().toFloat(&ok);
    if (!ok)
        rc = false;
    f2 = ui->edF2->text().toFloat(&ok);
    if (!ok)
        rc = false;
    f3 = ui->edF3->text().toFloat(&ok);
    if (!ok)
        rc = false;
    f4 = ui->edF4->text().toFloat(&ok);
    if (!ok)
        rc = false;

    agcw = ui->edAgcw->text().toFloat(&ok);
    if (!ok)
        rc = false;

    agcType = ui->cbAgcType->currentIndex();
    agc = ui->ckAgc->isChecked();
    norm = ui->ckNorm->isChecked();
    filt = ui->ckFilt->isChecked();

    if (agc && norm)
        norm = false;

    if (rc)
        FillForm();

    return rc;
}

void ProcParmDialog::on_ckAgc_toggled(bool checked)
{
    if (checked && ui->ckNorm->isChecked())
        ui->ckNorm->setChecked(false);
}

void ProcParmDialog::on_ckNorm_toggled(bool checked)
{
    if (checked && ui->ckAgc->isChecked())
        ui->ckAgc->setChecked(false);
}
