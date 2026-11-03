#include "velocitypick.h"
#include "fktransform.h"
#include "sgyfile.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

bool VelocityGeometry::contains(int trace) const
{
    return traces.contains(trace);
}
double VelocityGeometry::distance(int trace, const VelocityPick &pick) const
{
    const auto it = traces.constFind(trace);
    if (it == traces.constEnd())
        return std::numeric_limits<double>::quiet_NaN();
    return pick.distanceMode == 0 ? it->distance : it->offset;
}
bool fitVelocity(const VelocityPoint &a, const VelocityPoint &b,
                 VelocityPick *pick, QString *error)
{
    auto fail = [&](const QString &s) {
        if (error)
            *error = s;
        return false;
    };
    if (!std::isfinite(a.distance) || !std::isfinite(b.distance) ||
        !std::isfinite(a.timeMs) || !std::isfinite(b.timeMs))
        return fail(QObject::tr(
            "Distance/time is unavailable. Choose valid geometry."));
    const long double x1 = a.distance, x2 = b.distance, t1 = a.timeMs / 1000.,
                      t2 = b.timeMs / 1000.;
    if (std::abs(x2 - x1) <=
        1e-10L * qMax(1.L, qMax(std::abs(x1), std::abs(x2))))
        return fail(QObject::tr("The two points have the same distance."));
    double v, t0;
    if (pick->hyperbola) {
        if (t1 < 0 || t2 < 0)
            return fail(QObject::tr("Hyperbola times must be nonnegative "
                                    "relative to the raw sample origin."));
        const long double dx2 = (x2 - x1) * (x2 + x1);
        if (std::abs(dx2) < 1e-16L)
            return fail(
                QObject::tr("The two points have the same squared distance."));
        const long double k = (t2 - t1) * (t2 + t1) / dx2;
        const long double intercept = t1 * t1 - k * x1 * x1;
        if (k <= 0 || intercept < -1e-12L * qMax(1.L, t1 * t1))
            return fail(QObject::tr(
                "These points do not define a real velocity hyperbola."));
        v = double(1 / std::sqrt(k));
        t0 = double(1000 * std::sqrt(qMax(0.L, intercept)));
    } else {
        if (std::abs(t2 - t1) < 1e-12L)
            return fail(QObject::tr(
                "Equal times do not define a finite line velocity."));
        v = double((x2 - x1) / (t2 - t1));
        t0 = double(1000 * (t1 - x1 / v));
    }
    if (!std::isfinite(v) || !std::isfinite(t0) || v == 0)
        return fail(QObject::tr(
            "The velocity calculation is outside the numeric range."));
    pick->velocity = v;
    pick->t0 = t0;
    pick->first = a;
    pick->second = b;
    return true;
}
double velocityTime(const VelocityPick &pick, double distance)
{
    if (!std::isfinite(distance) || !std::isfinite(pick.velocity) ||
        pick.velocity == 0)
        return std::numeric_limits<double>::quiet_NaN();
    return pick.hyperbola
               ? 1000 * std::hypot(pick.t0 / 1000., distance / pick.velocity)
               : pick.t0 + 1000 * distance / pick.velocity;
}

