#include <QtTest>
#include <QtWidgets>
#include <QMimeData>
#include "mainwindow.h"
#include "workthread.h"
#include "customcontrols.h"

class TestWindow : public MainWindow {
public:
    void openFixture(const QString& path) {
        QMimeData mime;
        mime.setUrls(QList<QUrl>() << QUrl::fromLocalFile(path));
        QDropEvent event(QPointF(50, 50), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        dropEvent(&event);
    }
};

class UiRegression : public QObject {
    Q_OBJECT
    QString out;
    QString fixture;
    TestWindow* window = nullptr;

    template<class T> T* control(QObject* owner, const char* name) {
        T* result = owner->findChild<T*>(name);
        if (!result) qFatal("Missing widget: %s", name);
        return result;
    }
    template<class T> T* dialog() {
        for (QWidget* widget : QApplication::topLevelWidgets())
            if (T* result = qobject_cast<T*>(widget)) return result;
        return nullptr;
    }
    QImage screenshot(const QString& name) {
        QMetaObject::invokeMethod(window, "on_screenShotBtn_pressed", Qt::DirectConnection);
        const QFileInfoList files = QDir(out + "/data").entryInfoList(QStringList() << "*.png", QDir::Files, QDir::Time);
        if (files.isEmpty()) return QImage();
        QImage image(files.first().absoluteFilePath());
        image.save(out + "/" + name + ".png");
        return image;
    }
private slots:
    void initTestCase() {
        int fontId = QFontDatabase::addApplicationFont(":/fonts/Roboto-Regular-14.ttf");
        QVERIFY(fontId >= 0);
        QFont font(QFontDatabase::applicationFontFamilies(fontId).first()); font.setPointSizeF(9);
        QApplication::setFont(font);

        out = qEnvironmentVariable("SEISEE_TEST_OUT");
        QVERIFY(!out.isEmpty());
        QDir().mkpath(out + "/data");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, out + "/settings");
        fixture = out + "/data/fixture.sgy";
        QFile file(fixture); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(3200, ' '));
        QByteArray binary(400, 0);
        auto u16 = [](QByteArray& b, int o, int v) { b[o] = char(v >> 8); b[o+1] = char(v); };
        u16(binary, 16, 2000); u16(binary, 20, 1000); u16(binary, 24, 5);
        file.write(binary);
        for (int t = 0; t < 120; ++t) {
            QByteArray header(240, 0);
            header[3] = char(t+1); header[7] = char(t+1); header[11] = char(1);
            u16(header, 114, 1000); u16(header, 116, 2000);
            file.write(header);
            QDataStream stream(&file); stream.setByteOrder(QDataStream::BigEndian);
            stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
            for (int s = 0; s < 1000; ++s)
                stream << float(std::sin(s * .12 + t * .15) * std::exp(-s * .002));
        }
        file.close();
        window = new TestWindow;
        window->resize(1280, 820);
        window->show();
        control<QTabWidget>(window,"SeisTab")->setCurrentIndex(0);
        window->openFixture(fixture);
        QTest::qWait(600);
        QMetaObject::invokeMethod(window,"on_zoomAllBtn_pressed",Qt::DirectConnection);
        QTest::qWait(100);

