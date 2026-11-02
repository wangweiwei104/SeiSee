#include <QtTest>
#include <QtWidgets>
#include <complex>
#include <limits>
#include <stdexcept>
#include "fkspectrumdialog.h"
#include "mainwindow.h"
#include "sgyfile.h"

class FkRegression : public QObject
{
    Q_OBJECT
    QString out, pathA, pathB, pathHalf, pathShort;
    template <class T> T *get(QObject *owner, const char *name)
    {
        T *widget = owner->findChild<T *>(name);
        if (!widget)
            qFatal("Missing control: %s", name);
        return widget;
    }
    void fixture(const QString &path, int nx, int nt, int microseconds,
                 double scale = 1)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(3200, ' '));
        auto u16 = [](QByteArray &bytes, int offset, int v) {
            bytes[offset] = char(v >> 8);
            bytes[offset + 1] = char(v);
        };
        QByteArray binary(400, 0);
        u16(binary, 16, microseconds);
        u16(binary, 20, nt);
        u16(binary, 24, 5);
        file.write(binary);
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::BigEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        for (int x = 0; x < nx; ++x) {
            QByteArray header(240, 0);
            header[3] = char(x + 1);
            u16(header, 114, nt);
            u16(header, 116, microseconds);
            file.write(header);
            for (int t = 0; t < nt; ++t)
                stream << float(scale * (std::sin(.12 * t + .23 * x) +
                                         .3 * std::cos(.27 * t - .11 * x)));
        }
    }
