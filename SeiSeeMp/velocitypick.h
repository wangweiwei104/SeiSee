#ifndef VELOCITYPICK_H
#define VELOCITYPICK_H

#include <QColor>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QThread>
#include <QVector>
#include <QWaitCondition>
#include <limits>

struct VelocityPoint {
    int trace = -1;
    double distance = 0, timeMs = 0, displayTimeMs = 0;
};
struct VelocityPick {
    quint64 id = 0;
    bool hyperbola = true;
    double t0 = 0, velocity = 0, width = 1.5;
    int distanceMode = 0;
    QColor color = Qt::black;
    QString origin;
    VelocityPoint first, second;
};
struct VelocityTrace {
    double distance = std::numeric_limits<double>::quiet_NaN();
    double offset = 0, delayMs = 0;
};
struct VelocityGeometry {
    QHash<int, VelocityTrace> traces;
    int traceCount = 0;
    bool assumedMetres = false;
    bool contains(int trace) const;
    double distance(int trace, const VelocityPick &pick) const;
};
Q_DECLARE_METATYPE(VelocityGeometry)

struct VelocityGeometrySamples {
    QVector<int> indices;
    QVector<VelocityTrace> traces;
};
Q_DECLARE_METATYPE(VelocityGeometrySamples)

bool fitVelocity(const VelocityPoint &a, const VelocityPoint &b,
                 VelocityPick *pick, QString *error);
double velocityTime(const VelocityPick &pick, double distance);
bool readVelocityCsv(const QString &text, const QString &filename, int type,
                     const VelocityPick &defaults, QVector<VelocityPick> *out,
                     QString *error);
QString velocityCsv(const QVector<VelocityPick> &picks);

class VelocityGeometryWorker : public QThread
{
    Q_OBJECT
public:
    explicit VelocityGeometryWorker(QObject *parent = nullptr)
        : QThread(parent)
    {
    }
    QString path, key;
    int type = 1;
    void requestSamples(const QVector<int> &indices);
    void wake();
signals:
    void opened(int traceCount, bool assumedMetres);
    void samplesReady(VelocityGeometrySamples samples);
    void failed(QString message);
    void progress(int percent);

protected:
    void run() override;

private:
    QMutex requestMutex;
    QWaitCondition requestsAvailable;
    QList<QVector<int>> requests;
};

#endif