bool readVelocityCsv(const QString &text, const QString &filename, int type,
                     const VelocityPick &defaults, QVector<VelocityPick> *out,
                     QString *error)
{
    auto fail = [&](const QString &s) {
        if (error)
            *error = s;
        return false;
    };
    QVector<QStringList> rows;
    QStringList row;
    QString cell;
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        QChar c = text[i];
        if (c == QChar(0xfeff) && i == 0)
            continue;
        if (c == '"') {
            if (quoted && i + 1 < text.size() && text[i + 1] == '"') {
                cell += c;
                ++i;
            } else
                quoted = !quoted;
        } else if (!quoted && (c == ',' || c == '\r' || c == '\n')) {
            row.append(cell.trimmed());
            cell.clear();
            if (c != ',') {
                if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                    ++i;
                if (!row.join("").isEmpty())
                    rows.append(row);
                row.clear();
            }
        } else
            cell += c;
    }
    if (quoted)
        return fail(QObject::tr("CSV contains an unclosed quote."));
    row.append(cell.trimmed());
    if (!row.join("").isEmpty())
        rows.append(row);
    if (rows.size() < 2)
        return fail(QObject::tr("CSV contains no velocity rows."));
    if (rows.size() > 10001)
        return fail(QObject::tr("Use at most 10000 velocity rows."));
    QStringList header = rows.takeFirst();
    for (auto &s : header)
        s = s.toLower().remove(QRegularExpression("\\s"));
    auto column = [&](const QStringList &names) {
        for (const auto &n : names) {
            int i = header.indexOf(n);
            if (i >= 0)
                return i;
        }
        return -1;
    };
    const int ti = column({"t0(ms)", "t0ms", "t0", "t0(s)", "t0s"}),
              vi = column({"velocity(m/s)", "velocity", "v(m/s)", "v"}),
              mi = column({"type", "mode"}), di = column({"distance_source"});
    if (ti < 0 || vi < 0)
        return fail(
            QObject::tr("CSV requires t0(ms) and velocity(m/s) columns."));
    auto kind = [](QString s) {
        s = s.toLower();
        return s == "line" || s == "linear" || s == "x" ? 1
               : s == "curve" || s == "hyperbola" || s == "hyperbolic" ||
                       s == "c"
                   ? 2
                   : 0;
    };
    const QString base = QFileInfo(filename).fileName().toLower();
    const int inferred = base.contains("line") ? 1
                         : (base.contains("curve") || base.contains("hyper"))
                             ? 2
                             : 0;
    QVector<VelocityPick> result;
    for (int r = 0; r < rows.size(); ++r) {
        const auto &cols = rows[r];
        auto field = [&](int c) {
            return c >= 0 && c < cols.size() ? cols[c] : QString();
        };
        const int mode = type ? type : mi >= 0 ? kind(field(mi)) : inferred;
        if (!mode)
            return fail(QObject::tr("Row %1: specify CSV Type, a type column, "
                                    "or a line/curve filename.")
                            .arg(r + 2));
        bool ok1 = false, ok2 = false;
        VelocityPick p = defaults;
        p.first = VelocityPoint();
        p.second = VelocityPoint();
        p.hyperbola = mode == 2;
        p.t0 = field(ti).toDouble(&ok1);
        p.velocity = field(vi).toDouble(&ok2);
        if (header[ti] == "t0(s)" || header[ti] == "t0s")
            p.t0 *= 1000;
        if (!ok1 || !ok2 || !std::isfinite(p.t0) ||
            !std::isfinite(p.velocity) || p.velocity == 0 ||
            (p.hyperbola && (p.t0 < 0 || p.velocity < 0)))
            return fail(
                QObject::tr("Row %1: invalid time or velocity.").arg(r + 2));
        if (di >= 0) {
            QString d = field(di).toLower();
            p.distanceMode = d == "coordinates" ? 0 : d == "offset" ? 1 : -1;
            if (p.distanceMode < 0)
                return fail(
                    QObject::tr("Row %1: invalid distance_source.").arg(r + 2));
        }
        p.origin = filename;
        result.append(p);
    }
    *out = result;
    return true;
}
QString velocityCsv(const QVector<VelocityPick> &picks)
{
    QStringList lines = {"type,t0(ms),velocity(m/s),distance_source"};
    for (const auto &p : picks)
        lines.append(QString("%1,%2,%3,%4")
                         .arg(p.hyperbola ? "hyperbola" : "line")
                         .arg(QString::number(p.t0, 'g', 17))
                         .arg(QString::number(p.velocity, 'g', 17))
                         .arg(p.distanceMode == 0 ? "coordinates" : "offset"));
    return lines.join("\r\n") + "\r\n";
}

void VelocityGeometryWorker::requestSamples(const QVector<int> &indices)
{
    if (indices.isEmpty())
        return;
    QMutexLocker lock(&requestMutex);
    requests.append(indices);
    requestsAvailable.wakeOne();
}

void VelocityGeometryWorker::wake()
{
    requestsAvailable.wakeAll();
}

void VelocityGeometryWorker::run()
{
    try {
        if (type != 1 && type != 2)
            throw std::runtime_error(
                "Velocity picking currently supports SEG-Y and SU.");
        std::unique_ptr<SgyFile> file(type == 2 ? new SuFile : new SgyFile);
        if (spectrumSourceKey(path) != key)
            throw std::runtime_error("Source changed before reading headers.");
        file->setFname(path);
        file->setActive(true);
        if (!file->Active() || file->Nt() < 1)
            throw std::runtime_error("Open a valid file.");
        const int measurement =
            int(iFmt(2, file->binHed()->buf(), 55, file->Swap()));
        const double unit = measurement == 2 ? .3048 : 1;
        emit opened(file->Nt(), measurement != 1 && measurement != 2);
        while (!isInterruptionRequested()) {
            QVector<int> requested;
            {
                QMutexLocker lock(&requestMutex);
                while (requests.isEmpty() && !isInterruptionRequested())
                    requestsAvailable.wait(&requestMutex);
                if (isInterruptionRequested())
                    return;
                requested = requests.takeFirst();
            }
            QSet<int> unique;
            VelocityGeometrySamples samples;
            samples.indices.reserve(requested.size());
            samples.traces.reserve(requested.size());
            for (int i : requested) {
                if (i >= 0 && i < file->Nt())
                    unique.insert(i);
            }
            int done = 0;
            for (int i : unique) {
                if (isInterruptionRequested())
                    return;
                byte *raw = nullptr;
                int count = file->ReadRawTrace(raw, i, true);
                std::unique_ptr<byte[]> bytes(raw);
                if (count < 240)
                    throw std::runtime_error("Could not read a trace header.");
                auto number = [&](int pos, int size) {
                    return iFmt(size, raw, pos, file->Swap());
                };
                VelocityTrace t;
                t.offset = number(37, 4) * unit;
                t.delayMs = number(109, 2);
                const double scalar = number(71, 2),
                             multiplier = scalar > 0   ? scalar
                                          : scalar < 0 ? -1. / scalar
                                                       : 1.;
                const int coords = int(number(89, 2));
                if (coords == 0 || coords == 1)
                    t.distance =
                        std::hypot(number(81, 4) - number(73, 4),
                                   number(85, 4) - number(77, 4)) *
                        multiplier * unit;
                samples.indices.append(i);
                samples.traces.append(t);
                ++done;
                if (done % 64 == 0)
                    emit progress(int(100LL * done / unique.size()));
            }
            if (!samples.indices.isEmpty())
                emit samplesReady(samples);
            emit progress(100);
        }
    } catch (const std::exception &e) {
        if (!isInterruptionRequested())
            emit failed(QString::fromUtf8(e.what()));
    }
}
