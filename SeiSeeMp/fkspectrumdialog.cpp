#include "fkspectrumdialog.h"
#include "ui_fkspectrumdialog.h"
#include "fkcolormaps.h"
#include "gfxstyle.h"
#include "gfxselectionband.h"
#include "seisfile.h"
#include "Toast.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>
#include <climits>

class FkPlot : public QWidget
{
public:
    FkSpectrum spectrum;
    QString caption;
    double flo = 0, fhi = 1, klo = -1, khi = 1, clo = 0, chi = 1;
    int color = 0;
    bool logarithmic = false, smooth = true, stale = false,
         frequencyOnly = false;
    bool windowZoom = false, dragging = false;
    QPoint origin;
    QRectF dragView;
    GfxSelectionBand *rubberBand;
    QVector<QRectF> history;
    std::function<void(double, double, double, double)> viewChanged;

    explicit FkPlot(QWidget *parent, bool frequency)
        : QWidget(parent)
        , frequencyOnly(frequency)
    {
        setObjectName("fkPlot");
        setMinimumSize(480, 260);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFont(QFont(GfxStyle::LabelFontFamily,
                      int(GfxStyle::LabelFontPointSize)));
        rubberBand = new GfxSelectionBand(this);
    }
    QRectF dataRect() const
    {
        int fh = fontMetrics().height();
        double left =
            qMax(72, fontMetrics().horizontalAdvance("-0.00000") + 32);
        double right =
            frequencyOnly
                ? 25
                : qMax(135, fontMetrics().horizontalAdvance("-0.00000") + 70);
        double top = qMax(30, fh + 12);
        return QRectF(left, top, qMax(50.0, width() - left - right),
                      qMax(50.0, height() - top - 3 * fh - 20));
    }
    QRectF view() const
    {
        return QRectF(klo, flo, khi - klo, fhi - flo);
    }
    void setView(QRectF range, bool remember = true)
    {
        if (spectrum.amplitude.isEmpty())
            return;
        auto constrain = [](double a, double span, double low, double high) {
            span = qBound((high - low) * 1e-6, span, high - low);
            return QPair<double, double>(qBound(low, a, high - span), span);
        };
        auto f =
            constrain(range.y(), range.height(), 0, spectrum.frequencyLimit);
        auto k = frequencyOnly ? qMakePair(0.0, 1.0)
                               : constrain(range.x(), range.width(),
                                           -spectrum.wavenumberLimit,
                                           spectrum.wavenumberLimit);
        if (remember && range != view()) {
            if (history.size() >= 100)
                history.removeFirst();
            history.append(view());
        }
        if (viewChanged)
            viewChanged(f.first, f.first + f.second, k.first,
                        k.first + k.second);
    }
    void zoom(double factor, double horizontal = .5, double vertical = .5)
    {
        QRectF r = view();
        setView(QRectF(r.x() + r.width() * (1 - factor) * horizontal,
                       r.y() + r.height() * (1 - factor) * vertical,
                       r.width() * factor, r.height() * factor));
    }
    QPointF coordinate(const QPoint &p) const
    {
        const QRectF box = dataRect();
        if (frequencyOnly)
            return QPointF((box.bottom() - p.y()) / box.height(),
                           flo + (p.x() - box.left()) / box.width() *
                                     (fhi - flo));
        return QPointF(klo + (p.x() - box.left()) / box.width() * (khi - klo),
                       fhi - (p.y() - box.top()) / box.height() * (fhi - flo));
    }
    double value(int f, int k) const
    {
        double a = spectrum.amplitude[f * spectrum.nk + k];
        return spectrumAmplitudeValue(a, logarithmic);
    }
    QRgb rgb(double v) const
    {
        int index = qBound(
            0, int(255 * qBound(0.0, (v - clo) / (chi - clo), 1.0)), 255);
        QRgb c = fkColorMaps[color][index];
        return stale ? qRgb(qGray(c), qGray(c), qGray(c)) : c;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (spectrum.amplitude.isEmpty() || !dataRect().contains(event->pos()))
            return;
        if (windowZoom && event->button() == Qt::LeftButton) {
            origin = event->pos();
            rubberBand->setGeometry(QRect(origin, QSize()));
            rubberBand->show();
        } else if (event->button() == Qt::RightButton) {
            if (windowZoom) {
                windowZoom = false;
                rubberBand->hide();
                unsetCursor();
                return;
            }
            dragging = true;
            origin = event->pos();
            dragView = view();
            if (history.size() >= 100)
                history.removeFirst();
            history.append(view());
            setCursor(Qt::OpenHandCursor);
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (rubberBand->isVisible())
            rubberBand->setGeometry(QRect(origin, event->pos())
                                        .normalized()
                                        .intersected(dataRect().toRect()));
        else if (dragging) {
            QPoint delta = event->pos() - origin;
            QRectF range = dragView.translated(
                -delta.x() / dataRect().width() * dragView.width(),
                frequencyOnly
                    ? -delta.x() / dataRect().width() * dragView.height()
                    : delta.y() / dataRect().height() * dragView.height());
            setView(range, false);
        }
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (rubberBand->isVisible() && event->button() == Qt::LeftButton) {
            QRect box = rubberBand->geometry();
            rubberBand->hide();
            windowZoom = false;
            unsetCursor();
            if (box.width() > 3 && box.height() > 3)
                setView(QRectF(coordinate(box.topLeft()),
                               coordinate(box.bottomRight()))
                            .normalized());
        }
        if (event->button() == Qt::RightButton) {
            dragging = false;
            unsetCursor();
        }
    }
    void wheelEvent(QWheelEvent *event) override
    {
        if (dataRect().contains(event->pos()) && !windowZoom) {
            const QPointF p = coordinate(event->pos());
            zoom(event->angleDelta().y() > 0 ? .8 : 1.25,
                 (p.x() - klo) / (khi - klo), (p.y() - flo) / (fhi - flo));
        }
        event->accept();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        const QRectF box = dataRect();
        const int fh = p.fontMetrics().height();
        p.setPen(Qt::black);
        p.drawText(QRectF(box.left(), 2, box.width(), fh + 6), Qt::AlignCenter,
                   p.fontMetrics().elidedText(caption, Qt::ElideMiddle,
                                              int(box.width())));
        if (spectrum.amplitude.isEmpty())
            p.drawText(box, Qt::AlignCenter,
                       tr("Select an input range and click Calculate"));
        else if (frequencyOnly) {
            QPainterPath line;
            bool begun = false;
            for (int f = 0; f < spectrum.nf; ++f) {
                double hz = f * spectrum.df;
                if (hz < flo || hz > fhi)
                    continue;
                QPointF point(box.left() +
                                  (hz - flo) / (fhi - flo) * box.width(),
                              box.bottom() - (value(f, 0) - clo) / (chi - clo) *
                                                 box.height());
                if (!begun) {
                    line.moveTo(point);
                    begun = true;
                } else
                    line.lineTo(point);
            }
            p.save();
            p.setClipRect(box);
            p.setPen(QPen(stale ? Qt::gray : Qt::blue, 1));
            p.drawPath(line);
            p.restore();
        } else {
            double ratio = qMax(1.0, std::hypot(p.deviceTransform().m11(),
                                                p.deviceTransform().m12()));
            QImage image(qMax(1, int(box.width() * ratio)),
                         qMax(1, int(box.height() * ratio)),
                         QImage::Format_RGB32);
            image.setDevicePixelRatio(ratio);
            for (int y = 0; y < image.height(); ++y) {
                double hz = fhi - (fhi - flo) * (y + .5) / image.height();
                if (hz < 0 || hz > spectrum.frequencyLimit) {
                    auto row = reinterpret_cast<QRgb *>(image.scanLine(y));
                    std::fill(row, row + image.width(), qRgb(255, 255, 255));
                    continue;
                }
                double fi =
                    qBound(0.0, hz / spectrum.df, double(spectrum.nf - 1));
                int f0 = int(fi), f1 = qMin(f0 + 1, spectrum.nf - 1);
                double fy = fi - f0;
                QRgb *row = reinterpret_cast<QRgb *>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    double k = klo + (khi - klo) * (x + .5) / image.width();
                    if (k < -spectrum.wavenumberLimit ||
                        k > spectrum.wavenumberLimit) {
                        row[x] = qRgb(255, 255, 255);
                        continue;
                    }
                    double ki =
                        std::fmod((k - spectrum.k0) / spectrum.dk, spectrum.nk);
                    if (ki < 0)
                        ki += spectrum.nk;
                    if (smooth) {
                        int k0 = int(ki), k1 = (k0 + 1) % spectrum.nk;
                        double fx = ki - k0;
                        double a =
                            value(f0, k0) * (1 - fx) + value(f0, k1) * fx;
                        double b =
                            value(f1, k0) * (1 - fx) + value(f1, k1) * fx;
                        row[x] = rgb(a * (1 - fy) + b * fy);
                    } else
                        row[x] = rgb(value(int(std::round(fi)),
                                           int(std::round(ki)) % spectrum.nk));
                }
            }
            p.drawImage(box, image);
        }
        p.setPen(Qt::black);
        p.drawRect(box);
        const int ticks = qBound(
            2,
            int(qMin(box.width() /
                         (fontMetrics().horizontalAdvance("-0.0000") + 20),
                     box.height() / (3 * fh))),
            4);
        for (int i = 0; i <= ticks; ++i) {
            double a = double(i) / ticks, x = box.left() + a * box.width(),
                   y = box.bottom() - a * box.height();
            p.drawLine(QPointF(x, box.bottom()), QPointF(x, box.bottom() + 5));
            p.drawLine(QPointF(box.left() - 5, y), QPointF(box.left(), y));
            p.drawText(QRectF(x - box.width() / ticks / 2, box.bottom() + 7,
                              box.width() / ticks, fh + 3),
                       Qt::AlignCenter,
                       QString::number(frequencyOnly ? flo + a * (fhi - flo)
                                                     : klo + a * (khi - klo),
                                       'g', 4));
            p.drawText(QRectF(25, y - fh / 2, box.left() - 33, fh + 3),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(frequencyOnly ? clo + a * (chi - clo)
                                                     : flo + a * (fhi - flo),
                                       'g', 4));
        }
        p.drawText(QRectF(box.left(), height() - fh - 9, box.width(), fh + 3),
                   Qt::AlignCenter,
                   frequencyOnly ? tr("Frequency (Hz)")
                                 : tr("Wavenumber (cycles/m)"));
        p.save();
        p.translate(15, box.center().y());
        p.rotate(-90);
        p.drawText(QRectF(-box.height() / 2, -fh / 2, box.height(), fh + 3),
                   Qt::AlignCenter,
                   frequencyOnly
                       ? (logarithmic ? tr("Amplitude (dB)") : tr("Amplitude"))
                       : tr("Frequency (Hz)"));
        p.restore();
        if (!frequencyOnly) {
            QRectF bar(box.right() + 24, box.top(), 18, box.height());
            QLinearGradient gradient(bar.bottomLeft(), bar.topLeft());
            for (int i = 0; i < 256; ++i)
                gradient.setColorAt(
                    i / 255.0,
                    QColor::fromRgb(rgb(clo + (chi - clo) * i / 255)));
            p.fillRect(bar, gradient);
            p.drawRect(bar);
            p.drawText(QRectF(bar.left() - 4, bar.top() - fh - 4,
                              width() - bar.left(), fh + 3),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       logarithmic ? tr("dB") : tr("Amplitude"));
            for (int i = 0; i <= ticks; ++i)
                p.drawText(
                    QRectF(bar.right() + 4,
                           bar.bottom() - i * bar.height() / ticks - fh / 2,
                           width() - bar.right() - 5, fh + 3),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QString::number(clo + (chi - clo) * i / ticks, 'g', 4));
        }
        if (stale && !spectrum.amplitude.isEmpty()) {
            p.setPen(Qt::darkGray);
            p.drawText(box.adjusted(8, 5, -8, -5),
                       Qt::AlignTop | Qt::AlignRight,
                       tr("Different source - cached spectrum"));
        }
    }
};

