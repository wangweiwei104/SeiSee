#include "frequencyspectrumdialog.h"
#include "ui_frequencyspectrumdialog.h"
#include "gfxstyle.h"
#include "gfxselectionband.h"
#include "spectrumticks.h"
#include "seisfile.h"
#include "Toast.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>
#include <climits>

namespace {
    QString fileId(const QString &path)
    {
        QFileInfo info(path);
        QString id = info.canonicalFilePath();
        if (id.isEmpty())
            id = info.absoluteFilePath();
#ifdef Q_OS_WIN
        id = id.toCaseFolded();
#endif
        return id;
    }
    void iconButton(QPushButton *button, const QString &tip)
    {
        button->setFixedSize(GfxStyle::StandardIconButtonSize,
                             GfxStyle::StandardIconButtonSize);
        button->setIconSize(
            QSize(GfxStyle::StandardIconSize, GfxStyle::StandardIconSize));
        button->setAutoDefault(false);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
    }
    void colorChoices(QComboBox *combo)
    {
        combo->clear();
        for (const QString &name :
             {QString("Black"), QString("Blue"), QString("Red"),
              QString("DarkGreen"), QString("Magenta"), QString("Cyan"),
              QString("Orange"), QString("Gray")}) {
            QColor color(name);
            QPixmap icon(14, 14);
            icon.fill(color);
            combo->addItem(QIcon(icon), name, color);
        }
        combo->addItem(QObject::tr("Custom..."));
    }
    void selectColor(QComboBox *combo, const QColor &color)
    {
        for (int i = 0; i < combo->count(); ++i)
            if (combo->itemData(i).value<QColor>() == color) {
                combo->setCurrentIndex(i);
                return;
            }
        QPixmap icon(14, 14);
        icon.fill(color);
        combo->insertItem(combo->count() - 1, QIcon(icon), color.name(), color);
        combo->setCurrentIndex(combo->count() - 2);
    }
} // namespace

