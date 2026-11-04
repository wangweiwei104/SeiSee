#include "axisdialog.h"
#include "gfxstyle.h"
#include "hdrdef.h"
#include "fkcolormaps.h"
#include "sgyfile.h"
#include "ui_axisdialog.h"

#include <QDebug>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <algorithm>

namespace {

struct ColorStop
{
    int position;
    QColor color;
};

QList<QRgb> makePalette(const QList<ColorStop> &stops)
{
    QList<QRgb> palette;
    if (stops.isEmpty())
        return palette;

    for (int i = 0; i < 256; ++i) {
        int nextStop = 1;
        while (nextStop < stops.size() &&
               i > stops[nextStop].position) {
            ++nextStop;
        }

        if (nextStop == stops.size()) {
            palette.append(stops.last().color.rgb());
            continue;
        }

        const ColorStop &left = stops[qMax(0, nextStop - 1)];
        const ColorStop &right = stops[nextStop];
        const double range = right.position - left.position;
        const double fraction =
            range > 0 ? (i - left.position) / range : 0;
        palette.append(QColor(
            qRound(left.color.red() +
                   (right.color.red() - left.color.red()) * fraction),
            qRound(left.color.green() +
                   (right.color.green() - left.color.green()) * fraction),
            qRound(left.color.blue() +
                   (right.color.blue() - left.color.blue()) * fraction))
                           .rgb());
    }

    return palette;
}

QList<ColorStop> threeColorStops(const QColor &first, const QColor &middle,
                                 const QColor &last)
{
    return {{0, first}, {128, middle}, {255, last}};
}

} // namespace