FkSpectrumDialog::FkSpectrumDialog(SeisFile *file, const QString &dir,
                                   QWidget *parent, bool frequency)
    : SpectrumDialog(parent)
    , ui(new Ui::fkdialog)
    , directory(dir)
    , frequencyOnly(frequency)
{
    ui->setupUi(this);
    for (auto layout : ui->frame->findChildren<QLayout *>()) {
        layout->setSpacing(4);
        layout->setContentsMargins(4, 4, 4, 4);
    }
    setObjectName(frequencyOnly ? "FrequencySpectrumDialog"
                                : "FkSpectrumDialog");
    setWindowTitle(frequencyOnly ? tr("Frequency Spectrum")
                                 : tr("F-K Spectrum"));
    setWindowModality(Qt::NonModal);
    setModal(false);
    setSizeGripEnabled(true);
    qRegisterMetaType<FkSpectrum>("FkSpectrum");
    for (auto button : findChildren<QPushButton *>()) {
        button->setFixedSize(GfxStyle::StandardIconButtonSize,
                             GfxStyle::StandardIconButtonSize);
        button->setIconSize(
            QSize(GfxStyle::StandardIconSize, GfxStyle::StandardIconSize));
        button->setAutoDefault(false);
    }
    for (auto spin : {ui->fkFirst, ui->fkLast}) {
        spin->setRange(1, INT_MAX);
        spin->setKeyboardTracking(false);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(45);
    }
    for (auto spin : {ui->fkStart, ui->fkEnd, ui->fkSpacing}) {
        spin->setDecimals(3);
        spin->setRange(0, 1e12);
        spin->setKeyboardTracking(false);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(45);
    }
    ui->fkSpacing->setRange(.000001, 1e6);
    ui->fkSpacing->setDecimals(6);
    ui->fkSpacing->setValue(25);
    for (auto spin : {ui->fkFmin, ui->fkFmax, ui->fkKmin, ui->fkKmax,
                      ui->fkCmin, ui->fkCmax}) {
        spin->setRange(-1e30, 1e30);
        spin->setDecimals(9);
        spin->setKeyboardTracking(false);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(45);
    }
    ui->fkFmin->setMinimum(0);
    ui->fkFmax->setMinimum(0);
    ui->fkFmax->setValue(1);
    ui->fkKmin->setValue(-.02);
    ui->fkKmax->setValue(.02);
    ui->fkCmax->setValue(1);
    ui->fkPalette->clear();
    ui->fkPalette->addItems({"Jet", "Viridis", "Inferno", "Gray", "RdBu"});
    ui->fkScale->clear();
    ui->fkScale->addItems({"Abs", "Log10"});
    ui->fkScale->setToolTip(
        tr("Log10: amplitude in dB = 20*log10(max(amplitude, 1e-12)), "
           "reference amplitude 1."));
    ui->fkInterpolation->clear();
    ui->fkInterpolation->addItems({tr("Smooth"), tr("Nearest")});
    ui->fkFontSize->setRange(6, 24);
    ui->fkFontSize->setValue(9);
    ui->fkSelect->setCheckable(true);
    auto tooltip = [](QWidget *widget, const QString &text) {
        widget->setToolTip(text);
        widget->setAccessibleName(text);
    };
    tooltip(ui->fkAllTraces, tr("All traces (1-based, inclusive)"));
    tooltip(ui->fkAllTime, tr("All time samples (ms from sample zero)"));
    tooltip(ui->fkCurrentView,
            tr("Use the main window's current visible range"));
    tooltip(ui->fkSelect,
            tr("Select / resize an input window on the seismic view"));
    tooltip(ui->fkClearSelection,
            tr("Cancel selection and clear the rectangle"));
    tooltip(ui->fkCalculate, tr("Calculate / cancel spectrum (raw samples)"));
    tooltip(ui->fkFullK, tr("Show all wavenumbers"));
    tooltip(ui->fkFullF, tr("Show all frequencies"));
    tooltip(ui->fkAutoColor, tr("Automatic color range (P1 / P99.5)"));
    tooltip(ui->fkZoomAll, tr("Zoom all"));
    tooltip(ui->fkZoomIn, tr("Zoom in"));
    tooltip(ui->fkZoomOut, tr("Zoom out"));
    tooltip(ui->fkZoomWindow, tr("Window zoom"));
    tooltip(ui->fkZoomPrevious, tr("Previous zoom"));
    tooltip(ui->fkCapture, tr("Capture current spectrum"));
    tooltip(ui->fkExport, tr("Export high-resolution spectrum"));
    ui->fkSpacing->setToolTip(tr("Actual uniform trace spacing in metres; "
                                 "irregular spacing is not supported."));
    plot = new FkPlot(ui->plotFrame, frequencyOnly);
    auto canvas = new QVBoxLayout(ui->plotFrame);
    canvas->setContentsMargins(0, 0, 0, 0);
    canvas->addWidget(plot);
    fileStatus = new QLabel;
    fileStatus->setObjectName("fkFileStatus");
    fileStatus->setMinimumWidth(180);
    fileStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rangeStatus = new QLabel;
    rangeStatus->setObjectName("fkRangeStatus");
    messageStatus = new QLabel(tr("Ready"));
    messageStatus->setObjectName("fkStatus");
    for (auto label : {fileStatus, rangeStatus, messageStatus})
        label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    progress = new QProgressBar;
    progress->setObjectName("fkProgress");
    progress->setFixedWidth(110);
    progress->setValue(0);
    ui->fkStatusBar->setSizeGripEnabled(false);
    ui->fkStatusBar->addWidget(fileStatus, 2);
    ui->fkStatusBar->addWidget(rangeStatus, 2);
    ui->fkStatusBar->addWidget(messageStatus, 1);
    ui->fkStatusBar->addPermanentWidget(progress);
    if (frequencyOnly) {
        for (auto w : QList<QWidget *>(
                 {ui->fkKmin, ui->fkKmax, ui->fkFullK, ui->label_6, ui->label_8,
                  ui->fkSpacing, ui->label_10, ui->fkPalette, ui->label_19,
                  ui->fkInterpolation, ui->label_21}))
            w->setEnabled(false);
    }
    setSource(file);
    if (traceCount > 0) {
        ui->fkLast->setValue(traceCount);
        ui->fkEnd->setValue((sampleCount - 1) * dt * 1000);
    }
    loadSettings();
    plot->setFont(QFont(GfxStyle::LabelFontFamily, ui->fkFontSize->value()));
    connect(ui->fkFontSize, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() {
                plot->setFont(
                    QFont(GfxStyle::LabelFontFamily, ui->fkFontSize->value()));
                plot->update();
                saveSettings();
            });
    plot->viewChanged = [this](double f0, double f1, double k0, double k1) {
        restoring = true;
        ui->fkFmin->setValue(f0);
        ui->fkFmax->setValue(f1);
        ui->fkKmin->setValue(k0);
        ui->fkKmax->setValue(k1);
        restoring = false;
        applyDisplay();
        saveSettings();
    };
    connect(ui->fkAllTraces, &QPushButton::clicked, this, [this]() {
        if (traceCount > 0) {
            ui->fkFirst->setValue(1);
            ui->fkLast->setValue(traceCount);
        }
    });
    connect(ui->fkAllTime, &QPushButton::clicked, this, [this]() {
        if (sampleCount > 0) {
            ui->fkStart->setValue(0);
            ui->fkEnd->setValue((sampleCount - 1) * dt * 1000);
        }
    });
    connect(ui->fkCurrentView, &QPushButton::clicked, this,
            &FkSpectrumDialog::currentViewRequested);
    connect(ui->fkSelect, &QPushButton::toggled, this,
            &FkSpectrumDialog::selectionRequested);
    connect(ui->fkClearSelection, &QPushButton::clicked, this, [this]() {
        ui->fkSelect->setChecked(false);
        emit selectionRequested(false);
    });
    connect(ui->fkCalculate, &QPushButton::clicked, this, [this]() {
        if (worker) {
            worker->requestInterruption();
            ui->fkCalculate->setEnabled(false);
            messageStatus->setText(tr("Cancelling..."));
        } else
            begin();
    });
    connect(ui->fkFullK, &QPushButton::clicked, this, [this]() {
        if (plot->spectrum.amplitude.isEmpty())
            return;
        plot->setView(QRectF(-plot->spectrum.wavenumberLimit, plot->flo,
                             2 * plot->spectrum.wavenumberLimit,
                             plot->fhi - plot->flo));
    });
    connect(ui->fkFullF, &QPushButton::clicked, this, [this]() {
        plot->setView(QRectF(plot->klo, 0, plot->khi - plot->klo,
                             plot->spectrum.frequencyLimit));
    });
    connect(ui->fkAutoColor, &QPushButton::clicked, this, [this]() {
        applyDisplay(true);
        saveSettings();
    });
    connect(ui->fkZoomAll, &QPushButton::clicked, this, [this]() {
        const auto &s = plot->spectrum;
        plot->setView(QRectF(frequencyOnly ? 0 : -s.wavenumberLimit, 0,
                             frequencyOnly ? 1 : 2 * s.wavenumberLimit,
                             s.frequencyLimit));
    });
    connect(ui->fkZoomIn, &QPushButton::clicked, this,
            [this]() { plot->zoom(1.0 / 1.5); });
    connect(ui->fkZoomOut, &QPushButton::clicked, this,
            [this]() { plot->zoom(1.5); });
    connect(ui->fkZoomWindow, &QPushButton::clicked, this, [this]() {
        plot->windowZoom = true;
        plot->setCursor(
            QCursor(QPixmap(":/images/SelectionBlackCurs.xpm"), 8, 8));
    });
    connect(ui->fkZoomPrevious, &QPushButton::clicked, this, [this]() {
        if (!plot->history.isEmpty())
            plot->setView(plot->history.takeLast(), false);
    });
    connect(ui->fkCapture, &QPushButton::clicked, this,
            [this]() { exportImage(true); });
    connect(ui->fkExport, &QPushButton::clicked, this,
            [this]() { exportImage(false); });
    for (auto spin : {ui->fkFmin, ui->fkFmax, ui->fkKmin, ui->fkKmax,
                      ui->fkCmin, ui->fkCmax})
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this]() {
                    if (!restoring) {
                        haveDisplayRange = true;
                        applyDisplay();
                        saveSettings();
                    }
                });
    for (auto combo : {ui->fkPalette, ui->fkScale, ui->fkInterpolation})
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() {
                    if (!restoring) {
                        applyDisplay(sender() == ui->fkScale);
                        saveSettings();
                    }
                });
}

