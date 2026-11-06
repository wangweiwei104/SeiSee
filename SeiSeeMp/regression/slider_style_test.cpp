#include "customcontrols.h"

#include <QApplication>
#include <QCursor>
#include <QImage>
#include <QMouseEvent>
#include <QSlider>
#include <QTest>
#include <QVBoxLayout>

class TestSlider : public QSlider
{
public:
    explicit TestSlider(Qt::Orientation orientation) : QSlider(orientation) {}

    QRect handleRect() const
    {
        QStyleOptionSlider option;
        initStyleOption(&option);
        return style()->subControlRect(QStyle::CC_Slider, &option,
                                       QStyle::SC_SliderHandle, this);
    }
};

class SliderStyleTest : public QObject
{
    Q_OBJECT

private slots:
    void roundHandle_data();
    void roundHandle();
};

void SliderStyleTest::roundHandle_data()
{
    QTest::addColumn<Qt::Orientation>("orientation");
    QTest::addColumn<int>("value");
    for (Qt::Orientation orientation : {Qt::Horizontal, Qt::Vertical}) {
        for (int value : {0, 50, 100}) {
            const QByteArray name = QByteArray::number(int(orientation)) +
                                    "-" + QByteArray::number(value);
            QTest::newRow(name.constData()) << orientation << value;
        }
    }
}

static QRect blueBounds(const QImage &image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.blue() - pixel.red() > 40 &&
                pixel.blue() - pixel.green() > 20)
                bounds |= QRect(x, y, 1, 1);
        }
    }
    return bounds;
}

void SliderStyleTest::roundHandle()
{
    QFETCH(Qt::Orientation, orientation);
    QFETCH(int, value);

    QWidget window;
    QVBoxLayout layout(&window);
    TestSlider slider(orientation);
    slider.setRange(0, 100);
    slider.setValue(value);
    if (orientation == Qt::Horizontal)
        slider.setMinimumWidth(200);
    else
        slider.setMinimumHeight(200);
    layout.addWidget(&slider);
    qApp->setStyleSheet(sliderStyleSheet());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCursor::setPos(window.mapToGlobal(QPoint(0, 0)));
    QTest::mouseMove(&slider, slider.handleRect().center());
    QTest::qWait(20);
    QTest::mouseMove(&slider, QPoint(1, 1));
    QTest::qWait(50);

    const QRect handle = slider.handleRect();
    QCOMPARE(handle.size(), QSize(16, 16));
    const QImage idle = slider.grab().toImage();
    const QRect idleBounds = blueBounds(idle);
    QVERIFY(!idleBounds.isEmpty());
    const qreal scale = slider.devicePixelRatioF();
    QVERIFY2(qAbs(idleBounds.width() - 10 * scale) <= 2,
             qPrintable(QStringLiteral("idle width %1, scale %2")
                            .arg(idleBounds.width()).arg(scale)));
    QVERIFY2(qAbs(idleBounds.height() - 10 * scale) <= 2,
             qPrintable(QStringLiteral("idle height %1, scale %2")
                            .arg(idleBounds.height()).arg(scale)));
    QCOMPARE(idle.pixelColor(idleBounds.center()), QColor(ControlAccentColor));
    QVERIFY(idle.pixelColor(idleBounds.topLeft()) != QColor(ControlAccentColor));
    QVERIFY(idle.pixelColor(idleBounds.bottomRight()) != QColor(ControlAccentColor));

    QTest::mouseMove(&slider, handle.center());
    QTest::qWait(50);
    QCOMPARE(slider.handleRect(), handle);
    const QImage hover = slider.grab().toImage();
    const QRect bounds = blueBounds(hover);
    QVERIFY(!bounds.isEmpty());
    QVERIFY(qAbs(bounds.width() - 14 * scale) <= 2);
    QVERIFY(qAbs(bounds.height() - 14 * scale) <= 2);
    const QRect physicalHandle(qRound(handle.left() * scale),
                               qRound(handle.top() * scale),
                               qRound(handle.width() * scale),
                               qRound(handle.height() * scale));
    QVERIFY(physicalHandle.adjusted(-1, -1, 1, 1).contains(bounds));
    QVERIFY(bounds.width() > idleBounds.width());
    QVERIFY(bounds.height() > idleBounds.height());
    QVERIFY(qAbs(bounds.width() - bounds.height()) <= 1);
    QCOMPARE(hover.pixelColor(bounds.center()), QColor(ControlAccentHoverColor));
    QVERIFY(hover.pixelColor(bounds.topLeft()) != QColor(ControlAccentHoverColor));
    QVERIFY(hover.pixelColor(bounds.topRight()) != QColor(ControlAccentHoverColor));
    QVERIFY(hover.pixelColor(bounds.bottomLeft()) != QColor(ControlAccentHoverColor));
    QVERIFY(hover.pixelColor(bounds.bottomRight()) != QColor(ControlAccentHoverColor));

    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, handle.center());
    const QImage pressed = slider.grab().toImage();
    const QRect pressedBounds = blueBounds(pressed);
    QVERIFY(qAbs(pressedBounds.width() - 14 * scale) <= 2);
    QVERIFY(qAbs(pressedBounds.height() - 14 * scale) <= 2);
    QCOMPARE(pressed.pixelColor(bounds.center()), QColor(ControlAccentColor));
    QVERIFY(pressed.pixelColor(bounds.topLeft()) != QColor(ControlAccentColor));
    QPoint releasePos = handle.center();
    if (value == 50) {
        releasePos = orientation == Qt::Horizontal
                         ? QPoint(slider.width() - 12, handle.center().y())
                         : QPoint(handle.center().x(), 12);
        QMouseEvent move(QEvent::MouseMove, releasePos,
                         slider.mapToGlobal(releasePos), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&slider, &move);
        QVERIFY(slider.value() > 50);
    }
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, releasePos);
}

int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    SliderStyleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "slider_style_test.moc"
