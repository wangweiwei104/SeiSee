#include <QtTest>
#include <QtWidgets>
#include <complex>
#include <limits>
#include <stdexcept>
#include "fkspectrumdialog.h"
#include "mainwindow.h"
#include "sgyfile.h"
#include "Toast.h"

class FkRegression : public QObject
{
    Q_OBJECT
    QString out, pathA, pathB, pathLarge, pathHalf, pathDouble, pathShort;
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
    static void open(SgyFile &file, const QString &path)
    {
        file.setFname(path);
        file.setActive(true);
    }
    static int colored(const QImage &image)
    {
        int count = 0;
        for (int y = 40; y < image.height() - 80; ++y)
            for (int x = 80; x < image.width() - 220; ++x) {
                auto p = image.pixel(x, y);
                if (qRed(p) != qGreen(p) || qGreen(p) != qBlue(p))
                    ++count;
            }
        return count;
    }
    void captureZoomSelection(SpectrumDialog *dialog, const char *canvasName,
                              const QString &name)
    {
        QWidget *canvas = get<QWidget>(dialog, canvasName);
        QVERIFY(!get<QWidget>(canvas, "analysisSelectionBand")->isVisible());
        get<QPushButton>(dialog, "fkZoomWindow")->click();
        const QImage cursor =
            canvas->cursor().pixmap().toImage().convertToFormat(
                QImage::Format_ARGB32);
        QVERIFY(!cursor.isNull());
        int opaque = 0;
        for (int y = 0; y < cursor.height(); ++y)
            for (int x = 0; x < cursor.width(); ++x) {
                const auto pixel = cursor.pixel(x, y);
                if (qAlpha(pixel)) {
                    ++opaque;
                    QCOMPARE(qRed(pixel) + qGreen(pixel) + qBlue(pixel), 0);
                }
            }
        QVERIFY(opaque > 40);
        const QPoint from(canvas->width() * .35, canvas->height() * .35);
        const QPoint to(canvas->width() * .65, canvas->height() * .65);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
        QMouseEvent move(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        auto band = get<QWidget>(canvas, "analysisSelectionBand");
        QVERIFY(band->isVisible());
        dialog->grab().save(out + "/" + name + "-selection.png");
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, to);
        QVERIFY(!band->isVisible());
        get<QPushButton>(dialog, "fkZoomPrevious")->click();
    }
private slots:
    void initTestCase()
    {
        out = qEnvironmentVariable("SEISEE_TEST_OUT");
        QVERIFY(!out.isEmpty());
        QDir().mkpath(out);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           out + "/settings");
        QSettings settings("WW", "SeiSeeMp");
        settings.clear();
        QDir().mkpath(out + "/data");
        pathA = out + "/data/source-a.sgy";
        pathB = out + "/data/source-b.sgy";
        pathLarge = out + "/data/source-large.sgy";
        fixture(pathA, 96, 1024, 2000);
        fixture(pathB, 16, 256, 4000);
        fixture(pathLarge, 1024, 1024, 2000);
        pathHalf = out + "/data/half.sgy";
        pathDouble = out + "/data/double.sgy";
        pathShort = out + "/data/short.sgy";
        fixture(pathHalf, 96, 1024, 2000, .5);
        fixture(pathDouble, 96, 1024, 2000, 2);
        fixture(pathShort, 4, 32, 2000);
        int font =
            QFontDatabase::addApplicationFont(":/fonts/Roboto-Regular-14.ttf");
        QVERIFY(font >= 0);
        QApplication::setFont(
            QFont(QFontDatabase::applicationFontFamilies(font).first(), 9));
    }
    void fftMatchesIndependentDft()
    {
        const double pi = std::acos(-1.0);
        for (QSize size : {QSize(12, 32), QSize(17, 37), QSize(7, 31),
                           QSize(2, 8), QSize(3, 5)}) {
            const int nx = size.width(), nt = size.height();
            QVector<float> input(nx * nt);
            for (int x = 0; x < nx; ++x)
                for (int t = 0; t < nt; ++t)
                    input[x * nt + t] =
                        float(std::cos(.31 * t + .7 * x) +
                              .17 * std::sin(.67 * t - .21 * x));
            auto result = calculateFk(input, nx, nt, .002, 25);
            QCOMPARE(result.nf, nt / 2 + 1);
            QCOMPARE(result.nk, nx);
            QVERIFY(std::abs(result.df - 1 / (nt * .002)) < 1e-12);
            QVERIFY(std::abs(result.k0 + (nx / 2) / (nx * 25.)) < 1e-12);
            double error = 0, peak = 0;
            for (int f = 0; f < result.nf; ++f)
                for (int k = 0; k < nx; ++k) {
                    int unshifted = (k + (nx + 1) / 2) % nx;
                    std::complex<double> expected(0, 0);
                    for (int x = 0; x < nx; ++x)
                        for (int t = 0; t < nt; ++t) {
                            double phase = -2 * pi *
                                           (double(f) * t / nt +
                                            double(unshifted) * x / nx);
                            expected += double(input[x * nt + t]) *
                                        std::complex<double>(std::cos(phase),
                                                             std::sin(phase));
                        }
                    peak = qMax(peak, std::abs(expected));
                    error = qMax(error, std::abs(result.amplitude[f * nx + k] -
                                                 std::abs(expected)));
                }
            QVERIFY2(
                error / qMax(peak, 1e-12) < 2e-5,
                qPrintable(
                    QString("DFT error %1").arg(error / qMax(peak, 1e-12))));
        }
        QVector<float> bad(64, 0);
        QVERIFY_EXCEPTION_THROWN(calculateFk(bad, 8, 8, .002, 0),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(
            calculateFk(bad, 8, 8, .002, 25, false, []() { return true; }),
            std::runtime_error);
        bad[3] = std::numeric_limits<float>::quiet_NaN();
        QVERIFY_EXCEPTION_THROWN(calculateFk(bad, 8, 8, .002, 25),
                                 std::runtime_error);
    }
    void frequencyUsesMeanAmplitude()
    {
        QVector<float> samples(4 * 64);
        for (int x = 0; x < 4; ++x)
            for (int t = 0; t < 64; ++t)
                samples[x * 64 + t] =
                    float((x + 1) * std::cos(2 * std::acos(-1.0) * 8 * t / 64));
        auto result = calculateFrequency(samples, 4, 64, .002);
        QCOMPARE(result.nk, 1);
        QVERIFY(std::abs(result.amplitude[8] - 80) < 1e-4);
    }
    void nyquistEndpointsAndDecibels()
    {
        for (int nt : {64, 65}) {
            QVector<float> input(4 * nt);
            for (int x = 0; x < 4; ++x)
                for (int t = 0; t < nt; ++t)
                    input[x * nt + t] =
                        (t % 2 ? -1.f : 1.f) * (x % 2 ? -1.f : 1.f);
            const auto f = calculateFrequency(input, 4, nt, .002);
            const auto fk = calculateFk(input, 4, nt, .002, 25);
            QCOMPARE(f.frequencyLimit, 250.0);
            QCOMPARE(fk.frequencyLimit, 250.0);
            QCOMPARE(fk.wavenumberLimit, .02);
            QCOMPARE(f.nf, nt / 2 + 1);
            if (nt % 2 == 0) {
                QCOMPARE((f.nf - 1) * f.df, 250.0);
                QVERIFY(std::abs(f.amplitude.last() - nt) < 1e-4);
                QVERIFY(std::abs(fk.amplitude[(fk.nf - 1) * 4] - 4 * nt) <
                        1e-3);
            } else
                QVERIFY((f.nf - 1) * f.df < f.frequencyLimit);
        }
        QCOMPARE(spectrumAmplitudeValue(1, true), 0.0);
        QCOMPARE(spectrumAmplitudeValue(10, true), 20.0);
        QCOMPARE(spectrumAmplitudeValue(.1, true), -20.0);
        QCOMPARE(spectrumAmplitudeValue(0, true), -240.0);
    }
    void physicalDisplayBoundsAndAutomaticImageNaming()
    {
        QSettings settings("WW", "SeiSeeMp");
        settings.remove("FrequencySpectrum");
        settings.remove("FkSpectrum");
        // Migrate the actual old full-bin defaults; keep deliberate custom
        // limits.
        settings.setValue("FrequencySpectrum/display/initialized", true);
        settings.setValue("FrequencySpectrum/display/fkFmin", 0);
        settings.setValue("FrequencySpectrum/display/fkFmax",
                          511 * 500.0 / 1024);
        settings.setValue("FrequencySpectrum/display/fkCmin", 0);
        settings.setValue("FrequencySpectrum/display/fkCmax", 600);
        SgyFile file, other;
        open(file, pathA);
        open(other, pathB);
        FrequencySpectrumDialog frequency(&file, out);
        FkSpectrumDialog fk(&file, out);
        for (SpectrumDialog *dialog :
             QList<SpectrumDialog *>({&fk, &frequency})) {
            QSignalSpy updated(dialog, &SpectrumDialog::spectrumUpdated);
            dialog->show();
            get<QPushButton>(dialog, "fkCalculate")->click();
            QTRY_COMPARE(updated.count(), 1);
            QTRY_VERIFY(!dialog->isCalculating());
            QCOMPARE(get<QDoubleSpinBox>(dialog, "fkFmax")->value(), 250.0);
            if (dialog == &fk) {
                QCOMPARE(get<QDoubleSpinBox>(dialog, "fkKmin")->value(), -.02);
                QCOMPARE(get<QDoubleSpinBox>(dialog, "fkKmax")->value(), .02);
                captureZoomSelection(dialog, "fkPlot", "fk-black-cursor");
            }
            get<QDoubleSpinBox>(dialog, "fkFmax")->setValue(180);
            get<QComboBox>(dialog, "fkScale")->setCurrentIndex(1);
            dialog->grab().save(out + (dialog == &fk
                                           ? "/fk-decibels.png"
                                           : "/frequency-decibels.png"));
            QCOMPARE(updated.count(), 1);
            // Input edits/source switching must not misname a cached result.
            if (dialog == &frequency)
                get<QCheckBox>(dialog, "frequencySelection")->setChecked(true);
            get<QSpinBox>(dialog, "fkFirst")->setValue(8);
            dialog->setSource(&other);
            QString mode = dialog == &fk ? "fk" : "frequency";
            const QString pattern = "source-a_1_*_" + mode + "*png";
            QDir dir(QFileInfo(pathA).absolutePath());
            for (const auto &name : dir.entryList({pattern}, QDir::Files))
                QVERIFY(dir.remove(name));
            get<QPushButton>(dialog, "fkCapture")->click();
            auto names = dir.entryList({pattern}, QDir::Files);
            QCOMPARE(names.size(), 1);
            const QString first = names.first();
            QVERIFY(first.contains("_f_0_180"));
            QVERIFY(first.contains("_dB_"));
            get<QPushButton>(dialog, "fkCapture")->click();
            QVERIFY(dir.exists(QFileInfo(first).completeBaseName() + "_1.png"));
            dialog->setExportDpi(300);
            get<QPushButton>(dialog, "fkExport")->click();
            const QString large =
                QFileInfo(first).completeBaseName() + "_300.png";
            QVERIFY(dir.exists(large));
            QImage image(dir.filePath(large));
            QVERIFY(!image.isNull());
            QCOMPARE(image.dotsPerMeterX(), qRound(300 / .0254));
            QCOMPARE(updated.count(), 1);
            dialog->close();
        }
    }
    void sourceSwitchRetainsParametersAndCache()
    {
        QSettings settings("WW", "SeiSeeMp");
        settings.remove("FkSpectrum");
        SgyFile a, b;
        open(a, pathA);
        open(b, pathB);
        QVERIFY(a.Active());
        QVERIFY(b.Active());
        FkSpectrumDialog dialog(&a, out);
        dialog.show();
        QSignalSpy updated(&dialog, &FkSpectrumDialog::spectrumUpdated);
        get<QPushButton>(&dialog, "fkCalculate")->click();
        QTRY_COMPARE(updated.count(), 1);
        QTRY_VERIFY(!dialog.isCalculating());
        QVERIFY(dialog.isSourceCurrent());
        auto fmax = get<QDoubleSpinBox>(&dialog, "fkFmax");
        fmax->setValue(180);
        auto first = get<QSpinBox>(&dialog, "fkFirst");
        first->setValue(8);
        auto dx = get<QDoubleSpinBox>(&dialog, "fkSpacing");
        dx->setValue(17.5);
        auto cmap = get<QComboBox>(&dialog, "fkPalette");
        cmap->setCurrentIndex(1);
        double max = fmax->value(),
               low = get<QDoubleSpinBox>(&dialog, "fkCmin")->value();
        QWidget *plot = get<QWidget>(&dialog, "fkPlot");
        captureZoomSelection(&dialog, "fkPlot", "fk");
        QImage color = plot->grab().toImage();
        QVERIFY(colored(color) > 100);
        dialog.grab().save(out + "/fk-current.png");
        dialog.setSource(&b);
        QVERIFY(!dialog.isSourceCurrent());
        QVERIFY(!dialog.isCalculating());
        QCOMPARE(first->value(), 8);
        QCOMPARE(dx->value(), 17.5);
        QCOMPARE(fmax->value(), max);
        QCOMPARE(cmap->currentIndex(), 1);
        QCOMPARE(get<QDoubleSpinBox>(&dialog, "fkCmin")->value(), low);
        QImage gray = plot->grab().toImage();
        QCOMPARE(colored(gray), 0);
        dialog.grab().save(out + "/fk-other-source.png");
        dialog.setSource(&a);
        QVERIFY(dialog.isSourceCurrent());
        QCOMPARE(updated.count(), 1);
        QImage restored = plot->grab().toImage();
        QVERIFY(colored(restored) > 100);
        QCOMPARE(restored, color);
        dialog.grab().save(out + "/fk-restored.png");
        dialog.setSource(nullptr);
        QVERIFY(!dialog.isSourceCurrent());
        QCOMPARE(updated.count(), 1);
        dialog.setSource(&a);
        QVERIFY(dialog.savePng(out + "/fk-export.png"));
        QVERIFY(dialog.savePng(out + "/fk-export-300dpi.png", 300));
        QImage exported(out + "/fk-export-300dpi.png");
        QCOMPARE(exported.size(), plot->size() * (300.0 / plot->logicalDpiX()));
        QCOMPARE(exported.dotsPerMeterX(), qRound(300 / .0254));
        QVERIFY(colored(exported) > 100);
        dialog.setInitialRange(1, 16, 0, 254);
        get<QPushButton>(&dialog, "fkCalculate")->click();
        dialog.setSource(&b);
        QTRY_COMPARE(updated.count(), 2);
        QTRY_VERIFY(!dialog.isCalculating());
        QVERIFY(!dialog.isSourceCurrent());
        dialog.setSource(&a);
        QVERIFY(dialog.isSourceCurrent());
        dialog.close();
    }
    void settingsRestoreWithoutCalculation()
    {
        SgyFile file;
        open(file, pathA);
        {
            FkSpectrumDialog dialog(&file, out);
            get<QDoubleSpinBox>(&dialog, "fkFmin")->setValue(4.25);
            get<QDoubleSpinBox>(&dialog, "fkFmax")->setValue(120.5);
            get<QDoubleSpinBox>(&dialog, "fkKmin")->setValue(-.012);
            get<QDoubleSpinBox>(&dialog, "fkKmax")->setValue(.014);
            get<QDoubleSpinBox>(&dialog, "fkCmin")->setValue(-8);
            get<QDoubleSpinBox>(&dialog, "fkCmax")->setValue(4);
            get<QComboBox>(&dialog, "fkPalette")->setCurrentIndex(2);
            get<QComboBox>(&dialog, "fkScale")->setCurrentIndex(1);
            get<QComboBox>(&dialog, "fkInterpolation")->setCurrentIndex(1);
        }
        FkSpectrumDialog reopened(&file, out);
        QVERIFY(!reopened.isCalculating());
        QCOMPARE(get<QDoubleSpinBox>(&reopened, "fkFmin")->value(), 4.25);
        QCOMPARE(get<QDoubleSpinBox>(&reopened, "fkFmax")->value(), 120.5);
        QCOMPARE(get<QDoubleSpinBox>(&reopened, "fkKmin")->value(), -.012);
        QCOMPARE(get<QDoubleSpinBox>(&reopened, "fkKmax")->value(), .014);
        QCOMPARE(get<QDoubleSpinBox>(&reopened, "fkCmin")->value(), -8.);
        QCOMPARE(get<QDoubleSpinBox>(&reopened, "fkCmax")->value(), 4.);
        QCOMPARE(get<QComboBox>(&reopened, "fkPalette")->currentIndex(), 2);
        QCOMPARE(get<QComboBox>(&reopened, "fkScale")->currentIndex(), 1);
        QCOMPARE(get<QComboBox>(&reopened, "fkInterpolation")->currentIndex(),
                 1);
    }
    void iconsAndResources()
    {
        SgyFile file;
        open(file, pathA);
        FkSpectrumDialog dialog(&file, out);
        for (auto button : dialog.findChildren<QPushButton *>()) {
            QCOMPARE(button->size(), QSize(24, 24));
            QCOMPARE(button->iconSize(), QSize(20, 20));
            if (button->text().isEmpty())
                QVERIFY2(!button->icon().pixmap(20, 20).isNull(),
                         qPrintable(button->objectName()));
        }
        QVERIFY(!QPixmap(":/images/SelectWCurs.xpm").isNull());
    }
    void adjustableSelectionLocksNavigation()
    {
        QWidget *plot = new QWidget;
        plot->setFixedSize(700, 500);
        MyScrollArea area(plot, 3);
        area.resize(400, 300);
        area.show();
        QTest::qWait(50);
        QSignalSpy selections(&area, &MyScrollArea::selectionChanged),
            zooms(&area, &MyScrollArea::zoomChanged);
        area.setSelectionMode(true);
        auto drag = [&](QPoint from, QPoint to) {
            QTest::mousePress(area.viewport(), Qt::LeftButton, Qt::NoModifier,
                              from);
            QMouseEvent move(QEvent::MouseMove, to, Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(area.viewport(), &move);
            QTest::mouseRelease(area.viewport(), Qt::LeftButton, Qt::NoModifier,
                                to);
        };
        drag(QPoint(40, 40), QPoint(180, 180));
        QCOMPARE(selections.count(), 1);
        QCOMPARE(zooms.count(), 0);
        QRect r = selections.last()[0].toRect();
        QCOMPARE(r.topLeft(), QPoint(40, 40));
        drag(QPoint(180, 180), QPoint(220, 210));
        QCOMPARE(selections.count(), 2);
        QVERIFY(selections.last()[0].toRect().width() > r.width());
        drag(QPoint(90, 90), QPoint(100, 100));
        QCOMPARE(selections.count(), 3);
        area.setZoomW(1);
        QVERIFY(area.isSelectionMode());
        QWheelEvent wheel(QPointF(80, 80), QPointF(80, 80), QPoint(),
                          QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(area.viewport(), &wheel);
        QCOMPARE(zooms.count(), 0);
        area.setSelectionMode(false);
        QVERIFY(!area.isSelectionMode());
    }
    void mainWindowEntriesAndSourceSynchronization()
    {
        MainWindow window;
        window.show();
        QTest::qWait(250); // Let the saved-directory scan settle before opening
                           // a fixture.
        QVERIFY(window.openFile(pathA));
        QTest::qWait(500);
        QVERIFY(get<QWidget>(&window, "seisFrame")->isVisible());
        QMetaObject::invokeMethod(&window, "on_zoomAllBtn_pressed",
                                  Qt::DirectConnection);
        get<QPushButton>(&window, "fkBtn")->click();
        auto fk = window.findChild<FkSpectrumDialog *>("FkSpectrumDialog");
        QVERIFY(fk);
        QSignalSpy updated(fk, &FkSpectrumDialog::spectrumUpdated);
        get<QPushButton>(fk, "fkAllTraces")->click();
        get<QPushButton>(fk, "fkAllTime")->click();
        get<QPushButton>(fk, "fkCalculate")->click();
        QTRY_COMPARE(updated.count(), 1);
        QTRY_VERIFY(!fk->isCalculating());
        QVERIFY(window.openFile(pathB));
        QTest::qWait(250);
        QVERIFY(!fk->isSourceCurrent());
        QCOMPARE(updated.count(), 1);
        QVERIFY(window.openFile(pathA));
        QTest::qWait(250);
        QVERIFY(fk->isSourceCurrent());
        QCOMPARE(updated.count(), 1);
        get<QPushButton>(fk, "fkSelect")->setChecked(true);
        QVERIFY(!get<QPushButton>(&window, "zoomInBtn")->isEnabled());
        auto area = get<MyScrollArea>(&window, "seismicScrollArea");
        QVERIFY(area->isSelectionMode());
        QSignalSpy rectangles(area, &MyScrollArea::selectionChanged);
        QPoint from(20, 20), to(qMin(200, area->viewport()->width() - 1),
                                qMin(150, area->viewport()->height() - 1));
        QTest::mousePress(area->viewport(), Qt::LeftButton, Qt::NoModifier,
                          from);
        QMouseEvent move(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(area->viewport(), &move);
        QTest::mouseRelease(area->viewport(), Qt::LeftButton, Qt::NoModifier,
                            to);
        QCOMPARE(rectangles.count(), 1);
        window.grab().save(out + "/main-input-selection.png");
        QVERIFY(get<QSpinBox>(fk, "fkLast")->value() <= 96);
        get<QPushButton>(fk, "fkClearSelection")->click();
        QVERIFY(!area->isSelectionMode());
        QVERIFY(get<QPushButton>(&window, "zoomInBtn")->isEnabled());
        get<QPushButton>(&window, "frequencyBtn")->click();
        auto frequency = window.findChild<FrequencySpectrumDialog *>(
            "FrequencySpectrumDialog");
        QVERIFY(frequency);
        QSignalSpy frequencyUpdated(frequency,
                                    &SpectrumDialog::spectrumUpdated);
        get<QPushButton>(frequency, "fkCalculate")->click();
        QTRY_COMPARE(frequencyUpdated.count(), 1);
        QTRY_VERIFY(!frequency->isCalculating());
        frequency->grab().save(out + "/frequency-spectrum.png");
        QVERIFY(get<QAction>(&window, "actionFkSpectrum"));
        QVERIFY(get<QAction>(&window, "actionFrequencySpectrum"));
        window.grab().save(out + "/main-spectrum-buttons.png");
        window.close();
    }
    void firstOpenAndReopenHaveNoSelectionRectangle()
    {
        SgyFile file;
        open(file, pathA);
        FkSpectrumDialog fk(&file, out);
        FrequencySpectrumDialog frequency(&file, out);
        for (SpectrumDialog *dialog :
             QList<SpectrumDialog *>({&fk, &frequency})) {
            dialog->show();
            QTest::qWait(20);
            const QString name = dialog == &fk ? "fk" : "frequency";
            auto canvas = get<QWidget>(dialog, dialog == &fk ? "fkPlot"
                                                             : "frequencyPlot");
            auto band = get<QWidget>(canvas, "analysisSelectionBand");
            QVERIFY(!band->isVisible());
            dialog->grab().save(out + "/" + name + "-first-open.png");
            QSignalSpy updated(dialog, &SpectrumDialog::spectrumUpdated);
            get<QPushButton>(dialog, "fkCalculate")->click();
            QTRY_COMPARE(updated.count(), 1);
            QTRY_VERIFY(!dialog->isCalculating());
            get<QPushButton>(dialog, "fkZoomWindow")->click();
            QPoint from(canvas->width() * .35, canvas->height() * .35),
                to(canvas->width() * .65, canvas->height() * .65);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
            QMouseEvent move(QEvent::MouseMove, to, Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(canvas, &move);
            QVERIFY(band->isVisible());
            dialog->close();
            dialog->show();
            QTest::qWait(20);
            QVERIFY(!band->isVisible());
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, to);
            QCOMPARE(updated.count(), 1);
            dialog->close();
        }
    }
    void singleTraceFollowsMainWindowSelection()
    {
        QSettings settings("WW", "SeiSeeMp");
        settings.remove("FrequencySpectrum");
        MainWindow window;
        window.show();
        QTest::qWait(250);
        QVERIFY(window.openFile(pathA));
        QTest::qWait(500);
        QMetaObject::invokeMethod(&window, "on_zoomAllBtn_pressed",
                                  Qt::DirectConnection);
        auto area = get<MyScrollArea>(&window, "seismicScrollArea");
        QSignalSpy selected(&window, &MainWindow::traceSelectionChanged);
        auto clickTrace = [&](double fraction) {
            QTest::mouseClick(
                area->viewport(), Qt::LeftButton, Qt::NoModifier,
                QPoint(int(area->viewport()->width() * fraction), 80));
        };
        clickTrace(.3);
        QVERIFY(!selected.isEmpty());
        const int initial = selected.last()[0].toInt();
        get<QPushButton>(&window, "frequencyBtn")->click();
        auto dialog = window.findChild<FrequencySpectrumDialog *>(
            "FrequencySpectrumDialog");
        QVERIFY(dialog);
        auto first = get<QSpinBox>(dialog, "fkFirst"),
             last = get<QSpinBox>(dialog, "fkLast");
        QCOMPARE(first->value(), initial);
        const double start = get<QDoubleSpinBox>(dialog, "fkStart")->value(),
                     end = get<QDoubleSpinBox>(dialog, "fkEnd")->value();
        QSignalSpy updated(dialog, &SpectrumDialog::spectrumUpdated);
        // Rapid main-window selections coalesce into one automatic calculation.
        clickTrace(.45);
        clickTrace(.65);
        clickTrace(.55);
        int latest = selected.last()[0].toInt();
        QCOMPARE(first->value(), latest);
        QTRY_COMPARE(updated.count(), 1);
        QTRY_VERIFY(!dialog->isCalculating());
        auto verifyTrace = [&](int traceNumber) {
            SgyFile file;
            open(file, pathA);
            const int a = int(std::ceil(start / 2 - 1e-7)),
                      b = int(std::floor(end / 2 + 1e-7));
            QVector<float> raw(b - a + 1);
            auto samples = file.Rs(traceNumber - 1);
            std::copy(samples + a, samples + b + 1, raw.begin());
            auto expected = calculateFrequency(raw, 1, raw.size(), .002);
            auto actual = dialog->spectra();
            QCOMPARE(actual.size(), 1);
            QCOMPARE(actual[0].spectrum.nf, expected.nf);
            for (int f = 0; f < expected.nf; ++f)
                QVERIFY(std::abs(expected.amplitude[f] -
                                 actual[0].spectrum.amplitude[f]) <
                        qMax(1e-4, double(expected.amplitude[f]) * 2e-5));
        };
        verifyTrace(latest);
        // Region inputs remain manual and do not follow main-window clicks.
        get<QCheckBox>(dialog, "frequencySelection")->setChecked(true);
        first->setValue(7);
        last->setValue(11);
        clickTrace(.35);
        QTest::qWait(200);
        QCOMPARE(first->value(), 7);
        QCOMPARE(last->value(), 11);
        QCOMPARE(updated.count(), 1);
        get<QCheckBox>(dialog, "frequencySingle")->setChecked(true);
        get<QAbstractButton>(&window, "rbDirRev")->setChecked(true);
        clickTrace(.25);
        latest = selected.last()[0].toInt();
        QCOMPARE(first->value(), latest);
        QTRY_COMPARE(updated.count(), 2);
        QTRY_VERIFY(!dialog->isCalculating());
        verifyTrace(latest);
        // A selection arriving while a worker owns a request is not dropped.
        get<QPushButton>(dialog, "fkCalculate")->click();
        QVERIFY(dialog->isCalculating());
        clickTrace(.6);
        latest = selected.last()[0].toInt();
        QTRY_VERIFY(get<QLabel>(dialog, "fkRangeStatus")
                        ->text()
                        .contains(QString("Trace: %1 ").arg(latest)));
        QTRY_VERIFY(!dialog->isCalculating());
        verifyTrace(latest);
        const int count = updated.count();
        get<QCheckBox>(dialog, "frequencyGrid")->setChecked(true);
        get<QComboBox>(dialog, "fkScale")->setCurrentIndex(1);
        dialog->close();
        dialog->show();
        QTest::qWait(250);
        QCOMPARE(updated.count(), count);
        QCOMPARE(get<QDoubleSpinBox>(dialog, "fkFmax")->value(), 250.0);
        QCOMPARE(get<QDoubleSpinBox>(dialog, "fkStart")->value(), start);
        QCOMPARE(get<QDoubleSpinBox>(dialog, "fkEnd")->value(), end);
        auto dpiAction = get<QAction>(&window, "actionExportDpi400");
        auto dpiMenu = get<QMenu>(&window, "menu_Export_DPI");
        QSignalSpy dpiChanged(dpiMenu, &QMenu::triggered);
        dpiAction->trigger();
        QCOMPARE(dpiChanged.count(), 1);
        QCOMPARE(dpiAction->data().toInt(), 400);
        QCOMPARE(dialog->exportDpi(), 400);
        dialog->grab().save(out + "/frequency-linked-trace.png");
        window.close();
    }
    void frequencyModesAndMultipleComparisons()
    {
        QSettings settings("WW", "SeiSeeMp");
        settings.remove("FrequencySpectrum");
        SgyFile file;
        open(file, pathA);
        FrequencySpectrumDialog dialog(&file, out);
        dialog.show();
        auto single = get<QCheckBox>(&dialog, "frequencySingle"),
             region = get<QCheckBox>(&dialog, "frequencySelection");
        QVERIFY(single->isChecked());
        QVERIFY(!region->isChecked());
        QCOMPARE(single->layoutDirection(), Qt::LeftToRight);
        QCOMPARE(region->layoutDirection(), Qt::LeftToRight);
        QCOMPARE(get<QCheckBox>(&dialog, "frequencyGrid")->layoutDirection(),
                 Qt::LeftToRight);
        QTest::mouseClick(single, Qt::LeftButton);
        QVERIFY(single->isChecked());
        region->setChecked(true);
        QVERIFY(!single->isChecked());
        QVERIFY(get<QSpinBox>(&dialog, "fkLast")->isEnabled());
        get<QSpinBox>(&dialog, "fkFirst")->setValue(4);
        get<QSpinBox>(&dialog, "fkLast")->setValue(7);
        get<QDoubleSpinBox>(&dialog, "fkStart")->setValue(20);
        get<QDoubleSpinBox>(&dialog, "fkEnd")->setValue(146);
        single->setChecked(true);
        QVERIFY(!region->isChecked());
        QVERIFY(!get<QSpinBox>(&dialog, "fkLast")->isEnabled());
        QSignalSpy updated(&dialog, &SpectrumDialog::spectrumUpdated);
        dialog.addComparisonFiles({pathHalf, pathDouble, pathHalf, pathA});
        auto files = get<QComboBox>(&dialog, "comparisonFile");
        QCOMPARE(files->count(), 2);
        QCOMPARE(updated.count(), 0);
        auto mainColor = get<QComboBox>(&dialog, "frequencyColor"),
             compareColor = get<QComboBox>(&dialog, "comparisonColor");
        mainColor->setCurrentIndex(mainColor->findText("Red"));
        get<QComboBox>(&dialog, "frequencyLineWidth")->setCurrentText("3");
        files->setCurrentIndex(0);
        compareColor->setCurrentIndex(compareColor->findText("Blue"));
        get<QComboBox>(&dialog, "comparisonLineWidth")->setCurrentText("2");
        files->setCurrentIndex(1);
        compareColor->setCurrentIndex(compareColor->findText("DarkGreen"));
        get<QComboBox>(&dialog, "comparisonLineWidth")->setCurrentText("4");
        files->setCurrentIndex(0);
        QCOMPARE(get<QComboBox>(&dialog, "comparisonLineWidth")->currentText(),
                 QString("2"));
        get<QPushButton>(&dialog, "fkCalculate")->click();
        QTRY_COMPARE(updated.count(), 1);
        QTRY_VERIFY(!dialog.isCalculating());
        auto actual = dialog.spectra();
        QCOMPARE(actual.size(), 3);
        captureZoomSelection(&dialog, "frequencyPlot", "frequency");
        QImage initialStyle =
            get<QWidget>(&dialog, "frequencyPlot")->grab().toImage();
        mainColor->setCurrentIndex(mainColor->findText("Magenta"));
        get<QComboBox>(&dialog, "frequencyLineWidth")->setCurrentText("5");
        QVERIFY(initialStyle !=
                get<QWidget>(&dialog, "frequencyPlot")->grab().toImage());
        QCOMPARE(updated.count(), 1);
        mainColor->setCurrentIndex(mainColor->findText("Red"));
        get<QComboBox>(&dialog, "frequencyLineWidth")->setCurrentText("3");
        QVector<float> raw(64);
        auto trace = file.Rs(3);
        std::copy(trace + 10, trace + 74, raw.begin());
        auto expected = calculateFrequency(raw, 1, 64, .002);
        QCOMPARE(actual[0].spectrum.nf, 33);
        for (int i = 0; i < expected.nf; ++i) {
            double tolerance = qMax(1e-4, double(expected.amplitude[i]) * 2e-5);
            QVERIFY(std::abs(actual[0].spectrum.amplitude[i] -
                             expected.amplitude[i]) < tolerance);
            QVERIFY(std::abs(actual[1].spectrum.amplitude[i] -
                             .5 * expected.amplitude[i]) < tolerance);
            QVERIFY(std::abs(actual[2].spectrum.amplitude[i] -
                             2 * expected.amplitude[i]) < tolerance * 2);
        }
        dialog.grab().save(out + "/frequency-single-comparison.png");
        region->setChecked(true);
        get<QPushButton>(&dialog, "fkCalculate")->click();
        QTRY_COMPARE(updated.count(), 2);
        QTRY_VERIFY(!dialog.isCalculating());
        raw.resize(4 * 64);
        for (int x = 0; x < 4; ++x) {
            auto values = file.Rs(x + 3);
            std::copy(values + 10, values + 74, raw.begin() + x * 64);
        }
        expected = calculateFrequency(raw, 4, 64, .002);
        actual = dialog.spectra();
        QCOMPARE(actual.size(), 3);
        for (int f = 0; f < expected.nf; ++f)
            QVERIFY(std::abs(actual[0].spectrum.amplitude[f] -
                             expected.amplitude[f]) <
                    qMax(1e-4, double(expected.amplitude[f]) * 2e-5));
        auto grid = get<QCheckBox>(&dialog, "frequencyGrid");
        grid->setChecked(true);
        get<QSpinBox>(&dialog, "frequencyFontSize")->setValue(12);
        QWidget *plot = get<QWidget>(&dialog, "frequencyPlot");
        QCOMPARE(plot->font().pointSize(), 12);
        QCOMPARE(updated.count(), 2);
        get<QPushButton>(&dialog, "fkZoomAll")->click();
        dialog.grab().save(out + "/frequency-region-comparison.png");
        QImage current = plot->grab().toImage();
        SgyFile other;
        open(other, pathB);
        dialog.setSource(&other);
        QVERIFY(!dialog.isSourceCurrent());
        QCOMPARE(updated.count(), 2);
        QImage gray = plot->grab().toImage();
        gray.save(out + "/frequency-gray.png");
        QCOMPARE(colored(gray), 0);
        dialog.setSource(&file);
        QVERIFY(dialog.isSourceCurrent());
        QCOMPARE(current, plot->grab().toImage());
        QVERIFY(dialog.savePng(out + "/frequency-comparison-300dpi.png", 300));
        get<QPushButton>(&dialog, "comparisonRemove")->click();
        QCOMPARE(files->count(), 1);
        QCOMPARE(dialog.spectra().size(), 2);
        QCOMPARE(updated.count(), 2);
        dialog.close();
        FrequencySpectrumDialog reopened(&file, out);
        QVERIFY(get<QCheckBox>(&reopened, "frequencySelection")->isChecked());
        QVERIFY(get<QCheckBox>(&reopened, "frequencyGrid")->isChecked());
        QCOMPARE(get<QSpinBox>(&reopened, "frequencyFontSize")->value(), 12);
        QCOMPARE(get<QComboBox>(&reopened, "frequencyColor")
                     ->currentData()
                     .value<QColor>(),
                 QColor(Qt::red));
        QCOMPARE(get<QComboBox>(&reopened, "frequencyLineWidth")->currentText(),
                 QString("3"));
        reopened.addComparisonFiles({pathHalf});
        QCOMPARE(
            get<QComboBox>(&reopened, "comparisonLineWidth")->currentText(),
            QString("2"));
        QVERIFY(!reopened.isCalculating());
    }
    void frequencyRejectsMismatchedFilesAndRetainsCancelledCache()
    {
        QSettings settings("WW", "SeiSeeMp");
        settings.remove("FrequencySpectrum");
        SgyFile file;
        open(file, pathA);
        FrequencySpectrumDialog dialog(&file, out);
        dialog.show();
        dialog.addComparisonFiles({pathB, pathShort, pathHalf});
        QSignalSpy updated(&dialog, &SpectrumDialog::spectrumUpdated);
        get<QPushButton>(&dialog, "fkCalculate")->click();
        QTRY_COMPARE(updated.count(), 1);
        QTRY_VERIFY(!dialog.isCalculating());
        QCOMPARE(dialog.spectra().size(), 2);
        auto status = get<QLabel>(&dialog, "fkStatus");
        QVERIFY(status->text().contains("2 file(s) failed"));
        QVERIFY(status->toolTip().contains("Sample interval differs"));
        QVERIFY(status->toolTip().contains("short.sgy"));
        const auto cached = dialog.spectra();
        get<QPushButton>(&dialog, "fkCalculate")->click();
        get<QPushButton>(&dialog, "fkCalculate")->click();
        QTRY_VERIFY(!dialog.isCalculating());
        QCOMPARE(updated.count(), 1);
        QCOMPARE(dialog.spectra()[0].spectrum.amplitude,
                 cached[0].spectrum.amplitude);
        get<QPushButton>(&dialog, "fkCalculate")->click();
        auto worker = dialog.findChild<FkWorker *>();
        QVERIFY(worker);
        QVERIFY(worker->wait(5000));
        dialog.shutdown();
        QApplication::processEvents();
        QCOMPARE(updated.count(), 1);
        QVERIFY(!dialog.isCalculating());
    }
    void fkFontSizeIsPersistentAndDoesNotRecalculate()
    {
        SgyFile file;
        open(file, pathA);
        {
            FkSpectrumDialog dialog(&file, out);
            QSignalSpy updated(&dialog, &SpectrumDialog::spectrumUpdated);
            get<QSpinBox>(&dialog, "fkFontSize")->setValue(14);
            QCOMPARE(get<QWidget>(&dialog, "fkPlot")->font().pointSize(), 14);
            QCOMPARE(updated.count(), 0);
            get<QPushButton>(&dialog, "fkCalculate")->click();
            QTRY_COMPARE(updated.count(), 1);
            QTRY_VERIFY(!dialog.isCalculating());
            dialog.show();
            dialog.grab().save(out + "/fk-font14.png");
        }
        FkSpectrumDialog reopened(&file, out);
        QCOMPARE(get<QSpinBox>(&reopened, "fkFontSize")->value(), 14);
        get<QSpinBox>(&reopened, "fkFontSize")->setValue(9);
    }
    void closingWithQueuedResultIsSafe()
    {
        SgyFile file;
        open(file, pathA);
        FkSpectrumDialog dialog(&file, out);
        QSignalSpy updated(&dialog, &FkSpectrumDialog::spectrumUpdated);
        get<QPushButton>(&dialog, "fkCalculate")->click();
        FkWorker *worker = dialog.findChild<FkWorker *>();
        QVERIFY(worker);
        QVERIFY(worker->wait(
            5000)); // Queue result/finished, then close before delivery.
        dialog.shutdown();
        QApplication::processEvents();
        QCOMPARE(updated.count(), 0);
        QVERIFY(!dialog.isCalculating());
    }
    void toastQueueDoesNotRetainDestroyedParents()
    {
        const int limit = Toast::getMaximumOnScreen();
        const int initial = Toast::getCount();
        Toast::setMaximumOnScreen(1);
        auto owner = new QWidget;
        auto visible = new Toast(owner);
        visible->setDuration(0);
        visible->show();
        auto queued = new Toast(owner);
        queued->setDuration(0);
        queued->show();
        QVERIFY(Toast::getQueuedCount() >= 1);
        delete owner;
        QApplication::processEvents();
        QCOMPARE(Toast::getCount(), initial);
        Toast::setMaximumOnScreen(limit);
    }
    void largeSelectionToastAndCancellation()
    {
        SgyFile file;
        open(file, pathLarge);
        FkSpectrumDialog dialog(&file, out);
        dialog.show();
        get<QPushButton>(&dialog, "fkCalculate")->click();
        QVERIFY(dialog.isCalculating());
        QVERIFY(get<QPushButton>(&dialog, "fkCalculate")->isEnabled());
        bool warning = false;
        for (auto label : dialog.findChildren<QLabel *>())
            if (label->text().contains("Calculation may take"))
                warning = true;
        QVERIFY(warning);
        get<QPushButton>(&dialog, "fkCalculate")->click();
        QTRY_VERIFY(!dialog.isCalculating());
        dialog.close();
    }
};
QTEST_MAIN(FkRegression)
#include "fk_regression.moc"