void FkSpectrumDialog::setSource(SeisFile *file)
{
    const bool active = file && file->Active();
    path = active ? QString::fromLocal8Bit(file->Fname().c_str()) : QString();
    fileType = active ? (file->typeName() == "SEG-Y" ? 1 : file->typeIdx()) : 1;
    traceCount = active ? file->Nt() : 0;
    sampleCount = active ? file->Ns() : 0;
    dt = active ? file->Si() : 0;
    // Deliberately do not change input/display control values or start a
    // worker.
    updateSourceState();
}

bool FkSpectrumDialog::isSourceCurrent() const
{
    return !computedKey.isEmpty() && !path.isEmpty() &&
           computedKey == spectrumSourceKey(path);
}

void FkSpectrumDialog::updateSourceState()
{
    plot->stale = !isSourceCurrent();
    plot->update();
    fileStatus->setText(tr("Current File: %1").arg(QFileInfo(path).fileName()));
    fileStatus->setToolTip(path);
    ui->fkCalculate->setEnabled(worker || !path.isEmpty());
    if (!worker)
        messageStatus->setText(plot->spectrum.amplitude.isEmpty()
                                   ? tr("Click Calculate")
                               : plot->stale ? tr("Cached / other source")
                                             : tr("Cached / current source"));
}

