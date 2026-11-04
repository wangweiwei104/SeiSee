#include "aboutdialog.h"
#include "gfxstyle.h"
#include "mainwindow.h"
#include "ui_aboutdialog.h"
#include "util2.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    const QSize aboutIconSize(GfxStyle::AboutIconSize,
                              GfxStyle::AboutIconSize);
    ui->label_icon->setFixedSize(aboutIconSize);

    QFont titleFont = QApplication::font();
    titleFont.setPointSizeF(GfxStyle::AboutTitleFontPointSize);
    titleFont.setBold(true);
    ui->label_title->setFont(titleFont);
    ui->label_title->setStyleSheet("color: #294f91;");  //图标的主题色

    QFont subtitleFont = QApplication::font();
    subtitleFont.setPointSizeF(GfxStyle::AboutProductFontPointSize);
    ui->label_sub_title->setFont(subtitleFont);

    QFont detailsFont = QApplication::font();
    detailsFont.setPointSizeF(GfxStyle::AboutDetailsFontPointSize);
    ui->label_rev->setFont(detailsFont);
    ui->label_author->setFont(detailsFont);

    QString txt =
        Tprintf("<html><head/><body><p align=\"center\">Rev: %s (build: %s) "
                "</p></body></html>",
                VERSION, __DATE__);

    ui->label_rev->setText(txt);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
