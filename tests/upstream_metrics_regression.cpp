#include <QtTest>
#include <QtWidgets>
#include <QMimeData>
#include <stdexcept>
#include <limits>
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
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           qEnvironmentVariable("SEISEE_TEST_SETTINGS_ROOT",out + "/settings"));
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
    void fkMatchesRequestedPythonReference() {
        QFile reference(QFINDTESTDATA("fk_reference.json"));QVERIFY(reference.open(QIODevice::ReadOnly));
        auto cases=QJsonDocument::fromJson(reference.readAll()).object()["cases"].toArray();
        QCOMPARE(cases.size(),5);
        for(auto item:cases) {
            auto c=item.toObject();QVector<float> input;
            for(auto v:c["input"].toArray())input.append(float(v.toDouble()));
            auto result=calculateFk(input,c["nx"].toInt(),c["nt"].toInt(),c["dt"].toDouble(),c["dx"].toDouble(),c["preprocess"].toBool());
            auto expected=c["amplitude"].toArray();auto f=c["frequency"].toArray();auto k=c["wavenumber"].toArray();
            QCOMPARE(result.amplitude.size(),expected.size());QCOMPARE(result.nf,f.size());QCOMPARE(result.nk,k.size());
            double peak=0,error=0;for(auto v:expected)peak=qMax(peak,v.toDouble());
            for(int j=0;j<expected.size();++j)error=qMax(error,std::abs(result.amplitude[j]-expected[j].toDouble()));
            QVERIFY2(error/qMax(peak,1e-12)<2e-5,qPrintable(QString("FK reference error %1, nx=%2 nt=%3").arg(error/qMax(peak,1e-12)).arg(result.nk).arg(c["nt"].toInt())));
            for(int j=0;j<f.size();++j)QVERIFY(std::abs(result.df*j-f[j].toDouble())<1e-10);
            for(int j=0;j<k.size();++j)QVERIFY(std::abs(result.k0+result.dk*j-k[j].toDouble())<1e-10);
        }
        QVector<float> zero(7*37,0);
        auto result=calculateFk(zero,7,37,.002,12.5,true);
        for(float a:result.amplitude)QCOMPARE(a,0.0f);
        QVERIFY_EXCEPTION_THROWN(calculateFk(zero,7,37,.002,0,true),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(calculateFk(zero,7,37,.002,12.5,true,[](){return true;}),std::runtime_error);
        zero[0]=std::numeric_limits<float>::quiet_NaN();
        QVERIFY_EXCEPTION_THROWN(calculateFk(zero,7,37,.002,12.5,false),std::runtime_error);
    }
    void fkDefaultsToVisibleRange() {
        control<QRadioButton>(window,"rbDirNorm")->setChecked(true);
        QMetaObject::invokeMethod(window,"on_zoomAllBtn_pressed",Qt::DirectConnection);QTest::qWait(50);
        auto scroll=control<MyScrollArea>(window,"seismicScrollArea");
        auto open=[this](){control<QAction>(window,"actionFkSpectrum")->trigger();return dialog<FkSpectrumDialog>();};
        auto fk=open();QVERIFY(fk);
        QCOMPARE(control<QSpinBox>(fk,"fkFirst")->value(),1);
        QCOMPARE(control<QSpinBox>(fk,"fkLast")->value(),120);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkStart")->value(),0.0);
        QVERIFY(control<QDoubleSpinBox>(fk,"fkEnd")->value()>1900);
        fk->close();QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        for(int i=0;i<3;++i)QMetaObject::invokeMethod(window,"on_zoomInBtn_pressed",Qt::DirectConnection);
        scroll->horizontalScrollBar()->setValue(0);scroll->verticalScrollBar()->setValue(0);
        // No paint wait: defaults must use the new scale immediately.
        fk=open();QVERIFY(fk);
        int leftLast=control<QSpinBox>(fk,"fkLast")->value();
        double topEnd=control<QDoubleSpinBox>(fk,"fkEnd")->value();
        QVERIFY(leftLast<80);QVERIFY(topEnd<1000);
        fk->close();QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        scroll->horizontalScrollBar()->setValue(scroll->horizontalScrollBar()->maximum());
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        fk=open();QVERIFY(fk);
        int rightFirst=control<QSpinBox>(fk,"fkFirst")->value();
        QVERIFY(rightFirst>leftLast);
        QVERIFY(control<QDoubleSpinBox>(fk,"fkStart")->value()>topEnd);
        QCOMPARE(control<QSpinBox>(fk,"fkLast")->value(),120);
        double t0=control<QDoubleSpinBox>(fk,"fkStart")->value();
        double t1=control<QDoubleSpinBox>(fk,"fkEnd")->value();
        QVERIFY(t0<t1);QVERIFY(t1<=1998);
        QVERIFY(std::abs(t0/2-std::round(t0/2))<1e-6);
        control<QSpinBox>(fk,"fkFirst")->setValue(1);
        control<QAction>(window,"actionFkSpectrum")->trigger();
        QCOMPARE(control<QSpinBox>(fk,"fkFirst")->value(),1);
        fk->close();QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        fk=open();QVERIFY(fk);QCOMPARE(control<QSpinBox>(fk,"fkFirst")->value(),rightFirst);
        fk->close();QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        control<QRadioButton>(window,"rbDirRev")->setChecked(true);
        fk=open();QVERIFY(fk);
        QVERIFY(control<QSpinBox>(fk,"fkFirst")->value()<=control<QSpinBox>(fk,"fkLast")->value());
        QVERIFY(control<QSpinBox>(fk,"fkLast")->value()-control<QSpinBox>(fk,"fkFirst")->value()<80);
        fk->close();QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        control<QRadioButton>(window,"rbDirNorm")->setChecked(true);
        QMetaObject::invokeMethod(window,"on_zoomAllBtn_pressed",Qt::DirectConnection);QTest::qWait(20);
    }
    void fkSizePreferences() {
        control<QAction>(window,"actionFkSpectrum")->trigger();
        auto fk=dialog<FkSpectrumDialog>();QVERIFY(fk);
        if(qEnvironmentVariable("SEISEE_FK_SIZE_PROBE")=="1") {
            QCOMPARE(control<QSpinBox>(fk,"fkImageWidth")->value(),930);
            QCOMPARE(control<QSpinBox>(fk,"fkImageHeight")->value(),510);
            QCOMPARE(control<QWidget>(fk,"fkPlot")->size(),QSize(930,510));
            fk->close();return;
        }
        control<QSpinBox>(fk,"fkImageWidth")->setValue(930);
        control<QSpinBox>(fk,"fkImageHeight")->setValue(510);
        fk->close();QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        control<QAction>(window,"actionFkSpectrum")->trigger();fk=dialog<FkSpectrumDialog>();QVERIFY(fk);
        QCOMPARE(control<QSpinBox>(fk,"fkImageWidth")->value(),930);
        QCOMPARE(control<QSpinBox>(fk,"fkImageHeight")->value(),510);
        fk->close();QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        QProcess child;
        auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert("SEISEE_FK_SIZE_PROBE","1");
        environment.insert("SEISEE_TEST_OUT",out+"/size-probe");
        environment.insert("SEISEE_TEST_SETTINGS_ROOT",out+"/settings");
        child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(),QStringList()<<"fkSizePreferences");
        QVERIFY(child.waitForFinished(15000));QCOMPARE(child.exitStatus(),QProcess::NormalExit);
        QVERIFY2(child.exitCode()==0,child.readAllStandardOutput().constData());
    }
    void fkRangePaletteAndLifecycle() {
        control<QAction>(window,"actionFkSpectrum")->trigger();
        auto fk=dialog<FkSpectrumDialog>();QVERIFY(fk);QVERIFY(!fk->isModal());
        QCOMPARE(fk->windowModality(),Qt::NonModal);QVERIFY(QApplication::activeModalWidget()!=fk);QVERIFY(window->isEnabled());
        control<QSpinBox>(fk,"fkFirst")->setValue(4);
        control<QSpinBox>(fk,"fkLast")->setValue(20);
        control<QDoubleSpinBox>(fk,"fkEnd")->setValue(500);
        control<QDoubleSpinBox>(fk,"fkFmin")->setValue(5);
        control<QDoubleSpinBox>(fk,"fkFmax")->setValue(80);
        control<QDoubleSpinBox>(fk,"fkKmin")->setValue(-.02);
        control<QDoubleSpinBox>(fk,"fkKmax")->setValue(.02);
        control<QDoubleSpinBox>(fk,"fkCmin")->setValue(-4);
        control<QDoubleSpinBox>(fk,"fkCmax")->setValue(2);
        control<QPushButton>(fk,"fkCalculate")->click();QVERIFY(fk->isCalculating());
        QTRY_VERIFY_WITH_TIMEOUT(!fk->isCalculating(),15000);
        QVERIFY2(control<QLabel>(fk,"fkStatus")->text().startsWith("Ready |"),qPrintable(control<QLabel>(fk,"fkStatus")->text()));
        QVERIFY(control<QPushButton>(fk,"fkSave")->isEnabled());
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkFmax")->value(),80.0);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkCmin")->value(),-4.0);
        control<QSpinBox>(fk,"fkLast")->setValue(25);
        control<QDoubleSpinBox>(fk,"fkEnd")->setValue(600);
        control<QPushButton>(fk,"fkCalculate")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!fk->isCalculating(),15000);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkFmin")->value(),5.0);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkFmax")->value(),80.0);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkKmin")->value(),-.02);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkKmax")->value(),.02);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkCmin")->value(),-4.0);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkCmax")->value(),2.0);
        control<QDoubleSpinBox>(fk,"fkFmin")->setValue(5);
        control<QDoubleSpinBox>(fk,"fkFmax")->setValue(80);
        control<QDoubleSpinBox>(fk,"fkKmin")->setValue(-.02);
        control<QDoubleSpinBox>(fk,"fkKmax")->setValue(.02);
        control<QDoubleSpinBox>(fk,"fkCmin")->setValue(-4);
        control<QDoubleSpinBox>(fk,"fkCmax")->setValue(2);
        control<QPushButton>(fk,"fkApply")->click();
        auto plot=control<QWidget>(fk,"fkPlot");
        auto widthEdit=control<QSpinBox>(fk,"fkImageWidth");
        auto heightEdit=control<QSpinBox>(fk,"fkImageHeight");
        widthEdit->setFocus();QTest::keyClick(widthEdit,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(widthEdit,"1200");
        QCOMPARE(widthEdit->value(),1200);QCOMPARE(plot->width(),1200);
        heightEdit->setFocus();QTest::keyClick(heightEdit,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(heightEdit,"420");
        QCOMPARE(heightEdit->value(),420);QCOMPARE(plot->height(),420);
        QTest::keyClick(heightEdit,Qt::Key_Return);QVERIFY(!fk->isCalculating());
        control<QPushButton>(fk,"fkApplySize")->click();
        QVERIFY(fk->savePng(out+"/fk_typed_size.png"));QCOMPARE(QImage(out+"/fk_typed_size.png").size(),QSize(1200,420));
        auto viewport=control<QScrollArea>(fk,"fkViewport");
        QTRY_VERIFY(viewport->horizontalScrollBar()->maximum()>0);
        fk->grab().save(out+"/fk_jet_log.png");
        auto jet=plot->grab().toImage();
        control<QComboBox>(fk,"fkPalette")->setCurrentIndex(1);
        auto viridis=plot->grab().toImage();QVERIFY(jet!=viridis);
        control<QSpinBox>(fk,"fkImageWidth")->setValue(800);
        control<QSpinBox>(fk,"fkImageHeight")->setValue(460);
        QCOMPARE(plot->size(),QSize(800,460));
        control<QComboBox>(fk,"fkPalette")->setCurrentIndex(3);
        QVERIFY(fk->savePng(out+"/fk_smooth.png"));
        QImage smooth(out+"/fk_smooth.png");QCOMPARE(smooth.size(),QSize(800,460));
        control<QComboBox>(fk,"fkInterpolation")->setCurrentIndex(1);
        QVERIFY(fk->savePng(out+"/fk_nearest.png"));
        QImage nearest(out+"/fk_nearest.png");QVERIFY(smooth!=nearest);
        auto maxRowColors=[](const QImage& image) {
            int count=0;
            for(int y=40;y<image.height()-80;y+=5) {
                QSet<QRgb> colors;
                for(int x=80;x<image.width()-155;++x)colors.insert(image.pixel(x,y));
                count=qMax(count,colors.size());
            }
            return count;
        };
        QVERIFY(maxRowColors(smooth)>maxRowColors(nearest));
        control<QComboBox>(fk,"fkInterpolation")->setCurrentIndex(0);
        control<QComboBox>(fk,"fkPalette")->setCurrentIndex(1);
        control<QSpinBox>(fk,"fkImageWidth")->setValue(700);
        control<QSpinBox>(fk,"fkImageHeight")->setValue(600);
        QVERIFY(fk->savePng(out+"/fk_tall.png"));QCOMPARE(QImage(out+"/fk_tall.png").size(),QSize(700,600));
        control<QSpinBox>(fk,"fkImageWidth")->setValue(1000);
        control<QSpinBox>(fk,"fkImageHeight")->setValue(360);
        control<QComboBox>(fk,"fkScale")->setCurrentIndex(1);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkCmin")->value(),-4.0);
        QCOMPARE(control<QDoubleSpinBox>(fk,"fkCmax")->value(),2.0);
        control<QPushButton>(fk,"fkAutoColor")->click();
        fk->grab().save(out+"/fk_viridis_linear.png");
        control<QDoubleSpinBox>(fk,"fkFmin")->setValue(90);
        control<QPushButton>(fk,"fkApply")->click();
        QVERIFY(control<QLabel>(fk,"fkStatus")->text().contains("minimum"));
        control<QDoubleSpinBox>(fk,"fkFmin")->setValue(5);
        control<QSpinBox>(fk,"fkLast")->setValue(120);
        control<QDoubleSpinBox>(fk,"fkEnd")->setValue(1998);
        control<QPushButton>(fk,"fkCalculate")->click();QVERIFY(fk->isCalculating());
        fk->close();
        QTRY_VERIFY(dialog<FkSpectrumDialog>()==nullptr);
        control<QAction>(window,"actionFkSpectrum")->trigger();fk=dialog<FkSpectrumDialog>();QVERIFY(fk);
        control<QPushButton>(fk,"fkCalculate")->click();QVERIFY(fk->isCalculating());
        QVERIFY(window->close());QVERIFY(!fk->isCalculating());QVERIFY(!fk->isVisible());
        QTest::qWait(10);window->show();
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