void FkSpectrumDialog::setInitialRange(int firstTrace, int lastTrace,
                                       double startMs, double endMs)
{
    if (worker)
        return;
    ui->fkFirst->setValue(firstTrace);
    ui->fkLast->setValue(lastTrace);
    ui->fkStart->setValue(startMs);
    ui->fkEnd->setValue(endMs);
    messageStatus->setText(tr("Range selected; click Calculate"));
}

void FkSpectrumDialog::toast(const QString &text)
{
    Toast *notice = new Toast(this);
    notice->setDuration(6000);
    notice->setTitle(tr("Spectrum"));
    notice->setText(text);
    notice->setMaximumWidth(qMax(300, width() / 2));
    notice->applyPreset(ToastPreset::WARNING);
    notice->show();
}

void FkSpectrumDialog::begin()
{
    if (path.isEmpty() || !std::isfinite(dt) || dt <= 0)
        return;
    double a = std::ceil(ui->fkStart->value() / (dt * 1000) - 1e-7);
    double b = std::floor(ui->fkEnd->value() / (dt * 1000) + 1e-7);
    qint64 nx = qint64(ui->fkLast->value()) - ui->fkFirst->value() + 1;
    double nt = b - a + 1;
    if (nx < (frequencyOnly ? 1 : 2) || nt < 4 ||
        ui->fkLast->value() > traceCount || a < 0 || b >= sampleCount ||
        nx > 180180 || nt > 180180 || nx * nt > 8000000) {
        toast(tr("Select a valid range within this file: %1+ traces, 4+ "
                 "samples, at most 8 million samples and 180180 per axis.")
                  .arg(frequencyOnly ? 1 : 2));
        return;
    }
    if (nx * nt >= 1000000)
        toast(tr("Large selection (%1 million samples). Calculation may take "
                 "time; you can cancel it.")
                  .arg(nx * nt / 1e6, 0, 'f', 2));
    worker = new FkWorker(this);
    worker->path = path;
    worker->sourceKey = spectrumSourceKey(path);
    worker->fileType = fileType;
    worker->frequencyOnly = frequencyOnly;
    worker->dx = ui->fkSpacing->value();
    worker->first = ui->fkFirst->value() - 1;
    worker->last = ui->fkLast->value() - 1;
    worker->sampleFirst = int(a);
    worker->sampleLast = int(b);
    for (auto widget : ui->inputGroup->findChildren<QWidget *>())
        if (widget != ui->fkCalculate)
            widget->setEnabled(false);
    ui->fkCalculate->setEnabled(true);
    ui->fkCalculate->setIcon(QIcon(":/images/stop.png"));
    progress->setValue(0);
    messageStatus->setText(tr("Calculating..."));
    connect(worker, &FkWorker::progress, progress, &QProgressBar::setValue);
    connect(worker, &FkWorker::spectrumReady, this, &FkSpectrumDialog::receive);
    FkWorker *job = worker;
    connect(worker, &FkWorker::failed, this,
            [this, job](const QString &message) {
                if (worker != job)
                    return;
                messageStatus->setText(tr("Failed"));
                toast(message);
            });
    connect(worker, &QThread::finished, this, [this, job]() {
        if (worker != job)
            return;
        const bool cancelled = worker->isInterruptionRequested();
        worker->deleteLater();
        worker = nullptr;
        for (auto widget : ui->inputGroup->findChildren<QWidget *>())
            widget->setEnabled(true);
        if (frequencyOnly)
            ui->fkSpacing->setEnabled(false);
        ui->fkCalculate->setIcon(QIcon(":/images/Run.png"));
        updateSourceState();
        if (cancelled)
            messageStatus->setText(tr("Cancelled; cache retained"));
    });
    worker->start();
}

