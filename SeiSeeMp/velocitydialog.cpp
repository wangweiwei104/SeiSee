#include "velocitydialog.h"
#include "fktransform.h"
#include "gfxstyle.h"
#include "mystringtable.h"
#include "seisfile.h"
#include "Toast.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>

namespace {
    QGroupBox *group(const QString &name, QVBoxLayout *column)
    {
        auto box = new QGroupBox(name);
        column->addWidget(box);
        return box;
    }
    QLineEdit *readout(const char *name)
    {
        auto edit = new QLineEdit;
        edit->setObjectName(name);
        edit->setReadOnly(true);
        edit->setAlignment(Qt::AlignRight);
        edit->setMinimumWidth(70);
        edit->setText("—");
        return edit;
    }
} // namespace
VelocityDialog::VelocityDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("VelocityDialog");
    setWindowTitle(tr("Velocity Picking"));
    setWindowIcon(QIcon(":/images/SeiSeeMp.png"));
    setModal(false);
    resize(1010, 440);
    qRegisterMetaType<VelocityGeometry>("VelocityGeometry");
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(5);
    auto columns = new QHBoxLayout;
    columns->setSpacing(6);
    outer->addLayout(columns, 1);
    auto left = new QVBoxLayout, middle = new QVBoxLayout,
         right = new QVBoxLayout;
    columns->addLayout(left, 0);
    columns->addLayout(middle, 0);
    columns->addLayout(right, 1);
    auto modeBox = group(tr("Mode"), left);
    modeBox->setObjectName("velocityModeGroup");
    auto mode = new QGridLayout(modeBox);
    line = new QRadioButton(tr("Line"));
    line->setObjectName("velocityLine");
    line->setToolTip(
        tr("t = t0 + x / v; velocity retains the line slope sign."));
    hyperbola = new QRadioButton(tr("Hyperbola"));
    hyperbola->setObjectName("velocityHyperbola");
    hyperbola->setToolTip(
        tr("t = sqrt(t0^2 + (x / v)^2); both points define t0 and v."));
    hyperbola->setChecked(true);
    mode->addWidget(line, 0, 0);
    mode->addWidget(hyperbola, 0, 1);
    distanceMode = new QComboBox;
    distanceMode->setObjectName("velocityDistance");
    distanceMode->addItems({tr("Coordinates (m)"), tr("Offset (signed m)"),
                            tr("Trace spacing (m)")});
    distanceMode->setToolTip(
        tr("Coordinates: source/receiver distance, SEG-Y coordinate scalar and "
           "metre/foot units. Angular coordinates require Offset or spacing."));
    mode->addWidget(new QLabel(tr("Distance")), 1, 0);
    mode->addWidget(distanceMode, 1, 1);
    gatherMode = new QComboBox;
    gatherMode->setObjectName("velocityGather");
    gatherMode->addItems({tr("Auto"), tr("Field record (9)"),
                          tr("Source point (17)"), tr("Channel reset (13)"),
                          tr("Single gather")});
    mode->addWidget(new QLabel(tr("Gather by")), 2, 0);
    mode->addWidget(gatherMode, 2, 1);
    spacing = new QDoubleSpinBox;
    spacing->setObjectName("velocitySpacing");
    spacing->setDecimals(4);
    spacing->setRange(.0001, 1e8);
    spacing->setValue(25);
    spacing->setEnabled(false);
    mode->addWidget(new QLabel(tr("Dx (m)")), 3, 0);
    mode->addWidget(spacing, 3, 1);
    auto displayBox = group(tr("Display"), left);
    displayBox->setObjectName("velocityDisplayGroup");
    auto display = new QGridLayout(displayBox);
    color = new QComboBox;
    color->setObjectName("velocityColor");
    for (const QString &name :
         {QString("Black"), QString("Blue"), QString("Red"),
          QString("DarkGreen"), QString("Orange"), QString("Magenta")}) {
        QPixmap swatch(14, 14);
        swatch.fill(QColor(name));
        color->addItem(QIcon(swatch), name, QColor(name));
    }
    color->addItem(tr("Custom..."), QColor());
    width = new QComboBox;
    width->setObjectName("velocityWidth");
    width->addItems({"0.5", "1", "1.5", "2", "3", "4"});
    width->setCurrentText("1.5");
    display->addWidget(new QLabel(tr("Color")), 0, 0);
    display->addWidget(color, 0, 1);
    display->addWidget(new QLabel(tr("Line Width")), 1, 0);
    display->addWidget(width, 1, 1);
    visibleCurves = new QCheckBox(tr("Show curves"));
    visibleCurves->setObjectName("velocityVisible");
    visibleCurves->setChecked(true);
    display->addWidget(visibleCurves, 2, 0, 1, 2);
    display->addWidget(new QLabel(tr("Scope: all gathers in current file")), 3,
                       0, 1, 2);
    active = new QPushButton(tr("Start picking"));
    active->setObjectName("velocityActive");
    active->setCheckable(true);
    left->addWidget(active);
    left->addStretch();

    auto pointsBox = group(tr("Pick Points"), middle);
    pointsBox->setObjectName("velocityPointsGroup");
    auto points = new QGridLayout(pointsBox);
    points->addWidget(new QLabel(tr("Distance (m)")), 0, 1);
    points->addWidget(new QLabel(tr("Time (ms)")), 0, 2);
    const char *names[] = {"velocityP1Distance", "velocityP1Time",
                           "velocityP2Distance", "velocityP2Time"};
    for (int i = 0; i < 4; ++i) {
        pointFields[i] = readout(names[i]);
        points->addWidget(pointFields[i], i / 2 + 1, i % 2 + 1);
    }
    points->addWidget(new QLabel("P1"), 1, 0);
    points->addWidget(new QLabel("P2"), 2, 0);
    traceInfo = new QLabel(tr("Trace: —"));
    points->addWidget(traceInfo, 3, 0, 1, 3);
    cancelButton = new QPushButton(tr("Cancel current"));
    cancelButton->setObjectName("velocityCancel");
    points->addWidget(cancelButton, 4, 1, 1, 2);
    auto resultBox = group(tr("Result"), middle);
    resultBox->setObjectName("velocityResultGroup");
    auto result = new QGridLayout(resultBox);
    result->addWidget(new QLabel(tr("Velocity (m/s)")), 0, 0);
    result->addWidget(new QLabel(tr("t0 (ms)")), 0, 1);
    velocity = readout("velocityValue");
    intercept = readout("velocityT0");
    intercept->setToolTip(
        tr("Time origin is the raw trace sample origin. Displayed recording "
           "delay is removed while fitting and restored while drawing."));
    result->addWidget(velocity, 1, 0);
    result->addWidget(intercept, 1, 1);
    auto csvBox = group(tr("CSV"), middle);
    csvBox->setObjectName("velocityCsvGroup");
    auto csv = new QGridLayout(csvBox);
    auto importButton = new QPushButton(tr("Import CSV"));
    importButton->setObjectName("velocityImport");
    exportButton = new QPushButton(tr("Export CSV"));
    exportButton->setObjectName("velocityExport");
    csvType = new QComboBox;
    csvType->setObjectName("velocityCsvType");
    csvType->addItems({tr("Auto"), tr("Line"), tr("Hyperbola")});
    csv->addWidget(importButton, 0, 0);
    csv->addWidget(exportButton, 0, 1);
    csv->addWidget(new QLabel(tr("CSV Type")), 1, 0);
    csv->addWidget(csvType, 1, 1);
    csv->addWidget(new QLabel("t0(ms), velocity(m/s)"), 2, 0, 1, 2);
    middle->addStretch();
    auto picksBox = group(tr("Picks"), right);
    picksBox->setObjectName("velocityPicksGroup");
    auto list = new QVBoxLayout(picksBox);
    table = new MyStringTable;
    table->setObjectName("velocityTable");
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
    table->setMinimumSize(390, 240);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSortIndicator(2, Qt::AscendingOrder);
    list->addWidget(table, 1);
    auto actions = new QHBoxLayout;
    undoButton = new QPushButton(tr("Undo"));
    undoButton->setObjectName("velocityUndo");
    deleteButton = new QPushButton(tr("Delete selected"));
    deleteButton->setObjectName("velocityDelete");
    actions->addStretch();
    actions->addWidget(undoButton);
    actions->addWidget(deleteButton);
    list->addLayout(actions);
    auto footer = new QHBoxLayout;
    fileInfo = new QLabel;
    status = new QLabel;
    status->setObjectName("velocityStatus");
    status->setWordWrap(true);
    for (auto label : {fileInfo, status}) {
        label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
        label->setMargin(3);
    }
    progress = new QProgressBar;
    progress->setObjectName("velocityProgress");
    progress->setFixedWidth(95);
    progress->hide();
    footer->addWidget(fileInfo);
    footer->addWidget(status, 1);
    footer->addWidget(progress);
    outer->addLayout(footer);
    for (auto button : findChildren<QPushButton *>())
        button->setAutoDefault(false);
    QSettings settings("WW", "SeiSeeMp");
    settings.beginGroup("VelocityPicking");
    line->setChecked(settings.value("line", false).toBool());
    hyperbola->setChecked(!line->isChecked());
    distanceMode->setCurrentIndex(
        qBound(0, settings.value("distance", 0).toInt(), 2));
    gatherMode->setCurrentIndex(
        qBound(0, settings.value("gather", 0).toInt(), 4));
    spacing->setValue(settings.value("spacing", 25).toDouble());
    spacing->setEnabled(distanceMode->currentIndex() == 2);
    width->setCurrentText(settings.value("width", "1.5").toString());
    descending = settings.value("descending", false).toBool();
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
        spacing->setEnabled(distanceMode->currentIndex() == 2);
        cancel();
        saveSettings();
    };
    connect(line, &QRadioButton::toggled, this, [=]() { modeChanged(); });
    for (auto combo : {distanceMode, gatherMode})
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [=]() { modeChanged(); });
    connect(spacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [=]() { modeChanged(); });
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
    connect(table, &MyStringTable::hHeaderEvent, this, [this](int column) {
        if (column == 2) {
            descending = !descending;
            refreshTable();
            saveSettings();
        }
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
}
VelocityPick VelocityDialog::defaults() const
{
    VelocityPick p;
    p.hyperbola = hyperbola->isChecked();
    p.distanceMode = distanceMode->currentIndex();
    p.gatherMode = gatherMode->currentIndex();
    p.spacing = spacing->value();
    p.color = color->currentData().value<QColor>();
    p.width = width->currentText().toDouble();
    return p;
}
QVector<VelocityPick> VelocityDialog::ordered() const
{
    auto result = picks;
    std::stable_sort(result.begin(), result.end(),
                     [this](const VelocityPick &a, const VelocityPick &b) {
                         return a.t0 == b.t0 ? a.id < b.id
                                : descending ? a.t0 > b.t0
                                             : a.t0 < b.t0;
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
    if (newKey == key && (worker || !sourceGeometry.traces.isEmpty()))
        return;
    if (!key.isEmpty())
        sessions[key] = picks;
    setArmed(false);
    stopWorker();
    sourceGeometry = VelocityGeometry();
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
                if (worker == job && generation == epoch)
                    progress->setValue(n);
            });
    connect(job, &VelocityGeometryWorker::ready, this,
            [this, job, epoch](const VelocityGeometry &g) {
                if (worker != job || generation != epoch)
                    return;
                sourceGeometry = g;
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
    connect(job, &QThread::finished, this, [this, job, epoch]() {
        if (worker != job || generation != epoch)
            return;
        worker = nullptr;
        job->deleteLater();
        progress->hide();
        active->setEnabled(!sourceGeometry.traces.isEmpty());
        if (!sourceGeometry.traces.isEmpty()) {
            status->setText(
                sourceGeometry.assumedMetres
                    ? tr("Headers ready; unspecified length units assumed "
                         "metres.")
                    : tr("Headers ready. Pick on the main seismic view."));
            if (requestedPicking && isVisible())
                setArmed(true);
        }
    });
    job->start();
}
void VelocityDialog::setArmed(bool enabled)
{
    requestedPicking = enabled;
    enabled = enabled && !sourceGeometry.traces.isEmpty() && !key.isEmpty();
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
void VelocityDialog::point(int trace, double displayMs, bool displayDelay,
                           bool commit)
{
    if (!armed || trace < 0 || trace >= sourceGeometry.traces.size() ||
        !std::isfinite(displayMs))
        return;
    VelocityPoint p;
    p.trace = trace;
    p.distance = sourceGeometry.distance(trace, defaults());
    p.displayTimeMs = displayMs;
    p.timeMs =
        displayMs - (displayDelay ? sourceGeometry.traces[trace].delayMs : 0);
    if (!std::isfinite(p.distance)) {
        status->setText(tr("Coordinate units are angular or unavailable; "
                           "select Offset or spacing."));
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
        status->setText(tr("Preview across all gathers. Click to confirm."));
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
    status->setText(tr("Velocity saved across all gathers."));
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
        2, descending ? Qt::DescendingOrder : Qt::AscendingOrder);
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
            gatherMode->setCurrentIndex(p.gatherMode);
            spacing->setValue(p.spacing);
            spacing->setEnabled(p.distanceMode == 2);
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
    status->setText(
        tr("Imported %1 velocities across all gathers.").arg(additions.size()));
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
    return isVisible() && visibleCurves->isChecked() && !sourceGeometry.traces.isEmpty() &&
           (!picks.isEmpty() || first.trace >= 0);
}
void VelocityDialog::paintOverlay(
    QPainter &painter, const QRectF &clip, int from, int to,
    const std::function<QPointF(int, double)> &map, bool displayDelay) const
{
    if (!hasCurves())
        return;
    from = qBound(0, from, sourceGeometry.traces.size() - 1);
    to = qBound(0, to, sourceGeometry.traces.size() - 1);
    if (from > to)
        std::swap(from, to);
    painter.save();
    painter.setClipRect(clip, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);
    auto draw = [&](const VelocityPick &p, bool draft) {
        QPainterPath path;
        bool started = false;
        int previousGroup = -1;
        double previousX = -1e30;
        for (int trace = from; trace <= to; ++trace) {
            const int g = sourceGeometry.first(trace, p.gatherMode);
            const double t =
                velocityTime(p, sourceGeometry.distance(trace, p)) +
                (displayDelay ? sourceGeometry.traces[trace].delayMs : 0);
            const QPointF pos = map(trace, t);
            if (!std::isfinite(pos.x()) || !std::isfinite(pos.y()) ||
                std::abs(pos.x()) > 1e8 || std::abs(pos.y()) > 1e8) {
                started = false;
                previousGroup = g;
                continue;
            }
            const bool boundary = g != previousGroup;
            previousGroup = g;
            if (started && !boundary && std::abs(pos.x() - previousX) < .5 &&
                trace != to)
                continue;
            if (!started || boundary)
                path.moveTo(pos);
            else
                path.lineTo(pos);
            started = true;
            previousX = pos.x();
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
        const double t =
            p.timeMs +
            (displayDelay ? sourceGeometry.traces[p.trace].delayMs : 0);
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
    s.setValue("gather", gatherMode->currentIndex());
    s.setValue("spacing", spacing->value());
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
