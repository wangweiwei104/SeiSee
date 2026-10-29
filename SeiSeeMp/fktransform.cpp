#include "fktransform.h"
#include "cwp.h"
#include "sgyfile.h"
#include <QDateTime>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
    // CWP handles its supported lengths directly. Bluestein preserves the
    // original sample count for all other lengths, matching numpy.fft.fft
    // without zero padding.
    class FkForwardFft
    {
        int n, m;
        std::vector<complex> chirp, kernel, scratch;

    public:
        explicit FkForwardFft(int size)
            : n(size)
            , m(npfa(size) == size ? 0 : npfa(2 * size - 1))
        {
            if (!m)
                return;
            chirp.resize(n);
            kernel.resize(m);
            scratch.resize(m);
            const double pi = std::acos(-1.0);
            for (int j = 0; j < n; ++j) {
                const double phase = pi * std::fmod(double(j) * j, 2.0 * n) / n;
                chirp[j] =
                    cmplx(float(std::cos(phase)), float(-std::sin(phase)));
                kernel[j] = conjg(chirp[j]);
                if (j)
                    kernel[m - j] = kernel[j];
            }
            pfacc(-1, m, kernel.data());
        }
        void forward(std::vector<complex> &values)
        {
            if (!m) {
                pfacc(-1, n, values.data());
                return;
            }
            std::fill(scratch.begin(), scratch.end(), cmplx(0, 0));
            for (int j = 0; j < n; ++j)
                scratch[j] = cmul(values[j], chirp[j]);
            pfacc(-1, m, scratch.data());
            for (int j = 0; j < m; ++j)
                scratch[j] = cmul(scratch[j], kernel[j]);
            pfacc(1, m, scratch.data());
            for (int j = 0; j < n; ++j)
                values[j] = crmul(cmul(scratch[j], chirp[j]), 1.0f / m);
        }
        void inverse(std::vector<complex> &values)
        {
            for (auto &z : values)
                z.i = -z.i;
            forward(values);
            for (auto &z : values) {
                z.r /= n;
                z.i /= -n;
            }
        }
    };
} // namespace

FkSpectrum calculateFk(const QVector<float> &traces, int nx, int nt, double dt,
                       double dx, bool preprocess,
                       const std::function<bool()> &cancelled)
{
    if (nx < 2 || nt < 4 || nx > 180180 || nt > 180180 ||
        qint64(nx) * nt > 8000000 || traces.size() != qint64(nx) * nt ||
        !std::isfinite(dt) || !std::isfinite(dx) || dt <= 0 || dx <= 0)
        throw std::runtime_error(
            "Invalid FK range. Use 2+ traces, 4+ samples, at most 8 million "
            "cells and 180180 per axis.");
    if (preprocess && nx < 3)
        throw std::runtime_error("The spatial Hann window needs at least 3 "
                                 "traces. Disable preprocessing for 2 traces.");
    auto check = [&]() {
        if (cancelled && cancelled())
            throw std::runtime_error("Cancelled.");
    };
    FkSpectrum out;
    out.nk = nx;
    out.nf = nt / 2 + 1;
    out.df = 1.0 / (nt * dt);
    out.dk = 1.0 / (nx * dx);
    out.k0 = -(nx / 2) * out.dk;
    out.frequencyLimit = .5 / dt;
    out.wavenumberLimit = .5 / dx;
    out.amplitude.resize(out.nf * nx);
    QVector<float> data = traces;
    for (float v : data)
        if (!std::isfinite(v))
            throw std::runtime_error("Input contains NaN or infinity.");
    if (preprocess) {
        // Match preprocess_gather: trace means, then sample means, then 2-D
        // Hann.
        for (int x = 0; x < nx; ++x) {
            check();
            double mean = 0;
            for (int t = 0; t < nt; ++t)
                mean += data[x * nt + t];
            mean /= nt;
            for (int t = 0; t < nt; ++t)
                data[x * nt + t] -= float(mean);
        }
        for (int t = 0; t < nt; ++t) {
            double mean = 0;
            for (int x = 0; x < nx; ++x)
                mean += data[x * nt + t];
            mean /= nx;
            for (int x = 0; x < nx; ++x)
                data[x * nt + t] -= float(mean);
        }
        const double pi = std::acos(-1.0);
        QVector<float> wt(nt);
        for (int t = 0; t < nt; ++t)
            wt[t] = float(.5 - .5 * std::cos(2 * pi * t / (nt - 1)));
        for (int x = 0; x < nx; ++x) {
            check();
            float wx = float(.5 - .5 * std::cos(2 * pi * x / (nx - 1)));
            for (int t = 0; t < nt; ++t)
                data[x * nt + t] *= wx * wt[t];
        }
    }
    std::vector<complex> temporal(qint64(nx) * out.nf), line(nt);
    FkForwardFft timeFft(nt), spaceFft(nx);
    for (int x = 0; x < nx; ++x) {
        check();
        for (int t = 0; t < nt; ++t)
            line[t] = cmplx(data[x * nt + t], 0);
        timeFft.forward(line);
        for (int f = 0; f < out.nf; ++f)
            temporal[x * out.nf + f] = line[f];
    }
    data.clear();
    line.resize(nx);
    for (int f = 0; f < out.nf; ++f) {
        check();
        for (int x = 0; x < nx; ++x)
            line[x] = temporal[x * out.nf + f];
        spaceFft.forward(line);
        for (int k = 0; k < nx; ++k) {
            complex z = line[(k + (nx + 1) / 2) % nx];
            float value = std::hypot(z.r, z.i);
            if (!std::isfinite(value))
                throw std::runtime_error(
                    "FFT amplitude overflow. Reduce input amplitude or range.");
            out.amplitude[f * nx + k] = value;
        }
    }
    return out;
}

