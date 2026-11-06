#ifndef CUSTOMCONTROLS_H
#define CUSTOMCONTROLS_H

#include <QCheckBox>
#include <QFrame>
#include <QRadioButton>
#include <QScrollBar>
#include <QStyleOptionSlider>
#include <QStylePainter>

class QPainter;

static const char ControlAccentColor[] = "#1c6dd1";
static const char ControlAccentHoverColor[] = "#5797d7";

inline QString scrollBarStyleSheet()
{
    // Reserve end margins for the arrow buttons, outside the draggable groove.
    return QStringLiteral(
        "QScrollBar:horizontal { height: 17px; margin: 0 17px; border: 0; "
        "background: #f5f5f5; }"
        "QScrollBar:vertical { width: 17px; margin: 17px 0; border: 0; "
        "background: #f5f5f5; }"
        "QScrollBar::handle:horizontal { min-width: 28px; "
        "margin: 7px 2px; border-radius: 1px; background: #858585; }"
        "QScrollBar::handle:vertical { min-height: 28px; "
        "margin: 2px 7px; border-radius: 1px; background: #858585; }"
        "QScrollBar::handle:horizontal:hover { margin: 5px 2px; "
        "border-radius: 3px; background: %2; }"
        "QScrollBar::handle:vertical:hover { margin: 2px 5px; "
        "border-radius: 3px; background: %2; }"
        "QScrollBar::handle:horizontal:pressed { margin: 5px 2px; "
        "border-radius: 3px; background: %1; }"
        "QScrollBar::handle:vertical:pressed { margin: 2px 5px; "
        "border-radius: 3px; background: %1; }"
        "QScrollBar::handle:disabled { background: #cccccc; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: #f5f5f5; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
        "width: 17px; border: 0; background: #f5f5f5; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "height: 17px; border: 0; background: #f5f5f5; }"
        "QScrollBar::add-line:horizontal { subcontrol-origin: border; "
        "subcontrol-position: right; right: -17px; }"
        "QScrollBar::sub-line:horizontal { subcontrol-origin: border; "
        "subcontrol-position: left; left: -17px; }"
        "QScrollBar::add-line:vertical { subcontrol-origin: border; "
        "subcontrol-position: bottom; bottom: -17px; }"
        "QScrollBar::sub-line:vertical { subcontrol-origin: border; "
        "subcontrol-position: top; top: -17px; }"
        "QScrollBar::add-line:hover, QScrollBar::sub-line:hover { "
        "background: #e7f2fc; }"
        "QScrollBar::add-line:pressed, QScrollBar::sub-line:pressed { "
        "background: #d7eafc; }"
        "QScrollBar::left-arrow { image: url(:/images/scroll-left.svg); }"
        "QScrollBar::right-arrow { image: url(:/images/scroll-right.svg); }"
        "QScrollBar::up-arrow { image: url(:/images/scroll-up.svg); }"
        "QScrollBar::down-arrow { image: url(:/images/scroll-down.svg); }"
        "QScrollBar::left-arrow:hover { image: url(:/images/scroll-left-hover.svg); }"
        "QScrollBar::right-arrow:hover { image: url(:/images/scroll-right-hover.svg); }"
        "QScrollBar::up-arrow:hover { image: url(:/images/scroll-up-hover.svg); }"
        "QScrollBar::down-arrow:hover { image: url(:/images/scroll-down-hover.svg); }"
        "QScrollBar::left-arrow:pressed { image: url(:/images/scroll-left-pressed.svg); }"
        "QScrollBar::right-arrow:pressed { image: url(:/images/scroll-right-pressed.svg); }"
        "QScrollBar::up-arrow:pressed { image: url(:/images/scroll-up-pressed.svg); }"
        "QScrollBar::down-arrow:pressed { image: url(:/images/scroll-down-pressed.svg); }")
        .arg(QString::fromLatin1(ControlAccentColor))
        .arg(QString::fromLatin1(ControlAccentHoverColor));
}

inline QString sliderStyleSheet()
{
    // Keep the handle's repaint bounds fixed while the image changes size.
    return QStringLiteral(
        "QSlider:horizontal { min-height: 20px; }"
        "QSlider:vertical { min-width: 20px; }"
        "QSlider::groove:horizontal { height: 4px; margin: 0 6px; "
        "border: 0; border-radius: 2px; background: #D3D3D3; }"
        "QSlider::handle:horizontal { width: 16px; height: 16px; "
        "margin: -6px 0; }"
        "QSlider::groove:vertical { width: 4px; margin: 6px 0; "
        "border: 0; border-radius: 2px; background: #D3D3D3; }"
        "QSlider::handle:vertical { width: 16px; height: 16px; "
        "margin: 0 -6px; }"
        "QSlider::handle { border: 0; background: transparent; "
        "image: url(:/images/slider-handle.svg); }"
        "QSlider::handle:hover { image: url(:/images/slider-handle-hover.svg); }"
        "QSlider::handle:pressed { image: url(:/images/slider-handle-pressed.svg); }");
}

// 与 QGroupBox 样式一致的圆角边框容器，但没有标题。
class CustomFrame : public QFrame
{
    Q_OBJECT

public:
    explicit CustomFrame(QWidget *parent = nullptr);
};

class CustomCheckBox : public QCheckBox
{
    Q_OBJECT

public:
    explicit CustomCheckBox(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    // 供表格委托复用自定义外观，在目标绘图位置绘制控件。
    void paintOn(QPainter *painter, const QPoint &position) const;

protected:
    void paintEvent(QPaintEvent *event) override;
};

class CustomRadioButton : public QRadioButton
{
    Q_OBJECT

public:
    explicit CustomRadioButton(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

class AxisScrollBar : public QScrollBar
{
public:
    explicit AxisScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QScrollBar(orientation, parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QScrollBar::paintEvent(event);
        if (minimum() == maximum()) {
            QStyleOptionSlider option;
            initStyleOption(&option);
            option.rect = style()->subControlRect(
                QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarGroove, this);
            QStylePainter painter(this);
            painter.drawControl(QStyle::CE_ScrollBarAddPage, option);
        }
    }
};

#endif // CUSTOMCONTROLS_H
