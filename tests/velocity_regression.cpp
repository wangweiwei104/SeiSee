#include <QtTest>
#include <QtWidgets>
#include <cmath>
#include "velocitydialog.h"
#include "mainwindow.h"
#include "sgyfile.h"
#include "util2.h"
#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#endif

class VelocityRegression : public QObject
{
    Q_OBJECT
    QString out, path, feet, angular, other;
    template <class T> T *get(QObject *owner, const char *name)
    {
        auto w = owner->findChild<T *>(name);
        if (!w)
            qFatal("Missing %s", name);
        return w;
    }
    void fixture(const QString &name, int measurement = 1, int units = 1,
                 int headers = 0)
    {
        QFile file(name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(3200, ' '));
        auto put = [](QByteArray &b, int pos, qint32 value, int size) {
            quint32 v = quint32(value);
            for (int i = 0; i < size; ++i)
                b[pos + i] = char(v >> (8 * (size - i - 1)));
        };
        QByteArray binary(400, 0);
        put(binary, 16, 2000, 2);
        put(binary, 20, 1201, 2);
        put(binary, 24, 5, 2);
        put(binary, 54, measurement, 2);
        file.write(binary);
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::BigEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        for (int i = 0; i < 36; ++i) {
            const int g = i / 12, c = i % 12,
                      local = headers == 1 || headers == 3 ? i
                              : g == 2 ? 11 - c : c;
            QByteArray h(240, 0);
            put(h, 0, i + 1, 4);
            put(h, 8, 100 + g, 4);
            put(h, 12, c + 1, 4);
            put(h, 16, 200 + g, 4);
            if (headers == 1 || headers == 2)
                put(h, 8, i + 1, 4); // Common per-trace record numbering.
            if (headers == 1) {
                put(h, 16, 0, 4);
                put(h, 12, i + 1, 4);
            }
            if (headers == 3) {
                put(h, 8, 100, 4);
                put(h, 16, 200, 4);
                put(h, 12, 1, 4); // A constant channel is not a gather reset.
            }
            put(h, 36, (g == 0 ? -1 : 1) * local * 100, 4);
            put(h, 70, -10, 2);
            put(h, 72, 10000, 4);
            put(h, 76, 20000, 4);
            put(h, 80, 10000 + local * 1000, 4);
            put(h, 84, 20000, 4);
            put(h, 88, units, 2);
            put(h, 108, g * 100, 2);
            put(h, 114, 1201, 2);
            put(h, 116, 2000, 2);
            file.write(h);
            for (int t = 0; t < 1201; ++t) {
                double dt = t * 2 - std::hypot(500., local * 100. / 2);
                double z = std::acos(-1.) * .025 * dt;
                stream << float((1 - 2 * z * z) * std::exp(-z * z));
            }
        }
    }
    void open(SgyFile &f, const QString &name)
    {
        f.setFname(name);
        f.setActive(true);
        QVERIFY(f.Active());
    }
    int magenta(const QImage &img)
    {
        int n = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                auto p = img.pixel(x, y);
                if (qRed(p) > 180 && qBlue(p) > 180 && qGreen(p) < 100)
                    ++n;
            }
        return n;
    }
    QString write(const QString &name, const QString &text)
    {
        QFile f(out + "/" + name);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("write failed");
        f.write(text.toUtf8());
        f.close();
        return f.fileName();
    }
