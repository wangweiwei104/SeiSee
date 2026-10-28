#ifndef SPECTRUMDIALOG_H
#define SPECTRUMDIALOG_H

#include <QDialog>

class SeisFile;

// Shared main-window contract for FK and trace/region frequency analysis.
class SpectrumDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SpectrumDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
    }
    virtual void setSource(SeisFile *file) = 0;
    virtual void setInitialRange(int firstTrace, int lastTrace, double startMs,
                                 double endMs) = 0;
    virtual void shutdown() = 0;
    virtual bool isCalculating() const = 0;
    virtual bool isSourceCurrent() const = 0;
    virtual bool savePng(const QString &filename, int dpi = 0) = 0;
signals:
    void currentViewRequested();
    void selectionRequested(bool enabled);
    void spectrumUpdated();

protected:
    static bool savePlotImage(QWidget *plot, const QString &filename, int dpi);
};

#endif
