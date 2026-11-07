#include "edithdrdialog.h"
#include "gfxstyle.h"
#include "ui_edithdrdialog.h"

EditHdrDialog::EditHdrDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EditHdrDialog)
{
    ui->setupUi(this);
    int n;

    curtype = 1;

    ui->lbErr->setText("");

    busy = true;

    ui->cbForm->clear();

    for (n = 0; n < FormatList::count(); n++) {
        ui->cbForm->addItem(FormatList::namei(n));
    }

    ui->cbForm->addItem("Expression");

    busy = false;

    hdrGrid.setRowCount(0);
    hdrGrid.setColCount(2);
    hdrGrid.verticalHeader()->setVisible(false);

    hdrGrid.setRowCount(0);
    hdrGrid.setColCount(2);
    hdrGrid.verticalHeader()->setVisible(false);
    hdrGrid.setColLabel(0, "Name");
    hdrGrid.setColLabel(1, "Description");
    hdrGrid.setColWidthInCharacters(
        0, GfxStyle::HeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    hdrGrid.setColWidthToStretch(
        1, GfxStyle::TableColumnHorizontalPadding);

    QVBoxLayout *hdrGridLayout = new QVBoxLayout;
    hdrGridLayout->setMargin(GfxStyle::CompactLayoutMargin);
    hdrGridLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    hdrGridLayout->addWidget(&hdrGrid);
    ui->hdrsBox->setLayout(hdrGridLayout);

    insGrid.setRowCount(0);
    insGrid.setColCount(2);
    insGrid.verticalHeader()->setVisible(false);

    insGrid.setRowCount(0);
    insGrid.setColCount(2);
    insGrid.verticalHeader()->setVisible(false);
    insGrid.setColLabel(0, "Name");
    insGrid.setColLabel(1, "Description");
    insGrid.setColWidthInCharacters(
        0, GfxStyle::HeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    insGrid.setColWidthToStretch(
        1, GfxStyle::TableColumnHorizontalPadding);

    QVBoxLayout *insGridLayout = new QVBoxLayout;
    insGridLayout->setMargin(GfxStyle::CompactLayoutMargin);
    insGridLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    insGridLayout->addWidget(&insGrid);
    ui->frmHdrs->setLayout(insGridLayout);

    setChanged(false);

    ui->btnUpdate->setEnabled(false);
    ui->btnReset->setEnabled(false);

    connect(&hdrGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(hdrTabEvent(int, int)));

    enableEdit(true);
}

EditHdrDialog::~EditHdrDialog()
{
    delete ui;
}

void EditHdrDialog::setChanged(bool v)
{
    changed = v;

    if (v) {
        ui->cbSet->setEnabled(false);
        ui->btnApply->setEnabled(hdrGrid.isEnabled());
        ui->btnDiscard->setEnabled(hdrGrid.isEnabled());
        ui->btnClose->setEnabled(false);
    } else {
        ui->cbSet->setEnabled(true);
        ui->btnApply->setEnabled(false);
        ui->btnDiscard->setEnabled(false);
        ui->btnClose->setEnabled(hdrGrid.isEnabled());
    }
}

void EditHdrDialog::enableEdit(bool enable)
{
    if (enable) {
        ui->edName->setEnabled(enable);
        ui->edDesc->setEnabled(enable);
        ui->edPos->setEnabled(enable);
        ui->cbForm->setEnabled(enable);
        ui->txtExpr->setEnabled(enable);
    } else {
        ui->edName->setEnabled(enable);
        ui->edDesc->setEnabled(enable);
        ui->edPos->setEnabled(enable);
        ui->cbForm->setEnabled(enable);
        ui->txtExpr->setEnabled(enable);
    }
}

void EditHdrDialog::hdrTabEvent(int row, int mode)
{
    QString name;
    QString desc;
    QString estr;
    int fidx;
    int frmt;
    int edit = true;
    int pos;

    if (mode == 1) // && ui->tabMode->currentIndex()==0)
    {
        busy = true;

        name = hdrGrid.Cell(row, 0);
        desc = hdrGrid.Cell(row, 1);
        pos = hdrGrid.Tag(row, 0);
        frmt = hdrGrid.Tag(row, 1);
        estr = hdrGrid.Data(row, 1);

        fidx = FormatList::index(frmt);
        if (estr != 0)
            fidx = 7;

        ui->edName->setText(name);
        ui->edDesc->setText(desc);
        ui->edPos->setText(QString::number(pos));
        ui->cbForm->setCurrentIndex(fidx);
        ui->txtExpr->document()->setPlainText(estr);

        ui->btnDelItem->setEnabled(edit);

        busy = false;
    }
}

void EditHdrDialog::show()
{
    curtype = TraceHdrsSet.listIdx();
    ui->cbSet->setCurrentIndex(curtype - 1);

    int r = hdrGrid.curRow();

    FillHdrGrid();
    FillInsGrid();

    if (r > 0)
        hdrGrid.setCurCell(r);

    QDialog::show();
}

void EditHdrDialog::FillHdrGrid()
{
    int n;

    TrHdrDefList *hdrDefList = TraceHdrsSet.List(curtype);

    int la = hdrDefList->count();
    int l;

    for (n = l = 0; n < la; n++) {
        int edit = hdrDefList->item(n)->edit;
        if (edit)
            l++;
    }

    hdrGrid.setRowCount(l);

    for (n = l = 0; n < la; n++) {
        int edit = hdrDefList->item(n)->edit;
        if (edit) {
            QString desc = hdrDefList->item(n)->desc;
            QString name = hdrDefList->item(n)->name;
            QString estr = hdrDefList->item(n)->expr.ExprString();
            int frmt = hdrDefList->item(n)->frmt;
            int pos = hdrDefList->item(n)->pos;

            // if(estr!="") bstr="EXPR";

            hdrGrid.setCell(l, 0, name);
            hdrGrid.setCell(l, 1, desc);
            hdrGrid.setTag(l, 0, pos);
            hdrGrid.setTag(l, 1, frmt);
            hdrGrid.setData(l, 1, estr);
            l++;
        }
    }
}

void EditHdrDialog::FillInsGrid()
{
    int n;

    TrHdrDefList *hdrDefList = TraceHdrsSet.List(curtype);

    int la = hdrDefList->count();
    int l;

    for (n = l = 0; n < la; n++) {
        int edit = hdrDefList->item(n)->edit;
        if (!edit)
            l++;
    }

    insGrid.setRowCount(l);

    for (n = l = 0; n < la; n++) {
        int edit = hdrDefList->item(n)->edit;
        if (!edit) {
            QString desc = hdrDefList->item(n)->desc;
            QString name = hdrDefList->item(n)->name;
            QString estr = hdrDefList->item(n)->expr.ExprString();
            int frmt = hdrDefList->item(n)->frmt;
            int pos = hdrDefList->item(n)->pos;

            insGrid.setCell(l, 0, name);
            insGrid.setCell(l, 1, desc);
            insGrid.setTag(l, 0, pos);
            insGrid.setTag(l, 1, frmt);
            insGrid.setData(l, 1, estr);
            l++;
        }
    }
}

int EditHdrDialog::findrow(QString name)
{
    int n;

    for (n = 0; n < hdrGrid.RowCount(); n++) {
        if (name == hdrGrid.Cell(n, 0))
            return n;
    }

    return -1;
}

void EditHdrDialog::on_btnClose_clicked()
{
    close();
}

void EditHdrDialog::on_cbSet_currentIndexChanged(int index)
{
    curtype = index + 1;
    FillHdrGrid();
    FillInsGrid();
}

void EditHdrDialog::on_btnAddItem_clicked()
{
    int i;
    for (i = 1; i < 99999; i++) {
        QString name;
        name.sprintf("new%d", i);

        if (findrow(name) != -1)
            continue;

        int nr = hdrGrid.RowCount();

        hdrGrid.setRowCount(nr + 1);

        hdrGrid.setCell(nr, 0, name);
        hdrGrid.setCell(nr, 1, name);
        hdrGrid.setTag(nr, 0, 1);
        hdrGrid.setTag(nr, 1, 2);
        hdrGrid.setCurCell(nr);
        setChanged(true);

        break;
    }
}

void EditHdrDialog::onUserInput()
{
    if (busy)
        return;

    if (hdrGrid.curRow() < 0)
        return;

    ui->btnUpdate->setEnabled(true);
    ui->btnReset->setEnabled(true);
    hdrGrid.setEnabled(false);
    ui->btnApply->setEnabled(false);
    ui->btnDiscard->setEnabled(false);
    ui->btnClose->setEnabled(false);
}

void EditHdrDialog::on_cbSet_editTextChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    onUserInput();
}

void EditHdrDialog::on_btnReset_clicked()
{
    if (hdrGrid.curRow() < 0)
        return;

    hdrTabEvent(hdrGrid.curRow(), 1);
    ui->btnUpdate->setEnabled(false);
    ui->btnReset->setEnabled(false);
    hdrGrid.setEnabled(true);
    ui->btnApply->setEnabled(changed);
    ui->btnDiscard->setEnabled(changed);
    ui->btnClose->setEnabled(!changed);

    ui->lbErr->setText("");
}

void EditHdrDialog::on_btnUpdate_clicked()
{
    int hdrl = 240;
    if (curtype == 2)
        hdrl = 256;

    bool ok = true;

    QString name = ui->edName->text().trimmed();

    int crow = hdrGrid.curRow();
    int frow = findrow(name);

    if (frow >= 0 && frow != crow) {
        ui->lbErr->setText("Duplicate item name");
        return;
    }

    QString desc = ui->edDesc->text().trimmed();
    QString estr = ui->txtExpr->document()->toPlainText();

    estr = estr.trimmed();

    if (estr != "") {
        SeisTrExpr expr(estr);
        if (expr.Error()) {
            ui->lbErr->setText("Bad expression");
            return;
        }
    }

    int fi = ui->cbForm->currentIndex();
    int frmt = FormatList::codei(fi);
    int flen = frmt % 10;

    int pos = ui->edPos->text().toInt(&ok);
    if (!ok || pos <= 0 || pos + flen - 1 > hdrl) {
        ui->lbErr->setText("Illegal Position value");
        return;
    }

    ui->lbErr->setText("");

    QString bstr;

    if (estr == "")
        bstr.sprintf("%d-%d", pos, pos + flen - 1);
    else
        bstr = "EXPR";

    hdrGrid.setCell(crow, 0, name);
    hdrGrid.setCell(crow, 1, desc);
    hdrGrid.setTag(crow, 0, pos);
    hdrGrid.setTag(crow, 1, frmt);
    hdrGrid.setData(crow, 1, estr);

    ui->btnUpdate->setEnabled(false);
    ui->btnReset->setEnabled(false);
    hdrGrid.setEnabled(true);

    setChanged(true);
}

void EditHdrDialog::on_edName_textChanged(const QString &arg1)
{
    onUserInput();
}

void EditHdrDialog::on_edDesc_textChanged(const QString &arg1)
{
    onUserInput();
}

void EditHdrDialog::on_edPos_textChanged(const QString &arg1)
{
    onUserInput();
}

void EditHdrDialog::on_btnDelItem_clicked()
{
    int row = hdrGrid.curRow();

    if (row < 0)
        return;

    hdrGrid.delRow(row);

    row = row + 1;
    if (row > hdrGrid.RowCount() - 1)
        row = hdrGrid.RowCount() - 1;

    hdrGrid.setCurCell(row);

    setChanged(true);
}

void EditHdrDialog::on_btnDiscard_clicked()
{
    FillHdrGrid();
    setChanged(false);
}

void EditHdrDialog::on_btnApply_clicked()
{
    if (changed) {
        TrHdrDefList *hdrDefList = TraceHdrsSet.List(curtype);

        TrHdrDefList List;
        TrHdrDef Hdr;

        List.setType(curtype);

        int la = hdrDefList->count();
        int l, n;

        for (n = l = 0; n < la; n++) {
            int edit = hdrDefList->item(n)->edit;
            if (!edit) {
                List.add(*hdrDefList->item(n));
            }
        }

        int row;

        for (row = 0; row < hdrGrid.RowCount(); row++) {
            Hdr.name = hdrGrid.Cell(row, 0);
            Hdr.desc = hdrGrid.Cell(row, 1);
            Hdr.edit = 1; // hdrGrid.Tag (row,0);
            Hdr.pos = hdrGrid.Tag(row, 0);
            Hdr.frmt = hdrGrid.Tag(row, 1);
            Hdr.expr = SeisTrExpr(hdrGrid.Data(row, 1));

            List.add(Hdr);
        }

        TraceHdrsSet[curtype] = List;

        emit changedEvent();

        setChanged(false);
    }
}

void EditHdrDialog::on_txtExpr_textChanged()
{
    if (busy)
        return;

    int row = hdrGrid.curRow();
    if (row < 0)
        return;

    QString estr = ui->txtExpr->document()->toPlainText().trimmed();

    if (estr != "") {
        SeisTrExpr expr(estr);
        if (expr.Error()) {
            ui->lbErr->setText("Bad expression");
        } else {
            ui->lbErr->setText("");
        }
        ui->cbForm->setCurrentIndex(7);
    } else {
        int frmt = hdrGrid.Tag(row, 1);
        int fidx = FormatList::index(frmt);
        ui->cbForm->setCurrentIndex(fidx);
    }

    onUserInput();
}

void EditHdrDialog::on_tabMode_currentChanged(int index)
{
    if (index == 0) {
        ui->hdrsBox->setTitle("Select Header to edit");
    } else {
        ui->hdrsBox->setTitle("Insert Header to expression");
    }
}

void EditHdrDialog::on_insBtn_clicked()
{
    int row = insGrid.curRow();
    if (row < 0)
        return;

    int pos = insGrid.Tag(row, 0);
    int frmt = insGrid.Tag(row, 1);

    QString fun =
        "H(" + QString::number(pos) + "," + QString::number(frmt) + ")";

    ui->txtExpr->insertPlainText(fun);
}