private slots:
    void initTestCase()
    {
        int font =
            QFontDatabase::addApplicationFont(":/fonts/Roboto-Regular-14.ttf");
        QVERIFY(font >= 0);
        QApplication::setFont(
            QFont(QFontDatabase::applicationFontFamilies(font).first(), 9));
        QFontDatabase::addApplicationFont(":/fonts/TIMES.TTF");
        out = qEnvironmentVariable("SEISEE_VELOCITY_TEST_OUT");
        QVERIFY(!out.isEmpty());
        QDir().mkpath(out);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           out + "/settings");
        QSettings("WW", "SeiSeeMp").clear();
        path = out + "/gathers.sgy";
        feet = out + "/feet.sgy";
        angular = out + "/angular.sgy";
        other = out + "/other.sgy";
        fixture(path);
        fixture(feet, 2);
        fixture(angular, 1, 2);
        fixture(other);
    }
    void lineAndHyperbolaFit()
    {
        VelocityPoint a, b;
        a.trace = 1;
        b.trace = 2;
        a.distance = 100;
        b.distance = 1100;
        a.timeMs = 150;
        b.timeMs = 650;
        VelocityPick p;
        p.hyperbola = false;
        QString error;
        QVERIFY(fitVelocity(a, b, &p, &error));
        QVERIFY(std::abs(p.velocity - 2000) < 1e-8);
        QVERIFY(std::abs(p.t0 - 100) < 1e-8);
        p.hyperbola = true;
        a.distance = 500;
        b.distance = 1500;
        a.timeMs = 1000 * std::hypot(.5, .25);
        b.timeMs = 1000 * std::hypot(.5, .75);
        QVERIFY(fitVelocity(a, b, &p, &error));
        QVERIFY(std::abs(p.velocity - 2000) < 1e-8);
        QVERIFY(std::abs(p.t0 - 500) < 1e-8);
        QVERIFY(std::abs(velocityTime(p, 1500) - b.timeMs) < 1e-8);
        b.distance = a.distance;
        QVERIFY(!fitVelocity(a, b, &p, &error));
        b.distance = -a.distance;
        QVERIFY(!fitVelocity(a, b, &p, &error));
        b.distance = 1500;
        b.timeMs = 100;
        QVERIFY(!fitVelocity(a, b, &p, &error));
        p.hyperbola = false;
        b.timeMs = a.timeMs;
        QVERIFY(!fitVelocity(a, b, &p, &error));
        a.distance = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!fitVelocity(a, b, &p, &error));
    }
    void csvValidationAndRoundtrip()
    {
        VelocityPick d;
        d.distanceMode = 2;
        d.gatherMode = 3;
        d.spacing = 12.5;
        QVector<VelocityPick> p;
        QString error;
        QVERIFY(readVelocityCsv("\ufeff\"t0(ms)\",velocity(m/"
                                "s),freq0(Hz),Amp\r\n1000,2400,25,1\r\n",
                                "curve_vel.csv", 0, d, &p, &error));
        QCOMPARE(p.size(), 1);
        QVERIFY(p[0].hyperbola);
        QCOMPARE(p[0].spacing, 12.5);
        QVector<VelocityPick> again;
        QVERIFY(readVelocityCsv(velocityCsv(p), "velocities.csv", 0,
                                VelocityPick(), &again, &error));
        QCOMPARE(again[0].t0, 1000.);
        QCOMPARE(again[0].velocity, 2400.);
        QCOMPARE(again[0].distanceMode, 2);
        QCOMPARE(again[0].gatherMode, 3);
        QVERIFY(!readVelocityCsv("t0(ms),velocity(m/s)\n100,2000\n",
                                 "unknown.csv", 0, d, &again, &error));
        QCOMPARE(again.size(), 1);
        QVERIFY(readVelocityCsv("t0(s),velocity(m/s)\n.1,-2000\n",
                                "unknown.csv", 1, d, &again, &error));
        QCOMPARE(again[0].t0, 100.);
        QCOMPARE(again[0].velocity, -2000.);
        for (auto bad : {QString("t0(ms),velocity(m/s)\n100,0\n"),
                         QString("t0(ms),velocity(m/s)\n-100,2400\n"),
                         QString("t0(ms),velocity(m/s)\n100,NaN\n"),
                         QString("\"t0(ms),velocity(m/s)\n100,2400")})
            QVERIFY(!readVelocityCsv(bad, "curve.csv", 0, d, &again, &error));
    }
    void geometryScalarsUnitsAndGathers()
    {
        for (const auto &name : {path, feet, angular}) {
            SgyFile f;
            open(f, name);
            VelocityDialog dialog;
            dialog.setSource(&f);
            QTRY_VERIFY_WITH_TIMEOUT(!dialog.isPreparing(), 10000);
            const auto &g = dialog.geometry();
            QCOMPARE(g.traces.size(), 36);
            QCOMPARE(g.autoGather, 1);
            QCOMPARE(g.first(13, 0), 12);
            QCOMPARE(g.first(25, 3), 24);
            QCOMPARE(g.first(25, 4), 0);
            if (name == angular)
                QVERIFY(std::isnan(g.traces[1].distance));
            else
                QVERIFY(std::abs(g.traces[1].distance -
                                 (name == feet ? 30.48 : 100)) < 1e-9);
            QVERIFY(std::abs(g.traces[1].offset -
                             (name == feet ? -30.48 : -100)) < 1e-9);
            QCOMPARE(g.traces[13].delayMs, 100.);
            if (name != angular)
                QCOMPARE(g.traces[35].distance, 0.);
        }
    }
    void suGeometry()
    {
        QFile source(path);
        QVERIFY(source.open(QIODevice::ReadOnly));
        source.seek(3600);
        QFile output(out + "/gathers.su");
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(source.readAll());
        output.close();
        SuFile file;
        file.setFname(output.fileName());
        file.setActive(true);
        QVERIFY(file.Active());
        VelocityDialog dialog;
        dialog.setSource(&file);
        QTRY_VERIFY(!dialog.isPreparing());
        QCOMPARE(dialog.geometry().traces.size(), 36);
        QCOMPARE(dialog.geometry().traces[1].distance, 100.);
        QCOMPARE(dialog.geometry().first(13, 0), 12);
        QVERIFY(dialog.geometry().assumedMetres);
    }
    void dialogTwoClicksSortingCsvAndCache()
    {
        QSettings s("WW", "SeiSeeMp");
        s.remove("VelocityPicking");
        SgyFile f, b;
        open(f, path);
        open(b, other);
        VelocityDialog dialog;
        dialog.show();
        dialog.setSource(&f);
        QTRY_VERIFY(!dialog.isPreparing());
        get<QRadioButton>(&dialog, "velocityLine")->setChecked(true);
        dialog.setArmed(true);
        dialog.point(1, 150, false, true);
        dialog.point(8, 500, false, false);
        QVERIFY(dialog.records().isEmpty());
        QVERIFY(dialog.hasFirstPoint());
        dialog.point(8, 500, false, true);
        QCOMPARE(dialog.records().size(), 1);
        QVERIFY(!dialog.hasFirstPoint());
        QVERIFY(std::abs(dialog.records()[0].velocity - 2000) < 1e-8);
        QVERIFY(std::abs(dialog.records()[0].t0 - 100) < 1e-8);
        const auto one = dialog.records()[0].id;
        const QString imported = write(
            "curve_vel.csv", "t0(ms),velocity(m/s)\n900,2700\n400,2400\n");
        QVERIFY(dialog.importFiles({imported}));
        auto table = get<MyStringTable>(&dialog, "velocityTable");
        QCOMPARE(table->Cell(0, 2), QString("100"));
        QCOMPARE(table->Cell(1, 2), QString("400"));
        table->hHeaderPressed(2);
        QCOMPARE(table->Cell(0, 2), QString("900"));
        QCOMPARE(table->Tag(2, 0), qint64(one));
        table->setCurCell(1);
        get<QPushButton>(&dialog, "velocityDelete")->click();
        QCOMPARE(dialog.records().size(), 2);
        for (const auto &p : dialog.records())
            QVERIFY(p.t0 != 400);
        dialog.undo();
        QCOMPARE(dialog.records().size(), 3);
        QVERIFY(dialog.exportCsv(out + "/exported.csv"));
        QFile exported(out + "/exported.csv");
        QVERIFY(exported.open(QIODevice::ReadOnly));
        QVERIFY(exported.readAll().contains("900"));
        const int n = dialog.records().size();
        QVERIFY(!dialog.importFiles(
            {write("curve_bad.csv", "t0(ms),velocity(m/s)\n1000,0\n")}));
        QCOMPARE(dialog.records().size(), n);
        dialog.grab().save(out + "/velocity-dialog.png");
        dialog.setSource(&b);
        QVERIFY(dialog.records().isEmpty());
        QTRY_VERIFY(!dialog.isPreparing());
        dialog.setSource(&f);
        QTRY_VERIFY(!dialog.isPreparing());
        QCOMPARE(dialog.records().size(), n);
        dialog.close();
        dialog.show();
        QCOMPARE(dialog.records().size(), n);
        dialog.close();
    }
    void delayAlignmentAndOverlayBoundaries()
    {
        QSettings("WW", "SeiSeeMp").remove("VelocityPicking");
        SgyFile f;
        open(f, path);
        VelocityDialog dialog;
        dialog.show();
        dialog.setSource(&f);
        QTRY_VERIFY(!dialog.isPreparing());
        get<QRadioButton>(&dialog, "velocityLine")->setChecked(true);
        dialog.setArmed(true);
        dialog.point(13, 250, true, true);
        dialog.point(20, 600, true, true);
        QCOMPARE(dialog.records().size(), 1);
        QVERIFY(std::abs(dialog.records()[0].t0 - 100) < 1e-8);
        QVERIFY(std::abs(dialog.records()[0].velocity - 2000) < 1e-8);
        dialog.undo();
        get<QRadioButton>(&dialog, "velocityHyperbola")->setChecked(true);
        dialog.point(13, 100 + 1000 * std::hypot(.5, .05), true, true);
        dialog.point(20, 100 + 1000 * std::hypot(.5, .4), true, true);
        QCOMPARE(dialog.records().size(), 1);
        QVERIFY(std::abs(dialog.records()[0].velocity - 2000) < 1e-7);
        QVERIFY(std::abs(dialog.records()[0].t0 - 500) < 1e-8);
        dialog.undo();
        get<QComboBox>(&dialog, "velocityColor")
            ->setCurrentIndex(
                get<QComboBox>(&dialog, "velocityColor")->findText("Magenta"));
        QVERIFY(dialog.importFiles(
            {write("curve_overlay.csv", "t0(ms),velocity(m/s)\n300,2000\n")}));
        QImage image(760, 400, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        dialog.paintOverlay(
            painter, image.rect(), 0, 35,
            [](int trace, double ms) {
                return QPointF(20 + trace * 20, 20 + ms / 5);
            },
            false);
        painter.end();
        image.save(out + "/velocity-overlay.png");
        QVERIFY(magenta(image) > 300);
        for (int trace : {3, 15, 27}) {
            const double ms = velocityTime(
                dialog.records()[0], dialog.geometry().traces[trace].distance);
            const int x = 20 + trace * 20, y = qRound(20 + ms / 5);
            bool found = false;
            for (int yy = y - 3; yy <= y + 3; ++yy) {
                auto rgb = image.pixel(x, yy);
                if (qRed(rgb) > 180 && qBlue(rgb) > 180 && qGreen(rgb) < 100)
                    found = true;
            }
            QVERIFY(found);
        }
        for (int x = 242; x < 258; ++x)
            for (int y = 85; y < 145; ++y) {
                auto rgb = image.pixel(x, y);
                QVERIFY(!(qRed(rgb) > 180 && qBlue(rgb) > 180 &&
                          qGreen(rgb) < 100));
            }
        dialog.close();
    }
    void previewAndSavedLinesWithSparseGatherHeaders_data()
    {
        QTest::addColumn<int>("headers");
        QTest::addColumn<bool>("hyperbola");
        for (int headers : {1, 2, 3})
            for (bool hyperbola : {false, true})
                QTest::newRow(qPrintable(QString("headers-%1-%2")
                                            .arg(headers)
                                            .arg(hyperbola ? "hyperbola" : "line")))
                    << headers << hyperbola;
    }
    void previewAndSavedLinesWithSparseGatherHeaders()
    {
        QFETCH(int, headers);
        QFETCH(bool, hyperbola);
        QSettings("WW", "SeiSeeMp").remove("VelocityPicking");
        const QString name = out + QString("/sparse-headers-%1.sgy").arg(headers);
        fixture(name, 1, 1, headers);
        SgyFile file;
        open(file, name);
        VelocityDialog dialog;
        dialog.show();
        dialog.setSource(&file);
        QTRY_VERIFY(!dialog.isPreparing());
        get<QRadioButton>(&dialog, hyperbola ? "velocityHyperbola" : "velocityLine")
            ->setChecked(true);
        auto color = get<QComboBox>(&dialog, "velocityColor");
        color->setCurrentIndex(color->findText("Magenta"));
        dialog.setArmed(true);
        const double t1 = hyperbola ? 1000 * std::hypot(.3, .05) : 350;
        const double t2 = hyperbola ? 1000 * std::hypot(.3, .4) : 700;
        dialog.point(1, t1, false, true);
        dialog.point(8, t2, false, false);
        auto interiorPixels = [&]() {
            QImage image(760, 400, QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            dialog.paintOverlay(painter, image.rect(), 0, 35,
                                [](int trace, double ms) {
                                    return QPointF(20 + trace * 20, 20 + ms / 5);
                                }, false);
            painter.end();
            image.save(out + QString("/sparse-%1-%2-%3.png").arg(headers)
                                     .arg(hyperbola ? "hyperbola" : "line")
                                     .arg(dialog.records().isEmpty() ? "preview" : "saved"));
            // Exclude both point markers: dots alone must not pass this check.
            return magenta(image.copy(QRect(65, 0, 90, image.height())));
        };
        QVERIFY2(interiorPixels() > 20, "Missing mouse-following preview curve between the points");
        dialog.point(8, t2, false, true);
        QCOMPARE(dialog.records().size(), 1);
        QVERIFY2(interiorPixels() > 20, "Missing saved curve between the points");
        QCOMPARE(dialog.geometry().autoGather, headers == 2 ? 2 : 4);
    }
    void lockedDirectoryEntry()
    {
#ifdef Q_OS_WIN
        const QString name = out + "/locked-file.bin";
        int descriptor = -1;
        QCOMPARE(_sopen_s(&descriptor, name.toLocal8Bit().constData(),
                          _O_CREAT | _O_RDWR | _O_BINARY, _SH_DENYRW,
                          _S_IREAD | _S_IWRITE),
                 0);
        const QByteArray bytes(5000, 'x');
        const int n = _write(descriptor, bytes.constData(), bytes.size());
        int swap, cgg, junk;
        const int segy =
            is_segy_f(name.toLocal8Bit().constData(), swap, cgg, junk);
        const int su = is_su_f(name.toLocal8Bit().constData(), swap);
        _close(descriptor);
        QCOMPARE(n, bytes.size());
        QCOMPARE(segy, 0);
        QCOMPARE(su, 0);
#endif
    }
    void mainWindowInteractionAndExport_data()
    {
        QTest::addColumn<bool>("hyperbola");
        QTest::addColumn<bool>("frequency");
        QTest::newRow("line-fk") << false << false;
        QTest::newRow("hyperbola-fk") << true << false;
        QTest::newRow("line-frequency") << false << true;
        QTest::newRow("hyperbola-frequency") << true << true;
    }
    void mainWindowInteractionAndExport()
    {
        QFETCH(bool, hyperbola);
        QFETCH(bool, frequency);
        QSettings("WW", "SeiSeeMp").remove("VelocityPicking");
        const QString mousePath = out +
            (hyperbola ? "/mouse-hyperbola.sgy" : "/mouse-line.sgy");
        fixture(mousePath, 1, 1, 1);
        MainWindow window;
        window.show();
        QTest::qWait(250);
        QVERIFY(window.openFile(mousePath));
        QTest::qWait(350);
        QMetaObject::invokeMethod(&window, "on_zoomAllBtn_pressed",
                                  Qt::DirectConnection);
        auto button = get<QPushButton>(&window, "velocityBtn");
        QCOMPARE(button->size(), QSize(24, 24));
        QCOMPARE(button->iconSize(), QSize(20, 20));
        QVERIFY(get<QAction>(&window, "actionVelocityPicking"));
        button->click();
        auto dialog = window.findChild<VelocityDialog *>();
        QVERIFY(dialog);
        QVERIFY(dialog->isWindow());
        QTRY_VERIFY(!dialog->isPreparing());
        QTRY_VERIFY(dialog->isArmed());
        QVERIFY(!get<QPushButton>(&window, "zoomInBtn")->isEnabled());
        get<QRadioButton>(dialog, hyperbola ? "velocityHyperbola" : "velocityLine")
            ->setChecked(true);
        get<QComboBox>(dialog, "velocityColor")
            ->setCurrentIndex(
                get<QComboBox>(dialog, "velocityColor")->findText("Magenta"));
        auto area = get<MyScrollArea>(&window, "seismicScrollArea");
        QWidget *viewport = area->viewport();
        const QPoint a(viewport->width() * .12, 80),
            b(viewport->width() * .25, 140);
        QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, a);
        QVERIFY(dialog->hasFirstPoint());
        QMouseEvent move(QEvent::MouseMove, b, Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QApplication::sendEvent(viewport, &move);
        QVERIFY(dialog->records().isEmpty());
        const QRect interior(a.x() + 14, 0, b.x() - a.x() - 28,
                              viewport->height());
        const QImage preview = viewport->grab().toImage();
        QVERIFY2(magenta(preview.copy(interior)) > 10,
                 "Real mouse movement must draw a curve, not just point markers");
        preview.save(out + (hyperbola ? "/mouse-hyperbola-preview.png"
                                     : "/mouse-line-preview.png"));
        const QPoint next(b.x(), b.y() + 25);
        QMouseEvent nextMove(QEvent::MouseMove, next, Qt::NoButton, Qt::NoButton,
                             Qt::NoModifier);
        QApplication::sendEvent(viewport, &nextMove);
        const QImage moved = viewport->grab().toImage();
        QVERIFY(magenta(moved.copy(interior)) > 10);
        QVERIFY(preview.copy(interior) != moved.copy(interior));
        QApplication::sendEvent(viewport, &move);
        QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, b);
        QCOMPARE(dialog->records().size(), 1);
        QVERIFY2(magenta(viewport->grab().toImage().copy(interior)) > 10,
                 "The confirmed curve must remain visible between the points");
        get<QComboBox>(dialog, "velocityColor")
            ->setCurrentIndex(
                get<QComboBox>(dialog, "velocityColor")->findText("Magenta"));
        QImage main = window.grab().toImage();
        main.save(out + "/main-velocity-overlay.png");
        dialog->grab().save(out + "/velocity-dialog-ready.png");
        QVERIFY(magenta(main) > 30);
        QTest::mouseClick(viewport, Qt::RightButton, Qt::NoModifier, b);
        QVERIFY(!dialog->isArmed());
        QVERIFY(get<QPushButton>(&window, "zoomInBtn")->isEnabled());
        dialog->close();
        QVERIFY(!dialog->hasCurves());
        QCOMPARE(dialog->records().size(), 1);
        QTRY_COMPARE(magenta(viewport->grab().toImage().copy(interior)), 0);
        viewport->grab().save(out + "/velocity-window-closed.png");
        button->click();
        QCOMPARE(dialog->records().size(), 1);
        QVERIFY(dialog->isArmed());
        QVERIFY(dialog->hasCurves());
        QTRY_VERIFY(magenta(viewport->grab().toImage().copy(interior)) > 10);
        viewport->grab().save(out + "/velocity-window-reopened.png");
        get<QPushButton>(&window, frequency ? "frequencyBtn" : "fkBtn")->click();
        auto fk = window.findChild<SpectrumDialog *>(frequency ? "FrequencySpectrumDialog"
                                                              : "FkSpectrumDialog");
        QVERIFY(fk);
        if (frequency)
            get<QCheckBox>(fk, "frequencySelection")->setChecked(true);
        get<QPushButton>(fk, "fkSelect")->setChecked(true);
        QVERIFY(!dialog->isArmed());
        QVERIFY(area->isSelectionMode());
        button->click();
        QVERIFY(dialog->isArmed());
        QVERIFY(!area->isSelectionMode());
        QVERIFY(!get<QPushButton>(fk, "fkSelect")->isChecked());
        dialog->setArmed(false);
        fk->close();
        QDir folder(QFileInfo(path).absolutePath());
        auto previous = folder.entryList({"*.png"}, QDir::Files);
        get<QPushButton>(&window, "captureBtn")->click();
        auto current = folder.entryList({"*.png"}, QDir::Files);
        QImage low;
        for (auto name : current)
            if (!previous.contains(name))
                low.load(folder.filePath(name));
        QVERIFY(!low.isNull());
        QVERIFY(magenta(low) > 30);
        const auto before = folder.entryList({"*.png"}, QDir::Files);
        get<QAction>(&window, "actionExportDpi300")->trigger();
        get<QPushButton>(&window, "exportHiResBtn")->click();
        QTRY_VERIFY_WITH_TIMEOUT(
            folder.entryList({"*.png"}, QDir::Files).size() > before.size(),
            20000);
        auto after = folder.entryList({"*.png"}, QDir::Files);
        QString png;
        for (auto name : after)
            if (!before.contains(name))
                png = folder.filePath(name);
        QVERIFY(!png.isEmpty());
        QImage high(png);
        QVERIFY(!high.isNull());
        QVERIFY(magenta(high) > 100);
        QCOMPARE(high.dotsPerMeterX(), qRound(300 / .0254));
        // Verify physical alignment, not merely that the export has colored
        // pixels.
        int matched = 0, colored = 0;
        for (int y = 0; y < low.height(); ++y)
            for (int x = 0; x < low.width(); ++x) {
                auto pixel = low.pixel(x, y);
                if (qRed(pixel) < 181 || qBlue(pixel) < 181 ||
                    qGreen(pixel) >= 100)
                    continue;
                ++colored;
                const int hx = qRound((x + .5) * high.width() / low.width()),
                          hy = qRound((y + .5) * high.height() / low.height());
                bool found = false;
                for (int yy = qMax(0, hy - 5);
                     yy <= qMin(high.height() - 1, hy + 5); ++yy)
                    for (int xx = qMax(0, hx - 5);
                         xx <= qMin(high.width() - 1, hx + 5); ++xx) {
                        auto p = high.pixel(xx, yy);
                        found |=
                            qRed(p) > 180 && qBlue(p) > 180 && qGreen(p) < 100;
                    }
                if (found)
                    ++matched;
            }
        QVERIFY2(matched > .85 * colored,
                 qPrintable(QString("Export overlay alignment: %1/%2")
                                .arg(matched)
                                .arg(colored)));
        high.save(out + "/velocity-export-300dpi.png");
        QVERIFY(window.openFile(other));
        QTRY_VERIFY(!dialog->isPreparing());
        QVERIFY(dialog->records().isEmpty());
        QVERIFY(window.openFile(mousePath));
        QTRY_VERIFY(!dialog->isPreparing());
        QCOMPARE(dialog->records().size(), 1);
        get<QRadioButton>(dialog, "velocityHyperbola")->setChecked(true);
        auto colors = get<QComboBox>(dialog, "velocityColor");
        colors->setCurrentIndex(colors->findText("Blue"));
        QVERIFY(dialog->importFiles(
            {write("demo_velocity.csv",
                   "type,t0(ms),velocity(m/s)\nhyperbola,500,2000\n")}));
        QCOMPARE(dialog->records().size(), 2);
        window.grab().save(out + "/main-velocity-with-csv.png");
        dialog->grab().save(out + "/velocity-dialog-with-csv.png");
        window.close();
    }
    void mainWindowSelectionHighlightsCurve()
    {
        QSettings("WW", "SeiSeeMp").remove("VelocityPicking");
        MainWindow window;
        window.show();
        QTest::qWait(250);
        QVERIFY(window.openFile(path));
        QTest::qWait(350);
        QMetaObject::invokeMethod(&window, "on_zoomAllBtn_pressed",
                                  Qt::DirectConnection);
        auto button = get<QPushButton>(&window, "velocityBtn");
        button->click();
        auto dialog = window.findChild<VelocityDialog *>();
        QVERIFY(dialog);
        QTRY_VERIFY(!dialog->isPreparing());
        dialog->setArmed(false);
        auto color = get<QComboBox>(dialog, "velocityColor");
        color->setCurrentIndex(color->findText("Blue"));
        QVERIFY(dialog->importFiles({write("highlight.csv",
            "type,t0(ms),velocity(m/s)\nhyperbola,350,2000\nhyperbola,900,2000\n")}));
        QCOMPARE(dialog->records().size(), 2);
        const QString original = velocityCsv(dialog->records());
        auto table = get<MyStringTable>(dialog, "velocityTable");
        auto area = get<MyScrollArea>(&window, "seismicScrollArea");
        auto orangeCentre = [&](const QImage &image) {
            int pixels = 0;
            double sum = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const QColor c(image.pixel(x, y));
                    if (c.red() > 220 && c.green() > 110 && c.green() < 195 && c.blue() < 70) {
                        ++pixels;
                        sum += y;
                    }
                }
            return pixels > 100 ? sum / pixels : -1.;
        };
        table->setCurCell(0);
        const QImage first = area->viewport()->grab().toImage();
        const double firstY = orangeCentre(first);
        QVERIFY2(firstY >= 0, "Selecting a table row must highlight the main-view curve");
        first.save(out + "/velocity-first-selected.png");
        table->setCurCell(1);
        const QImage second = area->viewport()->grab().toImage();
        const double secondY = orangeCentre(second);
        QVERIFY2(secondY > firstY + 20, "Highlight must move to the selected curve");
        second.save(out + "/velocity-second-selected.png");
        QCOMPARE(velocityCsv(dialog->records()), original);
        table->hHeaderPressed(2); // Sorting must keep selection tied to its id.
        table->setCurCell(1);
        QVERIFY(orangeCentre(area->viewport()->grab().toImage()) < secondY - 20);
        QCOMPARE(velocityCsv(dialog->records()), original);
        get<QCheckBox>(dialog, "velocityVisible")->setChecked(false);
        QVERIFY(!dialog->hasCurves());
        dialog->close();
        QVERIFY(orangeCentre(area->viewport()->grab().toImage()) < 0);
        QCOMPARE(dialog->records().size(), 2);
        button->click();
        QTRY_VERIFY(dialog->hasCurves());
        QVERIFY(get<QCheckBox>(dialog, "velocityVisible")->isChecked());
        QVERIFY(orangeCentre(area->viewport()->grab().toImage()) >= 0);
        QCOMPARE(velocityCsv(dialog->records()), original);
        window.close();
    }
    void closeDuringQueuedGeometry()
    {
        SgyFile f;
        open(f, path);
        auto dialog = new VelocityDialog;
        dialog->setSource(&f);
        auto job = dialog->findChild<VelocityGeometryWorker *>();
        QVERIFY(job);
        QSignalSpy ready(dialog, &VelocityDialog::geometryReady);
        QVERIFY(job->wait(5000));
        dialog->shutdown();
        QApplication::processEvents();
        QCOMPARE(ready.count(), 0);
        delete dialog;
    }
};
QTEST_MAIN(VelocityRegression)
#include "velocity_regression.moc"