void FkSpectrumDialog::receive(const FkSpectrum &result)
{
    if (!worker || sender() != worker || worker->isInterruptionRequested())
        return;
    const auto &old = plot->spectrum;
    const int nt = worker->sampleLast - worker->sampleFirst + 1;
    const double legacyFrequencyMax = ((nt + 1) / 2 - 1) * result.df;
    const bool fullF =
        !haveDisplayRange ||
        (ui->fkFmin->value() == 0 &&
         ((!old.amplitude.isEmpty() &&
           std::abs(ui->fkFmax->value() - old.frequencyLimit) < 1e-6) ||
          (!physicalBounds &&
           std::abs(ui->fkFmax->value() - legacyFrequencyMax) < 1e-5)));
    const bool fullK =
        !haveDisplayRange ||
        ((!old.amplitude.isEmpty() &&
          std::abs(ui->fkKmin->value() + old.wavenumberLimit) < 1e-8 &&
          std::abs(ui->fkKmax->value() - old.wavenumberLimit) < 1e-8) ||
         (!physicalBounds && std::abs(ui->fkKmin->value() - result.k0) < 1e-8 &&
          std::abs(ui->fkKmax->value() -
                   (result.k0 + (result.nk - 1) * result.dk)) < 1e-8));
    plot->spectrum = result;
    setCaptureRange(
        worker->path, worker->first + 1, worker->last + 1,
        worker->sampleFirst /
            (result.df * (worker->sampleLast - worker->sampleFirst + 1)),
        worker->sampleLast /
            (result.df * (worker->sampleLast - worker->sampleFirst + 1)));
    computedKey = worker->sourceKey;
    plot->history.clear();
    plot->caption =
        tr("%1 | traces %2-%3 | %4-%5 ms")
            .arg(QFileInfo(worker->path).fileName())
            .arg(worker->first + 1)
            .arg(worker->last + 1)
            .arg(worker->sampleFirst * 1000.0 /
                     (result.df *
                      (worker->sampleLast - worker->sampleFirst + 1)),
                 0, 'g', 6)
            .arg(worker->sampleLast * 1000.0 /
                     (result.df *
                      (worker->sampleLast - worker->sampleFirst + 1)),
                 0, 'g', 6);
    if (!frequencyOnly)
        plot->caption += tr(" | dx=%1 m").arg(worker->dx, 0, 'g', 6);
    restoring = true;
    if (fullF) {
        ui->fkFmin->setValue(0);
        ui->fkFmax->setValue(result.frequencyLimit);
    }
    if (fullK) {
        ui->fkKmin->setValue(frequencyOnly ? 0 : -result.wavenumberLimit);
        ui->fkKmax->setValue(frequencyOnly ? 1 : result.wavenumberLimit);
    }
    restoring = false;
    applyDisplay(!haveDisplayRange);
    haveDisplayRange = true;
    physicalBounds = true;
    saveSettings();
    updateSourceState();
    emit spectrumUpdated();
}

