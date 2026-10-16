#ifndef FKSPECTRUMDIALOG_H
#define FKSPECTRUMDIALOG_H

#include <QDialog>
#include "workthread.h"
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QGroupBox;
class CustomCheckBox;
class FkPlot;

class FkSpectrumDialog : public QDialog {
    Q_OBJECT
public:
    explicit FkSpectrumDialog(SeisFile* file, const QString& directory, QWidget* parent=nullptr);
    ~FkSpectrumDialog() override;
    void shutdown();
    void reject() override;
    bool isCalculating() const { return worker!=nullptr; }
    bool savePng(const QString& filename);
    void setInitialRange(int firstTrace, int lastTrace, double startMs, double endMs);
private:
    QString path, directory;
    int fileType;
    double dt;
    FkWorker* worker=nullptr;
    bool haveDisplayRange=false;
    bool frequencyEdited=false, wavenumberEdited=false, colorsEdited=false;
    QGroupBox* inputGroup;
    QSpinBox *first, *last, *imageWidth, *imageHeight;
    QDoubleSpinBox *start, *end, *spacing, *fmin, *fmax, *kmin, *kmax, *cmin, *cmax;
    QComboBox *palette, *scale, *interpolation;
    CustomCheckBox* preprocess;
    QPushButton *calculate, *save;
    QLabel* status;
    QProgressBar* progress;
    FkPlot* plot;
    void begin();
    void receive(const FkSpectrum& result);
    bool applyDisplay(bool autoColors);
    void saveImage();
    void saveImageSize();
    void closeEvent(QCloseEvent* event) override;
};
#endif