class FrequencyPlot : public QWidget
{
public:
    QVector<FrequencyCurve> curves;
    QMap<QString, FrequencyLineStyle> styles;
    QString caption;
    double flo = 0, fhi = 1, alo = 0, ahi = 1;
    bool logarithmic = false, grid = false, stale = false, single = true,
         windowZoom = false, dragging = false;
    QPoint origin;
    QRectF dragView;
    GfxSelectionBand *band;
    QVector<QRectF> history;
    std::function<void(QRectF)> viewChanged;
    explicit FrequencyPlot(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName("frequencyPlot");
        setMinimumSize(480, 260);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFont(QFont(GfxStyle::LabelFontFamily,
                      int(GfxStyle::LabelFontPointSize)));
        band = new GfxSelectionBand(this);
    }
    QRectF dataRect() const
    {
        const auto metrics = fontMetrics();
        const int bottom = 3 * metrics.height() + 18;
        const double plotHeight =
            qMax(50, height() - metrics.height() - 16 - bottom);
        const auto ticks = spectrumTicks(
            alo, ahi, qBound(2, int(plotHeight / (metrics.height() * 3)), 6));
        int labelWidth = 0;
        for (const auto &label : ticks.labels)
            labelWidth = qMax(labelWidth, metrics.horizontalAdvance(label));
        double left = qMin(width() / 3.0, double(qMax(72, labelWidth + 32)));
        return QRectF(left, metrics.height() + 16,
                      qMax(50.0, width() - left - 25), plotHeight);
    }
    QRectF view() const
    {
        return QRectF(flo, alo, fhi - flo, ahi - alo);
    }
    double maxFrequency() const
    {
        return curves.isEmpty() ? 1 : curves.first().spectrum.frequencyLimit;
    }
    double value(float amplitude) const
    {
        return spectrumAmplitudeValue(amplitude, logarithmic);
    }
    void changeView(QRectF range, bool remember = true)
    {
        if (curves.isEmpty() || range.width() <= 0 || range.height() <= 0)
            return;
        double span =
            qBound(maxFrequency() * 1e-6, range.width(), maxFrequency());
        range.setRect(qBound(0.0, range.x(), maxFrequency() - span), range.y(),
                      span, range.height());
        if (remember) {
            if (history.size() >= 100)
                history.removeFirst();
            history.append(view());
        }
        if (viewChanged)
            viewChanged(range);
    }
    QPointF coordinate(QPoint pixel) const
    {
        auto box = dataRect();
        return QPointF(
            flo + (pixel.x() - box.left()) / box.width() * (fhi - flo),
            ahi - (pixel.y() - box.top()) / box.height() * (ahi - alo));
    }
    void zoom(double factor, double fx = .5, double fy = .5)
    {
        changeView(QRectF(flo + (1 - factor) * (fhi - flo) * fx,
                          alo + (1 - factor) * (ahi - alo) * fy,
                          (fhi - flo) * factor, (ahi - alo) * factor));
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (curves.isEmpty() || !dataRect().contains(event->pos()))
            return;
        origin = event->pos();
        if (windowZoom && event->button() == Qt::LeftButton) {
            band->setGeometry(QRect(origin, QSize()));
            band->show();
        } else if (event->button() == Qt::RightButton) {
            if (windowZoom) {
                windowZoom = false;
                band->hide();
                unsetCursor();
                return;
            }
            dragging = true;
            dragView = view();
            if (history.size() >= 100)
                history.removeFirst();
            history.append(view());
            setCursor(Qt::OpenHandCursor);
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (band->isVisible())
            band->setGeometry(QRect(origin, event->pos())
                                  .normalized()
                                  .intersected(dataRect().toRect()));
        else if (dragging) {
            auto delta = event->pos() - origin;
            changeView(dragView.translated(
                           -delta.x() / dataRect().width() * dragView.width(),
                           delta.y() / dataRect().height() * dragView.height()),
                       false);
        }
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && band->isVisible()) {
            auto rectangle = band->geometry();
            band->hide();
            windowZoom = false;
            unsetCursor();
            if (rectangle.width() > 3 && rectangle.height() > 3)
                changeView(QRectF(coordinate(rectangle.topLeft()),
                                  coordinate(rectangle.bottomRight()))
                               .normalized());
        }
        if (event->button() == Qt::RightButton) {
            dragging = false;
            unsetCursor();
        }
    }
    void wheelEvent(QWheelEvent *event) override
    {
        if (!windowZoom && dataRect().contains(event->pos())) {
            auto point = coordinate(event->pos());
            zoom(event->angleDelta().y() > 0 ? 1 / 1.5 : 1.5,
                 (point.x() - flo) / (fhi - flo),
                 (point.y() - alo) / (ahi - alo));
        }
        event->accept();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        const auto box = dataRect();
        const int fh = fontMetrics().height();
        painter.setPen(Qt::black);
        painter.drawText(QRectF(box.left(), 0, box.width(), fh + 8),
                         Qt::AlignCenter,
                         fontMetrics().elidedText(caption, Qt::ElideMiddle,
                                                  int(box.width())));
        const auto xticks = spectrumAxisTicks(flo, fhi, int(box.width()),
                                              fontMetrics(), true, 1);
        const auto yticks = spectrumAxisTicks(alo, ahi, int(box.height()),
                                              fontMetrics(), false);
        if (grid) {
            painter.setPen(QPen(QColor(210, 210, 210), 0, Qt::DashLine));
            for (double value : xticks.values) {
                double x =
                    box.left() + (value - flo) / (fhi - flo) * box.width();
                if (x <= box.left() || x >= box.right())
                    continue;
                painter.drawLine(QPointF(x, box.top()),
                                 QPointF(x, box.bottom()));
            }
            for (double value : yticks.values) {
                double y =
                    box.bottom() - (value - alo) / (ahi - alo) * box.height();
                if (y <= box.top() || y >= box.bottom())
                    continue;
                painter.drawLine(QPointF(box.left(), y),
                                 QPointF(box.right(), y));
            }
        }
        if (curves.isEmpty()) {
            painter.setPen(Qt::black);
            painter.drawText(box, Qt::AlignCenter,
                             single
                                 ? tr("Select a trace in the main window; the "
                                      "spectrum updates automatically")
                                 : tr("Select a region and click Calculate"));
        }
        painter.save();
        painter.setClipRect(box);
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (const auto &curve : curves) {
            QPainterPath line;
            bool started = false;
            for (int f = 0; f < curve.spectrum.nf; ++f) {
                double hz = f * curve.spectrum.df;
                if (hz < flo || hz > fhi)
                    continue;
                QPointF point(
                    box.left() + (hz - flo) / (fhi - flo) * box.width(),
                    box.bottom() - (value(curve.spectrum.amplitude[f]) - alo) /
                                       (ahi - alo) * box.height());
                if (!started) {
                    line.moveTo(point);
                    started = true;
                } else
                    line.lineTo(point);
            }
            const auto style = styles.value(curve.id);
            painter.setPen(
                QPen(stale ? QColor(145, 145, 145) : style.color, style.width));
            painter.drawPath(line);
        }
        painter.restore();
        painter.setPen(Qt::black);
        painter.drawRect(box);
        for (int i = 0; i < xticks.values.size(); ++i) {
            double x = box.left() +
                       (xticks.values[i] - flo) / (fhi - flo) * box.width();
            painter.drawLine(QPointF(x, box.bottom()),
                             QPointF(x, box.bottom() + 4));
            const double labelWidth =
                fontMetrics().horizontalAdvance(xticks.labels[i]) + 8;
            painter.drawText(QRectF(qBound(0.0, x - labelWidth / 2,
                                           qMax(0.0, width() - labelWidth)),
                                    box.bottom() + 6, labelWidth, fh + 2),
                             Qt::AlignCenter, xticks.labels[i]);
        }
        for (int i = 0; i < yticks.values.size(); ++i) {
            double y = box.bottom() -
                       (yticks.values[i] - alo) / (ahi - alo) * box.height();
            painter.drawLine(QPointF(box.left() - 4, y),
                             QPointF(box.left(), y));
            painter.drawText(QRectF(20, y - fh / 2, box.left() - 27, fh + 2),
                             Qt::AlignRight | Qt::AlignVCenter,
                             yticks.labels[i]);
        }
        painter.drawText(
            QRectF(box.left(), box.bottom() + fh + 12, box.width(), fh + 3),
            Qt::AlignCenter, tr("Frequency (Hz)"));
        painter.save();
        painter.translate(13, box.center().y());
        painter.rotate(-90);
        painter.drawText(
            QRectF(-box.height() / 2, -fh / 2, box.height(), fh + 2),
            Qt::AlignCenter,
            logarithmic ? tr("Amplitude (dB)") : tr("Amplitude"));
        painter.restore();
        int legendWidth = 0;
        for (int i = 0; i < curves.size(); ++i) {
            QString label = (i == 0 ? tr("Main: ") : QString("%1: ").arg(i)) +
                            QFileInfo(curves[i].path).fileName();
            legendWidth =
                qMax(legendWidth, fontMetrics().horizontalAdvance(label) + 35);
        }
        legendWidth = qMin(legendWidth, int(box.width() * .65));
        const int legendCount =
            qMin(curves.size(), qMax(1, int((box.height() - 16) / (fh + 3))));
        QRectF legend(box.right() - legendWidth - 8, box.top() + 8, legendWidth,
                      legendCount * (fh + 3));
        if (!curves.isEmpty())
            painter.fillRect(legend.adjusted(-4, -3, 4, 3),
                             QColor(255, 255, 255, 210));
        for (int i = 0; i < legendCount; ++i) {
            const auto &curve = curves[i];
            const auto style = styles.value(curve.id);
            double x = legend.left(), y = legend.top() + i * (fh + 3);
            painter.setPen(
                QPen(stale ? QColor(145, 145, 145) : style.color, style.width));
            painter.drawLine(QPointF(x, y + fh / 2),
                             QPointF(x + 22, y + fh / 2));
            painter.setPen(Qt::black);
            QString name = (i == 0 ? tr("Main: ") : QString("%1: ").arg(i)) +
                           QFileInfo(curve.path).fileName();
            if (i == legendCount - 1 && curves.size() > legendCount)
                name = tr("+ %1 comparison files")
                           .arg(curves.size() - legendCount + 1);
            painter.drawText(
                QRectF(x + 27, y, legend.width() - 27, fh + 2),
                Qt::AlignLeft | Qt::AlignVCenter,
                fontMetrics().elidedText(name, Qt::ElideMiddle,
                                         int(legend.width() - 27)));
        }
        if (stale && !curves.isEmpty()) {
            painter.setPen(QColor(145, 145, 145));
            painter.drawText(box.adjusted(6, 6, -6, -6),
                             Qt::AlignBottom | Qt::AlignRight,
                             tr("Different source - cached spectra"));
        }
    }
};

