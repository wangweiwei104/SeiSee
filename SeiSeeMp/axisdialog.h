#ifndef AXISDIALOG_H
#define AXISDIALOG_H

#include "hdrdef.h"
#include "mystringtable.h"
#include <QDialog>
#include <QList>
#include <QRgb>

namespace Ui {
    class AxisDialog;
}

class AxisDialog : public QDialog
{
    Q_OBJECT

private:
    MyStringTable selHdrGrid;
    MyStringTable aviHdrGrid;

    QStringList hdrList;

    bool changed;

public:
    explicit AxisDialog(QWidget *parent = 0);
    ~AxisDialog();

    TrHdrDefList *hdrDefList;
    CkListSet *ckList;
    double dT;
    int timeUnit;
    bool showTimeUnit;
    bool tL;
    int fontSize;
    int colorMapIndex;
    bool colorMapReversed;

    void show();
    static QStringList colorMapNames();
    static QList<QRgb> colorMapPalette(int index, bool reversed = false);
    static QList<QRgb> positiveGrayPalette();

private slots:
    void on_closeButton_pressed();

    void on_addBtn_pressed();

    void on_applyButton_pressed();

    void on_okButton_pressed();

    void on_delBtn_clicked();

    void on_dellAllBtn_clicked();

    void on_upBtn_clicked();

    void on_downBtn_clicked();

    void on_dTCbx_editTextChanged(const QString &arg1);

    void on_timeUnitCbx_currentIndexChanged(int index);
    void on_showTimeUnitCk_toggled(bool checked);
    void on_ckTimLines_toggled(bool checked);
    void on_fontSizeSpinBox_valueChanged(int value);
    void on_colorMapCbx_currentIndexChanged(int index);
    void on_colorMapReverseCk_toggled(bool checked);

private:
    Ui::AxisDialog *ui;

    void FillAviGrid();
    void FillForm();
    void FillTimeStepChoices();
    void FillColorMapPreviews();

signals:
    void changedEvent();
};

#endif // AXISDIALOG_H
