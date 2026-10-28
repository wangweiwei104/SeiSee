#ifndef FKTRANSFORM_H
#define FKTRANSFORM_H

#include <QThread>
#include <QVector>
#include <functional>

// Positive frequency rows (even-length Nyquist omitted), fftshift k columns.
struct FkSpectrum {
    QVector<float> amplitude;
    int nf = 0, nk = 0;
    double df = 0, dk = 0, k0 = 0;
};
Q_DECLARE_METATYPE(FkSpectrum)

FkSpectrum calculateFk(const QVector<float> &traces, int nx, int nt, double dt,
                       double dx, bool preprocess = false,
                       const std::function<bool()> &cancelled = {});
FkSpectrum calculateFrequency(const QVector<float> &traces, int nx, int nt,
                              double dt,
                              const std::function<bool()> &cancelled = {});

// No shared SeisFile: the worker opens and owns its own raw-data reader.
class FkWorker : public QThread
{
    Q_OBJECT
public:
    explicit FkWorker(QObject *parent = nullptr)
        : QThread(parent)
    {
    }
    QString path, sourceKey;
    int fileType = 1, first = 0, last = 0, sampleFirst = 0, sampleLast = 0;
    double dx = 25;
    double requiredSampleInterval = 0;
    bool frequencyOnly = false;
signals:
    void spectrumReady(FkSpectrum result);
    void failed(QString message);
    void progress(int percent);

protected:
    void run() override;
};

QString spectrumSourceKey(const QString &path);

#endif