        window->grab().save(out + "/main_window.png");
    }
    void numericGainAndRange() {
        QString scaled = out + "/data/half.sgy";
        QFile::remove(scaled); QVERIFY(QFile::copy(fixture, scaled));
        QFile file(scaled); QVERIFY(file.open(QIODevice::ReadWrite));
        double expected = 0;
        for(int t=0;t<120;++t) {
            QVERIFY(file.seek(3600 + t*(240+4000) + 240));
            QDataStream stream(&file); stream.setByteOrder(QDataStream::BigEndian);
            stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
            for(int n=0;n<1000;++n) {
                float value=float(std::sin(n*.12+t*.15)*std::exp(-n*.002));
                stream << float(value*.5f);
                if(t>=2 && t<12 && (t-2)%2==0 && n>=10 && n<=110) expected+=double(value)*value*.25;
            }
        }
        file.close();
        SgyFile reference, prediction;
        reference.setFname(fixture); reference.setActive(true);
        prediction.setFname(scaled); prediction.setActive(true);
        QVERIFY(reference.Active()); QVERIFY(prediction.Active());
        MetricsWorker worker(&reference,&prediction,true,2,12,2,20,220,true,true,true,true,false);
        QSignalSpy results(&worker,SIGNAL(emetrics(QString)));
        worker.process(); QCOMPARE(results.size(),1);
        QString report=results.first().first().toString();
        auto value=[report](const QString& metric) {
            return QRegularExpression("^"+metric+"\\s*=\\s*(\\S+)",QRegularExpression::MultilineOption).match(report).captured(1).toDouble();
        };
        QVERIFY(std::abs(value("MSE") - expected/(5*101)) < 1e-8);
        QVERIFY(std::abs(value("SNR") - 6.020599913) < 1e-7);
        QVERIFY(report.contains("Total traces used: 5"));
        QVERIFY(report.contains("Total samples used: 505"));
    }
    void metricsLayoutAndCalculation() {
        control<QAction>(window,"actionMetrics")->trigger();
        MetricsDialog* metrics = dialog<MetricsDialog>(); QVERIFY(metrics);
        QVERIFY(metrics->styleSheet().isEmpty());
        auto calculation = control<CustomRadioButton>(metrics,"rbCalculation");
        auto estimation = control<CustomRadioButton>(metrics,"rbEstimation");
        QCOMPARE(calculation->text(),QString("Calculation mode"));
        QCOMPARE(estimation->text(),QString("Estimation mode"));
        QCOMPARE(control<QLabel>(metrics,"label_file1")->text(),QString("Signal file:"));
        QCOMPARE(control<QLabel>(metrics,"label_file2")->text(),QString("Noisy file:"));
        for(auto box:metrics->findChildren<QCheckBox*>()) QVERIFY(qobject_cast<CustomCheckBox*>(box));
        for(auto radio:metrics->findChildren<QRadioButton*>()) QVERIFY(qobject_cast<CustomRadioButton*>(radio));
        QTest::mouseClick(control<CustomCheckBox>(metrics,"ckTrAll"),Qt::LeftButton);
        QVERIFY(control<QLineEdit>(metrics,"edTrMin")->isEnabled());
        QTest::mouseClick(control<CustomCheckBox>(metrics,"ckTrAll"),Qt::LeftButton);
        QVERIFY(!control<QLineEdit>(metrics,"edTrMin")->isEnabled());
        const QString compare = out + "/data/compare.sgy";
        QFile::remove(compare); QVERIFY(QFile::copy(fixture, compare));
        control<QLineEdit>(metrics,"pathFile2")->setText(compare);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),10000);
        auto table = control<QTableWidget>(metrics,"metricTable");
        QCOMPARE(table->item(0,1)->text(),QString("0"));
        QCOMPARE(table->item(4,1)->text(),QString("1"));
        QCOMPARE(control<QProgressBar>(metrics,"progressBar")->value(),100);
        QVERIFY(control<QToolButton>(metrics,"btnSaveResults")->isEnabled());
        metrics->grab().save(out + "/metrics_reference.png");
        // Select a different reference through the real dialog and verify it is read.
        const QString alternate = out + "/data/alternate.sgy";
        QFile::remove(alternate); QVERIFY(QFile::copy(fixture, alternate));
        QFile altered(alternate); QVERIFY(altered.open(QIODevice::ReadWrite));
        QVERIFY(altered.seek(3600 + 240));
        QDataStream changed(&altered); changed.setByteOrder(QDataStream::BigEndian);
        changed.setFloatingPointPrecision(QDataStream::SinglePrecision); changed << float(50);
        altered.close();
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QTimer chooseTimer;
        connect(&chooseTimer, &QTimer::timeout, [alternate]() {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (QFileDialog* chooser = qobject_cast<QFileDialog*>(widget)) {
                    if (chooser->isVisible()) {
                        if (QLineEdit* fileName = chooser->findChild<QLineEdit*>("fileNameEdit"))
                            fileName->setText(alternate);
                        QMetaObject::invokeMethod(chooser,"accept",Qt::QueuedConnection);
                    }
                }
            }
        });
        chooseTimer.start(200);
        QTimer deadline; deadline.setSingleShot(true);
        connect(&deadline, &QTimer::timeout, []() {
            if (QDialog* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) modal->reject();
        });
        deadline.start(5000);
        QMetaObject::invokeMethod(metrics,"on_btnOpenFile1_clicked",Qt::DirectConnection);
        chooseTimer.stop(); deadline.stop();
        QCOMPARE(QDir::cleanPath(control<QLineEdit>(metrics,"pathFile1")->text()), QDir::cleanPath(alternate));
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),10000);
        QVERIFY(table->item(0,1)->text().toDouble() > 0);
        const QString saved = out + "/metrics-export.txt";
        QTimer saveTimer;
        connect(&saveTimer,&QTimer::timeout,[saved]() {
            for(QWidget* widget: QApplication::topLevelWidgets()) {
                if(auto chooser=qobject_cast<QFileDialog*>(widget)) {
                    if(chooser->isVisible()) {
                        if(auto edit=chooser->findChild<QLineEdit*>("fileNameEdit")) edit->setText(saved);
                        QMetaObject::invokeMethod(chooser,"accept",Qt::QueuedConnection);
                    }
                } else if(auto message=qobject_cast<QMessageBox*>(widget)) {
                    if(message->isVisible()) QMetaObject::invokeMethod(message,"accept",Qt::QueuedConnection);
                }
            }
        });
        QFile::remove(saved); saveTimer.start(100);
        QMetaObject::invokeMethod(metrics,"on_btnSaveResults_clicked",Qt::DirectConnection);
        saveTimer.stop();
        QFile exported(saved); QVERIFY(exported.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(exported.readAll()).replace("\r\n","\n"),control<QPlainTextEdit>(metrics,"txtResults")->toPlainText());
        QTest::mouseClick(estimation,Qt::LeftButton);
        QCOMPARE(QDir::cleanPath(control<QLineEdit>(metrics,"pathFile1")->text()),QDir::cleanPath(fixture));
        QVERIFY(control<QLabel>(metrics,"modeHint")->text().contains("mean"));
        QCOMPARE(control<QToolButton>(metrics,"btnCalculate")->text(),QString("Estimate"));
        QCOMPARE(control<QLabel>(metrics,"label_file1")->text(),QString("Input file:"));
        QVERIFY(control<QLineEdit>(metrics,"pathFile2")->isHidden());
        QVERIFY(!calculation->isChecked()); QVERIFY(estimation->isChecked());
        QVERIFY(!control<QToolButton>(metrics,"btnSaveResults")->isEnabled());
        for(auto box:metrics->findChildren<CustomCheckBox*>())
            QVERIFY2(box->width() >= box->sizeHint().width(),qPrintable(box->objectName()));
        for(auto radio:metrics->findChildren<CustomRadioButton*>())
            QVERIFY2(radio->width() >= radio->sizeHint().width(),qPrintable(radio->objectName()));
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),10000);
        QVERIFY(control<QPlainTextEdit>(metrics,"txtResults")->toPlainText().contains("Mean trace"));
        control<QCheckBox>(metrics,"chkTPRS")->setChecked(true);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),20000);
        QVERIFY(control<QPlainTextEdit>(metrics,"txtResults")->toPlainText().contains("TPRS"));
        metrics->grab().save(out + "/metrics_real.png");
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        metrics->reject(); QVERIFY(metrics->isVisible());
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),10000);
        QVERIFY(control<QLineEdit>(metrics,"edMess")->text().contains("stopped"));
        QVERIFY(!control<QToolButton>(metrics,"btnSaveResults")->isEnabled());
        metrics->close();
    }
    void validationMessages() {
        control<QAction>(window,"actionMetrics")->trigger();
        MetricsDialog* metrics=dialog<MetricsDialog>(); QVERIFY(metrics);
        control<QRadioButton>(metrics,"rbCalculation")->setChecked(true);
        QString bad=out+"/data/bad-interval.sgy";
        QFile::remove(bad); QVERIFY(QFile::copy(fixture,bad));
        QFile file(bad); QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.seek(3216)); file.write(QByteArray::fromHex("0fa0")); file.close();
        control<QLineEdit>(metrics,"pathFile2")->setText(bad);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QVERIFY(control<QLineEdit>(metrics,"edMess")->text().contains("sample interval"));
        QVERIFY(!metrics->isCalculating());
        metrics->close();
    }
    void cleanupTestCase() {


        if(window) { window->close(); delete window; window=nullptr; }
    }
};
QTEST_MAIN(UiRegression)
#include "upstream_metrics_regression.moc"
