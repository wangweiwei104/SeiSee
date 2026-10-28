#ifndef FKSPECTRUMDIALOG_H
#define FKSPECTRUMDIALOG_H

#include "spectrumdialog.h"
#include "fktransform.h"

class SeisFile;
class FkPlot;
class QLabel;
class QProgressBar;
namespace Ui {
    class fkdialog;
}

class FkSpectrumDialog : public SpectrumDialog
{
    Q_OBJECT
public:
    explicit FkSpectrumDialog(SeisFile *file, const QString &directory,
                              QWidget *parent = nullptr,
                              bool frequencyOnly = false);
    ~FkSpectrumDialog() override;
    void setSource(SeisFile *file);
    void setInitialRange(int firstTrace, int lastTrace, double startMs,
                         double endMs);
    void shutdown();
    bool isCalculating() const
    {
        return worker != nullptr;
    }
    bool isSourceCurrent() const;
    bool savePng(const QString &filename, int dpi = 0);

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private:
    Ui::fkdialog *ui;
    FkPlot *plot;
    FkWorker *worker = nullptr;
    QLabel *fileStatus, *rangeStatus, *messageStatus;
    QProgressBar *progress;
    QString path, directory, computedKey;
    int fileType = 1, traceCount = 0, sampleCount = 0;
    double dt = 0;
    bool frequencyOnly = false, restoring = false, haveDisplayRange = false;
    void begin();
    void receive(const FkSpectrum &result);
    bool applyDisplay(bool autoColors = false);
    void updateSourceState();
    void updateRangeStatus();
    void loadSettings();
    void saveSettings();
    void exportImage(bool capture);
    void toast(const QString &text);
};

#endif
