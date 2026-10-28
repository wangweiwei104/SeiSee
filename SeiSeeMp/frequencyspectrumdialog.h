#ifndef FREQUENCYSPECTRUMDIALOG_H
#define FREQUENCYSPECTRUMDIALOG_H

#include "spectrumdialog.h"
#include "fktransform.h"
#include <QColor>
#include <QMap>

namespace Ui {
    class frequencydialog;
}
class FrequencyPlot;
class QLabel;
class QProgressBar;

struct FrequencyCurve {
    QString id, path, sourceKey;
    FkSpectrum spectrum;
};
struct FrequencyLineStyle {
    QColor color;
    double width;
    FrequencyLineStyle(QColor value = Qt::blue, double lineWidth = 1)
        : color(value)
        , width(lineWidth)
    {
    }
};

class FrequencySpectrumDialog : public SpectrumDialog
{
    Q_OBJECT
public:
    explicit FrequencySpectrumDialog(SeisFile *file, const QString &directory,
                                     QWidget *parent = nullptr);
    ~FrequencySpectrumDialog() override;
    void setSource(SeisFile *file) override;
    void setInitialRange(int firstTrace, int lastTrace, double startMs,
                         double endMs) override;
    void shutdown() override;
    bool isCalculating() const override
    {
        return batchRunning;
    }
    bool isSourceCurrent() const override;
    bool savePng(const QString &filename, int dpi = 0) override;
    // File selection and tests use the same validation/deduplication path.
    void addComparisonFiles(const QStringList &paths);
    QVector<FrequencyCurve> spectra() const;
    void setMainTrace(int traceNumber);

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private:
    struct Request {
        QString id, path, sourceKey;
        int fileType = 1, first = 0, last = 0, sampleFirst = 0, sampleLast = 0;
        double dt = 0;
    };
    Ui::frequencydialog *ui;
    FrequencyPlot *plot;
    QLabel *fileStatus, *rangeStatus, *compareStatus, *messageStatus;
    QProgressBar *progress;
    FkWorker *worker = nullptr;
    QVector<Request> requests;
    QVector<FrequencyCurve> pending;
    QStringList comparisons, errors;
    QMap<QString, FrequencyLineStyle> styles;
    QString path, directory, computedKey, computedRange;
    int traceCount = 0, sampleCount = 0, fileType = 1, jobIndex = 0;
    int mainTrace = 0;
    double dt = 0;
    bool batchRunning = false, cancelled = false, primaryFailed = false;
    bool restoring = false, haveDisplayRange = false, batchSingle = true;
    void begin();
    void launchNext();
    void finishBatch(bool success);
    void setInputMode();
    void updateSourceState();
    void updateComparisonControls();
    void updateLineStyle(bool primary);
    void refreshStyles();
    void applyDisplay();
    void autoAmplitude();
    void fullFrequency();
    void loadSettings();
    void saveSettings();
    void exportImage(bool capture);
    void toast(const QString &text);
};

#endif
