#include <QtTest>
#include <cmath>
#include <limits>
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
    void tprsSnrConversion() {
        const double inputs[] = {.84, .86, .90, .88};
        const double expected[] = {7.201593034, 7.88370415, 9.542425094, 8.653014262};
        for (int i=0;i<4;++i) QVERIFY(std::abs(MetricsWorker::snrFromTprs(inputs[i])-expected[i])<1e-7);
        QCOMPARE(MetricsWorker::snrFromTprs(.5),0.0);
        QCOMPARE(MetricsWorker::snrFromTprs(0),-std::numeric_limits<double>::infinity());
        QCOMPARE(MetricsWorker::snrFromTprs(1),std::numeric_limits<double>::infinity());
        QVERIFY(std::isnan(MetricsWorker::snrFromTprs(-.1)));
        QVERIFY(std::isnan(MetricsWorker::snrFromTprs(1.1)));
        QVERIFY(std::isnan(MetricsWorker::snrFromTprs(std::numeric_limits<double>::quiet_NaN())));
    }
    void cwpFloatTPRS() {
        for (float gain: {1.0f, 0.5f, 0.0f}) {
            QString path=out+QString("/data/tprs-%1.sgy").arg(gain);
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QByteArray(3200,' ')); QByteArray binary(400,0);
            auto u16=[](QByteArray& b,int o,int v){b[o]=char(v>>8);b[o+1]=char(v);};
            u16(binary,16,2000);u16(binary,20,70);u16(binary,24,5);file.write(binary);
            for(int t=0;t<2;++t) {
                QByteArray header(240,0);u16(header,114,70);u16(header,116,2000);file.write(header);
                QDataStream stream(&file);stream.setByteOrder(QDataStream::BigEndian);
                stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
                for(int i=0;i<70;++i)stream<<float(std::sin(2*3.141592653589793*i/10)*(t?gain:1));
            }
            file.close();SgyFile input;input.setFname(path);input.setActive(true);QVERIFY(input.Active());
            MetricsWorker worker(&input,nullptr,false,0,2,1,0,138,false,false,false,false,true);
            QSignalSpy reports(&worker,SIGNAL(emetrics(QString)));worker.process();QCOMPARE(reports.size(),1);
            QString report=reports.first().first().toString();
            double actual=QRegularExpression("^TPRS\\s*=\\s*(\\S+)",QRegularExpression::MultilineOption).match(report).captured(1).toDouble();
            double expected=2*gain/(1+gain*gain);
            QVERIFY2(std::abs(actual-expected)<2e-5,qPrintable(report));
            QVERIFY(report.split('\n').at(1).startsWith("Noisy file: "));
            for (int mode=0;mode<3;++mode) {
                MetricsWorker estimated(&input,nullptr,false,0,2,1,0,138,false,mode==2,false,true,mode!=0);
                QSignalSpy results(&estimated,SIGNAL(emetrics(QString)));estimated.process();QCOMPARE(results.size(),1);
                QString text=results.first().first().toString();
                QString snr=QRegularExpression("^SNR\\s*=\\s*(\\S+)",QRegularExpression::MultilineOption).match(text).captured(1);
                if (gain==0) QCOMPARE(snr,QString("-inf"));
                else if (gain==1) QCOMPARE(snr,QString("inf"));
                else QVERIFY2(std::abs(snr.toDouble()-MetricsWorker::snrFromTprs(actual))<1e-7,qPrintable(text));
                QCOMPARE(text.contains("\nTPRS ="),mode!=0);
                QVERIFY(text.contains("SNR method: 10*log10(TPRS/(1-TPRS))"));
            }
            MetricsWorker insufficient(&input,nullptr,false,0,1,1,0,138,false,false,false,true,false);
            QSignalSpy missing(&insufficient,SIGNAL(emetrics(QString)));insufficient.process();
            QCOMPARE(missing.size(),1);QVERIFY(missing.first().first().toString().contains("SNR  = N/A"));
        }
    }
    void metricsLayoutAndCalculation() {
        control<QAction>(window,"actionMetrics")->trigger();
        MetricsDialog* metrics=dialog<MetricsDialog>();QVERIFY(metrics);
        QCOMPARE(metrics->windowModality(),Qt::NonModal);QVERIFY(!metrics->isModal());QVERIFY(window->isEnabled());
        auto calculation=control<CustomRadioButton>(metrics,"rbCalculation");
        auto estimation=control<CustomRadioButton>(metrics,"rbEstimation");
        QVERIFY(!control<QLineEdit>(metrics,"pathFile1")->isEnabled());
        QVERIFY(control<QLineEdit>(metrics,"pathFile1")->isReadOnly());
        QVERIFY(!control<QToolButton>(metrics,"btnOpenFile1")->isEnabled());
        QCOMPARE(control<QLabel>(metrics,"label_file1")->text(),QString("Signal file:"));
        for(auto box:metrics->findChildren<QCheckBox*>())QVERIFY(qobject_cast<CustomCheckBox*>(box));
        for(auto radio:metrics->findChildren<QRadioButton*>())QVERIFY(qobject_cast<CustomRadioButton*>(radio));
        // With no chosen noisy file, estimation falls back to the captured input.
        QTest::mouseClick(estimation,Qt::LeftButton,Qt::NoModifier,QPoint(8,estimation->height()/2));
        QCOMPARE(QFileInfo(control<QLineEdit>(metrics,"pathFile1")->text()).canonicalFilePath(),QFileInfo(fixture).canonicalFilePath());
        QVERIFY(control<QToolButton>(metrics,"btnOpenFile1")->isEnabled());
        QTest::mouseClick(calculation,Qt::LeftButton,Qt::NoModifier,QPoint(8,calculation->height()/2));
        const QString compare=out+"/data/compare.sgy";
        QFile::remove(compare);QVERIFY(QFile::copy(fixture,compare));
        control<QLineEdit>(metrics,"pathFile2")->setText(compare);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),10000);
        auto table=control<MyStringTable>(metrics,"metricTable");
        QCOMPARE(table->Cell(0,1),QString("0"));QCOMPARE(table->Cell(4,1),QString("1"));
        QString report=control<QPlainTextEdit>(metrics,"txtResults")->toPlainText();
        QVERIFY(report.split('\n').at(1).startsWith("Signal file: "));
        QVERIFY(report.split('\n').at(2).startsWith("Noisy file: "));
        QVERIFY(report.contains("compare.sgy"));
        metrics->grab().save(out+"/metrics_reference.png");
        QTest::mouseClick(estimation,Qt::LeftButton,Qt::NoModifier,QPoint(8,estimation->height()/2));
        QCOMPARE(QFileInfo(control<QLineEdit>(metrics,"pathFile1")->text()).canonicalFilePath(),QFileInfo(compare).canonicalFilePath());
        QCOMPARE(control<QLabel>(metrics,"label_file1")->text(),QString("Noisy file:"));
        QVERIFY(control<QToolButton>(metrics,"btnOpenFile1")->isEnabled());
        QVERIFY(control<QLineEdit>(metrics,"pathFile1")->isEnabled());QVERIFY(!control<QLineEdit>(metrics,"pathFile1")->isReadOnly());
        QVERIFY(control<QLineEdit>(metrics,"pathFile2")->isHidden());
        const QString alternate=out+"/data/alternate.sgy";
        QFile::remove(alternate);QVERIFY(QFile::copy(fixture,alternate));
        QFile altered(alternate);QVERIFY(altered.open(QIODevice::ReadWrite));QVERIFY(altered.seek(3840));
        QDataStream changed(&altered);changed.setByteOrder(QDataStream::BigEndian);changed.setFloatingPointPrecision(QDataStream::SinglePrecision);changed<<float(50);altered.close();
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QTimer chooser;connect(&chooser,&QTimer::timeout,[alternate](){
            for(QWidget* w:QApplication::topLevelWidgets())if(auto d=qobject_cast<QFileDialog*>(w))if(d->isVisible()){
                d->findChild<QLineEdit*>("fileNameEdit")->setText(alternate);QMetaObject::invokeMethod(d,"accept",Qt::QueuedConnection);
            }
        });chooser.start(100);
        QMetaObject::invokeMethod(metrics,"on_btnOpenFile1_clicked",Qt::DirectConnection);chooser.stop();
        QCOMPARE(QFileInfo(control<QLineEdit>(metrics,"pathFile1")->text()).canonicalFilePath(),QFileInfo(alternate).canonicalFilePath());
        control<QCheckBox>(metrics,"chkTPRS")->setChecked(true);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),20000);
        report=control<QPlainTextEdit>(metrics,"txtResults")->toPlainText();QVERIFY(report.contains("alternate.sgy"));
        QVERIFY(report.contains("Mean trace"));QVERIFY(report.contains("TPRS"));
        metrics->grab().save(out+"/metrics_real.png");
        // Browser changes update the save destination while the non-modal dialog stays open.
        QString savedir=out+"/browser destination";QDir().mkpath(savedir);
        QString expected=QDir(savedir).absoluteFilePath("alternate-metrics.txt"),suggested;
        QFile::remove(expected);
        QAction directory;directory.setData(savedir);
        QMetaObject::invokeMethod(window,"on_menu_Recent_Folders_triggered",Qt::DirectConnection,Q_ARG(QAction*,&directory));
        QCOMPARE(QFileInfo(metrics->curDir).canonicalFilePath(),QFileInfo(savedir).canonicalFilePath());
        QTimer save;
        connect(&save,&QTimer::timeout,[&suggested](){
            for(QWidget* w:QApplication::topLevelWidgets())if(auto d=qobject_cast<QFileDialog*>(w))if(d->isVisible()){
                suggested=d->selectedFiles().value(0);QMetaObject::invokeMethod(d,"accept",Qt::QueuedConnection);
            }
        });save.start(100);QMetaObject::invokeMethod(metrics,"on_btnSaveResults_clicked",Qt::DirectConnection);save.stop();
        QCOMPARE(QDir::fromNativeSeparators(suggested),QDir::fromNativeSeparators(expected));
        QFile exported(expected);QVERIFY(exported.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(exported.readAll()).replace("\r\n","\n"),report);
        QTest::mouseClick(calculation,Qt::LeftButton,Qt::NoModifier,QPoint(8,calculation->height()/2));
        QCOMPARE(QFileInfo(control<QLineEdit>(metrics,"pathFile2")->text()).canonicalFilePath(),QFileInfo(alternate).canonicalFilePath());
        const QString fixedSignal=control<QLineEdit>(metrics,"pathFile1")->text();
        // Replacing the main-window file must not invalidate the dialog's captured signal.
        window->openFixture(compare);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),10000);
        QCOMPARE(control<QLineEdit>(metrics,"pathFile1")->text(),fixedSignal);
        QVERIFY(table->Cell(0,1).toDouble()>0);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!metrics->isCalculating(),10000);
        QVERIFY(!control<QToolButton>(metrics,"btnSaveResults")->isEnabled());
        metrics->close();
        window->openFixture(fixture);
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
    void nonModalCloseDuringCalculation() {
        control<QAction>(window,"actionMetrics")->trigger();
        MetricsDialog* metrics=dialog<MetricsDialog>();QVERIFY(metrics);
        control<QRadioButton>(metrics,"rbEstimation")->setChecked(true);
        control<QLineEdit>(metrics,"pathFile1")->setText(fixture);
        control<QCheckBox>(metrics,"chkTPRS")->setChecked(true);
        QMetaObject::invokeMethod(metrics,"on_btnCalculate_clicked",Qt::DirectConnection);
        QVERIFY(metrics->isCalculating());
        QVERIFY(window->close());
        QVERIFY(!window->isVisible());QVERIFY(!metrics->isVisible());QVERIFY(!metrics->isCalculating());
        QTest::qWait(50);
    }
    void cleanupTestCase() {


        if(window) { window->close(); delete window; window=nullptr; }
    }
};
QTEST_MAIN(UiRegression)
#include "upstream_metrics_regression.moc"
