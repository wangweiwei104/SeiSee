#ifndef CUSTOMCONTROLS_H
#define CUSTOMCONTROLS_H

#include <QCheckBox>
#include <QRadioButton>

class QPainter;

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

#endif // CUSTOMCONTROLS_H