FrequencySpectrumDialog::FrequencySpectrumDialog(SeisFile *file,
                                                 const QString &dir,
                                                 QWidget *parent)
    : SpectrumDialog(parent)
    , ui(new Ui::frequencydialog)
    , directory(dir)
{
    ui->setupUi(this);
    for (auto layout : ui->frame->findChildren<QLayout *>()) {
        layout->setSpacing(4);
        layout->setContentsMargins(4, 4, 4, 4);
    }
    setObjectName("FrequencySpectrumDialog");
    setWindowTitle(tr("Frequency Spectrum"));
    setWindowModality(Qt::NonModal);
    setModal(false);
    setSizeGripEnabled(true);
    resize(1000, 620);
    qRegisterMetaType<FkSpectrum>("FkSpectrum");
    automaticTimer = new QTimer(this);
    automaticTimer->setSingleShot(true);
    connect(automaticTimer, &QTimer::timeout, this,
            &FrequencySpectrumDialog::runAutomatic);
    auto modes = new QButtonGroup(this);
    modes->setExclusive(true);
    modes->addButton(ui->frequencySingle, 0);
    modes->addButton(ui->frequencySelection, 1);
    ui->frequencySingle->setChecked(true);
    for (auto spin : {ui->fkFirst, ui->fkLast}) {
        spin->setRange(1, INT_MAX);
        spin->setKeyboardTracking(false);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(45);
    }
    for (auto spin : {ui->fkStart, ui->fkEnd}) {
        spin->setDecimals(3);
        spin->setRange(0, 1e12);
        spin->setKeyboardTracking(false);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(45);
    }
    for (auto spin : QList<QDoubleSpinBox *>{ui->fkFmin, ui->fkFmax, ui->fkCmin,
                                             ui->fkCmax}) {
        spin->setRange(-1e100, 1e100);
        spin->setDecimals(6);
        spin->setKeyboardTracking(false);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(65);
    }
    for (auto spin : {ui->fkStart, ui->fkEnd}) {
        spin->setDisplayDecimals(0);
        spin->setSingleStep(1);
    }
    for (auto spin : {ui->fkFmin, ui->fkFmax}) {
        spin->setDisplayDecimals(1);
        spin->setSingleStep(.1);
    }
    ui->fkFmin->setMinimum(0);
    ui->fkFmax->setMinimum(0);
    ui->fkFmax->setValue(1);
    ui->fkCmax->setValue(1);
    ui->fkScale->clear();
    ui->fkScale->addItems({"Abs", "Log10"});
    ui->fkScale->setToolTip(
        tr("Log10: amplitude in dB = 20*log10(max(amplitude, 1e-12)), "
           "reference amplitude 1."));
    colorChoices(ui->frequencyColor);
    colorChoices(ui->comparisonColor);
    ui->comparisonColor->setCurrentIndex(1);
    for (auto combo : {ui->frequencyLineWidth, ui->comparisonLineWidth}) {
        combo->clear();
        combo->addItems({"0.5", "1", "1.5", "2", "3", "4", "5", "6"});
        combo->setCurrentText("1");
    }
    ui->frequencyFontSize->setRange(6, 24);
    ui->frequencyFontSize->setValue(9);
    // Keep the reference's compact File row, adding a target selector for
    // multiple overlays.
    auto files = new QComboBox(ui->comparisonGroup);
    files->setObjectName("comparisonFile");
    files->setMinimumWidth(75);
    files->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    ui->horizontalLayout_8->insertWidget(1, files, 1);
    ui->fkSelect->setCheckable(true);
    iconButton(ui->fkAllTraces, tr("All traces"));
    iconButton(ui->fkAllTime, tr("All time samples (ms)"));
    iconButton(ui->fkCurrentView,
               tr("Use the main window's current visible range"));
    iconButton(ui->fkSelect,
               tr("Select / resize a region in the main seismic view"));
    iconButton(ui->fkClearSelection, tr("Cancel selection"));
    iconButton(ui->fkCalculate, tr("Calculate / cancel all spectra"));
    iconButton(ui->frequencyFullRange, tr("Show all frequencies"));
    iconButton(ui->frequencyAutoAmplitude, tr("Fit amplitude to all curves"));
    iconButton(ui->comparisonAdd,
               tr("Add SEG-Y comparison files (multiple selection)"));
    iconButton(ui->comparisonRemove, tr("Remove selected comparison file"));
    iconButton(ui->fkZoomAll, tr("Zoom all"));
    iconButton(ui->fkZoomIn, tr("Zoom in"));
    iconButton(ui->fkZoomOut, tr("Zoom out"));
    iconButton(ui->fkZoomWindow, tr("Window zoom"));
    iconButton(ui->fkZoomPrevious, tr("Previous zoom"));
    iconButton(ui->fkCapture, tr("Capture spectrum"));
    iconButton(ui->fkExport, tr("Export high-resolution spectrum"));
    plot = new FrequencyPlot(ui->plotFrame);
    auto canvas = new QVBoxLayout(ui->plotFrame);
    canvas->setContentsMargins(0, 0, 0, 0);
    canvas->addWidget(plot);
    fileStatus = new QLabel;
    fileStatus->setObjectName("fkFileStatus");
    rangeStatus = new QLabel;
    rangeStatus->setObjectName("fkRangeStatus");
    compareStatus = new QLabel;
    compareStatus->setObjectName("comparisonStatus");
    messageStatus = new QLabel;
    messageStatus->setObjectName("fkStatus");
    for (auto label : {fileStatus, rangeStatus, compareStatus, messageStatus}) {
        label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        ui->frequencyStatusBar->addWidget(label,
                                          label == messageStatus ? 1 : 2);
    }
    progress = new QProgressBar;
    progress->setObjectName("fkProgress");
    progress->setFixedWidth(105);
    progress->setValue(0);
    ui->frequencyStatusBar->addPermanentWidget(progress);
    ui->frequencyStatusBar->setSizeGripEnabled(false);
    styles["main"] = {Qt::black, 1};
    loadSettings();
    setSource(file);
    if (traceCount > 0) {
        ui->fkLast->setValue(traceCount);
        ui->fkEnd->setValue((sampleCount - 1) * dt * 1000);
    }
    setInputMode();
    updateComparisonControls();
    refreshStyles();
    applyDisplay();
    plot->viewChanged = [this](QRectF range) {
        restoring = true;
        ui->fkFmin->setValue(range.left());
        ui->fkFmax->setValue(range.right());
        ui->fkCmin->setValue(range.top());
        ui->fkCmax->setValue(range.bottom());
        restoring = false;
        haveDisplayRange = true;
        applyDisplay();
        saveSettings();
    };
    connect(ui->frequencySingle, &QCheckBox::toggled, this,
            [this](bool checked) {
                setInputMode();
                if (checked && mainTrace > 0)
                    setMainTrace(mainTrace);
                if (checked)
                    requestAutomatic();
                else {
                    automaticTimer->stop();
                    automaticPending = false;
                }
                saveSettings();
            });
    connect(ui->fkCurrentView, &QPushButton::clicked, this,
            &SpectrumDialog::currentViewRequested);
    connect(ui->fkSelect, &QPushButton::toggled, this,
            &SpectrumDialog::selectionRequested);
    connect(ui->fkClearSelection, &QPushButton::clicked, this, [this]() {
        ui->fkSelect->setChecked(false);
        emit selectionRequested(false);
    });
    connect(ui->fkAllTraces, &QPushButton::clicked, this, [this]() {
        if (traceCount) {
            ui->fkFirst->setValue(1);
            ui->fkLast->setValue(traceCount);
        }
    });
    connect(ui->fkAllTime, &QPushButton::clicked, this, [this]() {
        if (sampleCount) {
            ui->fkStart->setValue(0);
            ui->fkEnd->setValue((sampleCount - 1) * dt * 1000);
        }
    });
    connect(ui->fkCalculate, &QPushButton::clicked, this, [this]() {
        automaticTimer->stop();
        automaticPending = false;
        if (batchRunning) {
            cancelled = true;
            if (worker)
                worker->requestInterruption();
            ui->fkCalculate->setEnabled(false);
        } else
            begin();
    });
    connect(ui->comparisonAdd, &QPushButton::clicked, this, [this]() {
        addComparisonFiles(QFileDialog::getOpenFileNames(
            this, tr("Select comparison SEG-Y files"), directory,
            tr("SEG-Y (*.sgy *.segy);;All files (*)")));
    });
    connect(ui->comparisonRemove, &QPushButton::clicked, this, [this, files]() {
        int index = files->currentIndex();
        if (index < 0)
            return;
        QString id = fileId(comparisons.takeAt(index));
        for (int i = plot->curves.size() - 1; i > 0; --i)
            if (plot->curves[i].id == id)
                plot->curves.remove(i);
        updateComparisonControls();
        applyDisplay();
        requestAutomatic();
    });
    connect(files, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { updateComparisonControls(); });
    for (auto combo : {ui->frequencyColor, ui->frequencyLineWidth})
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() { updateLineStyle(true); });
    for (auto combo : {ui->comparisonColor, ui->comparisonLineWidth})
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() { updateLineStyle(false); });
    for (auto spin : QList<QDoubleSpinBox *>{ui->fkFmin, ui->fkFmax, ui->fkCmin,
                                             ui->fkCmax})
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this]() {
                    if (!restoring) {
                        haveDisplayRange = true;
                        applyDisplay();
                        saveSettings();
                    }
                });
    connect(ui->fkScale, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                applyDisplay();
                autoAmplitude();
                saveSettings();
            });
    connect(ui->frequencyGrid, &QCheckBox::toggled, this, [this]() {
        applyDisplay();
        saveSettings();
    });
    connect(ui->frequencyFontSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() {
                applyDisplay();
                saveSettings();
            });
    connect(ui->frequencyFullRange, &QPushButton::clicked, this,
            &FrequencySpectrumDialog::fullFrequency);
    connect(ui->frequencyAutoAmplitude, &QPushButton::clicked, this,
            &FrequencySpectrumDialog::autoAmplitude);
    connect(ui->fkZoomAll, &QPushButton::clicked, this, [this]() {
        fullFrequency();
        autoAmplitude();
    });
    connect(ui->fkZoomIn, &QPushButton::clicked, this,
            [this]() { plot->zoom(1 / 1.5); });
    connect(ui->fkZoomOut, &QPushButton::clicked, this,
            [this]() { plot->zoom(1.5); });
    connect(ui->fkZoomWindow, &QPushButton::clicked, this, [this]() {
        plot->windowZoom = true;
        plot->setCursor(
            QCursor(QPixmap(":/images/SelectionBlackCurs.xpm"), 8, 8));
    });
    connect(ui->fkZoomPrevious, &QPushButton::clicked, this, [this]() {
        if (!plot->history.isEmpty())
            plot->changeView(plot->history.takeLast(), false);
    });
    connect(ui->fkCapture, &QPushButton::clicked, this,
            [this]() { exportImage(true); });
    connect(ui->fkExport, &QPushButton::clicked, this,
            [this]() { exportImage(false); });
    connect(ui->fkFirst, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { requestAutomatic(); });
    for (auto spin : {ui->fkStart, ui->fkEnd})
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this]() { requestAutomatic(); });
}