bool FkSpectrumDialog::applyDisplay(bool autoColors)
{
    const auto &s = plot->spectrum;
    if (s.amplitude.isEmpty())
        return false;
    if (ui->fkFmin->value() >= ui->fkFmax->value() ||
        (!frequencyOnly && ui->fkKmin->value() >= ui->fkKmax->value()) ||
        (!autoColors && ui->fkCmin->value() >= ui->fkCmax->value())) {
        messageStatus->setText(tr("Invalid display range"));
        return false;
    }
    bool log = ui->fkScale->currentIndex() == 1;
    if (autoColors) {
        std::vector<double> values;
        for (int f = 0; f < s.nf; ++f)
            if (f * s.df >= ui->fkFmin->value() - 1e-9 &&
                f * s.df <= ui->fkFmax->value() + 1e-9)
                for (int k = 0; k < s.nk; ++k)
                    if (frequencyOnly ||
                        (s.k0 + k * s.dk >= ui->fkKmin->value() - 1e-9 &&
                         s.k0 + k * s.dk <= ui->fkKmax->value() + 1e-9)) {
                        double v = s.amplitude[f * s.nk + k];
                        values.push_back(spectrumAmplitudeValue(v, log));
                    }
        if (values.empty()) {
            messageStatus->setText(tr("No bins in range"));
            return false;
        }
        std::sort(values.begin(), values.end());
        double lo = frequencyOnly && !log
                        ? 0
                        : values[size_t(.01 * (values.size() - 1))];
        double hi = frequencyOnly ? values.back()
                                  : values[size_t(.995 * (values.size() - 1))];
        if (hi - lo < 1e-12)
            hi = lo + 1e-12;
        restoring = true;
        int decimals =
            log ? 9
                : qBound(9,
                         int(-std::floor(std::log10(qMax(hi - lo, 1e-20)))) + 4,
                         20);
        ui->fkCmin->setDecimals(decimals);
        ui->fkCmax->setDecimals(decimals);
        ui->fkCmin->setValue(lo);
        ui->fkCmax->setValue(hi);
        restoring = false;
    }
    plot->flo = ui->fkFmin->value();
    plot->fhi = ui->fkFmax->value();
    plot->klo = ui->fkKmin->value();
    plot->khi = ui->fkKmax->value();
    plot->clo = ui->fkCmin->value();
    plot->chi = ui->fkCmax->value();
    plot->logarithmic = log;
    plot->smooth = ui->fkInterpolation->currentIndex() == 0;
    plot->color = ui->fkPalette->currentIndex();
    plot->update();
    updateRangeStatus();
    return true;
}