AxisDialog::AxisDialog(QWidget *parent)
    : QDialog(parent)
    , changed(false)
    , hdrDefList(nullptr)
    , ckList(nullptr)
    , dT(0.1)
    , timeUnit(0)
    , showTimeUnit(false)
    , tL(false)
    , fontSize(GfxStyle::LabelFontPointSize)
    , colorMapIndex(0)
    , colorMapReversed(false)
    , ui(new Ui::AxisDialog)
{
    ui->setupUi(this);
    ui->colorMapCbx->addItems(colorMapNames());
    FillColorMapPreviews();
    // ui->colorMapCbx->setMaxVisibleItems(ui->colorMapCbx->count());
    // ui->timeAxisOptionsLayout->setColumnStretch(2, 1);
    selHdrGrid.setRowCount(0);
    selHdrGrid.setColCount(1);
    selHdrGrid.verticalHeader()->setVisible(false);
    selHdrGrid.horizontalHeader()->setVisible(false);
    selHdrGrid.setColWidthInCharacters(
        0, GfxStyle::SelectedHeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);

    QVBoxLayout *selGridLayout = new QVBoxLayout;
    selGridLayout->setMargin(GfxStyle::CompactLayoutMargin);
    selGridLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    selGridLayout->addWidget(&selHdrGrid);
    ui->selHdrsBox->setLayout(selGridLayout);

    aviHdrGrid.setRowCount(0);
    aviHdrGrid.setColCount(2);
    aviHdrGrid.verticalHeader()->setVisible(false);
    aviHdrGrid.horizontalHeader()->setVisible(false);

    aviHdrGrid.setColWidthInCharacters(
        0, GfxStyle::AvailableHeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    aviHdrGrid.setColWidthToStretch(
        1, GfxStyle::TableColumnHorizontalPadding);

    QVBoxLayout *aviGridLayout = new QVBoxLayout;
    aviGridLayout->setMargin(GfxStyle::CompactLayoutMargin);
    aviGridLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    aviGridLayout->addWidget(&aviHdrGrid);
    ui->aviHdrsBox->setLayout(aviGridLayout);
}

QStringList AxisDialog::colorMapNames()
{
    return QStringList() << "RWB"
                         << "Grey"
                         << "Single Gray"
                         << "Inferno"
                         << "Jet"
                         << "Plasma"
                         << "RdBu"
                         << "Vik"
                         << "Viridis"
                         << "Black-White-Brown"
                         << "Black-White-Red"
                         << "Block"
                         << "Blue-White-Yellow"
                         << "Red-White-Black (SU)";
}

QList<QRgb> AxisDialog::colorMapPalette(int index, bool reversed)
{
    QList<QRgb> palette;
    switch (index) {
    case 0:
        palette = makePalette(threeColorStops(Qt::red, Qt::white, Qt::blue));
        break;
    case 1:
        palette = makePalette({{0, Qt::white}, {255, Qt::black}});
        break;
    case 2:
        palette = positiveGrayPalette();
        break;
    case 3:
        for (int i = 0; i < 256; ++i)
            palette.append(fkColorMaps[2][i]);
        break;
    case 4:
        for (int i = 0; i < 256; ++i)
            palette.append(fkColorMaps[0][i]);
        break;
    case 5:
        palette = makePalette({{0, QColor(13, 8, 135)},
                               {64, QColor(126, 3, 168)},
                               {128, QColor(204, 71, 120)},
                               {192, QColor(248, 149, 64)},
                               {255, QColor(240, 249, 33)}});
        break;
    case 6:
        for (int i = 0; i < 256; ++i)
            palette.append(fkColorMaps[4][i]);
        break;
    case 7:
        palette = makePalette({{0, QColor(0, 18, 75)},
                               {43, QColor(45, 80, 120)},
                               {85, QColor(130, 155, 168)},
                               {127, QColor(242, 241, 238)},
                               {170, QColor(218, 157, 137)},
                               {212, QColor(166, 72, 66)},
                               {255, QColor(83, 20, 27)}});
        break;
    case 8:
        for (int i = 0; i < 256; ++i)
            palette.append(fkColorMaps[1][i]);
        break;
    case 9:
        palette = makePalette(
            threeColorStops(Qt::black, Qt::white, QColor(150, 75, 0)));
        break;
    case 10:
        palette = makePalette(threeColorStops(Qt::black, Qt::white, Qt::red));
        break;
    case 11: {
        QList<ColorStop> stops;
        const QColor colors[] = {
            Qt::black, Qt::blue, Qt::cyan, Qt::green,
            Qt::yellow, Qt::red, Qt::white
        };
        for (int i = 0; i < 7; ++i)
            stops.append({i * 255 / 6, colors[i]});
        palette = makePalette(stops);
        break;
    }
    case 12:
        palette = makePalette(threeColorStops(Qt::blue, Qt::white, Qt::yellow));
        break;
    case 13:
        palette = makePalette({{0, QColor(204, 13, 13)},
                               {115, QColor(230, 217, 166)},
                               {128, QColor(191, 230, 179)},
                               {140, QColor(166, 230, 242)},
                               {255, Qt::black}});
        break;
    default:
        palette = makePalette(threeColorStops(Qt::red, Qt::white, Qt::blue));
        break;
    }

    if (reversed)
        std::reverse(palette.begin(), palette.end());
    return palette;
}

QList<QRgb> AxisDialog::positiveGrayPalette()
{
    return makePalette({{0, Qt::white},
                        {128, Qt::white},
                        {255, Qt::black}});
}

void AxisDialog::FillColorMapPreviews()
{
    const int iconWidth = 120;
    const int iconHeight = 14;
    for (int i = 0; i < ui->colorMapCbx->count(); ++i) {
        QPixmap preview(iconWidth, iconHeight);
        QPainter painter(&preview);
        const QList<QRgb> palette =
            colorMapPalette(i, colorMapReversed);
        for (int x = 0; x < iconWidth; ++x) {
            const int paletteIndex =
                x * (palette.size() - 1) / (iconWidth - 1);
            painter.setPen(QColor::fromRgb(palette[paletteIndex]));
            painter.drawLine(x, 0, x, iconHeight);
        }
        ui->colorMapCbx->setItemIcon(i, QIcon(preview));
    }
}

AxisDialog::~AxisDialog()
{
    delete ui;
}

void AxisDialog::on_closeButton_pressed()
{
    close();
}

void AxisDialog::FillAviGrid()
{
    int n, i;
    int l = hdrDefList->count();

    for (n = i = 0; n < l; n++) {
        QString name = hdrDefList->item(n)->name;

        if (hdrList.contains(name))
            continue;
        i++;
    }

    aviHdrGrid.setRowCount(i);

    for (n = i = 0; n < l; n++) {
        QString desc = hdrDefList->item(n)->desc;
        QString name = hdrDefList->item(n)->name;

        if (hdrList.contains(name))
            continue;

        aviHdrGrid.setCell(i, 0, name);
        aviHdrGrid.setCell(i, 1, desc);

        i++;
    }
}

void AxisDialog::FillForm()
{
    hdrList = ckList->List();

    hdrDefList = TraceHdrsSet.List();

    FillAviGrid();

    int n;

    int l = hdrList.count();
    selHdrGrid.setRowCount(l);

    for (n = 0; n < l; n++) {
        selHdrGrid.setCell(n, 0, hdrList[n]);
    }

    ui->timeUnitCbx->setCurrentIndex(timeUnit);
    FillTimeStepChoices();

    {
        const QSignalBlocker blocker(ui->colorMapCbx);
        ui->colorMapCbx->setCurrentIndex(
            qBound(0, colorMapIndex, ui->colorMapCbx->count() - 1));
    }
    ui->colorMapReverseCk->setChecked(colorMapReversed);
    ui->showTimeUnitCk->setChecked(showTimeUnit);
    ui->ckTimLines->setChecked(tL);
    ui->fontSizeSpinBox->setValue(fontSize);
    changed = false;
}

void AxisDialog::show()
{
    FillForm();
    QDialog::show();
}

void AxisDialog::on_applyButton_pressed()
{
    if (!changed)
        return;

    int n;

    hdrList.clear();
    for (n = 0; n < selHdrGrid.RowCount(); n++) {
        hdrList.append(selHdrGrid.Cell(n, 0));
    }
    ckList->setList(hdrList);
    emit changedEvent();
}

void AxisDialog::on_okButton_pressed()
{
    on_applyButton_pressed();
    close();
}

void AxisDialog::on_addBtn_pressed()
{
    int row = aviHdrGrid.curRow();

    if (row >= 0) {
        QString name = aviHdrGrid.Cell(row, 0);

        int nr = selHdrGrid.RowCount();
        selHdrGrid.setRowCount(nr + 1);
        selHdrGrid.setCell(nr, 0, name);
        aviHdrGrid.delRow(row);
        hdrList.append(name);
        changed = true;
    }
}

void AxisDialog::on_delBtn_clicked()
{
    int row = selHdrGrid.curRow();

    if (row >= 0) {
        QString name = selHdrGrid.Cell(row, 0);
        hdrList.removeAll(name);
        selHdrGrid.delRow(row);
        FillAviGrid();
        changed = true;
    }
}

void AxisDialog::on_dellAllBtn_clicked()
{
    if (hdrList.count() < 1)
        return;

    hdrList.clear();
    selHdrGrid.setRowCount(0);
    FillAviGrid();
    changed = true;
}

void AxisDialog::on_upBtn_clicked()
{
    int row = selHdrGrid.curRow();

    if (row < 1)
        return;

    QString v1 = selHdrGrid.Cell(row - 1, 0);
    QString v2 = selHdrGrid.Cell(row, 0);
    selHdrGrid.setCell(row - 1, 0, v2);
    selHdrGrid.setCell(row, 0, v1);

    selHdrGrid.setCurCell(row - 1);
    changed = true;
}

void AxisDialog::on_downBtn_clicked()
{
    int row = selHdrGrid.curRow();

    if (row > selHdrGrid.RowCount() - 2)
        return;

    QString v1 = selHdrGrid.Cell(row, 0);
    QString v2 = selHdrGrid.Cell(row + 1, 0);
    selHdrGrid.setCell(row, 0, v2);
    selHdrGrid.setCell(row + 1, 0, v1);

    selHdrGrid.setCurCell(row + 1);
    changed = true;
}

void AxisDialog::on_dTCbx_editTextChanged(const QString &arg1)
{
    Q_UNUSED(arg1);

    QString txt = ui->dTCbx->currentText();
    bool ok;

    const double displayStep = txt.toDouble(&ok);
    if (!ok || displayStep <= 0)
        return;

    const double stepInSeconds =
        displayStep / (timeUnit == 0 ? 1000.0 : 1.0);
    if (!qFuzzyCompare(dT, stepInSeconds)) {
        dT = stepInSeconds;
        changed = true;
    }
}

void AxisDialog::on_timeUnitCbx_currentIndexChanged(int index)
{
    if (timeUnit == index)
        return;

    timeUnit = index;
    FillTimeStepChoices();
    changed = true;
}

void AxisDialog::on_showTimeUnitCk_toggled(bool checked)
{
    showTimeUnit = checked;
    changed = true;
}

void AxisDialog::on_ckTimLines_toggled(bool checked)
{
    tL = checked;
    changed = true;
}

void AxisDialog::on_fontSizeSpinBox_valueChanged(int value)
{
    if (fontSize == value)
        return;

    fontSize = value;
    changed = true;
}

void AxisDialog::on_colorMapCbx_currentIndexChanged(int index)
{
    if (colorMapIndex == index)
        return;

    colorMapIndex = index;
    changed = true;
}

void AxisDialog::on_colorMapReverseCk_toggled(bool checked)
{
    if (colorMapReversed == checked)
        return;

    colorMapReversed = checked;
    FillColorMapPreviews();
    changed = true;
}

void AxisDialog::FillTimeStepChoices()
{
    static const int stepsInMilliseconds[] = {
        10, 20, 25, 50, 100, 200, 500, 1000, 2000, 5000
    };

    const QSignalBlocker blocker(ui->dTCbx);
    ui->dTCbx->clear();
    for (int stepMs : stepsInMilliseconds) {
        const double step =
            timeUnit == 0 ? stepMs : stepMs / 1000.0;
        ui->dTCbx->addItem(
            QString::number(step, 'g', timeUnit == 0 ? 6 : 3));
    }

    const double displayStep = dT * (timeUnit == 0 ? 1000.0 : 1.0);
    ui->dTCbx->setEditText(
        QString::number(displayStep, 'g', timeUnit == 0 ? 6 : 6));
}