FkSpectrum calculateFrequency(const QVector<float> &traces, int nx, int nt,
                              double dt, const std::function<bool()> &cancelled)
{
    if (nx < 1 || nt < 4 || nt > 180180 || qint64(nx) * nt > 8000000 ||
        traces.size() != qint64(nx) * nt || !std::isfinite(dt) || dt <= 0)
        throw std::runtime_error("Invalid frequency range.");
    FkSpectrum result;
    result.nf = nt / 2 + 1;
    result.nk = 1;
    result.df = 1.0 / (nt * dt);
    result.frequencyLimit = .5 / dt;
    result.amplitude.fill(0, result.nf);
    FkForwardFft fft(nt);
    std::vector<complex> line(nt);
    for (int x = 0; x < nx; ++x) {
        if (cancelled && cancelled())
            throw std::runtime_error("Cancelled.");
        for (int t = 0; t < nt; ++t) {
            const float value = traces[x * nt + t];
            if (!std::isfinite(value))
                throw std::runtime_error("Input contains NaN or infinity.");
            line[t] = cmplx(value, 0);
        }
        fft.forward(line);
        for (int f = 0; f < result.nf; ++f) {
            const float value = std::hypot(line[f].r, line[f].i) / nx;
            if (!std::isfinite(value))
                throw std::runtime_error("FFT amplitude overflow.");
            result.amplitude[f] += value;
            if (!std::isfinite(result.amplitude[f]))
                throw std::runtime_error("Spectrum overflow.");
        }
    }
    return result;
}

QString spectrumSourceKey(const QString &path)
{
    if (path.isEmpty())
        return QString();
    const QFileInfo info(path);
    QString name = info.canonicalFilePath();
    if (name.isEmpty())
        name = info.absoluteFilePath();
#ifdef Q_OS_WIN
    name = name.toCaseFolded();
#endif
    return name + QLatin1Char('|') + QString::number(info.size()) +
           QLatin1Char('|') +
           QString::number(info.lastModified().toMSecsSinceEpoch());
}

double spectrumAmplitudeValue(double amplitude, bool decibels)
{
    return decibels ? 20 * std::log10(qMax(amplitude, 1e-12)) : amplitude;
}

void FkWorker::run()
{
    try {
        std::unique_ptr<SeisFile> reader(
            fileType == 2   ? static_cast<SeisFile *>(new SuFile)
            : fileType == 3 ? static_cast<SeisFile *>(new CstFile)
                            : static_cast<SeisFile *>(new SgyFile));
        if (spectrumSourceKey(path) != sourceKey)
            throw std::runtime_error(
                "Source changed before reading. Click Calculate again.");
        reader->setFname(path);
        reader->setActive(true);
        if (requiredSampleInterval > 0 &&
            std::abs(reader->Si() - requiredSampleInterval) >
                requiredSampleInterval * 1e-9)
            throw std::runtime_error(
                "Sample interval differs from the main file. Comparison "
                "requires the same sampling; no resampling is applied.");
        const qint64 nx = qint64(last) - first + 1;
        const qint64 nt = qint64(sampleLast) - sampleFirst + 1;
        if (!reader->Active() || first < 0 || last >= reader->Nt() ||
            sampleFirst < 0 || sampleLast >= reader->Ns() ||
            nx < (frequencyOnly ? 1 : 2) || nt < 4 || nx > 180180 ||
            nt > 180180 || nx * nt > 8000000)
            throw std::runtime_error("Invalid input range. Use at most 8 "
                                     "million samples and 180180 per axis.");
        QVector<float> data(int(nx * nt), 0);
        for (int x = 0; x < nx; ++x) {
            if (isInterruptionRequested())
                return;
            const float *trace = reader->Rs(first + x);
            if (!trace)
                throw std::runtime_error("Could not read a trace.");
            std::copy(trace + sampleFirst, trace + sampleLast + 1,
                      data.begin() + x * int(nt));
            emit progress(40 * (x + 1) / int(nx));
        }
        const auto cancelled = [this]() { return isInterruptionRequested(); };
        const FkSpectrum result =
            frequencyOnly ? calculateFrequency(data, int(nx), int(nt),
                                               reader->Si(), cancelled)
                          : calculateFk(data, int(nx), int(nt), reader->Si(),
                                        dx, false, cancelled);
        if (spectrumSourceKey(path) != sourceKey)
            throw std::runtime_error(
                "Source changed during calculation. Result discarded.");
        if (!isInterruptionRequested()) {
            emit progress(100);
            emit spectrumReady(result);
        }
    } catch (const std::exception &error) {
        if (!isInterruptionRequested())
            emit failed(QString::fromLocal8Bit(error.what()));
    }
}
