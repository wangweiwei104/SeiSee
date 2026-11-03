#include "velocitydialog.h"
#include "fktransform.h"
#include "mystringtable.h"
#include "seisfile.h"
#include "Toast.h"
#include "ui_velocitydialog.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>

VelocityDialog::VelocityDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::VelocityDialog)
{
    ui->setupUi(this);
    qRegisterMetaType<VelocityGeometry>("VelocityGeometry");
    qRegisterMetaType<VelocityGeometrySamples>("VelocityGeometrySamples");
    line = ui->velocityLine;
    hyperbola = ui->velocityHyperbola;
    distanceMode = ui->velocityDistance;
    color = ui->velocityColor;
    width = ui->velocityWidth;
    visibleCurves = ui->velocityVisible;
    active = ui->velocityActive;
    pointFields[0] = ui->velocityP1Distance;
    pointFields[1] = ui->velocityP1Time;
    pointFields[2] = ui->velocityP2Distance;
    pointFields[3] = ui->velocityP2Time;
    traceInfo = ui->velocityTraceInfo;
    cancelButton = ui->velocityCancel;
    velocity = ui->velocityValue;
    intercept = ui->velocityT0;
    auto importButton = ui->velocityImport;
    exportButton = ui->velocityExport;
    csvType = ui->velocityCsvType;
    table = ui->velocityTable;
    undoButton = ui->velocityUndo;
    deleteButton = ui->velocityDelete;
    fileInfo = ui->fileInfo;
    status = ui->velocityStatus;
    progress = ui->velocityProgress;

    color->clear();
    for (const QString &name :
         {QString("Black"), QString("Blue"), QString("Red"),
          QString("DarkGreen"), QString("Orange"), QString("Magenta")}) {
        QPixmap swatch(14, 14);
        swatch.fill(QColor(name));
        color->addItem(QIcon(swatch), name, QColor(name));
    }
    color->addItem(tr("Custom..."), QColor());
    intercept->setToolTip(
        tr("Time origin is the raw trace sample origin. Displayed recording "
           "delay is removed while fitting and restored while drawing."));

    table->setColCount(5);
    table->verticalHeader()->hide();
    const QStringList headers = {"ID", tr("Type"), "t0 (ms)",
                                 tr("Velocity (m/s)"), tr("Source")};
    for (int i = 0; i < 5; ++i) {
        table->setColLabel(i, headers[i]);
        table->setColEdit(i, false);
    }
    table->setColWidthInCharacters(0, 4);
    table->setColWidthInCharacters(1, 9);
    table->setColWidthInCharacters(2, 10);
    table->setColWidthInCharacters(3, 13);
    table->setColWidthToStretch(4);
    table->horizontalHeader()->setSortIndicatorShown(true);
    for (auto button : findChildren<QPushButton *>())
        button->setAutoDefault(false);
    QSettings settings("WW", "SeiSeeMp");
    settings.beginGroup("VelocityPicking");
    line->setChecked(settings.value("line", false).toBool());
    hyperbola->setChecked(!line->isChecked());
    const int savedDistance = settings.value("distance", 0).toInt();
    distanceMode->setCurrentIndex(savedDistance == 1 ? 1 : 0);
    width->setCurrentText(settings.value("width", "1.5").toString());
    sortColumn = qBound(0, settings.value("sortColumn", 2).toInt(), 4);
    descending = settings.value("descending", false).toBool();
    table->horizontalHeader()->setSortIndicator(
        sortColumn,
        descending ? Qt::DescendingOrder : Qt::AscendingOrder);
    const QColor saved(settings.value("color", "black").toString());
    int ci = color->findData(saved);
    if (ci < 0 && saved.isValid()) {
        color->insertItem(0, saved.name(), saved);
        ci = 0;
    }
    if (ci >= 0)
        color->setCurrentIndex(ci);
    lastColor = color->currentData().value<QColor>();
    auto modeChanged = [this]() {
        if (updating)
            return;
        cancel();
        saveSettings();
    };
    connect(line, &QRadioButton::toggled, this, [=]() { modeChanged(); });
    connect(distanceMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=]() { modeChanged(); });
    auto styleChanged = [this]() {
        if (updating)
            return;
        QColor c = color->currentData().value<QColor>();
        if (!c.isValid()) {
            c = QColorDialog::getColor(lastColor, this, tr("Curve color"));
            if (!c.isValid()) {
                updating = true;
                color->setCurrentIndex(color->findData(lastColor));
                updating = false;
                return;
            }
            updating = true;
            color->insertItem(0, c.name(), c);
            color->setCurrentIndex(0);
            updating = false;
        }
        lastColor = c;
        for (auto &p : picks)
            if (p.id == selected) {
                p.color = c;
                p.width = width->currentText().toDouble();
            }
        preview.color = c;
        preview.width = width->currentText().toDouble();
        saveSettings();
        emit overlayChanged();
    };
    for (auto combo : {color, width})
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [=]() { styleChanged(); });
    connect(visibleCurves, &QCheckBox::toggled, this,
            [this]() { emit overlayChanged(); });
    connect(active, &QPushButton::toggled, this, &VelocityDialog::setArmed);
    connect(cancelButton, &QPushButton::clicked, this, &VelocityDialog::cancel);
    connect(undoButton, &QPushButton::clicked, this, &VelocityDialog::undo);
    connect(ui->velocityManualAdd, &QPushButton::clicked, this,
            &VelocityDialog::addManualPick);
    connect(deleteButton, &QPushButton::clicked, this, [this]() {
        if (!selected)
            return;
        snapshot();
        for (int i = 0; i < picks.size(); ++i)
            if (picks[i].id == selected) {
                picks.remove(i);
                break;
            }
        selected = 0;
        changed();
    });
    connect(table, &MyStringTable::rowEvent, this, [this](int row, int) {
        if (!updating && row >= 0)
            selectRecord(quint64(table->Tag(row, 0)));
    });
    connect(table->horizontalHeader(), &QHeaderView::sectionClicked, this,
            [this](int column) {
                if (column == sortColumn)
                    descending = !descending;
                else {
                    sortColumn = column;
                    descending = false;
                }
                refreshTable();
                saveSettings();
            });
    connect(importButton, &QPushButton::clicked, this, [this]() {
        importFiles(QFileDialog::getOpenFileNames(
            this, tr("Import velocity CSV"), QFileInfo(path).absolutePath(),
            tr("CSV (*.csv);;All files (*)")));
    });
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        const QString name = QFileDialog::getSaveFileName(
            this, tr("Export velocities"),
            QFileInfo(path).absolutePath() + "/" +
                QFileInfo(path).completeBaseName() + "_velocity.csv",
            tr("CSV (*.csv)"));
        if (!name.isEmpty())
            exportCsv(name);
    });
    updateFields();
    refreshTable();
    status->setText(tr("Open a SEG-Y file."));
}
VelocityDialog::~VelocityDialog()
{
    shutdown();
    delete ui;
}
VelocityPick VelocityDialog::defaults() const
{
    VelocityPick p;
    p.hyperbola = hyperbola->isChecked();
    p.distanceMode = distanceMode->currentIndex();
    p.color = color->currentData().value<QColor>();
    p.width = width->currentText().toDouble();
    return p;
}
QVector<VelocityPick> VelocityDialog::ordered() const
{
    auto result = picks;
    std::stable_sort(result.begin(), result.end(),
                     [this](const VelocityPick &a, const VelocityPick &b) {
                         int comparison = 0;
                         switch (sortColumn) {
                         case 0:
                             comparison =
                                 a.id < b.id ? -1 : a.id > b.id ? 1 : 0;
                             break;
                         case 1: {
                             const QString typeA =
                                 a.hyperbola ? QStringLiteral("Hyperbola")
                                             : QStringLiteral("Line");
                             const QString typeB =
                                 b.hyperbola ? QStringLiteral("Hyperbola")
                                             : QStringLiteral("Line");
                             comparison = QString::compare(
                                 typeA, typeB, Qt::CaseInsensitive);
                             break;
                         }
                         case 3:
                             comparison = a.velocity < b.velocity
                                              ? -1
                                              : a.velocity > b.velocity ? 1 : 0;
                             break;
                         case 4: {
                             const QString sourceA =
                                 a.origin.isEmpty()
                                     ? tr("Pick")
                                     : QFileInfo(a.origin).fileName();
                             const QString sourceB =
                                 b.origin.isEmpty()
                                     ? tr("Pick")
                                     : QFileInfo(b.origin).fileName();
                             comparison = QString::compare(
                                 sourceA, sourceB, Qt::CaseInsensitive);
                             break;
                         }
                         case 2:
                         default:
                             comparison =
                                 a.t0 < b.t0 ? -1 : a.t0 > b.t0 ? 1 : 0;
                             break;
                         }
                         if (comparison == 0)
                             comparison =
                                 a.id < b.id ? -1 : a.id > b.id ? 1 : 0;
                         return descending ? comparison > 0 : comparison < 0;
                     });
    return result;
}
void VelocityDialog::stopWorker()
{
    ++generation;
    if (worker) {
        auto job = worker;
        worker = nullptr;
        job->disconnect(this);
        job->requestInterruption();
        job->wake();
        job->wait();
        delete job;
    }
    progress->hide();
}
void VelocityDialog::setSource(SeisFile *file)
{
    const QString newPath = file && file->Active()
                                ? QString::fromLocal8Bit(file->Fname().c_str())
                                : QString();
    const QString newKey = spectrumSourceKey(newPath);
    if (newKey == key && (worker || sourceGeometry.traceCount > 0))
        return;
    if (!key.isEmpty())
        sessions[key] = picks;
    setArmed(false);
    stopWorker();
    sourceGeometry = VelocityGeometry();
    geometryCacheOrder.clear();
    pendingGeometry.clear();
    path = newPath;
    key = newKey;
    picks = sessions.value(key);
    history.clear();
    selected = 0;
    nextId = 1;
    for (const auto &p : picks)
        nextId = qMax(nextId, p.id + 1);
    fileInfo->setText(tr("Current File: %1").arg(QFileInfo(path).fileName()));
    fileInfo->setToolTip(path);
    changed();
    if (path.isEmpty()) {
        status->setText(tr("No current source"));
        active->setEnabled(false);
        return;
    }
    fileType = file->typeName() == "SEG-Y" ? 1 : file->typeIdx();
    active->setEnabled(false);
    auto job = new VelocityGeometryWorker(this);
    worker = job;
    job->path = path;
    job->key = key;
    job->type = fileType;
    const quint64 epoch = generation;
    progress->setValue(0);
    progress->show();
    status->setText(tr("Reading geometry headers..."));
    connect(job, &VelocityGeometryWorker::progress, this,
            [this, job, epoch](int n) {
                if (worker == job && generation == epoch) {
                    progress->setValue(n);
                    progress->setVisible(n < 100);
                }
            });
    connect(job, &VelocityGeometryWorker::opened, this,
            [this, job, epoch](int traceCount, bool assumedMetres) {
                if (worker != job || generation != epoch)
                    return;
                sourceGeometry.traceCount = traceCount;
                sourceGeometry.assumedMetres = assumedMetres;
                active->setEnabled(true);
                progress->hide();
                status->setText(
                    assumedMetres
                        ? tr("Ready; loading visible headers. Unspecified "
                             "length units assumed metres.")
                        : tr("Ready; loading visible headers as needed."));
                emit geometryReady();
                emit overlayChanged();
                if (requestedPicking && isVisible())
                    setArmed(true);
            });
    connect(job, &VelocityGeometryWorker::samplesReady, this,
            [this, job, epoch](const VelocityGeometrySamples &samples) {
                if (worker != job || generation != epoch)
                    return;
                for (int i = 0; i < samples.indices.size(); ++i) {
                    const int trace = samples.indices[i];
                    pendingGeometry.remove(trace);
                    if (!sourceGeometry.traces.contains(trace)) {
                        sourceGeometry.traces.insert(trace, samples.traces[i]);
                        geometryCacheOrder.enqueue(trace);
                    } else {
                        sourceGeometry.traces[trace] = samples.traces[i];
                    }
                }
                constexpr int geometryCacheLimit = 16384;
                while (geometryCacheOrder.size() > geometryCacheLimit) {
                    const int trace = geometryCacheOrder.dequeue();
                    sourceGeometry.traces.remove(trace);
                }
                emit geometryReady();
                emit overlayChanged();
            });
    connect(job, &VelocityGeometryWorker::failed, this,
            [this, job, epoch](const QString &s) {
                if (worker == job && generation == epoch) {
                    requestedPicking = false;
                    notice(s);
                }
            });
    job->start();
}
void VelocityDialog::setArmed(bool enabled)
{
    requestedPicking = enabled;
    enabled = enabled && sourceGeometry.traceCount > 0 && !key.isEmpty();
    const bool was = armed;
    armed = enabled;
    {
        QSignalBlocker block(active);
        active->setChecked(armed);
    }
    active->setText(armed ? tr("Stop picking") : tr("Start picking"));
    cancel();
    if (was != armed)
        emit armedChanged(armed);
}
void VelocityDialog::requestGeometry(int firstTrace, int lastTrace,
                                     int sampleCount)
{
    if (!worker || sourceGeometry.traceCount < 1 || sampleCount < 1)
        return;
    firstTrace = qBound(0, firstTrace, sourceGeometry.traceCount - 1);
    lastTrace = qBound(0, lastTrace, sourceGeometry.traceCount - 1);
    if (firstTrace > lastTrace)
        std::swap(firstTrace, lastTrace);
    const qint64 span = qint64(lastTrace) - firstTrace;
    const qint64 step = qMax<qint64>(1, (span + sampleCount - 1) / sampleCount);
    QVector<int> needed;
    for (qint64 trace = firstTrace; trace <= lastTrace; trace += step) {
        const int index = int(trace);
        if (!sourceGeometry.contains(index) && !pendingGeometry.contains(index)) {
            needed.append(index);
            pendingGeometry.insert(index);
        }
    }
    if (!sourceGeometry.contains(lastTrace) && !pendingGeometry.contains(lastTrace)) {
        needed.append(lastTrace);
        pendingGeometry.insert(lastTrace);
    }
    if (!needed.isEmpty())
        worker->requestSamples(needed);
}
void VelocityDialog::cancel()
{
    first = VelocityPoint();
    second = VelocityPoint();
    previewValid = false;
    updateFields();
    emit overlayChanged();
}
void VelocityDialog::snapshot()
{
    history.append(picks);
    if (history.size() > 30)
        history.removeFirst();
}
void VelocityDialog::undo()
{
    cancel();
    if (history.isEmpty())
        return;
    picks = history.takeLast();
    selected = 0;
    changed();
}
void VelocityDialog::addManualPick()
{
    if (key.isEmpty()) {
        notice(tr("Open a source file first."));
        return;
    }
    if (picks.size() >= 10000) {
        notice(tr("Use at most 10000 velocity curves."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add velocity"));
    auto form = new QFormLayout(&dialog);
    auto type = new QComboBox(&dialog);
    type->addItems({tr("Line"), tr("Hyperbola")});
    type->setCurrentIndex(hyperbola->isChecked() ? 1 : 0);

    auto t0 = new QDoubleSpinBox(&dialog);
    t0->setDecimals(1);
    t0->setRange(-1e9, 1e9);
    t0->setValue(0);

    auto speed = new QDoubleSpinBox(&dialog);
    speed->setDecimals(2);
    speed->setRange(-1e9, 1e9);
    speed->setValue(1500);

    auto updateRanges = [type, t0, speed]() {
        const bool isHyperbola = type->currentIndex() == 1;
        t0->setMinimum(isHyperbola ? 0 : -1e9);
        speed->setMinimum(isHyperbola ? 0.000001 : -1e9);
    };
    connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
            [updateRanges]() { updateRanges(); });
    updateRanges();

    form->addRow(tr("Type"), type);
    form->addRow(tr("t0 (ms)"), t0);
    form->addRow(tr("Velocity (m/s)"), speed);
    auto buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (speed->value() == 0) {
        notice(tr("Velocity must be nonzero."));
        return;
    }

    VelocityPick pick = defaults();
    pick.id = nextId++;
    pick.hyperbola = type->currentIndex() == 1;
    pick.t0 = t0->value();
    pick.velocity = speed->value();
    pick.origin = tr("Manual");
    snapshot();
    selected = pick.id;
    picks.append(pick);
    cancel();
    changed();
    selectRecord(selected);
    status->setText(tr("Manually added velocity."));
}
void VelocityDialog::point(int trace, double displayMs, bool displayDelay,
                           bool commit)
{
    if (!armed || trace < 0 || trace >= sourceGeometry.traceCount ||
        !std::isfinite(displayMs))
        return;
    if (!sourceGeometry.contains(trace)) {
        requestGeometry(trace, trace, 1);
        status->setText(tr("Loading geometry for the selected trace."));
        return;
    }
    VelocityPoint p;
    p.trace = trace;
    p.distance = sourceGeometry.distance(trace, defaults());
    p.displayTimeMs = displayMs;
    p.timeMs = displayMs -
               (displayDelay ? sourceGeometry.traces.value(trace).delayMs : 0);
    if (!std::isfinite(p.distance)) {
        status->setText(tr("Coordinate units are angular or unavailable; "
                           "select Offset."));
        return;
    }
    if (first.trace < 0) {
        if (commit) {
            first = p;
            selected = 0;
            refreshTable();
            second = VelocityPoint();
            previewValid = false;
            updateFields();
            emit overlayChanged();
            status->setText(
                tr("Move the mouse; click a second point to finish."));
        }
        return;
    }
    second = p;
    preview = defaults();
    QString error;
    previewValid = fitVelocity(first, second, &preview, &error);
    updateFields();
    emit overlayChanged();
    if (!previewValid) {
        status->setText(error);
        return;
    }
    if (!commit) {
        status->setText(tr("Preview curve. Click to confirm."));
        return;
    }
    if (picks.size() >= 10000) {
        notice(tr("Use at most 10000 velocity curves."));
        return;
    }
    snapshot();
    preview.id = nextId++;
    selected = preview.id;
    picks.append(preview);
    cancel();
    changed();
    status->setText(tr("Velocity saved."));
}
void VelocityDialog::refreshTable()
{
    updating = true;
    const auto sorted = ordered();
    table->setRowCount(sorted.size());
    int row = -1;
    for (int i = 0; i < sorted.size(); ++i) {
        const auto &p = sorted[i];
        table->setCell(i, 0, QString::number(p.id));
        table->setTag(i, 0, p.id);
        table->setCell(i, 1, p.hyperbola ? "Hyperbola" : "Line");
        table->setCell(i, 2, QString::number(p.t0, 'g', 8));
        table->setCell(i, 3, QString::number(p.velocity, 'g', 8));
        table->setCellToolTip(i, 2, QString::number(p.t0, 'g', 17));
        table->setCellToolTip(i, 3, QString::number(p.velocity, 'g', 17));
        table->setCell(i, 4,
                       p.origin.isEmpty() ? tr("Pick")
                                          : QFileInfo(p.origin).fileName());
        table->setCellToolTip(i, 4, p.origin);
        if (p.id == selected)
            row = i;
    }
    table->horizontalHeader()->setSortIndicator(
        sortColumn, descending ? Qt::DescendingOrder : Qt::AscendingOrder);
    if (row >= 0)
        table->setCurCell(row);
    else
        table->clearSelection();
    updating = false;
    undoButton->setEnabled(!history.isEmpty());
    deleteButton->setEnabled(selected != 0);
    exportButton->setEnabled(!picks.isEmpty());
}
void VelocityDialog::updateFields()
{
    const VelocityPick *p = nullptr;
    for (const auto &item : picks)
        if (item.id == selected) {
            p = &item;
            break;
        }
    const auto &a = first.trace >= 0 ? first : p ? p->first : first;
    const auto &b = first.trace >= 0 ? second : p ? p->second : second;
    pointFields[0]->setText(a.trace >= 0 ? QString::number(a.distance, 'g', 10)
                                         : "—");
    pointFields[1]->setText(
        a.trace >= 0 ? QString::number(a.displayTimeMs, 'g', 10) : "—");
    pointFields[2]->setText(b.trace >= 0 ? QString::number(b.distance, 'g', 10)
                                         : "—");
    pointFields[3]->setText(
        b.trace >= 0 ? QString::number(b.displayTimeMs, 'g', 10) : "—");
    traceInfo->setText(
        a.trace >= 0
            ? tr("Trace: %1 → %2")
                  .arg(a.trace + 1)
                  .arg(b.trace >= 0 ? QString::number(b.trace + 1) : "—")
            : tr("Trace: —"));
    const auto result =
        first.trace >= 0 ? (previewValid ? &preview : nullptr) : p;
    velocity->setText(result ? QString::number(result->velocity, 'g', 12)
                             : "—");
    intercept->setText(result ? QString::number(result->t0, 'g', 12) : "—");
    cancelButton->setEnabled(first.trace >= 0);
}
void VelocityDialog::selectRecord(quint64 id)
{
    selected = id;
    cancel();
    updating = true;
    for (const auto &p : picks)
        if (p.id == id) {
            line->setChecked(!p.hyperbola);
            hyperbola->setChecked(p.hyperbola);
            distanceMode->setCurrentIndex(p.distanceMode);
            int c = color->findData(p.color);
            if (c < 0) {
                color->insertItem(0, p.color.name(), p.color);
                c = 0;
            }
            color->setCurrentIndex(c);
            lastColor = p.color;
            width->setCurrentText(QString::number(p.width));
            break;
        }
    updating = false;
    deleteButton->setEnabled(id != 0);
    updateFields();
    emit overlayChanged();
}
void VelocityDialog::changed()
{
    if (!key.isEmpty())
        sessions[key] = picks;
    refreshTable();
    updateFields();
    emit recordsChanged();
    emit overlayChanged();
}
bool VelocityDialog::importFiles(const QStringList &filenames)
{
    if (filenames.isEmpty())
        return false;
    if (key.isEmpty()) {
        notice(tr("Open a source file first."));
        return false;
    }
    QVector<VelocityPick> additions;
    for (const auto &name : filenames) {
        QFile file(name);
        if (file.size() > 10 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
            notice(tr("Could not read %1 (maximum 10 MB).").arg(name));
            return false;
        }
        QVector<VelocityPick> part;
        QString error;
        if (!readVelocityCsv(QString::fromUtf8(file.readAll()), name,
                             csvType->currentIndex(), defaults(), &part,
                             &error)) {
            notice(QFileInfo(name).fileName() + ": " + error);
            return false;
        }
        additions += part;
    }
    if (picks.size() + additions.size() > 10000) {
        notice(tr("Use at most 10000 velocity rows."));
        return false;
    }
    snapshot();
    for (auto &p : additions) {
        p.id = nextId++;
        picks.append(p);
        selected = p.id;
    }
    cancel();
    changed();
    selectRecord(selected);
    status->setText(tr("Imported %1 velocities.").arg(additions.size()));
    return true;
}
bool VelocityDialog::exportCsv(const QString &filename)
{
    if (picks.isEmpty())
        return false;
    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        notice(file.errorString());
        return false;
    }
    const auto bytes = velocityCsv(ordered()).toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        notice(tr("Could not save CSV: %1").arg(filename));
        return false;
    }
    status->setText(
        tr("CSV saved: %1").arg(QDir::toNativeSeparators(filename)));
    return true;
}
bool VelocityDialog::hasCurves() const
{
    return isVisible() && visibleCurves->isChecked() &&
           sourceGeometry.traceCount > 0 &&
           (!picks.isEmpty() || first.trace >= 0);
}
void VelocityDialog::paintOverlay(
    QPainter &painter, const QRectF &clip, int from, int to,
    const std::function<QPointF(int, double)> &map, bool displayDelay) const
{
    if (!hasCurves())
        return;
    from = qBound(0, from, sourceGeometry.traceCount - 1);
    to = qBound(0, to, sourceGeometry.traceCount - 1);
    if (from > to)
        std::swap(from, to);
    painter.save();
    painter.setClipRect(clip, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);
    auto draw = [&](const VelocityPick &p, bool draft) {
        QPainterPath path;
        bool started = false;
        double previousX = -1e30;
        const int pixelSamples = qMax(1, qCeil(clip.width()));
        const qint64 traceStep = qMax<qint64>(
            1, (qint64(to) - from + pixelSamples - 1) / pixelSamples);
        for (qint64 trace = from; trace <= to; trace += traceStep) {
            const int traceIndex = int(trace);
            if (!sourceGeometry.contains(traceIndex)) {
                started = false;
                continue;
            }
            const double t =
                velocityTime(p, sourceGeometry.distance(traceIndex, p)) +
                (displayDelay
                     ? sourceGeometry.traces.value(traceIndex).delayMs
                     : 0);
            const QPointF pos = map(traceIndex, t);
            if (!std::isfinite(pos.x()) || !std::isfinite(pos.y()) ||
                std::abs(pos.x()) > 1e8 || std::abs(pos.y()) > 1e8) {
                started = false;
                continue;
            }
            if (started && std::abs(pos.x() - previousX) < .5 &&
                traceIndex != to)
                continue;
            if (!started)
                path.moveTo(pos);
            else
                path.lineTo(pos);
            started = true;
            previousX = pos.x();
        }
        if (to != from && sourceGeometry.contains(to)) {
            const double t =
                velocityTime(p, sourceGeometry.distance(to, p)) +
                (displayDelay ? sourceGeometry.traces.value(to).delayMs : 0);
            const QPointF pos = map(to, t);
            if (std::isfinite(pos.x()) && std::isfinite(pos.y())) {
                if (!started)
                    path.moveTo(pos);
                else
                    path.lineTo(pos);
            }
        }
        const bool highlighted = !draft && p.id == selected;
        const double stroke = p.width + (highlighted ? 1.5 : 0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, stroke + (highlighted ? 5 : 1.5),
                            draft ? Qt::DashLine : Qt::SolidLine));
        painter.drawPath(path);
        if (highlighted) {
            painter.setPen(QPen(QColor("#ff9f1c"), stroke + 3,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
        painter.setPen(
            QPen(p.color, stroke, draft ? Qt::DashLine : Qt::SolidLine));
        painter.drawPath(path);
    };
    for (const auto &p : picks)
        if (p.id != selected)
            draw(p, false);
    // Paint the selected curve last so overlapping curves cannot cover it.
    for (const auto &p : picks)
        if (p.id == selected)
            draw(p, false);
    if (previewValid && first.trace >= 0)
        draw(preview, true);
    auto marker = [&](const VelocityPoint &p, QColor color) {
        if (p.trace < from || p.trace > to)
            return;
        if (!sourceGeometry.contains(p.trace))
            return;
        const double t = p.timeMs +
                         (displayDelay
                              ? sourceGeometry.traces.value(p.trace).delayMs
                              : 0);
        painter.setPen(QPen(color, 1));
        painter.setBrush(Qt::white);
        painter.drawEllipse(map(p.trace, t), 3, 3);
    };
    if (first.trace >= 0) {
        marker(first, Qt::black);
        if (second.trace >= 0)
            marker(second, previewValid ? Qt::black : Qt::red);
    } else
        for (const auto &p : picks)
            if (p.id == selected) {
                marker(p.first, p.color);
                marker(p.second, p.color);
            }
    painter.restore();
}
void VelocityDialog::notice(const QString &message)
{
    status->setText(message);
    auto toast = new Toast(this);
    toast->setDuration(6000);
    toast->setTitle(tr("Velocity Picking"));
    toast->setText(message);
    toast->applyPreset(ToastPreset::WARNING);
    toast->show();
}
void VelocityDialog::saveSettings()
{
    QSettings s("WW", "SeiSeeMp");
    s.beginGroup("VelocityPicking");
    s.setValue("line", line->isChecked());
    s.setValue("distance", distanceMode->currentIndex());
    s.setValue("sortColumn", sortColumn);
    s.setValue("color", color->currentData().value<QColor>().name());
    s.setValue("width", width->currentText());
    s.setValue("descending", descending);
}
void VelocityDialog::shutdown()
{
    setArmed(false);
    stopWorker();
    saveSettings();
}
void VelocityDialog::closeEvent(QCloseEvent *event)
{
    shutdown();
    QDialog::closeEvent(event);
}
void VelocityDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    visibleCurves->setChecked(true);
    emit overlayChanged();
}
void VelocityDialog::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    setArmed(false);
    emit overlayChanged();
}
void VelocityDialog::reject()
{
    shutdown();
    QDialog::reject();
}
void VelocityDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (hasFirstPoint())
            cancel();
        else
            setArmed(false);
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        undo();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}
