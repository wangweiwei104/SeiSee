#ifndef METRICSDIALOG_H
#define METRICSDIALOG_H

#include <QDialog>
#include "sgyfile.h"
#include <QLineEdit>

class QThread;

namespace Ui {
class MetricsDialog;
}

class MetricsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetricsDialog(QWidget *parent = nullptr);
    ~MetricsDialog();

    void show();
    void reject() override;
    void shutdown();
    bool isCalculating() const { return running; }

    SeisFile* sf;      // Transient main-window input; captured independently when shown.
    QString*  savDir;  // 保存目录
    QString   curDir;  // 当前目录

signals:
    void stop();

private slots:
    void on_btnCalculate_clicked();
    void on_btnClose_clicked();
    void on_btnOpenFile1_clicked();    // 【新增】打开信号文件
    void on_btnOpenFile2_clicked();
    void on_btnSaveResults_clicked();

    void on_rbCalculation_toggled(bool checked);
    void on_rbEstimation_toggled(bool checked);
    void on_ckTrAll_clicked();
    void on_ckTmAll_clicked();

    void on_pathFile1_textChanged(const QString &arg1);  // 【新增】
    void on_pathFile2_textChanged(const QString &arg1);

    void x_metrics(QString results);
    void x_progr(int pers, QString mess);
    void x_fin(QString mess);

    void updateAvailableMetrics();

private:
    Ui::MetricsDialog *ui;

    QString  fname;      // Fixed signal file path
    QString  fname2;     // Shared noisy file path for calculation and estimation
    SeisFile* sf2;       // Owned noisy file reader
    SeisFile* signalFile = nullptr; // Independent snapshot of the main signal file.
    bool     running;
    bool     stopRequested;
    QThread* calculationThread = nullptr;
    QString completionMessage;

    void setRunning(bool active);
    void resetResults();
    void showMessage(const QString& title, const QString& message);
    SeisFile* primaryFile() const;
    void closeEvent(QCloseEvent* event) override;

    void FillLimits();
    void ShowProgress(QString mes, int pers);
    void UpdateUIMode();
    int  ReadTxt(QLineEdit* edt, bool& ok);

    bool openNoisyFile();
    void chooseNoisyFile();
    QString defaultReportPath() const;
};

#endif // METRICSDIALOG_H