void FkSpectrumDialog::updateRangeStatus()
{
    const QString text = frequencyOnly
                             ? tr("f: %1-%2 Hz").arg(plot->flo).arg(plot->fhi)
                             : tr("k: %1-%2 | f: %3-%4 Hz")
                                   .arg(plot->klo, 0, 'g', 4)
                                   .arg(plot->khi, 0, 'g', 4)
                                   .arg(plot->flo, 0, 'g', 4)
                                   .arg(plot->fhi, 0, 'g', 4);
    rangeStatus->setText(text);
    rangeStatus->setToolTip(plot->caption + "\n" + text);
}

void FkSpectrumDialog::loadSettings()
{
    ui->fkCmin->setDecimals(20);
    ui->fkCmax->setDecimals(20);
    restoring = true;
    QSettings settings("WW", "SeiSeeMp");
    settings.beginGroup(frequencyOnly ? "FrequencySpectrum" : "FkSpectrum");
    ui->fkFontSize->setValue(settings.value("advance/fontSize", 9).toInt());
    haveDisplayRange = settings.value("display/initialized", false).toBool();
    physicalBounds = settings.value("display/boundsVersion", 0).toInt() >= 2;
    for (auto spin : {ui->fkFmin, ui->fkFmax, ui->fkKmin, ui->fkKmax,
                      ui->fkCmin, ui->fkCmax}) {
        bool ok = false;
        double v =
            settings.value("display/" + spin->objectName(), spin->value())
                .toDouble(&ok);
        if (ok && std::isfinite(v))
            spin->setValue(v);
    }
    for (auto combo : {ui->fkPalette, ui->fkScale, ui->fkInterpolation}) {
        int i = settings.value("advance/" + combo->objectName(), 0).toInt();
        combo->setCurrentIndex(qBound(0, i, combo->count() - 1));
    }
    if (ui->fkScale->currentIndex() == 1 &&
        settings.value("display/amplitudeVersion", 1).toInt() < 2) {
        ui->fkCmin->setValue(20 * ui->fkCmin->value());
        ui->fkCmax->setValue(20 * ui->fkCmax->value());
    }
    if (ui->fkFmin->value() >= ui->fkFmax->value() ||
        ui->fkKmin->value() >= ui->fkKmax->value() ||
        ui->fkCmin->value() >= ui->fkCmax->value())
        haveDisplayRange = false;
    restoring = false;
}

