#include "customcontrols.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionFocusRect>

namespace {

constexpr int IndicatorSize = 16;
constexpr int IndicatorSpacing = 6;

QRect indicatorRect(const QWidget *widget)
{
    const int y = (widget->height() - IndicatorSize) / 2;
    const int x = widget->layoutDirection() == Qt::RightToLeft
                      ? widget->width() - IndicatorSize
                      : 0;
    return QRect(x, y, IndicatorSize, IndicatorSize);
}

QRect labelRect(const QWidget *widget, const QRect &indicator)
{
    QRect label = widget->rect();
    if (widget->layoutDirection() == Qt::RightToLeft)
        label.setRight(indicator.left() - IndicatorSpacing);
    else
        label.setLeft(indicator.right() + 1 + IndicatorSpacing);
    return label;
}

QColor indicatorColor(const QStyleOptionButton &option)
{
    if (!(option.state & QStyle::State_Enabled))
        return option.palette.color(QPalette::Disabled, QPalette::Text);
    if (option.state & QStyle::State_MouseOver)
        return option.palette.color(QPalette::Active, QPalette::Highlight);
    return option.palette.color(QPalette::Active, QPalette::Mid);
}

QSize controlSizeHint(const QWidget *widget, const QString &text,
                      const QIcon &icon, const QSize &iconSize)
{
    const QFontMetrics metrics(widget->font());
    const int spacing = text.isEmpty() ? 0 : IndicatorSpacing;
    const int iconWidth = icon.isNull() ? 0 : iconSize.width() + spacing;
    return QSize(IndicatorSize + spacing + metrics.horizontalAdvance(text) +
                     iconWidth,
                 qMax(IndicatorSize, qMax(metrics.height(), iconSize.height())));
}

void drawLabel(const QStyleOptionButton &option, const QRect &indicator,
               QPainter *painter, const QWidget *widget, bool radio)
{
    QStyleOptionButton labelOption(option);
    labelOption.rect = labelRect(widget, indicator);
    widget->style()->drawControl(radio ? QStyle::CE_RadioButtonLabel
                                       : QStyle::CE_CheckBoxLabel,
                                 &labelOption, painter, widget);

    if (option.state & QStyle::State_HasFocus) {
        QStyleOptionFocusRect focusOption;
        focusOption.QStyleOption::operator=(option);
        focusOption.rect = labelOption.rect.adjusted(-2, 0, 2, 0);
        focusOption.backgroundColor = option.palette.color(QPalette::Window);
        widget->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOption,
                                       painter, widget);
    }
}

} // namespace

CustomCheckBox::CustomCheckBox(QWidget *parent)
    : QCheckBox(parent)
{
}

QSize CustomCheckBox::sizeHint() const
{
    return controlSizeHint(this, text(), icon(), iconSize());
}

QSize CustomCheckBox::minimumSizeHint() const
{
    return sizeHint();
}

void CustomCheckBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    // 普通控件从自身原点绘制；表格委托可传入单元格中的目标位置。
    paintOn(&painter, QPoint());
}

void CustomCheckBox::paintOn(QPainter *painter,
                             const QPoint &position) const
{
    QStyleOptionButton option;
    initStyleOption(&option);
    const QRect indicator = indicatorRect(this);
    const QRectF box = QRectF(indicator).adjusted(0.75, 0.75, -0.75, -0.75);
    const bool checked = option.state & QStyle::State_On;
    const QColor border = indicatorColor(option);

    painter->save();
    // 临时平移画布绘制，完成后恢复调用方的画布状态。
    painter->translate(position);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(border, 1.5));
    painter->setBrush(checked ? option.palette.color(QPalette::Active,
                                                     QPalette::Highlight)
                              : option.palette.color(QPalette::Base));
    painter->drawRoundedRect(box, 2.5, 2.5);

    if (checked) {
        QPainterPath check;
        check.moveTo(indicator.left() + indicator.width() * 0.22,
                     indicator.top() + indicator.height() * 0.52);
        check.lineTo(indicator.left() + indicator.width() * 0.43,
                     indicator.top() + indicator.height() * 0.72);
        check.lineTo(indicator.left() + indicator.width() * 0.80,
                     indicator.top() + indicator.height() * 0.29);
        painter->setPen(QPen(option.palette.color(QPalette::Active,
                                                  QPalette::HighlightedText),
                             2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(check);
    }

    drawLabel(option, indicator, painter, this, false);
    painter->restore();
}

CustomRadioButton::CustomRadioButton(QWidget *parent)
    : QRadioButton(parent)
{
}

QSize CustomRadioButton::sizeHint() const
{
    return controlSizeHint(this, text(), icon(), iconSize());
}

QSize CustomRadioButton::minimumSizeHint() const
{
    return sizeHint();
}

void CustomRadioButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStyleOptionButton option;
    initStyleOption(&option);
    const QRect indicator = indicatorRect(this);
    const QRectF outer = QRectF(indicator).adjusted(1.0, 1.0, -1.0, -1.0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(indicatorColor(option), 1.5));
    painter.setBrush(option.palette.color(QPalette::Base));
    painter.drawEllipse(outer);

    if (option.state & QStyle::State_On) {
        const qreal dotSize = outer.width() * 0.5;
        painter.setPen(Qt::NoPen);
        painter.setBrush(option.palette.color(QPalette::Active,
                                              QPalette::Highlight));
        painter.drawEllipse(QRectF(outer.center().x() - dotSize / 2,
                                   outer.center().y() - dotSize / 2, dotSize,
                                   dotSize));
    }

    drawLabel(option, indicator, &painter, this, true);
}
