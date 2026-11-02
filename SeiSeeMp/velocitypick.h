#ifndef VELOCITYPICK_H
#define VELOCITYPICK_H

#include <QColor>
#include <QThread>
#include <QVector>
#include <limits>

struct VelocityPoint {
    int trace = -1;
    double distance = 0, timeMs = 0, displayTimeMs = 0;
};
struct VelocityPick {
    quint64 id = 0;
    bool hyperbola = true;
    double t0 = 0, velocity = 0, width = 1.5, spacing = 25;
    int distanceMode = 0, gatherMode = 0;
    QColor color = Qt::black;
    QString origin;
    VelocityPoint first, second;
};
struct VelocityTrace {
    double distance = std::numeric_limits<double>::quiet_NaN();
    double offset = 0, delayMs = 0;
    int record = 0, source = 0, channel = 0;
    int firstRecord = 0, firstSource = 0, firstChannel = 0;
};
struct VelocityGeometry {
    QVector<VelocityTrace> traces;
    int autoGather = 4;
    bool assumedMetres = false;
    int first(int trace, int mode) const;
    double distance(int trace, const VelocityPick &pick) const;
};
Q_DECLARE_METATYPE(VelocityGeometry)

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
signals:
    void ready(VelocityGeometry geometry);
    void failed(QString message);
    void progress(int percent);

protected:
    void run() override;
};

#endif