void FkSpectrumDialog::saveSettings()
{
    if (restoring)
        return;
    QSettings settings("WW", "SeiSeeMp");
    settings.beginGroup(frequencyOnly ? "FrequencySpectrum" : "FkSpectrum");
    settings.setValue("advance/fontSize", ui->fkFontSize->value());
    settings.setValue("display/initialized", haveDisplayRange);
    settings.setValue("display/boundsVersion", physicalBounds ? 2 : 0);
    settings.setValue("display/amplitudeVersion", 2);
    for (auto spin : {ui->fkFmin, ui->fkFmax, ui->fkKmin, ui->fkKmax,
                      ui->fkCmin, ui->fkCmax})
        settings.setValue("display/" + spin->objectName(), spin->value());
    for (auto combo : {ui->fkPalette, ui->fkScale, ui->fkInterpolation})
        settings.setValue("advance/" + combo->objectName(),
                          combo->currentIndex());
}

bool FkSpectrumDialog::savePng(const QString &filename, int dpi)
{
    return !plot->spectrum.amplitude.isEmpty() &&
           savePlotImage(plot, filename, dpi);
}

void FkSpectrumDialog::exportImage(bool capture)
{
    if (plot->spectrum.amplitude.isEmpty()) {
        toast(tr("Calculate a spectrum first."));
        return;
    }
    const int dpi = capture ? 0 : exportDpi();
    const QString details = QString("fk_%1_f_%2_%3_k_%4_%5")
                                .arg(plot->logarithmic ? "dB" : "Abs")
                                .arg(plot->flo, 0, 'g', 8)
                                .arg(plot->fhi, 0, 'g', 8)
                                .arg(plot->klo, 0, 'g', 8)
                                .arg(plot->khi, 0, 'g', 8);
    const QString name = nextImagePath(details, dpi);
    imageExportNotice(savePng(name, dpi), name);
}

void FkSpectrumDialog::shutdown()
{
    saveSettings();
    plot->rubberBand->hide();
    plot->windowZoom = false;
    plot->dragging = false;
    plot->unsetCursor();
    ui->fkSelect->setChecked(false);
    emit selectionRequested(false);
    if (worker) {
        worker->disconnect();
        worker->requestInterruption();
        worker->wait();
        delete worker;
        worker = nullptr;
        for (auto widget : ui->inputGroup->findChildren<QWidget *>())
            widget->setEnabled(true);
        if (frequencyOnly)
            ui->fkSpacing->setEnabled(false);
        ui->fkCalculate->setEnabled(true);
        ui->fkCalculate->setIcon(QIcon(":/images/Run.png"));
    }
}
void FkSpectrumDialog::closeEvent(QCloseEvent *event)
{
    shutdown();
    QDialog::closeEvent(event);
}
void FkSpectrumDialog::reject()
{
    shutdown();
    QDialog::reject();
}
FkSpectrumDialog::~FkSpectrumDialog()
{
    shutdown();
    delete ui;
}
