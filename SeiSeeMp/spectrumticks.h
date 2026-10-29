#ifndef SPECTRUMTICKS_H
#define SPECTRUMTICKS_H

#include <QFontMetrics>
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
inline SpectrumTicks spectrumTicks(double low, double high, int intervals,
                                   int fixedDecimals = -1)
{
    SpectrumTicks ticks;
    const double span = high - low;
    if (!std::isfinite(low) || !std::isfinite(high) || !std::isfinite(span) ||
        span <= 0)
        return ticks;
    const double resolution =
        fixedDecimals < 0 ? 0 : std::pow(10., -fixedDecimals);
    const double raw = qMax(span / qBound(2, intervals, 12), resolution);
    if (raw <= 0)
        return ticks;
    const double power = std::pow(10.0, std::floor(std::log10(raw)));
    if (!std::isfinite(power) || power <= 0)
        return ticks;
    const double fraction = raw / power;
    double multiple = 1;
    for (double candidate : {2.0, 2.5, 5.0, 10.0})
        if ((fixedDecimals < 0 ||
             std::abs(candidate * power / resolution -
                      std::round(candidate * power / resolution)) < 1e-7) &&
            std::abs(candidate - fraction) < std::abs(multiple - fraction))
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
        if (fixedDecimals >= 0) {
            label = QString::number(value, 'f', fixedDecimals);
        } else if (value != 0 &&
                   (std::abs(value) >= 1e9 || std::abs(value) < 1e-5)) {
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

// Shared by the frequency and FK plots: account for available pixels, font,
// numeric span and the actual neighboring label widths.
inline SpectrumTicks spectrumAxisTicks(double low, double high, int pixels,
                                       const QFontMetrics &font,
                                       bool horizontal, int fixedDecimals = -1)
{
    const int sampleWidth =
        qMax(font.horizontalAdvance(
                 QString::number(low, 'f', qMax(0, fixedDecimals))),
             font.horizontalAdvance(
                 QString::number(high, 'f', qMax(0, fixedDecimals))));
    int intervals = qBound(
        2,
        pixels / (horizontal ? qMax(60, sampleWidth + 24) : font.height() * 3),
        12);
    for (;;) {
        auto ticks = spectrumTicks(low, high, intervals, fixedDecimals);
        bool overlap = false;
        for (int i = 1; i < ticks.values.size(); ++i) {
            const double space =
                (ticks.values[i] - ticks.values[i - 1]) / (high - low) * pixels;
            const double needed =
                horizontal ? (font.horizontalAdvance(ticks.labels[i - 1]) +
                              font.horizontalAdvance(ticks.labels[i])) /
                                     2. +
                                 12
                           : font.height() * 2.;
            overlap |= space < needed;
        }
        if (!overlap || intervals <= 2)
            return ticks;
        --intervals;
    }
}

#endif