void FrequencySpectrumDialog::setSource(SeisFile *file)
{
    const bool active = file && file->Active();
    const QString nextPath =
        active ? QString::fromLocal8Bit(file->Fname().c_str()) : QString();
    if (path != nextPath) {
        mainTrace = 0;
        automaticTimer->stop();
        automaticPending = false;
    }
    path = nextPath;
    traceCount = active ? file->Nt() : 0;
    sampleCount = active ? file->Ns() : 0;
    dt = active ? file->Si() : 0;
    fileType = active ? (file->typeName() == "SEG-Y" ? 1 : file->typeIdx()) : 1;
    updateSourceState();
}
void FrequencySpectrumDialog::setMainTrace(int traceNumber)
{
    if (traceNumber < 1 || traceNumber > traceCount)
        return;
    mainTrace = traceNumber;
    if (!ui->frequencySingle->isChecked())
        return;
    ui->fkFirst->setValue(traceNumber);
    if (!batchRunning)
        messageStatus->setText(
            tr("Trace %1 selected; updating spectrum...").arg(traceNumber));
    requestAutomatic();
}
bool FrequencySpectrumDialog::isSourceCurrent() const
{
    return !computedKey.isEmpty() && !path.isEmpty() &&
           computedKey == spectrumSourceKey(path);
}
void FrequencySpectrumDialog::updateSourceState()
{
    plot->stale = !isSourceCurrent();
    plot->update();
    fileStatus->setText(tr("Current File: %1").arg(QFileInfo(path).fileName()));
    fileStatus->setToolTip(path);
    ui->fkCalculate->setEnabled(batchRunning || !path.isEmpty());
    if (!batchRunning)
        messageStatus->setText(
            plot->curves.isEmpty()
                ? (ui->frequencySingle->isChecked()
                       ? tr("Automatic single-trace calculation")
                       : tr("Click Calculate"))
            : plot->stale ? tr("Cached / other source")
                          : tr("Cached / current source"));
}
void FrequencySpectrumDialog::setInputMode()
{
    const bool region = ui->frequencySelection->isChecked();
    ui->fkLast->setEnabled(region);
    ui->label_3->setEnabled(region);
    ui->fkAllTraces->setEnabled(region);
    for (auto button : {ui->fkCurrentView, ui->fkSelect})
        button->setEnabled(region);
    if (!region) {
        ui->fkSelect->setChecked(false);
        emit selectionRequested(false);
    }
    ui->fkFirst->setToolTip(region ? tr("First trace (1-based)")
                                   : tr("Single trace number (1-based)"));
}
void FrequencySpectrumDialog::setInitialRange(int first, int last, double start,
                                              double end)
{
    if (batchRunning)
        return;
    ui->fkFirst->setValue(first);
    ui->fkLast->setValue(last);
    ui->fkStart->setValue(start);
    ui->fkEnd->setValue(end);
}
void FrequencySpectrumDialog::addComparisonFiles(const QStringList &paths)
{
    if (batchRunning)
        return;
    const QList<QColor> colors = {Qt::blue,    Qt::red,          Qt::darkGreen,
                                  Qt::magenta, QColor("orange"), Qt::cyan};
    for (const auto &name : paths) {
        QFileInfo info(name);
        if (!info.isFile()) {
            toast(tr("File not found: %1").arg(name));
            continue;
        }
        const QString id = fileId(name);
        bool duplicate = (id == fileId(path));
        for (const auto &existing : comparisons)
            duplicate = duplicate || fileId(existing) == id;
        if (duplicate)
            continue;
        if (!styles.contains(id))
            styles[id] = {colors[comparisons.size() % colors.size()], 1};
        comparisons.append(info.absoluteFilePath());
    }
    updateComparisonControls();
    messageStatus->setText(tr("Files changed; click Calculate"));
    requestAutomatic();
}
void FrequencySpectrumDialog::updateComparisonControls()
{
    auto files = findChild<QComboBox *>("comparisonFile");
    const int old = files->currentIndex();
    const QSignalBlocker blocker(files);
    files->clear();
    for (int i = 0; i < comparisons.size(); ++i) {
        files->addItem(QString("%1: %2").arg(i + 1).arg(
            QFileInfo(comparisons[i]).fileName()));
        files->setItemData(i, comparisons[i], Qt::ToolTipRole);
    }
    files->setCurrentIndex(
        comparisons.isEmpty() ? -1 : qBound(0, old, comparisons.size() - 1));
    bool enabled = !comparisons.isEmpty();
    ui->comparisonColor->setEnabled(enabled);
    ui->comparisonLineWidth->setEnabled(enabled);
    ui->comparisonRemove->setEnabled(enabled);
    if (enabled) {
        auto style = styles.value(fileId(comparisons[files->currentIndex()]));
        QSignalBlocker a(ui->comparisonColor), b(ui->comparisonLineWidth);
        selectColor(ui->comparisonColor, style.color);
        ui->comparisonLineWidth->setCurrentText(QString::number(style.width));
        files->setToolTip(comparisons[files->currentIndex()]);
    }
    compareStatus->setText(tr("Compare File: %1").arg(comparisons.size()));
    compareStatus->setToolTip(comparisons.join("\n"));
}
void FrequencySpectrumDialog::updateLineStyle(bool primary)
{
    if (restoring)
        return;
    auto files = findChild<QComboBox *>("comparisonFile");
    if (!primary && files->currentIndex() < 0)
        return;
    QString id = primary ? "main" : fileId(comparisons[files->currentIndex()]);
    auto color = primary ? ui->frequencyColor : ui->comparisonColor;
    auto width = primary ? ui->frequencyLineWidth : ui->comparisonLineWidth;
    QColor chosen = color->currentData().value<QColor>();
    if (!chosen.isValid()) {
        chosen = QColorDialog::getColor(styles.value(id).color, this,
                                        tr("Curve color"));
        QSignalBlocker blocker(color);
        selectColor(color, chosen.isValid() ? chosen : styles.value(id).color);
    }
    if (chosen.isValid())
        styles[id].color = chosen;
    styles[id].width = width->currentText().toDouble();
    refreshStyles();
    saveSettings();
}
void FrequencySpectrumDialog::refreshStyles()
{
    plot->styles = styles;
    plot->update();
}

