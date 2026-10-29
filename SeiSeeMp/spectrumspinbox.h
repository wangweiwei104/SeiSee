#ifndef SPECTRUMSPINBOX_H
#define SPECTRUMSPINBOX_H

#include <QDoubleSpinBox>
#include <QLineEdit>
#include <cmath>

// Display precision must not round the stored Nyquist/sample boundaries.
class SpectrumSpinBox : public QDoubleSpinBox
{
public:
    explicit SpectrumSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
    {
    }
    void setDisplayDecimals(int digits)
    {
        displayDecimals = digits;
        lineEdit()->setText(prefix() + textFromValue(value()) + suffix());
    }

protected:
    QString textFromValue(double number) const override
    {
        if (displayDecimals < 0)
            return QDoubleSpinBox::textFromValue(number);
        if (std::abs(number) < .5 * std::pow(10., -displayDecimals))
            number = 0;
        auto format = locale();
        if (!isGroupSeparatorShown())
            format.setNumberOptions(format.numberOptions() |
                                    QLocale::OmitGroupSeparator);
        return format.toString(number, 'f', displayDecimals);
    }

private:
    int displayDecimals = -1;
};

#endif
