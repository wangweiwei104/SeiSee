#ifndef VELOCITYDIALOG_H
#define VELOCITYDIALOG_H

#include "velocitypick.h"
#include <QDialog>
#include <QHash>
#include <QPainter>
#include <functional>

class SeisFile;
class MyStringTable;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;
class QRadioButton;

class VelocityDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VelocityDialog(QWidget *parent = nullptr);
    ~VelocityDialog() override;
    void setSource(SeisFile *file);
    void shutdown();
    bool isPreparing() const
    {
        return worker != nullptr;
    }
    bool isArmed() const
    {
        return armed;
    }
    bool hasFirstPoint() const
    {
        return first.trace >= 0;
    }
    bool hasCurves() const;
    void setArmed(bool enabled);
    void cancel();
    void undo();
    void point(int trace, double displayMs, bool displayDelay, bool commit);
    bool importFiles(const QStringList &filenames);
    bool exportCsv(const QString &filename);
    const QVector<VelocityPick> &records() const
    {
        return picks;
    }
    const VelocityGeometry &geometry() const
    {
        return sourceGeometry;
    }
    void paintOverlay(QPainter &painter, const QRectF &clip, int firstTrace,
                      int lastTrace,
                      const std::function<QPointF(int, double)> &map,
                      bool displayDelay) const;
signals:
    void armedChanged(bool enabled);
    void overlayChanged();
    void recordsChanged();
    void geometryReady();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void reject() override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRadioButton *line, *hyperbola;
    QComboBox *distanceMode, *gatherMode, *color, *width, *csvType;
    QDoubleSpinBox *spacing;
    QLineEdit *pointFields[4], *velocity, *intercept;
    QLabel *traceInfo, *fileInfo, *status;
    QPushButton *active, *cancelButton, *undoButton, *deleteButton,
        *exportButton;
    QProgressBar *progress;
    QCheckBox *visibleCurves;
    MyStringTable *table;
    QString path, key;
    int fileType = 1;
    bool armed = false, requestedPicking = false, updating = false,
         descending = false;
    quint64 nextId = 1, selected = 0, generation = 0;
    QVector<VelocityPick> picks;
    QVector<QVector<VelocityPick>> history;
    QHash<QString, QVector<VelocityPick>> sessions;
    VelocityPoint first, second;
    VelocityPick preview;
    QColor lastColor = Qt::black;
    bool previewValid = false;
    VelocityGeometry sourceGeometry;
    VelocityGeometryWorker *worker = nullptr;
    VelocityPick defaults() const;
    QVector<VelocityPick> ordered() const;
    void stopWorker();
    void refreshTable();
    void updateFields();
    void selectRecord(quint64 id);
    void snapshot();
    void changed();
    void notice(const QString &message);
    void saveSettings();
};

#endif