QVector<FrequencyCurve> FrequencySpectrumDialog::spectra() const
{
    return plot->curves;
}

QString FrequencySpectrumDialog::requestSignature() const
{
    QStringList fields = {spectrumSourceKey(path),
                          QString::number(fileType),
                          QString::number(ui->frequencySingle->isChecked()),
                          QString::number(ui->fkFirst->value()),
                          QString::number(ui->frequencySingle->isChecked()
                                              ? ui->fkFirst->value()
                                              : ui->fkLast->value()),
                          QString::number(ui->fkStart->value(), 'g', 17),
                          QString::number(ui->fkEnd->value(), 'g', 17)};
    for (const auto &name : comparisons)
        fields.append(spectrumSourceKey(name));
    return fields.join('\n');
}
void FrequencySpectrumDialog::requestAutomatic()
{
    if (restoring || !isVisible() || !ui->frequencySingle->isChecked() ||
        path.isEmpty())
        return;
    const auto signature = requestSignature();
    if ((!batchRunning && signature == computedSignature) ||
        (batchRunning && !cancelled && signature == runningSignature)) {
        automaticPending = false;
        automaticTimer->stop();
        return;
    }
    automaticPending = true;
    automaticTimer->start(120);
}
void FrequencySpectrumDialog::runAutomatic()
{
    if (!automaticPending || !isVisible() ||
        !ui->frequencySingle->isChecked() || path.isEmpty())
        return;
    if (batchRunning) {
        cancelled = true;
        if (worker)
            worker->requestInterruption();
        return;
    }
    if (requestSignature() == computedSignature) {
        automaticPending = false;
        return;
    }
    begin();
}
void FrequencySpectrumDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    requestAutomatic();
}