private slots:
    void initTestCase()
    {
        out = qEnvironmentVariable("SEISEE_TEST_OUT");
        QVERIFY(!out.isEmpty());
        QDir().mkpath(out + "/data");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           out + "/settings");
        QSettings settings("WW", "SeiSeeMp");
        settings.clear();
        pathA = out + "/data/source-a.sgy";
        pathB = out + "/data/source-b.sgy";
        pathHalf = out + "/data/half.sgy";
        pathShort = out + "/data/short.sgy";
        fixture(pathA, 96, 1024, 2000);
        fixture(pathB, 16, 256, 4000);
        fixture(pathHalf, 96, 1024, 2000, .5);
        fixture(pathShort, 4, 32, 2000);
        const int font =
            QFontDatabase::addApplicationFont(":/fonts/Roboto-Regular-14.ttf");
        QVERIFY(font >= 0);
        QApplication::setFont(
            QFont(QFontDatabase::applicationFontFamilies(font).first(), 9));
    }
    void fkSelectionFollowsInputsAndSurvivesFileChanges()
    {
        MainWindow window;
        window.show();
        QTest::qWait(250);
        QVERIFY(window.openFile(pathA));
        QTest::qWait(250);
        QMetaObject::invokeMethod(&window, "on_zoomAllBtn_pressed",
                                  Qt::DirectConnection);
        get<QPushButton>(&window, "fkBtn")->click();
        auto fk = get<FkSpectrumDialog>(&window, "FkSpectrumDialog");
        auto area = get<MyScrollArea>(&window, "seismicScrollArea");
        get<QPushButton>(fk, "fkSelect")->setChecked(true);
        QSignalSpy calculated(fk, &SpectrumDialog::spectrumUpdated);
        QSignalSpy mouseRanges(area, &MyScrollArea::selectionChanged);
        auto first = get<QSpinBox>(fk, "fkFirst");
        auto last = get<QSpinBox>(fk, "fkLast");
        auto start = get<QDoubleSpinBox>(fk, "fkStart");
        auto end = get<QDoubleSpinBox>(fk, "fkEnd");
        // Use actual keyboard commits, including fractional milliseconds.
        auto type = [](QAbstractSpinBox *spin, const QString &text) {
            spin->setFocus();
            spin->selectAll();
            QTest::keyClicks(spin, text);
            QTest::keyClick(spin, Qt::Key_Return);
        };
        type(first, "10");
        type(last, "40");
        type(start, "200.5");
        type(end, "800.5");
        const QRectF requested = fk->inputRange();
        QCOMPARE(requested, QRectF(QPointF(10, 200.5), QPointF(40, 800.5)));
        auto band = get<QWidget>(area->viewport(), "analysisSelectionBand");
        QTRY_VERIFY(band->isVisible());
        const QRect original = band->geometry();
        type(last, "60");
        QVERIFY(band->geometry().right() > original.right());
        type(start, "400.5");
        QVERIFY(band->geometry().top() > original.top());
        type(first, "20");
        QVERIFY(band->geometry().left() > original.left());
        type(end, "600.5");
        QVERIFY(band->geometry().bottom() < original.bottom());
        QCOMPARE(mouseRanges.count(), 0); // No pixel-to-input feedback loop.
        fk->setInitialRange(10, 40, 200.5, 800.5);
        const QRect beforeSwitch = band->geometry();
        window.grab().save(out + "/fk-input-range.png");

        QVERIFY(window.openFile(pathHalf)); // Same geometry, different data.
        QTest::qWait(250); // Let directory selection and viewport layout settle.
        QCOMPARE(get<QLabel>(fk, "fkFileStatus")->toolTip(), QDir::fromNativeSeparators(pathHalf));
        QTRY_VERIFY(band->isVisible());
        QCOMPARE(fk->inputRange(), requested);
        QCOMPARE(band->geometry(), beforeSwitch);
        QVERIFY(area->isSelectionMode());
        QVERIFY(!get<QPushButton>(&window, "zoomInBtn")->isEnabled());
        window.grab().save(out + "/fk-range-after-file-switch.png");
        QVERIFY(window.openFile(pathB)); // Fewer traces, different dt.
        QTest::qWait(250);
        QCOMPARE(get<QLabel>(fk, "fkFileStatus")->toolTip(), QDir::fromNativeSeparators(pathB));
        QTRY_VERIFY(band->isVisible());
        QCOMPARE(fk->inputRange(), requested);
        QVERIFY(band->geometry().width() < beforeSwitch.width());
        QVERIFY(window.openFile(pathShort)); // No overlap: hide the projection.
        QTest::qWait(250);
        QCOMPARE(get<QLabel>(fk, "fkFileStatus")->toolTip(), QDir::fromNativeSeparators(pathShort));
        QVERIFY(!band->isVisible());
        QCOMPARE(fk->inputRange(), requested);
        QVERIFY(window.openFile(pathA));
        QTest::qWait(250);
        QCOMPARE(get<QLabel>(fk, "fkFileStatus")->toolTip(), QDir::fromNativeSeparators(pathA));
        QTRY_VERIFY(band->isVisible());
        QCOMPARE(band->geometry(), beforeSwitch);
        QCOMPARE(fk->inputRange(), requested);
        QCOMPARE(calculated.count(), 0);

        // Mouse edits still update parameters after a programmatic projection.
        const QPoint from = band->geometry().center();
        const QPoint to = from + QPoint(10, 10);
        QTest::mousePress(area->viewport(), Qt::LeftButton, Qt::NoModifier, from);
        QMouseEvent move(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(area->viewport(), &move);
        QTest::mouseRelease(area->viewport(), Qt::LeftButton, Qt::NoModifier, to);
        QCOMPARE(mouseRanges.count(), 1);
        QVERIFY(fk->inputRange() != requested);
        QVERIFY(band->isVisible());
        const QRectF draggedRange = fk->inputRange();
        const QRect draggedBox = band->geometry();
        QVERIFY(window.openFile(pathHalf));
        QTest::qWait(250);
        QCOMPARE(get<QLabel>(fk, "fkFileStatus")->toolTip(), QDir::fromNativeSeparators(pathHalf));
        QCOMPARE(fk->inputRange(), draggedRange);
        QVERIFY(band->isVisible());
        QCOMPARE(band->geometry(), draggedBox);
        get<QPushButton>(fk, "fkClearSelection")->click();
        QVERIFY(!band->isVisible());
        QVERIFY(!area->isSelectionMode());
        QVERIFY(window.openFile(pathHalf));
        QTest::qWait(100);
        QVERIFY(!band->isVisible());
        window.close();
    }
};
QTEST_MAIN(FkRegression)
#include "fk_regression.moc"
