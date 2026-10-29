#ifndef SPECTRUMTICKS_H
#define SPECTRUMTICKS_H

#include <QStringList>
#include <QVector>
#include <algorithm>
#include <cmath>

struct SpectrumTicks {
    QVector<double> values;
    QStringList labels;
    double step = 0;
};

// Choose regular multiples inside the requested view, without moving its limits
// or changing FFT bin coordinates. Generate by index to avoid accumulated
// drift.
inline SpectrumTicks spectrumTicks(double low, double high, int intervals)
{
    SpectrumTicks ticks;
    const double span = high - low;
    if (!std::isfinite(low) || !std::isfinite(high) || !std::isfinite(span) ||
        span <= 0)
        return ticks;
    const double raw = span / qBound(2, intervals, 12);
    if (raw <= 0)
        return ticks;
    const double power = std::pow(10.0, std::floor(std::log10(raw)));
    if (!std::isfinite(power) || power <= 0)
        return ticks;
    const double fraction = raw / power;
    double multiple = 1;
    for (double candidate : {2.0, 2.5, 5.0, 10.0})
        if (std::abs(candidate - fraction) < std::abs(multiple - fraction))
            multiple = candidate;
    ticks.step = multiple * power;
    if (!std::isfinite(ticks.step) || ticks.step <= 0)
        return ticks;
    const double first = std::ceil(low / ticks.step - 1e-10);
    const double tolerance = ticks.step * 1e-9;
    const int decimals =
        qBound(0, 1 - int(std::floor(std::log10(ticks.step))), 15);
    for (int i = 0; i < 64; ++i) {
        double value = (first + i) * ticks.step;
        if (!std::isfinite(value) || value > high + tolerance)
            break;
        if (value < low - tolerance)
            continue;
        if (std::abs(value) < tolerance)
            value = 0;
        // Suppress duplicate coordinates when a large offset approaches the
        // resolution of double precision.
        if (!ticks.values.isEmpty() && value <= ticks.values.last())
            continue;
        QString label;
        if (value != 0 && (std::abs(value) >= 1e9 || std::abs(value) < 1e-5)) {
            const int digits =
                qBound(4,
                       int(std::ceil(std::log10(std::abs(value)) -
                                     std::log10(ticks.step))) +
                           2,
                       17);
            label = QString::number(value, 'g', digits);
        } else {
            label = QString::number(value, 'f', decimals);
            if (label.contains('.')) {
                while (label.endsWith('0'))
                    label.chop(1);
                if (label.endsWith('.'))
                    label.chop(1);
            }
        }
        ticks.values.append(value);
        ticks.labels.append(label);
    }
    return ticks;
}

#endif