void FrequencySpectrumDialog::begin()
{
    automaticTimer->stop();
    automaticPending = false;
    if (path.isEmpty() || dt <= 0)
        return;
    const int first = ui->fkFirst->value() - 1,
              last = ui->frequencySingle->isChecked() ? first
                                                      : ui->fkLast->value() - 1;
    double a = std::ceil(ui->fkStart->value() / (dt * 1000) - 1e-7),
           b = std::floor(ui->fkEnd->value() / (dt * 1000) + 1e-7);
    qint64 nx = qint64(last) - first + 1;
    double nt = b - a + 1;
    if (nx < 1 || last >= traceCount || a < 0 || b >= sampleCount || nt < 4 ||
        nx > 180180 || nt > 180180 || nx * nt > 8000000) {
        toast(tr("Choose a valid trace/time range: 4+ samples, at most 8 "
                 "million samples and 180180 per axis."));
        return;
    }
    if (nx * nt >= 1000000)
        toast(tr("Large selection. Calculating all files may take time; you "
                 "can cancel."));
    requests.clear();
    runningSignature = requestSignature();
    pending.clear();
    errors.clear();
    jobIndex = 0;
    cancelled = false;
    primaryFailed = false;
    batchSingle = ui->frequencySingle->isChecked();
    auto request = [&](QString name, QString id, int type) {
        Request r;
        r.path = name;
        r.id = id;
        r.sourceKey = spectrumSourceKey(name);
        r.fileType = type;
        r.first = first;
        r.last = last;
        r.sampleFirst = int(a);
        r.sampleLast = int(b);
        r.dt = dt;
        requests.append(r);
    };
    request(path, "main", fileType);
    for (const auto &name : comparisons)
        request(name, fileId(name), 1);
    batchRunning = true;
    ui->modeGroup->setEnabled(false);
    ui->comparisonGroup->setEnabled(false);
    for (auto widget : ui->inputGroup->findChildren<QWidget *>())
        if (widget != ui->fkCalculate)
            widget->setEnabled(false);
    ui->fkCalculate->setEnabled(true);
    ui->fkCalculate->setIcon(QIcon(":/images/stop.png"));
    progress->setValue(0);
    launchNext();
}
void FrequencySpectrumDialog::launchNext()
{
    const Request request = requests[jobIndex];
    auto job = new FkWorker(this);
    worker = job;
    job->path = request.path;
    job->sourceKey = request.sourceKey;
    job->fileType = request.fileType;
    job->first = request.first;
    job->last = request.last;
    job->sampleFirst = request.sampleFirst;
    job->sampleLast = request.sampleLast;
    job->frequencyOnly = true;
    job->requiredSampleInterval = request.dt;
    messageStatus->setText(
        tr("Calculating %1/%2").arg(jobIndex + 1).arg(requests.size()));
    connect(job, &FkWorker::progress, this, [this, job](int percent) {
        if (worker == job && sender() == job)
            progress->setValue((100 * jobIndex + percent) / requests.size());
    });
    connect(job, &FkWorker::spectrumReady, this,
            [this, job, request](const FkSpectrum &spectrum) {
                if (worker != job || sender() != job || cancelled)
                    return;
                FrequencyCurve curve;
                curve.id = request.id;
                curve.path = request.path;
                curve.sourceKey = request.sourceKey;
                curve.spectrum = spectrum;
                pending.append(curve);
            });
    connect(job, &FkWorker::failed, this,
            [this, job, request](const QString &message) {
                if (worker != job || sender() != job)
                    return;
                errors.append(QFileInfo(request.path).fileName() + ": " +
                              message);
                if (jobIndex == 0)
                    primaryFailed = true;
            });
    connect(job, &QThread::finished, this, [this, job]() {
        if (worker != job || sender() != job)
            return;
        bool stopped = cancelled || job->isInterruptionRequested();
        worker = nullptr;
        job->deleteLater();
        if (stopped || primaryFailed) {
            finishBatch(false);
            return;
        }
        if (++jobIndex == requests.size())
            finishBatch(true);
        else
            launchNext();
    });
    job->start();
}
void FrequencySpectrumDialog::finishBatch(bool success)
{
    batchRunning = false;
    ui->modeGroup->setEnabled(true);
    ui->comparisonGroup->setEnabled(true);
    for (auto widget : ui->inputGroup->findChildren<QWidget *>())
        widget->setEnabled(true);
    setInputMode();
    ui->fkCalculate->setIcon(QIcon(":/images/Run.png"));
    if (success && !pending.isEmpty()) {
        const int nt =
            requests.first().sampleLast - requests.first().sampleFirst + 1;
        const double legacyFrequencyMax =
            ((nt + 1) / 2 - 1) * pending.first().spectrum.df;
        const bool fullF =
            !haveDisplayRange ||
            (ui->fkFmin->value() == 0 &&
             ((!plot->curves.isEmpty() &&
               std::abs(ui->fkFmax->value() - plot->maxFrequency()) < 1e-6) ||
              (!physicalBounds &&
               std::abs(ui->fkFmax->value() - legacyFrequencyMax) < 1e-5)));
        computedSignature = runningSignature;
        plot->curves = pending;
        computedKey = pending.first().sourceKey;
        plot->single = batchSingle;
        plot->history.clear();
        const auto &r = requests.first();
        setCaptureRange(r.path, r.first + 1, r.last + 1, r.sampleFirst * r.dt,
                        r.sampleLast * r.dt);
        computedRange =
            batchSingle ? tr("Trace: %1").arg(r.first + 1)
                        : tr("Traces: %1-%2").arg(r.first + 1).arg(r.last + 1);
        plot->caption =
            tr("%1 | %2 %3-%4 | %5-%6 ms")
                .arg(QFileInfo(r.path).fileName())
                .arg(batchSingle ? tr("Single trace") : tr("Region mean"))
                .arg(r.first + 1)
                .arg(r.last + 1)
                .arg(r.sampleFirst * r.dt * 1000, 0, 'f', 0)
                .arg(r.sampleLast * r.dt * 1000, 0, 'f', 0);
        if (fullF) {
            restoring = true;
            ui->fkFmin->setValue(0);
            ui->fkFmax->setValue(plot->maxFrequency());
            restoring = false;
        }
        if (!haveDisplayRange)
            autoAmplitude();
        applyDisplay();
        haveDisplayRange = true;
        physicalBounds = true;
        progress->setValue(100);
        emit spectrumUpdated();
    }
    pending.clear();
    updateSourceState();
    if (success && ui->frequencySingle->isChecked() &&
        ui->fkFirst->value() != requests.first().first + 1)
        messageStatus->setText(tr("Trace %1 selected; updating spectrum...")
                                   .arg(ui->fkFirst->value()));
    if (cancelled)
        messageStatus->setText(tr("Cancelled; cache retained"));
    if (!errors.isEmpty()) {
        messageStatus->setText(tr("%1 file(s) failed").arg(errors.size()));
        messageStatus->setToolTip(errors.join("\n"));
        toast(errors.join("\n"));
    }
    saveSettings();
    if (automaticPending && isVisible() && ui->frequencySingle->isChecked() &&
        !automaticTimer->isActive())
        automaticTimer->start(0);
}
void FrequencySpectrumDialog::applyDisplay()
{
    if (ui->fkFmin->value() >= ui->fkFmax->value() ||
        ui->fkCmin->value() >= ui->fkCmax->value()) {
        messageStatus->setText(tr("Invalid display range"));
        return;
    }
    plot->flo = ui->fkFmin->value();
    plot->fhi = ui->fkFmax->value();
    plot->alo = ui->fkCmin->value();
    plot->ahi = ui->fkCmax->value();
    plot->logarithmic = ui->fkScale->currentIndex() == 1;
    plot->grid = ui->frequencyGrid->isChecked();
    QFont font(GfxStyle::LabelFontFamily, ui->frequencyFontSize->value());
    plot->setFont(font);
    plot->update();
    rangeStatus->setText(
        (computedRange.isEmpty() ? tr("Trace") : computedRange) +
        tr(" | f: %1-%2 Hz")
            .arg(plot->flo, 0, 'f', 1)
            .arg(plot->fhi, 0, 'f', 1));
    rangeStatus->setToolTip(plot->caption);
}
void FrequencySpectrumDialog::fullFrequency()
{
    if (plot->curves.isEmpty())
        return;
    restoring = true;
    ui->fkFmin->setValue(0);
    ui->fkFmax->setValue(plot->maxFrequency());
    restoring = false;
    haveDisplayRange = true;
    applyDisplay();
    saveSettings();
}
void FrequencySpectrumDialog::autoAmplitude()
{
    if (plot->curves.isEmpty())
        return;
    bool log = ui->fkScale->currentIndex() == 1;
    double lo = log ? 1e100 : 0, hi = -1e100;
    for (const auto &curve : plot->curves)
        for (int f = 0; f < curve.spectrum.nf; ++f)
            if (f * curve.spectrum.df >= ui->fkFmin->value() &&
                f * curve.spectrum.df <= ui->fkFmax->value()) {
                double value = curve.spectrum.amplitude[f];
                value = spectrumAmplitudeValue(value, log);
                lo = qMin(lo, value);
                hi = qMax(hi, value);
            }
    if (hi < lo)
        return;
    if (hi - lo < 1e-12)
        hi = lo + qMax(1e-12, std::abs(lo) * 1e-6);
    restoring = true;
    ui->fkCmin->setDecimals(12);
    ui->fkCmax->setDecimals(12);
    ui->fkCmin->setValue(lo);
    ui->fkCmax->setValue(hi + (hi - lo) * .04);
    restoring = false;
    applyDisplay();
    saveSettings();
}
void FrequencySpectrumDialog::loadSettings()
{
    restoring = true;
    QSettings settings("WW", "SeiSeeMp");
    settings.beginGroup("FrequencySpectrum");
    // Each new frequency window starts in single-trace mode.
    ui->frequencySingle->setChecked(true);
    haveDisplayRange = settings.value("display/initialized", false).toBool();
    physicalBounds = settings.value("display/boundsVersion", 0).toInt() >= 2;
    for (auto spin : QList<QDoubleSpinBox *>{ui->fkFmin, ui->fkFmax, ui->fkCmin,
                                             ui->fkCmax}) {
        bool ok = false;
        double v =
            settings.value("display/" + spin->objectName(), spin->value())
                .toDouble(&ok);
        if (ok && std::isfinite(v))
            spin->setValue(v);
    }
    ui->frequencyGrid->setChecked(
        settings.value("display/grid", false).toBool());
    ui->frequencyFontSize->setValue(
        settings.value("display/fontSize", 9).toInt());
    ui->fkScale->setCurrentIndex(
        qBound(0, settings.value("advance/fkScale", 0).toInt(), 1));
    if (ui->fkScale->currentIndex() == 1 &&
        settings.value("display/amplitudeVersion", 1).toInt() < 2) {
        ui->fkCmin->setValue(20 * ui->fkCmin->value());
        ui->fkCmax->setValue(20 * ui->fkCmax->value());
    }
    int count = settings.beginReadArray("lineStyles");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        QString id = settings.value("id").toString();
        QColor color(settings.value("color").toString());
        double width = settings.value("width", 1).toDouble();
        if (!id.isEmpty() && color.isValid() && std::isfinite(width) &&
            width >= .5 && width <= 6)
            styles[id] = {color, width};
    }
    settings.endArray();
    selectColor(ui->frequencyColor, styles.value("main").color);
    ui->frequencyLineWidth->setCurrentText(
        QString::number(styles.value("main").width));
    if (ui->fkFmin->value() >= ui->fkFmax->value() ||
        ui->fkCmin->value() >= ui->fkCmax->value())
        haveDisplayRange = false;
    restoring = false;
}
void FrequencySpectrumDialog::saveSettings()
{
    if (restoring)
        return;
    QSettings settings("WW", "SeiSeeMp");
    settings.beginGroup("FrequencySpectrum");
    settings.setValue("display/initialized", haveDisplayRange);
    settings.setValue("display/boundsVersion", physicalBounds ? 2 : 0);
    settings.setValue("display/amplitudeVersion", 2);
    for (auto spin : QList<QDoubleSpinBox *>{ui->fkFmin, ui->fkFmax, ui->fkCmin,
                                             ui->fkCmax})
        settings.setValue("display/" + spin->objectName(), spin->value());
    settings.setValue("display/grid", ui->frequencyGrid->isChecked());
    settings.setValue("display/fontSize", ui->frequencyFontSize->value());
    settings.setValue("advance/fkScale", ui->fkScale->currentIndex());
    settings.beginWriteArray("lineStyles", styles.size());
    int i = 0;
    for (auto it = styles.cbegin(); it != styles.cend(); ++it) {
        settings.setArrayIndex(i++);
        settings.setValue("id", it.key());
        settings.setValue("color", it.value().color.name());
        settings.setValue("width", it.value().width);
    }
    settings.endArray();
}
void FrequencySpectrumDialog::toast(const QString &text)
{
    auto notice = new Toast(this);
    notice->setDuration(7000);
    notice->setTitle(tr("Frequency Spectrum"));
    notice->setText(text);
    notice->setMaximumWidth(qMax(300, width() / 2));
    notice->applyPreset(ToastPreset::WARNING);
    notice->show();
}
bool FrequencySpectrumDialog::savePng(const QString &filename, int dpi)
{
    return !plot->curves.isEmpty() && savePlotImage(plot, filename, dpi);
}
void FrequencySpectrumDialog::exportImage(bool capture)
{
    if (plot->curves.isEmpty()) {
        toast(tr("Calculate spectra first."));
        return;
    }
    const int dpi = capture ? 0 : exportDpi();
    const QString details = QString("frequency_%1_%2_f_%3_%4_compare_%5")
                                .arg(plot->single ? "single" : "selection")
                                .arg(plot->logarithmic ? "dB" : "Abs")
                                .arg(plot->flo, 0, 'g', 8)
                                .arg(plot->fhi, 0, 'g', 8)
                                .arg(plot->curves.size() - 1);
    const QString name = nextImagePath(details, dpi);
    imageExportNotice(savePng(name, dpi), name);
}
void FrequencySpectrumDialog::shutdown()
{
    automaticTimer->stop();
    automaticPending = false;
    saveSettings();
    plot->band->hide();
    plot->windowZoom = false;
    plot->dragging = false;
    plot->unsetCursor();
    cancelled = true;
    ui->fkSelect->setChecked(false);
    emit selectionRequested(false);
    if (worker) {
        auto job = worker;
        worker = nullptr;
        job->disconnect();
        job->requestInterruption();
        job->wait();
        delete job;
    }
    pending.clear();
    batchRunning = false;
    ui->modeGroup->setEnabled(true);
    ui->comparisonGroup->setEnabled(true);
    for (auto widget : ui->inputGroup->findChildren<QWidget *>())
        widget->setEnabled(true);
    setInputMode();
    ui->fkCalculate->setIcon(QIcon(":/images/Run.png"));
    updateSourceState();
}
void FrequencySpectrumDialog::closeEvent(QCloseEvent *event)
{
    shutdown();
    QDialog::closeEvent(event);
}
void FrequencySpectrumDialog::reject()
{
    shutdown();
    QDialog::reject();
}
FrequencySpectrumDialog::~FrequencySpectrumDialog()
{
    shutdown();
    delete ui;
}
