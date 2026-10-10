#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QBoxLayout>
#include <QActionGroup>
#include <QComboBox>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPainterPath>
#include <QPen>
#include <QProxyStyle>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStyleOptionMenuItem>
#include <QTabBar>
#include <QTextCodec>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

#include "Toast.h"

#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gfxobj.h"
#include "gfxstyle.h"
#include "gfxsrcseismemsegd.h"
#include "sgyfile.h"

#include "seistrexpr.h"

#include "furlib.h"
#include "util2.h"
#include "util2qt.h"

namespace {

void showScreenshotToast(QWidget *parent, const QString &title,
                         const QString &text, ToastPreset preset)
{
    Toast *toast = new Toast(parent);
    toast->setDuration(5000);
    toast->setTitle(title);
    toast->setText(text);
    toast->setBorderRadius(10);
    toast->setMaximumWidth(qMax(250, parent->width() / 2));
    toast->applyPreset(preset);
    toast->show();
}

class ExportDpiMenuStyle final : public QProxyStyle
{
public:
    explicit ExportDpiMenuStyle(QObject *parent)
        : QProxyStyle()
    {
        setParent(parent);
    }

    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter,
                     const QWidget *widget = nullptr) const override
    {
        const auto *menuOption =
            element == CE_MenuItem
                ? qstyleoption_cast<const QStyleOptionMenuItem *>(option)
                : nullptr;
        if (!menuOption ||
            menuOption->checkType != QStyleOptionMenuItem::Exclusive ||
            !menuOption->checked) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }

        QStyleOptionMenuItem itemWithoutCheck(*menuOption);
        itemWithoutCheck.checked = false;
        QProxyStyle::drawControl(element, &itemWithoutCheck, painter, widget);

        const int horizontalMargin =
            pixelMetric(PM_MenuHMargin, menuOption, widget);
        const int indicatorColumn =
            qMax(menuOption->maxIconWidth, menuOption->rect.height()) +
            2 * horizontalMargin;
        const QRect markerRect(menuOption->rect.left(), menuOption->rect.top(),
                               indicatorColumn, menuOption->rect.height());
        const qreal markSize = qMin(
            markerRect.height() * 0.52,
            qMax(12.0, menuOption->fontMetrics.height() * 0.7));
        const QPointF center = markerRect.center();
        QPainterPath checkMark;
        checkMark.moveTo(center.x() - markSize * 0.34,
                         center.y() + markSize * 0.01);
        checkMark.lineTo(center.x() - markSize * 0.1,
                         center.y() + markSize * 0.25);
        checkMark.lineTo(center.x() + markSize * 0.36,
                         center.y() - markSize * 0.27);

        const QColor color =
            menuOption->state & State_Selected
                ? menuOption->palette.color(QPalette::HighlightedText)
                : menuOption->palette.color(QPalette::Text);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(color, qMax(2.0, markSize * 0.13),
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(checkMark);
        painter->restore();
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter,
                       const QWidget *widget = nullptr) const override
    {
        if ((element != PE_IndicatorArrowRight &&
             element != PE_IndicatorArrowLeft) ||
            !qobject_cast<const QMenu *>(widget)) {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }

        const qreal arrowHeight = qMin(
            qMax<qreal>(10, option->rect.height() - 8),
            qMax<qreal>(10, option->fontMetrics.height() * 0.55));
        const qreal arrowWidth = arrowHeight * 0.55;
        const bool pointsRight = element == PE_IndicatorArrowRight;
        const qreal tipX = pointsRight ? option->rect.right()
                                       : option->rect.left();
        const qreal baseX = pointsRight ? tipX - arrowWidth
                                        : tipX + arrowWidth;
        const QRectF bounds(qMin(tipX, baseX),
                            option->rect.center().y() - arrowHeight / 2,
                            arrowWidth, arrowHeight);
        QPainterPath arrow;
        if (pointsRight) {
            arrow.moveTo(bounds.left(), bounds.top());
            arrow.lineTo(bounds.right(), bounds.center().y());
            arrow.lineTo(bounds.left(), bounds.bottom());
        } else {
            arrow.moveTo(bounds.right(), bounds.top());
            arrow.lineTo(bounds.left(), bounds.center().y());
            arrow.lineTo(bounds.right(), bounds.bottom());
        }
        arrow.closeSubpath();

        const QColor color =
            option->state & State_Enabled
                ? option->palette.color(
                      (option->state & State_Selected)
                          ? QPalette::HighlightedText
                          : QPalette::Text)
                : option->palette.color(QPalette::Disabled, QPalette::Text);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawPath(arrow);
        painter->restore();
    }
};

} // namespace

class VerticalTimeLabel : public QWidget
{
public:
    explicit VerticalTimeLabel(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        m_font.setFamily(GfxStyle::LabelFontFamily);
        m_font.setStyleHint(GfxStyle::LabelFontStyleHint);
        m_font.setPointSizeF(GfxStyle::LabelFontPointSize);
        refreshSize();
    }

    // 设置完整字体（自动触发重绘）
    void setFont(const QFont &font)
    {
        m_font = font;
        refreshSize();
        update(); // 刷新界面
    }

    // 设置字体点大小（自动触发重绘）
    void SetFontSize(int size)
    {
        m_font.setPointSizeF(size);
        refreshSize();
        update();
    }

    void refreshSize()
    {
        QFontMetrics metrics(m_font, this);
        const int padding = GfxStyle::TimeLabelPadding;
        setFixedSize(metrics.height() + padding,
                     metrics.horizontalAdvance(
                         QString::fromLatin1(GfxStyle::TimeLabelText)) +
                         padding);
    }

    int rotatedTextWidth() const
    {
        return QFontMetrics(m_font, this).height();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing);

        // 使用成员字体 m_font
        painter.setFont(m_font);

        QFontMetrics metrics(m_font, this);
        painter.translate(
            metrics.ascent(),
            (height() + metrics.horizontalAdvance(
                            QString::fromLatin1(GfxStyle::TimeLabelText))) /
                2.0);
        painter.rotate(-90);
        painter.drawText(QPointF(0, 0),
                         QString::fromLatin1(GfxStyle::TimeLabelText));
    }

private:
    QFont m_font; // 存储当前使用的字体
};

#ifdef _MSC_VER
// #include <vld.h>
#pragma warning(push)
#pragma warning(disable : 4267 4100 4068)
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

#if QT_VERSION < 0x050000
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf-8"));
#endif

    qRegisterMetaType<DirList>("DirList");

    busy = false;
    expu_running = false;

    QString dir = QDir::tempPath();
    undofn = dir + "/" + QUuid::createUuid().toString() + ".tmp";

    QStringList paths = QCoreApplication::libraryPaths();
    paths.append(".");
    paths.append("imageformats");
    paths.append("platforms");
    paths.append("sqldrivers");
    QCoreApplication::setLibraryPaths(paths);

    ui->setupUi(this);
    ui->exportHiResBtn->setToolTip(tr("Export high-resolution image"));

    ui->menuView->setStyle(new ExportDpiMenuStyle(this));
    m_exportDpiMenu = ui->menuView->addMenu(tr("Export image DPI"));
    m_exportDpiMenu->setStyle(new ExportDpiMenuStyle(this));
    QActionGroup *exportDpiGroup = new QActionGroup(this);
    exportDpiGroup->setExclusive(true);
    for (int dpi : {300, 400, 600}) {
        QAction *dpiAction =
            m_exportDpiMenu->addAction(tr("%1 DPI").arg(dpi));
        dpiAction->setCheckable(true);
        dpiAction->setData(dpi);
        dpiAction->setChecked(dpi == m_exportDpi);
        exportDpiGroup->addAction(dpiAction);
    }
    connect(exportDpiGroup, &QActionGroup::triggered, this,
            [this](QAction *action) { m_exportDpi = action->data().toInt(); });
    ui->dirGroup->setMinimumWidth(130);
    ui->dirGroup->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    ui->HdrBox->setMinimumWidth(
        ui->InfoTab->tabBar()->sizeHint().width() +
        ui->HdrBox->layout()->contentsMargins().left() +
        ui->HdrBox->layout()->contentsMargins().right());
    ui->HdrBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ui->SeisTab->setMinimumWidth(0);
    ui->SeisTab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    ui->frame_2->setMinimumWidth(0);
    ui->HeBox->setMinimumWidth(0);
    ui->groupBox_6->setMinimumWidth(0);

    QGridLayout *scaleLayout =
        qobject_cast<QGridLayout *>(ui->groupBox_3->layout());
    const QList<QSlider *> scaleSliders = {
        ui->TrSlider, ui->TmSlider, ui->GnSlider
    };
    for (QSlider *slider : scaleSliders) {
        slider->setMinimumWidth(0);
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    if (scaleLayout)
        scaleLayout->setColumnStretch(1, 1);

    const int numericEditWidth =
        QFontMetrics(ui->edTr->font()).horizontalAdvance(
            QStringLiteral("-123.4567")) +
        16;
    const QList<QLineEdit *> scaleEdits = {
        ui->edTr, ui->edTm, ui->edGn
    };
    for (QLineEdit *edit : scaleEdits)
        edit->setMinimumWidth(numericEditWidth);
    ui->groupBox_3->setMinimumWidth(
        ui->groupBox_3->minimumSizeHint().width());

    for (QGroupBox *optionalGroup : {ui->groupBox_2, ui->groupBox_13}) {
        optionalGroup->setMinimumWidth(0);
    }
    connect(ui->splitter, &QSplitter::splitterMoved, this,
            [this](int, int) {
                QTimer::singleShot(
                    0, this, [this]() { updateCompactSeismicControls(); });
            });

    ui->autoGainBtn->setIcon(QIcon(":/images/Autogain.png"));
    ui->autoGainBtn->setText(QString());

    QBoxLayout *zoomLayout =
        qobject_cast<QBoxLayout *>(ui->zoomAllBtn->parentWidget()->layout());
    if (zoomLayout) {
        const QList<QPushButton *> zoomButtons = {
            ui->zoomAllBtn, ui->zoomInBtn, ui->zoomOutBtn,
            ui->zoomWinBtn, ui->zoomPreBtn
        };
        for (QPushButton *button : zoomButtons)
            zoomLayout->removeWidget(button);
        for (int index = 0; index < zoomButtons.size(); ++index)
            zoomLayout->insertWidget(index, zoomButtons[index]);
    }

    setWindowIcon(QApplication::windowIcon());
    connect(ui->SeisTab, &QTabWidget::currentChanged, this,
            [this](int index) {
                if (index != ui->SeisTab->indexOf(ui->HdrsPg))
                    return;

                QTimer::singleShot(0, this, [this]() {
                    if (!ui || !ui->seisFrame->isVisible())
                        return;

                    seisView.refreshScreenDpi();
                    hdrsView.refreshScreenDpi();
                    hdrsBottomView.refreshScreenDpi();
                    timeView.refreshScreenDpi();
                    timeRightView.refreshScreenDpi();
                    hlabView.refreshScreenDpi();
                    seisSct.Preset();
                    if (ui->HdrsPg->layout())
                        ui->HdrsPg->layout()->activate();
                    if (ui->seisFrame->layout())
                        ui->seisFrame->layout()->activate();
                    updateCompactSeismicControls();
                    ArrangeSections();
                    // seisView.update();
                    // hdrsView.update();
                    // hdrsBottomView.update();
                    // timeView.update();
                    // timeRightView.update();
                    // hlabView.update();
                });
            });

    resize(GfxStyle::MainWindowWidth, GfxStyle::MainWindowHeight);
    QTimer::singleShot(0, this,
                       [this]() { updateCompactSeismicControls(); });
    QFont directoryGridFont = QApplication::font();
    directoryGridFont.setPointSizeF(GfxStyle::DirectoryGridFontPointSize);
    ui->dirFrame->setFont(directoryGridFont);

    eExpIdx = 0;

    m_statMess = new QLabel("SeiSeeMp", this);
    m_statMess->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statTrcn = new QLabel("Trace ", this);
    m_statTrcn->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statTime = new QLabel("Time=", this);
    m_statTime->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    //  m_statAmp  = new QLabel("Amp=", this);
    m_statAmp = new QLabel("", this);
    m_statAmp->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    //  m_statFfid  = new QLabel("FFID=", this);
    m_statFfid = new QLabel("", this);
    m_statFfid->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    //  m_statSp = new QLabel("SP=", this);
    m_statSp = new QLabel("", this);
    m_statSp->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    m_statusRight = new QLabel(VERSION, this);
    m_statusRight->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    statusBar()->addPermanentWidget(m_statMess, 1);
    statusBar()->addPermanentWidget(m_statTrcn, 1);
    statusBar()->addPermanentWidget(m_statTime, 1);
    statusBar()->addPermanentWidget(m_statAmp, 1);
    statusBar()->addPermanentWidget(m_statFfid, 1);
    statusBar()->addPermanentWidget(m_statSp, 1);
    statusBar()->addPermanentWidget(m_statusRight, 2);

    progressBar = new QProgressBar;
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setTextVisible(true);
    progressBar->setFormat("xxxx%p%");
    progressBar->setTextVisible(false);
    this->statusBar()->addPermanentWidget(progressBar, 1);

    progressBar->setValue(0);
    progressBar->setMinimum(0);
    progressBar->setMaximum(100);

    chsSel.resize(1000);
    chsSel.fill(false);

    _f1 = 10;
    _f2 = 20;
    _f3 = 500;
    _f4 = 1000;

    _agcw = 1000;
    _agcType = 0;

    _dly_min = 0;
    _dly_max = 0;

    //----------------------------------------------------------
    dirGrid.setRowCount(0);
    dirGrid.setColCount(2);
    dirGrid.verticalHeader()->setVisible(false);

    dirGrid.setColLabel(0, "Name");
    dirGrid.setColLabel(1, "Type");
    dirGrid.setColWidthInCharacters(
        1, GfxStyle::DirectoryTypeCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    dirGrid.setColWidthToStretch(
        0, GfxStyle::TableColumnHorizontalPadding);
    // 增加鼠标滑过文件名的tips
    dirGrid.setMouseTracking(true);
    dirGrid.viewport()->setMouseTracking(true);
    dirGrid.setContextMenuPolicy(Qt::CustomContextMenu);
    // 增加了文件目录右键三个菜单项
    connect(&dirGrid, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &position) {
                const QModelIndex index = dirGrid.indexAt(position);
                if (!index.isValid())
                    return;

                const int row = index.row();
                if (dirGrid.Tag(row, 0) != 1)
                    return;

                dirGrid.setCurrentIndex(index);

                const QString fileName = dirGrid.Cell(row, 0);
                const QString fullPath =
                    QFileInfo(QDir(curDir).filePath(fileName)).absoluteFilePath();

                QMenu menu(&dirGrid);
                QAction *copyNameAction = menu.addAction(tr("Copy file name"));
                QAction *copyPathAction = menu.addAction(tr("Copy full path"));
                QAction *openFolderAction =
                    menu.addAction(tr("Open containing folder"));
                connect(copyNameAction, &QAction::triggered, this,
                        [fileName]() {
                            QApplication::clipboard()->setText(fileName);
                        });
                connect(copyPathAction, &QAction::triggered, this,
                        [fullPath]() {
                            QApplication::clipboard()->setText(fullPath);
                        });
                const QString containingFolder =
                    QFileInfo(fullPath).absolutePath();
                connect(openFolderAction, &QAction::triggered, this,
                        [this, containingFolder]() {
                            if (!QDesktopServices::openUrl(
                                    QUrl::fromLocalFile(containingFolder))) {
                                QMessageBox::warning(
                                    this, tr("Unable to open folder"),
                                    tr("Could not open the folder containing "
                                       "this file."));
                            }
                        });

                menu.exec(dirGrid.viewport()->mapToGlobal(position));
            });

    QVBoxLayout *dirGridLayout = new QVBoxLayout;
    dirGridLayout->setMargin(GfxStyle::CompactLayoutMargin);
    dirGridLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    dirGridLayout->addWidget(&dirGrid);
    ui->dirFrame->setLayout(dirGridLayout);

    //----------------------------------------------------------
    binHdrGrid.setRowCount(0);
    binHdrGrid.setColCount(3);
    binHdrGrid.verticalHeader()->setVisible(false);
    binHdrGrid.setColLabel(0, "Value");
    binHdrGrid.setColLabel(1, "Description");
    binHdrGrid.setColLabel(2, "Bytes");
    binHdrGrid.setColWidthInCharacters(
        0, GfxStyle::BinaryHeaderValueCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    binHdrGrid.setColWidthToContents(
        1, GfxStyle::TableColumnHorizontalPadding);
    binHdrGrid.setColWidthInCharacters(
        2, GfxStyle::HeaderBytesCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);

    QVBoxLayout *binHdrLayout = new QVBoxLayout;
    binHdrLayout->setMargin(GfxStyle::CompactLayoutMargin);
    binHdrLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    binHdrLayout->addWidget(&binHdrGrid);
    ui->binHdrFrame->setLayout(binHdrLayout);

    //----------------------------------------------------------
    trcHdrGrid.setRowCount(0);
    trcHdrGrid.setColCount(4);
    trcHdrGrid.verticalHeader()->setVisible(false);
    trcHdrGrid.setColLabel(0, "Value");
    trcHdrGrid.setColLabel(1, "Bytes");
    trcHdrGrid.setColLabel(2, "Name");
    trcHdrGrid.setColLabel(3, "Description");
    trcHdrGrid.setColWidthInCharacters(
        0, GfxStyle::TraceHeaderValueCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    trcHdrGrid.setColWidthInCharacters(
        1, GfxStyle::HeaderBytesCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    trcHdrGrid.setColWidthInCharacters(
        2, GfxStyle::HeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    trcHdrGrid.setColWidthToStretch(
        3, GfxStyle::TableColumnHorizontalPadding);

    QVBoxLayout *trcHdrLayout = new QVBoxLayout;
    trcHdrLayout->setMargin(GfxStyle::CompactLayoutMargin);
    trcHdrLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    trcHdrLayout->addWidget(&trcHdrGrid);
    ui->trcHdrFrame->setLayout(trcHdrLayout);

    //----------------------------------------------------------
    trcDatGrid.setRowCount(0);
    trcDatGrid.setColCount(3);
    trcDatGrid.verticalHeader()->setVisible(false);
    trcDatGrid.setColLabel(0, "Index");
    trcDatGrid.setColLabel(1, "Time");
    trcDatGrid.setColLabel(2, "Sample");
    trcDatGrid.setColWidthInCharacters(
        0, GfxStyle::TraceDataIndexCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    trcDatGrid.setColWidthInCharacters(
        1, GfxStyle::TraceDataTimeCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    trcDatGrid.setColWidthToStretch(
        2, GfxStyle::TableColumnHorizontalPadding);

    QVBoxLayout *trcDatLayout = new QVBoxLayout;
    trcDatLayout->setMargin(GfxStyle::CompactLayoutMargin);
    trcDatLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    trcDatLayout->addWidget(&trcDatGrid);
    ui->trcDatFrame->setLayout(trcDatLayout);

    //----------------------------------------------------------
    hdrListCkGrid.setRowCount(0);
    hdrListCkGrid.setColCount(4);
    hdrListCkGrid.verticalHeader()->setVisible(false);
    hdrListCkGrid.setColLabel(0, "");
    hdrListCkGrid.setColLabel(1, "Bytes");
    hdrListCkGrid.setColLabel(2, "Name");
    hdrListCkGrid.setColLabel(3, "Description");
    hdrListCkGrid.setColWidthToCheckIndicator(
        0, GfxStyle::TableColumnHorizontalPadding +
               GfxStyle::CheckIndicatorColumnExtraPadding);
    hdrListCkGrid.setColWidthInCharacters(
        1, GfxStyle::HeaderBytesCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    hdrListCkGrid.setColWidthInCharacters(
        2, GfxStyle::HeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    hdrListCkGrid.setColWidthToContents(
        3, GfxStyle::TableColumnHorizontalPadding);
    hdrListCkGrid.setColChkbx(0, true);

    QVBoxLayout *hdrListCkLayout = new QVBoxLayout;
    hdrListCkLayout->setMargin(GfxStyle::CompactLayoutMargin);
    hdrListCkLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    hdrListCkLayout->addWidget(&hdrListCkGrid);
    ui->hdrListCkFrame->setLayout(hdrListCkLayout);

    hdrElstCkGrid.setRowCount(0);
    hdrElstCkGrid.setColCount(4);
    hdrElstCkGrid.verticalHeader()->setVisible(false);
    hdrElstCkGrid.setColLabel(0, "");
    hdrElstCkGrid.setColLabel(1, "Bytes");
    hdrElstCkGrid.setColLabel(2, "Name");
    hdrElstCkGrid.setColLabel(3, "Description");
    hdrElstCkGrid.setColWidthToCheckIndicator(
        0, GfxStyle::TableColumnHorizontalPadding +
               GfxStyle::CheckIndicatorColumnExtraPadding);
    hdrElstCkGrid.setColWidthInCharacters(
        1, GfxStyle::HeaderBytesCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    hdrElstCkGrid.setColWidthInCharacters(
        2, GfxStyle::HeaderNameCharacterCount,
        GfxStyle::TableColumnHorizontalPadding);
    hdrElstCkGrid.setColWidthToContents(
        3, GfxStyle::TableColumnHorizontalPadding);
    hdrElstCkGrid.setColChkbx(0, true);

    QVBoxLayout *hdrElstCkLayout = new QVBoxLayout;
    hdrElstCkLayout->setMargin(GfxStyle::CompactLayoutMargin);
    hdrElstCkLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    hdrElstCkLayout->addWidget(&hdrElstCkGrid);
    ui->hdrElstCkFrame->setLayout(hdrElstCkLayout);

    //----------------------------------------------------------

    hdrListDtGrid.setRowCount(0);
    hdrListDtGrid.setColCount(1);
    hdrListDtGrid.setColLabel(0, "Trace");
    hdrListDtGrid.setColWidthInCharacters(
        0, GfxStyle::TraceHeaderValueCharacterCount);

    QVBoxLayout *hdrListDtLayout = new QVBoxLayout;
    hdrListDtLayout->setMargin(GfxStyle::CompactLayoutMargin);
    hdrListDtLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    hdrListDtLayout->addWidget(&hdrListDtGrid);
    ui->hdrListDtFrame->setLayout(hdrListDtLayout);

    hdrElstDtGrid.setRowCount(0);
    hdrElstDtGrid.setColCount(1);
    hdrElstDtGrid.setColLabel(0, "Trace");
    hdrElstDtGrid.setColWidthInCharacters(
        0, GfxStyle::TraceHeaderValueCharacterCount);

    QVBoxLayout *hdrElstDtLayout = new QVBoxLayout;
    hdrElstDtLayout->setMargin(GfxStyle::CompactLayoutMargin);
    hdrElstDtLayout->setSpacing(GfxStyle::CompactLayoutSpacing);
    hdrElstDtLayout->addWidget(&hdrElstDtGrid);
    ui->hdrElstDtFrame->setLayout(hdrElstDtLayout);

    //----------------------------------------------------------

    seisSct.setWFcolor(Qt::black);
    seisSct.setWLcolor(Qt::black);

    seisView.setXs(1);
    seisView.setYs(1);

    timeAxis.setView(&timeView);
    timeRightAxis.setView(&timeRightView);
    hdrsAxis.setView(&hdrsView);
    hdrsBottomAxis.setView(&hdrsBottomView);
    seisSct.setView(&seisView);
    hdrsLab.setView(&hlabView);

    timeRightAxis.setRightSide(true);
    hdrsBottomAxis.setBottomSide(true);
    timeAxis.setTrim(false);
    timeRightAxis.setTrim(false);
    hdrsAxis.setTrim(false);
    hdrsBottomAxis.setTrim(false);
    hdrsLab.setTrim(false);

    seisSct.setSrc(&seisSrc);
    hdrsAxis.setSrc(&seisSrc);
    hdrsBottomAxis.setSrc(&seisSrc);

    timeAxis.setYo(0);
    timeAxis.setXo(0);
    timeAxis.setX1(0);
    timeAxis.setX2(13);
    timeAxis.setY1(0);
    timeAxis.setY2(6);
    timeAxis.setYs(10);
    timeAxis.setTi(0.1);
    timeRightAxis.setYo(0);
    timeRightAxis.setXo(0);
    timeRightAxis.setX1(0);
    timeRightAxis.setX2(13);
    timeRightAxis.setY1(0);
    timeRightAxis.setY2(6);
    timeRightAxis.setYs(10);
    timeRightAxis.setTi(0.1);

    seisSct.setTi(0.1);

    hdrsScrl = new MyScrollArea(&hdrsView, 2);
    hdrsBottomScrl = new MyScrollArea(&hdrsBottomView, 2);
    seisScrl = new MyScrollArea(&seisView, 3);
    timeScrl = new MyScrollArea(&timeView, 1);
    timeRightScrl = new MyScrollArea(&timeRightView, 1);
    horizontalAxisScrollBar = new QScrollBar(Qt::Horizontal, this);
    verticalAxisScrollBar = new QScrollBar(Qt::Vertical, this);

    // 给滚动条区域和角落区域设置背景色，用在其隐藏时保留占位区域，确保视觉上保持一致
    const QColor axisScrollBarTrackColor =
        horizontalAxisScrollBar->palette().color(QPalette::Window);

    auto applySystemPaletteBackground = [](QWidget *widget,
                                           const QColor &color) {
        widget->setAutoFillBackground(true);
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Window, color);
        widget->setPalette(palette);
    };

    QWidget *horizontalAxisScrollBarArea = new QWidget(ui->seisFrame);
    applySystemPaletteBackground(horizontalAxisScrollBarArea,
                                 axisScrollBarTrackColor);
    QHBoxLayout *horizontalAxisScrollBarAreaLayout =
        new QHBoxLayout(horizontalAxisScrollBarArea);
    horizontalAxisScrollBarAreaLayout->setContentsMargins(
        GfxStyle::ScrollBarAreaMargin, GfxStyle::ScrollBarAreaMargin,
        GfxStyle::ScrollBarAreaMargin, GfxStyle::ScrollBarAreaMargin);
    horizontalAxisScrollBarAreaLayout->setSpacing(GfxStyle::ZeroLayoutSpacing);
    horizontalAxisScrollBarAreaLayout->addWidget(horizontalAxisScrollBar);

    QWidget *verticalAxisScrollBarArea = new QWidget(ui->seisFrame);
    applySystemPaletteBackground(verticalAxisScrollBarArea,
                                 axisScrollBarTrackColor);
    QVBoxLayout *verticalAxisScrollBarAreaLayout =
        new QVBoxLayout(verticalAxisScrollBarArea);
    verticalAxisScrollBarAreaLayout->setContentsMargins(
        GfxStyle::ScrollBarAreaMargin, GfxStyle::ScrollBarAreaMargin,
        GfxStyle::ScrollBarAreaMargin, GfxStyle::ScrollBarAreaMargin);
    verticalAxisScrollBarAreaLayout->setSpacing(GfxStyle::ZeroLayoutSpacing);
    verticalAxisScrollBarAreaLayout->addWidget(verticalAxisScrollBar);

    QWidget *axisScrollBarCorner = new QWidget(ui->seisFrame);
    applySystemPaletteBackground(axisScrollBarCorner, axisScrollBarTrackColor);

    // 隐藏滚动条时保留其布局空间，由外层容器继续绘制灰色背景。
    // 目的是避免zoomALLBtn、zoomHallBtn、zoomVallBtn三个按钮执行时发生bug,在底部和右侧产生白条
    QSizePolicy horizontalScrollBarPolicy =
        horizontalAxisScrollBar->sizePolicy();
    horizontalScrollBarPolicy.setRetainSizeWhenHidden(true);
    horizontalAxisScrollBar->setSizePolicy(horizontalScrollBarPolicy);

    QSizePolicy verticalScrollBarPolicy = verticalAxisScrollBar->sizePolicy();
    verticalScrollBarPolicy.setRetainSizeWhenHidden(true);
    verticalAxisScrollBar->setSizePolicy(verticalScrollBarPolicy);

    connect(seisScrl->horizontalScrollBar(), SIGNAL(rangeChanged(int, int)),
            this, SLOT(syncHorizontalAxisScrollBar(int, int)));
    connect(seisScrl->verticalScrollBar(), SIGNAL(rangeChanged(int, int)), this,
            SLOT(syncVerticalAxisScrollBar(int, int)));
    connect(seisScrl->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            horizontalAxisScrollBar, SLOT(setValue(int)));
    connect(horizontalAxisScrollBar, SIGNAL(valueChanged(int)),
            seisScrl->horizontalScrollBar(), SLOT(setValue(int)));
    connect(seisScrl->verticalScrollBar(), SIGNAL(valueChanged(int)),
            verticalAxisScrollBar, SLOT(setValue(int)));
    connect(verticalAxisScrollBar, SIGNAL(valueChanged(int)),
            seisScrl->verticalScrollBar(), SLOT(setValue(int)));

    timeLabel = new VerticalTimeLabel(timeScrl->viewport());
    auto arrangeOnDpiChange = [this]() {
        QTimer::singleShot(0, this, [this]() {
            if (ui && ui->seisFrame->isVisible())
                ArrangeSections();
        });
    };
    connect(&timeView, &GfxView::dpiChanged, this, arrangeOnDpiChange);
    connect(&timeRightView, &GfxView::dpiChanged, this, arrangeOnDpiChange);
    connect(&seisView, &GfxView::dpiChanged, this, arrangeOnDpiChange);
    connect(&hdrsView, &GfxView::dpiChanged, this, arrangeOnDpiChange);
    connect(&hdrsBottomView, &GfxView::dpiChanged, this,
            arrangeOnDpiChange);
    connect(&hlabView, &GfxView::dpiChanged, this, arrangeOnDpiChange);
    timeLabel->show();

    // 创建 QFont 对象
    QFont monoFont(GfxStyle::TextEditorFontFamily);
    // 使用样式提示让系统自动选择等宽字体
    // monoFont.setStyleHint(QFont::Monospace);
    monoFont.setStyleHint(GfxStyle::TextEditorFontStyleHint);
    monoFont.setPointSizeF(GfxStyle::TextEditorFontPointSize);

    // 应用到 QPlainTextEdit
    ui->InfoTxt->setFont(monoFont);

    ui->TxtHdrEdit->setFont(monoFont);

    myEventCatcher *ef;

    ef = new myEventCatcher(1, this);
    ui->edGn->installEventFilter(ef);

    connect(ef, SIGNAL(whellEvent(int, QWheelEvent *)), this,
            SLOT(edWhellEvent(int, QWheelEvent *)));

    ef = new myEventCatcher(2, this);
    ui->edTm->installEventFilter(ef);

    connect(ef, SIGNAL(whellEvent(int, QWheelEvent *)), this,
            SLOT(edWhellEvent(int, QWheelEvent *)));

    ef = new myEventCatcher(3, this);
    ui->edTr->installEventFilter(ef);

    myTextEditEventCatcher *tf = new myTextEditEventCatcher(1, this);
    ui->TxtHdrEdit->installEventFilter(tf);

    connect(tf, SIGNAL(keyEvent(int, QKeyEvent *)), this,
            SLOT(txtEditKeyEvent(int, QKeyEvent *)));

    connect(ui->TxtHdrEdit, SIGNAL(textChanged()), this,
            SLOT(txtEditChanged()));

    connect(&binHdrGrid, SIGNAL(textChanged()), this,
            SLOT(binHdrGridChangedEvent()));

    connect(ef, SIGNAL(whellEvent(int, QWheelEvent *)), this,
            SLOT(edWhellEvent(int, QWheelEvent *)));

    connect(seisScrl, SIGNAL(mouseEvent(QMouseEvent *)), this,
            SLOT(viewMouseEvent(QMouseEvent *)));

    connect(seisScrl, SIGNAL(zoomW(QRect)), this, SLOT(winZoom(QRect)));

    connect(seisScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoom(double, double, int, int)));

    connect(timeScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoomV(double, double, int, int)));

    connect(timeRightScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoomV(double, double, int, int)));

    connect(hdrsScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoomH(double, double, int, int)));

    connect(hdrsBottomScrl, SIGNAL(zoomChanged(double, double, int, int)), this,
            SLOT(sclZoomH(double, double, int, int)));

    connect(&dirGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(dirGridEvent(int, int)));

    connect(&hdrListCkGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(hdrListCkEvent(int, int)));

    connect(&hdrElstCkGrid, SIGNAL(rowEvent(int, int)), this,
            SLOT(hdrElstCkEvent(int, int)));

    connect(&hdrListDtGrid, SIGNAL(rowEvent(qint64, int)), this,
            SLOT(hdrListDtGridEvent(qint64, int)));

    connect(&hdrListDtGrid, SIGNAL(cellDataRequest(int, int, QString &)), this,
            SLOT(hdrListDtGridDataEvent(int, int, QString &)));

    connect(&hdrElstDtGrid, SIGNAL(cellDataRequest(int, int, QString &)), this,
            SLOT(hdrElstDtGridDataEvent(int, int, QString &)));

    connect(&hdrElstDtGrid, SIGNAL(headerEvent(int)), this,
            SLOT(hdrElstDtGridHeaderEvent(int)));

    connect(&ProcParmDlg, SIGNAL(changedEvent()), this, SLOT(procParmEvent()));

    connect(&AxisDlg, SIGNAL(changedEvent()), this, SLOT(axesDlgEvent()));

    connect(&EdHdrDlg, SIGNAL(changedEvent()), this, SLOT(hdreDlgEvent()));

    connect(&hdrListDtGrid, SIGNAL(headerEvent(int)), this,
            SLOT(hdrListDtGridHeaderEvent(int)));

    /*
        connect
                (
                    &seisSct,
                    SIGNAL(epers(int)),
                    this,
                    SLOT(x_pers(int))
                    );
    */

    hdrsScrl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hdrsScrl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeScrl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeScrl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeRightScrl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeRightScrl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hdrsBottomScrl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hdrsBottomScrl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    seisScrl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    seisScrl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QGridLayout *seisLayout;

    seisLayout = new QGridLayout;

    seisLayout->setMargin(GfxStyle::ZeroLayoutMargin);
    seisLayout->setSpacing(GfxStyle::ZeroLayoutSpacing);

    seisLayout->addWidget(&hlabView, 0, 0);
    seisLayout->addWidget(timeScrl, 1, 0);
    seisLayout->addWidget(hdrsScrl, 0, 1);
    seisLayout->addWidget(seisScrl, 1, 1);
    seisLayout->addWidget(hdrsBottomScrl, 2, 1);
    seisLayout->addWidget(timeRightScrl, 1, 2);
    seisLayout->addWidget(horizontalAxisScrollBarArea, 3, 0, 1, 3);
    seisLayout->addWidget(verticalAxisScrollBarArea, 0, 3, 3, 1);
    seisLayout->addWidget(axisScrollBarCorner, 3, 3);
    seisLayout->setColumnStretch(1, 1);
    seisLayout->setRowStretch(1, 1);

    ui->seisFrame->setLayout(seisLayout);
    ui->seisFrame->setMouseTracking(true);

    GetRegInfo();

    setWindowTitle(QString("SeiSeeMp (") + QString(__DATE__) + QString(")"));

    // double   v[3] = {-1,0,1};
    QRgb c[3];

    c[0] = QColor(0, 0, 255).rgb();
    c[1] = QColor(255, 255, 255).rgb();
    c[2] = QColor(255, 0, 0).rgb();

    checkTxtHdrChange();
    checkBinHdrChange();

    SetSearchControls(true);

    ui->seisFrame->setVisible(false);

    //    exprList["CDP"] = SeisTrExpr("H(21,4)");
    //    exprList["SP" ] = SeisTrExpr("L-N+1");

    // CreatePalette24(c,v,-1,1,rgb,3); !!!
}

void MainWindow::hdrListDtGridDataEvent(int r, int c, QString &v)
{
    int itm = hdrListDtGrid.colTag(c);

    ////

    double val = seisSrc.Th(r, itm);

    int ival = val;
    if (ival == val)
        v.sprintf("%d", ival);
    else
        v.sprintf("%g", val);
}

void MainWindow::hdrElstDtGridDataEvent(int r, int c, QString &v)
{
    int itm = hdrElstDtGrid.colTag(c);
    SeisFile *sf = seisSrc.Sfile();

    double val;
    bool err = false;

    if (c == 0) {
        val = r + 1;
    } else if (c & 1) {
        val = seisSrc.Th(r, itm);
    } else {
        int n = (c - 1) >> 1;

        QString name = hdrElstDtGrid.colLabel(n * 2 + 1);

        val = 0;

        if (exprList.contains(name)) {
            SeisTrExpr *expr = &exprList[name];
            val = sf->Re(r, expr);
            if (expr->Error())
                err = true;
        } else {
            val = 0;
            err = true;
        }
    }

    if (err) {
        v = "";
    } else {
        int ival = val;
        if (ival == val)
            v.sprintf("%d", ival);
        else
            v.sprintf("%g", val);
    }
}

void MainWindow::txtEditChanged()
{
    checkTxtHdrChange();
}

void MainWindow::txtEditKeyEvent(int tag, QKeyEvent *event)
{
    //    qDebug() << "key=" << event->key();

    Q_UNUSED(tag);

    bool ins = ui->TxtHdrEdit->overwriteMode();

    if (event->key() == Qt::Key_Insert)
        ui->TxtHdrEdit->setOverwriteMode(!ins);

    if (ui->TxtHdrEdit->overwriteMode())
        ui->txtIns->setText("OVR");
    else
        ui->txtIns->setText("INS");
}

void MainWindow::edWhellEvent(int tag, QWheelEvent *event)
{
    double xs, ys;
    int xo, yo;
    double d = event->delta();
    double z;

    if (tag == 1) {
        z = 1 + 0.01 * fabs(d);
        if (d < 0)
            z = 1 / z;
        //      double g = seisSct.Gw()*z;
        double g = seisSct.Gw() * z;
        seisSct.setGc(g);
        seisSct.setGw(g);
        FillEdits();
    } else if (tag == 2) {
        GetZoom(xs, ys, xo, yo);
        z = 1 + 0.002 * fabs(d);
        if (d < 0)
            z = 1 / z;
        ys = ys * z;
        SetZoomC(xs, ys, 0, 0);
    } else if (tag == 3) {
        GetZoom(xs, ys, xo, yo);
        z = 1 + 0.002 * fabs(d);
        if (d < 0)
            z = 1 / z;
        xs = xs * z;
        SetZoomC(xs, ys, 0, 0);
    }
}

void MainWindow::execProc(SeisFile *sf, int ns, double si, float *smp)
{
    Q_UNUSED(sf);

    int n;
    float maxa;

    int rc = 0;

    if (ui->ckFilt->isChecked())
        BandPassFilter(smp, smp, _f1, _f2, _f3, _f4, si, ns);

    if (ui->ckAgc->isChecked()) {
        try {
            n = round(_agcw * 1e-3 / si);
            if (n > ns)
                n = ns;
        } catch (...) {
            n = -1;
        }

        if (n > 0) {
            maxa = MaxAmp(smp, ns);
            Normalize(smp, maxa, ns);
            Agc(smp, n, ns, _agcType);
            maxa = MaxAmp(smp, ns);
            Normalize(smp, maxa, ns);
        }
    } else if (ui->ckNorm->isChecked()) {
        maxa = MaxAmp(smp, ns);
        Normalize(smp, maxa, ns);
    }

    rc = 1;
}

MainWindow::~MainWindow()
{
    /*
        if(WorkThread)
        {
            if(WorkThread->isRunning())
            {
                WorkThread->Stop();
                WorkThread->wait();
            }
            delete WorkThread;
        }
    */
    delete ui;
}

void MainWindow::SetZoomC(double xs, double ys, int xc, int yc)
{
    double xsp, ysp;
    int xo, yo;

    GetZoom(xsp, ysp, xo, yo);

    int x = xc + xo;
    int y = yc + yo;

    double xa = x / xsp;
    double ya = y / ysp;

    x = xa * xs;
    y = ya * ys;

    xo = x - xc;
    yo = y - yc;

    SetZoom(xs, ys, xo, yo);
}

void MainWindow::sclZoomV(double, double zy, int xc, int yc)
{
    sclZoom(1, zy, xc, yc);
}

void MainWindow::sclZoomH(double zx, double, int xc, int yc)
{
    sclZoom(zx, 1, xc, yc);
}

void MainWindow::winZoom(QRect r)
{
    //    QApplication::beep ();

    int x1 = r.left();
    //  int x2 = r.right();
    int y1 = r.top();
    //  int y2 = r.bottom();

    double xsp, ysp;
    int xo, yo;

    GetZoom(xsp, ysp, xo, yo);

    int x = x1 + xo;
    int y = y1 + yo;

    double xa = x / xsp;
    double ya = y / ysp;

    double ws = seisScrl->width();
    double hs = seisScrl->height();

    double wr = r.width();
    double hr = r.height();

    double xz = ws / wr;
    double yz = hs / hr;

    double xs = xsp * xz;
    double ys = ysp * yz;

    x = xa * xs;
    y = ya * ys;

    xo = x;
    yo = y;

    SetZoom(xs, ys, xo, yo);
}

void MainWindow::sclZoom(double zx, double zy, int xc, int yc)
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    SetZoomC(xs * zx, ys * zy, xc, yc);
}

void MainWindow::ChangeDataDir(QString Dname, QString Find)
{
    emit stop_dirs();

    curDir = Dname;

    find = Find;

    QThread *thread = new QThread;
    DirScanWorker *worker = new DirScanWorker(Dname);
    worker->moveToThread(thread);
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(edir(DirList)), this, SLOT(x_dir(DirList)));
    connect(worker, SIGNAL(eprogr(int, QString)), this,
            SLOT(x_progr(int, QString)));
    connect(worker, SIGNAL(efin(QString)), this, SLOT(x_fin(QString)));

    connect(this, SIGNAL(stop_dirs()), worker, SLOT(stop()),
            Qt::DirectConnection);

    thread->start();
}

void MainWindow::x_dir(DirList dlist)
{
    FillDirGrid(dlist);
    ShowProgress("", -1);
}

void MainWindow::x_delay(int dmin, int dmax)
{
    qDebug() << dmin << dmax;
    _dly_min = dmin;
    _dly_max = dmax;

    InitSeisView();
}

void MainWindow::x_progr(int pers, QString mess)
{
    ShowProgress(mess, pers);
}

void MainWindow::x_fin(QString mess)
{
    ShowProgress(mess, -1);
}

void MainWindow::ScanLtod()
{
    /*
        if(WorkThread)
        {
            if(WorkThread->isRunning())
            {
                WorkThread->Stop();
                WorkThread->wait();
            }
            delete WorkThread;
        }

        ShowProgress("Scanning LTOD tape image ...", 0);

        LtdScanThread* LtdScaner = new LtdScanThread();

        WorkThread = LtdScaner;

        LtdScaner->Read(curFileName, this);
    */
}

void MainWindow::FillDirGrid(QList<DirItem> list)
{
    int ni, nr;

    dlist = list;

    ni = dlist.count();

    dirGrid.setRowCount(ni);

    for (nr = 0; nr < ni; nr++) {
        DirItem di = dlist[nr];

        if (di.ftype == 0) // DIR
        {
            dirGrid.setCell(nr, 0, di.fname);
            dirGrid.setImgName(nr, 0, ":/images/Dir.png");
            // dirGrid.setCell(nr,1,"");
            dirGrid.setCell(nr, 1, "DIR");
            dirGrid.setTag(nr, 0, 0);
        } else if (di.ftype == 10) // SeisFile
        {
            int type = di.type;

            if (type) {
                QString fn = di.fname;
                type = di.type;

                dirGrid.setTag(nr, 0, 1);
                dirGrid.setTag(nr, 1, type);

                dirGrid.setCell(nr, 0, fn);
                dirGrid.setCellToolTip(nr, 0, fn);
                // dirGrid.setCell(nr, 1, "");

                if (type == 1) {
                    dirGrid.setImgName(nr, 0, ":/images/SeisFile.png");
                    dirGrid.setCell(nr, 1, "SEG-Y");
                } else if (type == 2) {
                    dirGrid.setImgName(nr, 0, ":/images/sufile.png");
                    dirGrid.setCell(nr, 1, "SU");
                } else if (type == 3) {
                    dirGrid.setImgName(nr, 0, ":/images/cstfile.png");
                    dirGrid.setCell(nr, 1, "CST");
                }
            }
        }
    }

    int nf = -1;

    if (find != "") {
        for (nr = 0; nr < ni; nr++) {
            QString name = dirGrid.Cell(nr, 0);
            if (name == find) {
                nf = nr;
                break;
            }
        }

        if (nf >= 0) {
            dirGrid.setCurCell(nf);
        } else {
            return;
        }
    }

    dirGrid.setCurCell(0);
    //  dirGrid.setCurCell(nf);
}

void MainWindow::SelectTrace(int it)
{
    if (it >= 0 && it < seisSrc.Nt()) {
        int trcn = seisSrc.Th(it, 0);

        char s[2048];

        sprintf(s, "Trace %d", trcn);

        m_statTrcn->setText(s);
        ui->InfoTab->setTabText(2, s);

        seisSct.setSLcolor(QColor(0, 0, 255).rgb());
        seisSct.setSelTr(it);

        for (int i = 0; i < trcHdrGrid.RowCount(); i++) {
            int val = seisSrc.Th(it, i + 1);
            QString v;
            v.sprintf("%d", val);

            trcHdrGrid.setCell(i, 0, v);
        }

        trcDatGrid.setRowCount(seisSrc.Ns());

        SeisFile *sf = seisSrc.Sfile();
        float *smp = sf->Rs(it);

        if (smp)
            for (int i = 0; i < trcDatGrid.RowCount(); i++) {
                double val = smp[i];

                QString v;
                v.sprintf("%10.6f", val);

                trcDatGrid.setCell(i, 0, QString::number(i));
                trcDatGrid.setCell(i, 1, QString::number(i * sf->Si() * 1000.));
                trcDatGrid.setCell(i, 2, v);
            }

        busy = true;
        //      hdrListDtGrid.setCurCell(it);
        hdrListDtGrid.setCurRow(it);

        busy = false;
    } else {
        seisSct.setSelTr(-1);
        for (int i = 0; i < trcHdrGrid.RowCount(); i++) {
            trcHdrGrid.setCell(i, 0, "");
            ui->InfoTab->setTabText(2, "Trace");
            hdrListDtGrid.setCurCell(-1);
        }
    }

    SetTrEdCurVal();
}

void MainWindow::viewMouseEvent(QMouseEvent *event)
{
    if (event->type() == QEvent::MouseMove) {
        int x = event->x() - seisView.x();
        ;
        int y = event->y() - seisView.y();

        double t = round(seisSct.pix2y(y) * 1000);
        double it = round(seisSct.pix2x(x));

        m_statTime->setText(Tprintf("Time= %g", t));

        if (it >= 0 && it < seisSrc.Nt()) {
            int trcn = seisSrc.Th(it, 0);
            m_statTrcn->setText(Tprintf("Trace %d", trcn));
        } else {
            m_statTrcn->setText(Tprintf("Trace "));
        }
    } else if (event->type() == QEvent::MouseButtonPress &&
               event->button() == Qt::LeftButton) {
        int x = event->x() - seisView.x();
        ;

        int xa = 0;
        int xb;
        // int n;

        xb = xa + seisSct.WidthView();

        if (x >= xa && x <= xb) {
            int it = round(seisSct.pix2x(x));

            SelectTrace(it);
        } else {
            SelectTrace(-1);
        }
    }
}

void MainWindow::SetHdrDatList()
{
    SeisFile *sf = seisSrc.Sfile();
    if (!sf)
        return;

    TrHdrDef *h;
    QString sidx;
    int sel = -1;

    int i, j;
    int l = 0;

    sidx = ui->cbSidx->currentText();

    if (sidx == "Trace")
        sel = 0;

    hdrListCk.clear();
    ui->cbSidx->clear();

    ui->cbSidx->addItem("Trace");

    for (i = 0; i < hdrListCkGrid.RowCount(); i++) {
        if (hdrListCkGrid.Check(i, 0)) {
            l++;
        }
    }

    hdrListDtGrid.setColCount(l + 1);

    if (sel >= 0)
        ui->cbSidx->setCurrentIndex(sel);
    hdrListDtGrid.setRowCount(sf->Nt());

    for (i = 0, j = 1; i < hdrListCkGrid.RowCount(); i++) {
        if (hdrListCkGrid.Check(i, 0)) {
            h = sf->THdef(i + 1);

            QString name = h->name;

            hdrListDtGrid.setColLabel(j, name);
            hdrListDtGrid.setColWidthInCharacters(
                j, GfxStyle::TraceHeaderValueCharacterCount);
            hdrListDtGrid.setColTag(j, i + 1);
            hdrListCk.set(name);
            ui->cbSidx->addItem(name);
            if (sidx == name)
                sel = j;
            j++;
        }
    }
}

void MainWindow::SetHdrDatElst()
{
    SeisFile *sf = seisSrc.Sfile();
    if (!sf)
        return;

    TrHdrDef *h;
    QString sidx;
    //  int       sel = -1;

    int i, j;
    int l = 0;

    //  sidx = ui->cbSidx->currentText();

    //  if(sidx=="Trace") sel=0;

    hdrElstCk.clear();
    //  ui->cbSidx->clear();

    //  ui->cbSidx->addItem("Trace");

    for (i = 0; i < hdrElstCkGrid.RowCount(); i++) {
        if (hdrElstCkGrid.Check(i, 0)) {
            l++;
        }
    }

    hdrElstDtGrid.setColCount(l * 2 + 1);

    // if(sel>=0) ui->cbSidx->setCurrentIndex(sel);
    hdrElstDtGrid.setRowCount(sf->Nt());

    for (i = 0, j = 1; i < hdrElstCkGrid.RowCount(); i++) {
        if (hdrElstCkGrid.Check(i, 0)) {
            h = sf->THdef(i + 1);

            QString name = h->name;

            hdrElstDtGrid.setColLabel(j, name);
            hdrElstDtGrid.setColWidthInCharacters(
                j, GfxStyle::TraceHeaderValueCharacterCount);
            hdrElstDtGrid.setColTag(j, i + 1);
            hdrElstCk.set(name);
            //          ui->cbSidx->addItem(name);
            //          if(sidx==name) sel=j;
            j++;

            QString e = "";
            //          if(exprList.contains(name))
            //          e=exprList[name].ExprString();
            if (exprList.contains(name))
                e = exprList[name].ExprString();

            hdrElstDtGrid.setColLabel(j, e);
            hdrElstDtGrid.setColWidthInCharacters(
                j, GfxStyle::TraceHeaderValueCharacterCount);
            hdrElstDtGrid.setColTag(j, i + 1);
            j++;
        }
    }

    //  if(sel>=0) ui->cbSidx->setCurrentIndex(sel);
    //  hdrElstDtGrid.setRowCount(sf->Nt());
}

void MainWindow::hdrListCkEvent(int row, int mode)
{
    Q_UNUSED(row);

    if (mode != 4) {
        return;
    }

    SetHdrDatList();
}

void MainWindow::hdrElstCkEvent(int row, int mode)
{
    Q_UNUSED(row);

    if (mode != 4) {
        return;
    }

    SetHdrDatElst();
}

void MainWindow::hdrListDtGridEvent(qint64 row, int mode)
{
    if (mode == 1) {
        if (busy)
            return;

        SelectTrace(row);

        int x = seisSct.x2fpix(row);

        double xs, ys;
        int xo, yo;

        GetZoom(xs, ys, xo, yo);

        double ws = seisScrl->width() / 2;

        SetZoom(xs, ys, x - ws, yo);
    }
}

void MainWindow::dirGridEvent(int row, int mode)
{
    QString name = dirGrid.Cell(row, 0);
    int tag = dirGrid.Tag(row, 0);
    int frm = dirGrid.Tag(row, 1);

    m_statTime->setText("Time=");
    // m_statAmp ->setText("Amp="  );

    if (mode == 2) {
        // m_statFfid ->setText("FFID="  );
        // m_statSp   ->setText("SP="  );

        curFn = "";

        setWindowTitle("");
        if (tag == 0) {
            if (name == "..") {
                QDir dir(curDir);

                QString name = dir.dirName();

                dir.cdUp();

                QString dirname = dir.absolutePath();

                ChangeDataDir(dirname, name);
            } else if (name == ".") {
                QDir dir(curDir);

                QString name = QFileInfo(curFileName).fileName();

                QString dirname = dir.absolutePath();

                ChangeDataDir(dirname, name);
            } else {
                ChangeDataDir(curDir + "/" + name);
            }
        } else if (tag == 2) {
            // curFileName = curDir+"/"+name;
            // ScanLtod();
        }
    } else if (mode == 1 && tag == 1) {
        setWindowTitle("");
        curFileName = curDir + "/" + name;

        OpenSeisFile(frm);
    } else if (mode == 1 && tag == 3 && frm > 0) {
        setWindowTitle("");
        // ReadSegdInLtod(row);
    } else if (mode == 1) {
        // m_statFfid ->setText("FFID="  );
        // m_statSp   ->setText("SP="  );

        curFn = "";

        setWindowTitle(QString("Dir: ") + curDir);

        Clear();
    }
}

void MainWindow::xprint(QPlainTextEdit *edit, QString str)
{
    edit->moveCursor(QTextCursor::End);
    edit->insertPlainText(str);
    edit->moveCursor(QTextCursor::End);
}

int MainWindow::xprintf(QPlainTextEdit *edit, const char *fmt, ...)
{
    char buffer[2048];
    va_list argptr;
    int cnt;

    va_start(argptr, fmt);
    cnt = vsnprintf(buffer, 2047, fmt, argptr);
    va_end(argptr);

    xprint(edit, buffer);

    return cnt;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    SaveRegInfo();
    AboutDlg.close();
    AxisDlg.close();
    DiffDlg.close();
    EdHdrDlg.close();
    ProcParmDlg.close();
    SaveAsDlg.close();
    QMainWindow::closeEvent(event);
}

void MainWindow::ShowProgress(QString mes, int pers)
{
    if (pers < 0) {
        progressBar->setValue(0);
        // progressBar->setVisible(false);
        m_statMess->setStyleSheet("");
        m_statMess->setText(mes);
    } else {
        progressBar->setValue(pers);
        // if(!progressBar->isVisible()) progressBar->setVisible(true);

        if (mes != "")
            m_statMess->setStyleSheet(
                "QLabel { background-color : lime; color : black; }");
        else
            m_statMess->setStyleSheet("");

        m_statMess->setText(mes);
    }
}

int MainWindow::checkBinHdrChange()
{
    // return:
    //  0 - not changed
    // -1 - data error
    //  1 - changed

    int nr = binHdrGrid.RowCount();
    SeisFile *sf = seisSrc.Sfile();

    if (sf == NULL || !sf->Active() || nr < 1) {
        ui->btnBinRst->hide();
        ui->btnBinUpd->hide();
        return 0;
    }

    int n;

    int changed = 0;

    QVector<QString> vals;

    for (n = 0; n < nr; n++) {
        QString s = binHdrGrid.Cell(n, 0);
        vals.append(s);
    }

    changed = sf->binHed()->check(vals);

    if (changed == 1) {
        ui->btnBinRst->show();
        ui->btnBinUpd->show();
        dirGrid.setEnabled(false);
    } else {
        ui->btnBinRst->hide();
        ui->btnBinUpd->hide();
        dirGrid.setEnabled(true);
    }

    return changed;
}

void MainWindow::FillBinHdrGrid()
{
    SeisFile *sf = seisSrc.Sfile();

    if (sf == NULL || !sf->Active()) {
        binHdrGrid.setRowCount(0);
        return;
    }

    int n;

    busy = true;

    binHdrGrid.setRowCount(sf->binHed()->count());

    for (n = 0; n < sf->binHed()->count(); n++) {
        binHdrGrid.setCell(n, 0, sf->binHed()->sval(n)); //->binHed->sval(n));
        binHdrGrid.setCell(n, 1, sf->binHed()->name(n));
        binHdrGrid.setCell(n, 2, sf->binHed()->bstr(n));

        if (sf->binHed()->edit(n)) {
            binHdrGrid.setCellEdit(n, 0, true);
        } else {
            if (ui->ckTrEd->isChecked()) {
                binHdrGrid.setCellColor(n, 0, Qt::red);
                binHdrGrid.setCellEdit(n, 0, true);
            } else {
                binHdrGrid.setCellColor(n, 0, Qt::lightGray);
            }
        }
    }
    busy = false;
}

int MainWindow::ScanDelay()
{
    emit stop_dlys();

    SeisFile *sf = seisSrc.Sfile();

    _dly_min = 0;
    _dly_max = 0;

    if (!sf)
        return 0;

    if (sf->ThIdx("DELRECT") < 0)
        return 0;

    QThread *thread = new QThread;
    DelayScanWorker *worker = new DelayScanWorker(sf);
    worker->moveToThread(thread);
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(edelay(int, int)), this, SLOT(x_delay(int, int)));
    connect(worker, SIGNAL(eprogr(int, QString)), this,
            SLOT(x_progr(int, QString)));
    connect(worker, SIGNAL(efin(QString)), this, SLOT(x_fin(QString)));
    connect(this, SIGNAL(stop_dlys()), worker, SLOT(stop()),
            Qt::DirectConnection);

    thread->start();

    return 0;
}

int MainWindow::OpenSeisFile(int frm)
{
    int n;

    emit stop_dlys();

    ClrUndoE();

    n = ui->InfoTab->currentIndex();

    SeisFile *sf = seisSrc.Sfile();

    seisSrc.setSfile(NULL);

    if (sf)
        delete sf;

    if (frm == 1) {
        sf = new SgyFile(this);
        ui->InfoTab->setTabEnabled(1, true);
    } else if (frm == 2) {
        sf = new SuFile(this);
        if (n == 1)
            ui->InfoTab->setCurrentIndex(0);
        ui->InfoTab->setTabEnabled(1, false);
    } else if (frm == 3) {
        if (n == 1)
            ui->InfoTab->setCurrentIndex(0);
        sf = new CstFile(this);
        ui->InfoTab->setTabEnabled(1, false);
    }

    connect(sf, SIGNAL(execProc(SeisFile *, int, double, float *)), this,
            SLOT(execProc(SeisFile *, int, double, float *)),
            Qt::DirectConnection);

    sf->setFname(curFileName);
    sf->setActive(true);
    ui->InfoTxt->clear();

    xprintf(ui->InfoTxt, "Summary Information\n");
    xprintf(ui->InfoTxt, "------------------------------\n");
    xprintf(ui->InfoTxt, "File name:\n%s\n", q2s(curFileName));

    setWindowTitle(curFileName);

    if (frm == 1) {
        xprintf(ui->InfoTxt, "File type: SEG-Y\n\n");

        if (sf->TxtCode() == 1)
            xprintf(ui->InfoTxt, "Text header encoding: EBCDIC\n");
        else
            xprintf(ui->InfoTxt, "Text header encoding: ASCII\n");

        hdrListCk.setListIdx(1);
        hdrAxisCk.setListIdx(1);
        TraceHdrsSet.setListIdx(1);
    } else if (frm == 2) {
        xprintf(ui->InfoTxt, "File type: SU\n");
        hdrListCk.setListIdx(1);
        hdrAxisCk.setListIdx(1);
        TraceHdrsSet.setListIdx(1);
    } else if (frm == 3) {
        xprintf(ui->InfoTxt, "File type: CST\n");
        hdrListCk.setListIdx(2);
        hdrAxisCk.setListIdx(2);
        TraceHdrsSet.setListIdx(2);
        // ui->BinHdrTab->hide();
    }

    if (sf->Swap() == 1)
        xprintf(ui->InfoTxt, "Byte order: Big endian\n");
    else
        xprintf(ui->InfoTxt, "Byte order: Little endian\n");

    xprintf(ui->InfoTxt, "\n");
    xprintf(ui->InfoTxt, "# Traces            : %d\n", sf->Nt());
    xprintf(ui->InfoTxt, "# Samples           : %d\n\n", sf->Ns());
    xprintf(ui->InfoTxt, "Sample Format       : %d", sf->Format());
    xprint(ui->InfoTxt, " - " + sf->FormatName() + "\n");
    xprintf(ui->InfoTxt, "Sample Interval (μs): %d\n", int(sf->Si() * 1000000));

    if (sf->Jnk())
        xprintf(ui->InfoTxt, "\nWarning: %d junk bytes at the end of file",
                sf->Jnk());

    ui->TxtHdrEdit->clear();
    ui->TxtHdrEdit->insertPlainText(sf->TxtHed());
    ui->TxtHdrEdit->document()->clearUndoRedoStacks();
    checkTxtHdrChange();
    checkBinHdrChange();

    QTextCursor tmpCursor = ui->TxtHdrEdit->textCursor();
    tmpCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor, 1);
    ui->TxtHdrEdit->setTextCursor(tmpCursor);
    on_TxtHdrEdit_cursorPositionChanged();

    ui->InfoTab->setTabText(2, "Trace");

    seisSrc.setSfile(sf);

    _dly_min = 0;
    _dly_max = 0;

    if (ui->ckDly->isChecked() && sf->ThIdx("DELRECT") >= 0) {
        ScanDelay();

        return 0;
    }

    InitSeisView();

    return 0;
}

int MainWindow::InitSeisView()
{
    SeisFile *sf = seisSrc.Sfile();

    if (ui->ckDly->isChecked()) {
        sf->setDly(1);
    } else {
        sf->setDly(0);
    }

    ui->ckTrEd->setChecked(false);
    seisSct.setSelTr(-1);

    seisSct.setXs(1);
    seisSct.setYs(10);

    hdrsAxis.setXs(1);
    hdrsAxis.setYs(10);
    hdrsBottomAxis.setXs(1);
    hdrsBottomAxis.setYs(10);

    hdrsAxis.setHdrList(hdrAxisCk.List());
    hdrsBottomAxis.setHdrList(hdrAxisCk.List());
    hdrsLab.setHdrList(hdrAxisCk.List());

    if (ui->rbDirRev->isChecked()) {
        seisSct.setX1(sf->Nt());
        seisSct.setX2(-1);
        hdrsAxis.setX1(sf->Nt());
        hdrsAxis.setX2(-1);
        hdrsBottomAxis.setX1(sf->Nt());
        hdrsBottomAxis.setX2(-1);
    } else {
        seisSct.setX1(-1);
        seisSct.setX2(sf->Nt());
        hdrsAxis.setX1(-1);
        hdrsAxis.setX2(sf->Nt());
        hdrsBottomAxis.setX1(-1);
        hdrsBottomAxis.setX2(sf->Nt());
    }

    double tmin = sf->Tmin() + _dly_min / 1000.;
    double tmax = sf->Tmax() + _dly_max / 1000.;

    seisSct.setY1(tmin);
    seisSct.setY2(tmax); // !!!
    timeAxis.setY1(tmin);
    timeAxis.setY2(tmax); // !!!
    timeRightAxis.setY1(tmin);
    timeRightAxis.setY2(tmax);

    seisSct.setTrim(true);

    ArrangeSections();

    FillBinHdrGrid();
    FillHdrListGrids();
    FillHdrElstGrids();

    SetSearchControls(true);

    return 0;
}

void MainWindow::Clear()
{
    ui->seisFrame->setVisible(false);
    ui->InfoTxt->clear();
    ui->TxtHdrEdit->clear();
}

int MainWindow::allSectWidth()
{
    double w = 0;

    w = seisSct.WidthPix() + 1;

    return w;
}

void MainWindow::ArrangeSections()
{
    ui->seisFrame->setVisible(true);

    int hh = timeAxis.HeightView();
    int axisHeight = qMax(hh, seisScrl->viewport()->height());

    seisView.setFixedHeight(hh);
    timeRightView.setFixedHeight(axisHeight);
    timeView.setFixedHeight(axisHeight);

    int nh = hdrAxisCk.List().count();

    int ht = timeView.getGfx()->GetTextHeight();

    int rowGap =
        timeView.getGfx()->ScaleY(GfxStyle::HeaderLabelRowGap);
    int verticalPadding =
        timeView.getGfx()->ScaleY(GfxStyle::HeaderRowsBottomPadding);
    int h = nh * (ht + rowGap) + verticalPadding;

    hdrsScrl->setFixedHeight(h); //(nh+1)*13);
    hdrsBottomScrl->setFixedHeight(h);
    hdrsView.setFixedHeight(h);
    hdrsBottomView.setFixedHeight(h);
    hlabView.setFixedHeight(h); //(nh+1)*13);

    double x = 0;
    double w = 0;

    seisSct.setXo(x);
    hdrsAxis.setXo(x);
    hdrsBottomAxis.setXo(x);
    x += (seisSct.X2() - seisSct.X1());
    w += seisSct.WidthView() + GfxStyle::AxisLineWidthPixels;

    seisView.setFixedWidth(w);
    int axisWidth = qMax(static_cast<int>(w), seisScrl->viewport()->width());
    hdrsView.setFixedWidth(axisWidth);
    hdrsBottomView.setFixedWidth(axisWidth);

    timeLabel->refreshSize();
    Gfx *timeGfx = timeView.getGfx();
    int headerTextStart = hdrsLab.TextStartX();
    int timeLabelGap = timeGfx->ScaleX(GfxStyle::TimeLabelToAxisGap);
    int labelAndTicksWidth =
        qMax(timeAxis.RequiredWidth(), timeRightAxis.RequiredWidth());
    int timeAxisWidth = headerTextStart + timeLabel->rotatedTextWidth() +
                        timeLabelGap + labelAndTicksWidth;
    timeScrl->setFixedWidth(timeAxisWidth);
    timeRightScrl->setFixedWidth(timeAxisWidth);
    hlabView.setFixedWidth(timeAxisWidth);
    timeView.setFixedWidth(timeAxisWidth);
    timeRightView.setFixedWidth(timeAxisWidth);
    timeLabel->move(headerTextStart,
                    timeGfx->ScaleY(GfxStyle::TimeLabelTopOffset));

    syncHorizontalAxisScrollBar(seisScrl->horizontalScrollBar()->minimum(),
                                seisScrl->horizontalScrollBar()->maximum());
    syncVerticalAxisScrollBar(seisScrl->verticalScrollBar()->minimum(),
                              seisScrl->verticalScrollBar()->maximum());

    QTimer::singleShot(0, this, SLOT(fitAxesToViewport()));
}

void MainWindow::fitAxesToViewport()
{
    if (!ui || !ui->seisFrame->isVisible())
        return;

    if (ui->seisFrame->layout())
        ui->seisFrame->layout()->activate();

    int axisHeight =
        qMax(timeAxis.HeightView(), seisScrl->viewport()->height());
    int axisWidth = qMax(static_cast<int>(seisSct.WidthView() + 1),
                         seisScrl->viewport()->width());

    timeView.setFixedHeight(axisHeight);
    timeRightView.setFixedHeight(axisHeight);
    hdrsView.setFixedWidth(axisWidth);
    hdrsBottomView.setFixedWidth(axisWidth);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    QTimer::singleShot(0, this, [this]() { updateCompactSeismicControls(); });
    if (ui && ui->seisFrame->isVisible())
        ArrangeSections();
}

void MainWindow::updateCompactSeismicControls()
{
    if (!ui || ui->frame->width() <= 0)
        return;

    QHBoxLayout *layout = qobject_cast<QHBoxLayout *>(ui->frame->layout());
    if (!layout)
        return;

    const int spacing = qMax(0, layout->spacing());
    const QMargins margins = layout->contentsMargins();
    const QList<QGroupBox *> allGroups = {
        ui->groupBox, ui->groupBox_2, ui->groupBox_3, ui->groupBox_4,
        ui->groupBox_13
    };
    for (QGroupBox *group : allGroups)
        group->setVisible(true);

    const int availableWidth = ui->frame->width();
    const int fixedWidth = margins.left() + margins.right() +
                           ui->frame->frameWidth() * 2;
    const QList<QGroupBox *> collapseOrder = {
        ui->groupBox_13, ui->groupBox_2, ui->groupBox_4, ui->groupBox
    };

    const auto requiredWidth = [&allGroups, fixedWidth, spacing]() {
        int width = fixedWidth;
        int visibleCount = 0;
        for (QGroupBox *group : allGroups) {
            if (group->isHidden())
                continue;
            width += qMax(group->minimumWidth(),
                          group->minimumSizeHint().width());
            ++visibleCount;
        }
        if (visibleCount > 1)
            width += spacing * (visibleCount - 1);
        return width;
    };

    for (QGroupBox *group : collapseOrder) {
        if (requiredWidth() <= availableWidth)
            break;
        group->hide();
    }
}

void MainWindow::syncHorizontalAxisScrollBar(int minimum, int maximum)
{
    QScrollBar *source = seisScrl->horizontalScrollBar();
    QSignalBlocker blocker(horizontalAxisScrollBar);
    bool shouldShow = maximum > minimum;
    bool visibilityChanged = horizontalAxisScrollBar->isHidden() == shouldShow;

    horizontalAxisScrollBar->setRange(minimum, maximum);
    horizontalAxisScrollBar->setPageStep(source->pageStep());
    horizontalAxisScrollBar->setSingleStep(source->singleStep());
    horizontalAxisScrollBar->setValue(source->value());
    horizontalAxisScrollBar->setVisible(shouldShow);

    if (visibilityChanged)
        QTimer::singleShot(0, this, SLOT(fitAxesToViewport()));
}

void MainWindow::syncVerticalAxisScrollBar(int minimum, int maximum)
{
    QScrollBar *source = seisScrl->verticalScrollBar();
    QSignalBlocker blocker(verticalAxisScrollBar);
    bool shouldShow = maximum > minimum;
    bool visibilityChanged = verticalAxisScrollBar->isHidden() == shouldShow;

    verticalAxisScrollBar->setRange(minimum, maximum);
    verticalAxisScrollBar->setPageStep(source->pageStep());
    verticalAxisScrollBar->setSingleStep(source->singleStep());
    verticalAxisScrollBar->setValue(source->value());
    verticalAxisScrollBar->setVisible(shouldShow);

    if (visibilityChanged)
        QTimer::singleShot(0, this, SLOT(fitAxesToViewport()));
}

void MainWindow::ResetSrc()
{
    //    int n;
    if (seisSrc.Sfile())
        seisSrc.Sfile()->Reset();
}

void MainWindow::on_ckAgc_toggled(bool checked)
{
    if (checked && ui->ckNorm->isChecked())
        ui->ckNorm->setChecked(false);

    ResetSrc();
}

void MainWindow::on_ckNorm_toggled(bool checked)
{
    if (checked && ui->ckAgc->isChecked())
        ui->ckAgc->setChecked(false);

    ResetSrc();
}

void MainWindow::on_ckFilt_toggled(bool)
{
    ResetSrc();
}

void MainWindow::on_ckWiggle_toggled(bool checked)
{
    seisSct.setDispWig(checked);
}

void MainWindow::on_ckGray_toggled(bool checked)
{
    if (checked && ui->ckColor->isChecked())
        ui->ckColor->setChecked(false);

    seisSct.setDispCol(checked);

    QList<QRgb> grpal;

    if (ui->ckColor->isChecked())
        seisSct.setPal(rgb);
    else
        seisSct.setPal(grpal);
}

void MainWindow::on_ckColor_toggled(bool checked)
{
    if (checked && ui->ckGray->isChecked())
        ui->ckGray->setChecked(false);

    seisSct.setDispCol(checked);

    QList<QRgb> grpal;

    double v[3] = {-1, 0, 1};
    QRgb c[3];
    QList<QRgb> pal;

    c[0] = QColor(255, 0, 0).rgb();
    c[1] = QColor(255, 255, 255).rgb();
    c[2] = QColor(0, 0, 255).rgb();

    pal = CreatePalette24(c, v, -1, 1, 3);

    if (ui->ckColor->isChecked())
        seisSct.setPal(pal);
    else
        seisSct.setPal(grpal);
}

void MainWindow::on_rbNon_toggled(bool checked)
{
    if (checked)
        seisSct.setWFill(0);
}

void MainWindow::on_rbPos_toggled(bool checked)
{
    if (checked)
        seisSct.setWFill(1);
}

void MainWindow::on_rbNeg_toggled(bool checked)
{
    if (checked)
        seisSct.setWFill(-1);
}

void MainWindow::FillEdits()
{
    ui->edTr->setText(Tprintf("%g", 10 / seisView.Xs()));
    ui->edTm->setText(Tprintf("%g", seisView.Ys()));
    ui->edGn->setText(Tprintf("%g", seisSct.Gw()));
}

void MainWindow::FillControls()
{
    ui->ckWiggle->setChecked(seisSct.DispWig());
    ui->ckColor->setChecked(seisSct.DispCol());

    if (seisSct.DispCol()) {
        if (seisSct.Pal()) {
            ui->ckGray->setChecked(false);
            ui->ckColor->setChecked(true);
        } else {
            ui->ckGray->setChecked(true);
            ui->ckColor->setChecked(false);
        }
    } else {
        ui->ckGray->setChecked(false);
        ui->ckColor->setChecked(false);
    }

    int v = seisSct.WFill();

    ui->rbNeg->setChecked((v == -1));
    ui->rbNon->setChecked((v == 0));
    ui->rbPos->setChecked((v == 1));

    ui->ckTimLines->setChecked(seisSct.Tl());
}

void MainWindow::GetRegInfo()
{
    QSettings settings("WW", "SeiSeeMp");

    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

    QString dir = settings.value("DataDir").toString();
    ChangeDataDir(dir);

    txtHdrDir = settings.value("TxtHdrDir").toString();
    savDir = settings.value("SaveDir").toString();

    int color_mode = settings.value("DispCol", 1).toInt();
    bool wigle_mode = settings.value("DispWig", false).toBool();

    QList<QRgb> grpal;

    if (color_mode) {
        seisSct.setDispCol(true);

        if (color_mode == 1)
            seisSct.setPal(grpal);
        else
            seisSct.setPal(rgb);
    } else {
        if (!wigle_mode) {
            seisSct.setDispCol(true);
            seisSct.setPal(grpal);
        } else {
            seisSct.setDispCol(false);
        }
    }

    seisSct.setDispWig(wigle_mode);

    seisSct.setWFill(settings.value("WFill", 1).toInt());

    seisSct.setGc(settings.value("Gc", 1).toDouble());
    seisSct.setGw(settings.value("Gw", 1).toDouble());

    ui->ckFilt->setChecked(settings.value("Filt", false).toBool());
    ui->ckAgc->setChecked(settings.value("Agc", false).toBool());
    ui->ckNorm->setChecked(settings.value("Norm", true).toBool());

    if (ui->ckNorm->isChecked() && ui->ckAgc->isChecked())
        ui->ckAgc->setChecked(false);

    ui->rbDirRev->setChecked(settings.value("Rev", false).toBool());

    ui->ckDly->setChecked(settings.value("Dly", false).toBool());

    _f1 = settings.value("F1", 10).toInt();
    _f2 = settings.value("F2", 20).toInt();
    _f3 = settings.value("F3", 500).toInt();
    _f4 = settings.value("F4", 1000).toInt();

    _agcw = settings.value("AgcW", 1000).toInt();
    const int savedAgcType = settings.value("AgcType", 0).toInt();
    _agcType = qBound(0, savedAgcType, 1);

    const int savedSeismicDpi = settings.value("SeismicDpi", 300).toInt();
    m_exportDpi =
        savedSeismicDpi == 400 || savedSeismicDpi == 600
            ? savedSeismicDpi
            : 300;
    if (m_exportDpiMenu) {
        for (QAction *action : m_exportDpiMenu->actions())
            action->setChecked(action->data().toInt() == m_exportDpi);
    }

    double xs = settings.value("Xs", 1.0).toDouble();
    double ys = settings.value("Ys", 1.0).toDouble();

    const char *h[2] = {"SgyHdrsTab", "CstHdrsTab"};

    QString ck;
    QStringList hlist;

    int i, n;

    for (n = 0; n < 2; n++) {
        hdrListCk.setListIdx(n + 1);

        int size = settings.beginReadArray(h[n]);

        for (i = 0; i < size; i++) {
            settings.setArrayIndex(i);
            ck = settings.value("ck").toString();
            hdrListCk.set(ck);
        }

        settings.endArray();
    }

    const char *a[2] = {"SgyHdrsAx", "CstHdrsAx"};

    for (n = 0; n < 2; n++) {
        hlist.clear();
        hdrAxisCk.setListIdx(n + 1);

        int size = settings.beginReadArray(a[n]);

        for (i = 0; i < size; i++) {
            settings.setArrayIndex(i);
            ck = settings.value("ck").toString();
            hlist.append(ck);
        }

        if (hlist.count() == 0) {
            hlist.append("Trace");
        }

        hdrAxisCk.setList(hlist);

        settings.endArray();
    }

    const char *f[2] = {"SgyHdrsDef", "CstHdrsDef"};

    for (n = 0; n < 2; n++) {
        int size = settings.beginReadArray(f[n]);

        TrHdrDefList *List = TraceHdrsSet.List(n + 1);

        for (i = 0; i < size; i++) {
            TrHdrDef hdr;

            settings.setArrayIndex(i);

            QString estr = settings.value("expr").toString().trimmed();

            hdr.name = settings.value("name").toString();
            hdr.desc = settings.value("desc").toString();
            hdr.frmt = settings.value("frmt").toInt();
            hdr.pos = settings.value("pos").toInt();
            hdr.edit = 2;

            if (estr != "") {
                hdr.expr.setExprString(estr);
            }

            List->add(hdr);
        }

        settings.endArray();
    }

    seisView.setXs(xs);
    seisView.setYs(ys);
    hdrsView.setXs(xs);
    hdrsBottomView.setXs(xs);
    timeView.setYs(ys);
    timeRightView.setYs(ys);

    double ti = settings.value("Ti", "0.1").toDouble();
    bool tl = settings.value("Tl", "false").toBool();

    timeAxis.setTi(ti);
    seisSct.setTi(ti);
    seisSct.setTl(tl);

    ArrangeSections();

    FillEdits();
    FillControls();
}

void MainWindow::SaveRegInfo()
{
    QSettings settings("WW", "SeiSeeMp");
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());

    settings.setValue("DataDir", curDir);
    settings.setValue("SaveDir", savDir);
    settings.setValue("TxtHdrDir", txtHdrDir);

    if (seisSct.DispCol()) {
        if (seisSct.Pal())
            settings.setValue("DispCol", 2);
        else
            settings.setValue("DispCol", 1);
    } else {
        settings.setValue("DispCol", 0);
    }

    settings.setValue("DispWig", seisSct.DispWig());

    settings.setValue("WFill", seisSct.WFill());

    settings.setValue("Gc", seisSct.Gc());
    settings.setValue("Gw", seisSct.Gw());

    settings.setValue("Filt", ui->ckFilt->isChecked());
    settings.setValue("Agc", ui->ckAgc->isChecked());
    settings.setValue("Norm", ui->ckNorm->isChecked());

    settings.setValue("Rev", ui->rbDirRev->isChecked());

    settings.setValue("Dly", ui->ckDly->isChecked());

    settings.setValue("F1", _f1);
    settings.setValue("F2", _f2);
    settings.setValue("F3", _f3);
    settings.setValue("F4", _f4);

    settings.setValue("AgcW", _agcw);
    settings.setValue("AgcType", _agcType);
    settings.setValue("SeismicDpi", m_exportDpi);

    settings.setValue("Xs", seisView.Xs());
    settings.setValue("Ys", seisView.Ys());

    QStringList Cks;
    int i, n;

    const char *h[2] = {"SgyHdrsTab", "CstHdrsTab"};

    for (n = 0; n < 2; n++) {
        hdrListCk.setListIdx(n + 1);
        Cks = hdrListCk.List();

        settings.remove(h[n]);
        settings.beginWriteArray(h[n]);

        for (i = 0; i < Cks.size(); i++) {
            QString ck = Cks[i];
            settings.setArrayIndex(i);
            settings.setValue("ck", ck);
        }

        settings.endArray();
    }

    const char *a[2] = {"SgyHdrsAx", "CstHdrsAx"};

    for (n = 0; n < 2; n++) {
        hdrAxisCk.setListIdx(n + 1);
        Cks = hdrAxisCk.List();

        settings.remove(a[n]);
        settings.beginWriteArray(a[n]);

        for (i = 0; i < Cks.size(); i++) {
            QString ck = Cks[i];
            settings.setArrayIndex(i);
            settings.setValue("ck", ck);
        }

        settings.endArray();
    }

    const char *f[2] = {"SgyHdrsDef", "CstHdrsDef"};

    for (n = 0; n < 2; n++) {
        TrHdrDefList *List = TraceHdrsSet.List(n + 1);
        //      int l = List->count();
        int j;

        settings.remove(f[n]);
        settings.beginWriteArray(f[n]);

        for (i = j = 0; i < List->count(); i++) {
            TrHdrDef *hdr = List->item(i);

            if (hdr->edit) {
                settings.setArrayIndex(j++);
                settings.setValue("name", hdr->name);
                settings.setValue("desc", hdr->desc);
                settings.setValue("frmt", hdr->frmt);
                settings.setValue("pos", hdr->pos);
                settings.setValue("expr", hdr->expr.ExprString());
            }
        }

        settings.endArray();
    }

    settings.setValue("Ti", timeAxis.Ti());
    settings.setValue("Tl", seisSct.Tl());
}

void MainWindow::on_zoomAllBtn_pressed()
{
    double ws = seisScrl->width() - 2;
    double wv = allSectWidth();
    double hs = seisScrl->height() - 2;
    double hv = timeAxis.HeightPix();

    if (wv == 0)
        return;
    if (hv == 0)
        return;

    double xs = ws / wv;
    double ys = hs / hv;

    SetZoom(xs, ys, 0, 0);
}

void MainWindow::on_zoomVallBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    double hs = seisScrl->viewport()->height() - 2;
    double hv = timeAxis.HeightPix();

    if (hv == 0)
        return;

    ys = hs / hv;

    SetZoom(xs, ys, xo, 0);
}

void MainWindow::on_zoomHallBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    double ws = seisScrl->viewport()->width() - 2;
    double wv = allSectWidth();

    if (wv == 0)
        return;

    xs = ws / wv;

    SetZoom(xs, ys, 0, yo);
}

void MainWindow::GetZoom(double &xs, double &ys, int &xo, int &yo)
{
    xs = seisView.Xs();
    ys = seisView.Ys();

    xo = seisScrl->horizontalScrollBar()->value();
    yo = seisScrl->verticalScrollBar()->value();
}

void MainWindow::SetZoom(double xs, double ys, int xo, int yo, bool push)
{
    double xsp, ysp;
    int xop, yop;

    GetZoom(xsp, ysp, xop, yop);

    if (xs == 0) {
        xs = xsp;
        xo = xop;
    }

    if (ys == 0) {
        ys = ysp;
        yo = yop;
    }

    if (push)
        zstack.push(zdata(xsp, ysp, xop, yop));

    seisView.setXs(xs);
    seisView.setYs(ys);
    hdrsView.setXs(xs);
    hdrsBottomView.setXs(xs);
    timeView.setYs(ys);
    timeRightView.setYs(ys);

    ArrangeSections();

    seisScrl->setHscroll(xo);
    seisScrl->setVscroll(yo);

    xo = seisScrl->Hscroll();
    yo = seisScrl->Vscroll();

    hdrsScrl->setHscroll(xo);
    hdrsBottomScrl->setHscroll(xo);
    timeScrl->setVscroll(yo);
    timeRightScrl->setVscroll(yo);

    FillEdits();
}

void MainWindow::on_edTr_editingFinished()
{
    double xs, ys;
    int xo, yo;
    bool ok;

    GetZoom(xs, ys, xo, yo);

    QString txt = ui->edTr->text();

    xs = txt.toFloat(&ok);
    if (!ok)
        return;

    xs = 10 / xs;

    SetZoomC(xs, ys, 0, 0);
}

void MainWindow::on_edTm_editingFinished()
{
    double xs, ys;
    int xo, yo;
    bool ok;

    GetZoom(xs, ys, xo, yo);

    QString txt = ui->edTm->text();

    ys = txt.toFloat(&ok);
    if (!ok)
        return;

    SetZoomC(xs, ys, 0, 0);
}

void MainWindow::on_edGn_editingFinished()
{
    bool ok;
    double g;

    g = ui->edGn->text().toFloat(&ok);
    if (!ok)
        return;

    seisSct.setGc(g);
    seisSct.setGw(g);

    FillEdits();
}

void MainWindow::on_zoomPreBtn_pressed()
{
    zdata z;

    if (zstack.isEmpty())
        return;

    z = zstack.pop();

    SetZoom(z.xs, z.ys, z.xo, z.yo, false);
}

void MainWindow::on_zoomOutBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    SetZoomC(xs / 1.5, ys / 1.5, 0, 0);
}

void MainWindow::on_zoomInBtn_pressed()
{
    double xs, ys;
    int xo, yo;

    GetZoom(xs, ys, xo, yo);

    SetZoomC(xs * 1.5, ys * 1.5, 0, 0);
}

void MainWindow::on_selDirBtn_pressed()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Open Directory", curDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dir == "")
        return;
    ChangeDataDir(dir);
}

void MainWindow::on_opFileBtn_pressed()
{
    on_actionOpen_File_triggered();
}


void MainWindow::on_refreshBtn_pressed()
{
    int row = dirGrid.curRow();

    if (row >= 0) {
        QString name = dirGrid.Cell(row, 0);
        int tag = dirGrid.Tag(row, 0);
        //      int       frm  = dirGrid.Tag (row,1);

        if (name == "." || tag == 3) {
            ScanLtod();
            find = name;
        } else {
            ChangeDataDir(curDir, name);
        }
    }
}

void MainWindow::on_procParmBtn_pressed()
{
    ProcParmDlg.f1 = _f1;
    ProcParmDlg.f2 = _f2;
    ProcParmDlg.f3 = _f3;
    ProcParmDlg.f4 = _f4;
    ProcParmDlg.agcw = _agcw;
    ProcParmDlg.agcType = _agcType;
    ProcParmDlg.agc = ui->ckAgc->isChecked();
    ProcParmDlg.norm = ui->ckNorm->isChecked();
    ProcParmDlg.filt = ui->ckFilt->isChecked();

    ProcParmDlg.show();
}

void MainWindow::procParmEvent()
{
    _f1 = ProcParmDlg.f1;
    _f2 = ProcParmDlg.f2;
    _f3 = ProcParmDlg.f3;
    _f4 = ProcParmDlg.f4;
    _agcw = ProcParmDlg.agcw;
    _agcType = ProcParmDlg.agcType;

    ui->ckAgc->setChecked(ProcParmDlg.agc);
    ui->ckNorm->setChecked(ProcParmDlg.norm);
    ui->ckFilt->setChecked(ProcParmDlg.filt);

    ResetSrc();
}

/*

bool  MainWindow::event(QEvent *evt)
{
    if (evt->type() == QEvent::User+1)
    {
        return true;
    }
    else if (evt->type() == QEvent::User+2)
    {
        DirScanEvent* e = (DirScanEvent*)evt;

        if(e->pers>=0)
        {
            ShowProgress("Scanning Directory...",e->pers);
        }
        else if(e->pers == -1)
        {
            FillDirGrid(WorkThread->dlist);
            ShowProgress("",0);
        }
        else if(e->pers == -2)
        {
            ShowProgress("",0);
        }

        return true;
    }
    else if (evt->type() == QEvent::User+3)
    {
        LtdScanEvent* e = (LtdScanEvent*)evt;

        if(e->pers>=0)
        {
            ShowProgress("Scanning LTOD image...",e->pers);
        }
        else if(e->pers == -1)
        {
            FillDirGrid(WorkThread->dlist);
            ShowProgress("",0);
        }
        else if(e->pers == -2)
        {
            ShowProgress("",0);
        }

        return true;
    }
    return QMainWindow::event(evt);
}
*/

void MainWindow::on_zoomWinBtn_pressed()
{
    seisScrl->setZoomW(1);
}

void MainWindow::on_actionOpen_Directory_triggered()
{
    on_selDirBtn_pressed();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDlg.show();
}

void MainWindow::on_TrSlider_valueChanged(int p)
{
    double r;

    if (p > 0) {
        r = 1 + p / 100.;
        SetZoomC(xss * r, yss, 0, 0);
    } else {
        r = 1 - p / 100.;
        SetZoomC(xss / r, yss, 0, 0);
    }
}

void MainWindow::on_TrSlider_sliderReleased()
{
    int xo, yo;
    GetZoom(xss, yss, xo, yo);
    ui->TrSlider->setValue(0);
}

void MainWindow::on_TrSlider_sliderPressed()
{
    int xo, yo;

    GetZoom(xss, yss, xo, yo);
}

void MainWindow::on_TmSlider_sliderPressed()
{
    int xo, yo;

    GetZoom(xss, yss, xo, yo);
}

void MainWindow::on_TmSlider_sliderReleased()
{
    int xo, yo;
    GetZoom(xss, yss, xo, yo);
    ui->TmSlider->setValue(0);
}

void MainWindow::on_TmSlider_valueChanged(int p)
{
    double r;

    if (p > 0) {
        r = 1 + p / 100.;
        SetZoomC(xss, yss * r, 0, 0);
    } else {
        r = 1 - p / 100.;
        SetZoomC(xss, yss / r, 0, 0);
    }
}

void MainWindow::on_GnSlider_sliderPressed()
{
    gss = seisSct.Gw();
}

void MainWindow::on_GnSlider_sliderReleased()
{
    gss = seisSct.Gw();
    ui->GnSlider->setValue(0);
}

void MainWindow::on_GnSlider_valueChanged(int p)
{
    double r;

    if (p > 0) {
        r = 1 + p / 100.;
        seisSct.setGc(gss * r);
        seisSct.setGw(gss * r);
    } else {
        r = 1 - p / 100.;
        seisSct.setGc(gss / r);
        seisSct.setGw(gss / r);
    }
    FillEdits();
}

void MainWindow::on_TxtHdrEdit_cursorPositionChanged()
{
    QTextCursor cursor = ui->TxtHdrEdit->textCursor();

    int col = cursor.columnNumber() + 1;
    int row = cursor.blockNumber() + 1;

    ui->txtCol->setText(QString("Col=%1").arg(col));
    ui->txtRow->setText(QString("Row=%1").arg(row));
}

void MainWindow::on_axisBtn_pressed()
{
    AxisDlg.ckList = &hdrAxisCk;
    AxisDlg.dT = timeAxis.Ti() * 1000;
    AxisDlg.tL = seisSct.Tl();

    AxisDlg.show();
}

void MainWindow::on_captureBtn_pressed()
{
    captureCurrentView(0);
}

void MainWindow::on_exportHiResBtn_pressed()
{
    captureCurrentView(m_exportDpi);
}

void MainWindow::captureCurrentView(int requestedDpi)
{
    SeisFile *file = seisSrc.Sfile();
    if (!file || !file->Active()) {
        showScreenshotToast(
            this, tr("Screenshot unavailable"),
            tr("Open a seismic data file before capturing."),
            ToastPreset::WARNING);
        return;
    }

    const bool exportHighResolution = requestedDpi > 0;
    const qreal captureScaleX =
        exportHighResolution
            ? static_cast<qreal>(requestedDpi) / timeView.dpiX()
            : timeView.devicePixelRatio();
    const qreal captureScaleY =
        exportHighResolution
            ? static_cast<qreal>(requestedDpi) / timeView.dpiY()
            : timeView.devicePixelRatio();
    const int captureDpiX =
        exportHighResolution
            ? requestedDpi
            : qMax(1, qRound(timeView.dpiX() * captureScaleX));
    const int captureDpiY =
        exportHighResolution
            ? requestedDpi
            : qMax(1, qRound(timeView.dpiY() * captureScaleY));
    const QRect captureRect(
        QPoint(0, 0),
        QPoint(timeRightScrl->geometry().right() + 1,
               hdrsBottomScrl->geometry().bottom() + 1));
    const QSize captureSize(
        qMax(1, qRound(captureRect.width() * captureScaleX)),
        qMax(1, qRound(captureRect.height() * captureScaleY)));
    QImage capturedImage(captureSize, QImage::Format_RGB32);
    if (capturedImage.isNull()) {
        showScreenshotToast(
            this, tr("Screenshot failed"),
            tr("Could not capture the visible seismic area."),
            ToastPreset::ERROR_NOTIFICATION);
        return;
    }
    capturedImage.fill(ui->seisFrame->palette().color(QPalette::Window));
    capturedImage.setDotsPerMeterX(
        qRound(captureDpiX * GfxStyle::MillimetersPerMeter /
               GfxStyle::MillimetersPerInch));
    capturedImage.setDotsPerMeterY(
        qRound(captureDpiY * GfxStyle::MillimetersPerMeter /
               GfxStyle::MillimetersPerInch));

    QPainter capturePainter(&capturedImage);
    capturePainter.scale(captureScaleX, captureScaleY);
    QList<GfxView *> captureViews = {
        &timeView, &timeRightView, &hdrsView,
        &hdrsBottomView, &seisView, &hlabView
    };
    if (exportHighResolution) {
        for (GfxView *view : captureViews)
            view->setCaptureScale(captureScaleY);
    }
    ui->seisFrame->render(&capturePainter, -captureRect.topLeft(),
                          QRegion(captureRect));
    capturePainter.end();
    if (exportHighResolution) {
        for (GfxView *view : captureViews) {
            view->setCaptureScale(0);
            view->update();
        }
    }

    const int horizontalOffset = seisScrl->horizontalScrollBar()->value();
    const int verticalOffset = seisScrl->verticalScrollBar()->value();
    const QSize visibleSize = seisScrl->viewport()->size();
    const double firstTrace =
        seisSrc.Tx(seisSct.pix2x(horizontalOffset));
    const double lastTrace =
        seisSrc.Tx(seisSct.pix2x(horizontalOffset + visibleSize.width()));
    const double firstTime = seisSct.pix2y(verticalOffset);
    const double lastTime =
        seisSct.pix2y(verticalOffset + visibleSize.height());

    const QFileInfo fileInfo(curFileName);
    QString captureFileName =
        QString("%1_%2_%3_%4_%5")
            .arg(fileInfo.baseName())
            .arg(qRound(qMin(firstTrace, lastTrace)) + 1)
            .arg(qRound(qMax(firstTrace, lastTrace)) + 1)
            .arg(QString::number(qMin(firstTime, lastTime), 'g', 8))
            .arg(QString::number(qMax(firstTime, lastTime), 'g', 8));

    if (ui->ckWiggle->isChecked()) {
        captureFileName += "_wiggle";
        if (ui->rbNon->isChecked())
            captureFileName += "_non";
        if (ui->rbNeg->isChecked())
            captureFileName += "_neg";
    }
    if (ui->ckGray->isChecked())
        captureFileName += "_gray";
    if (ui->ckColor->isChecked())
        captureFileName += "_color";
    if (ui->ckTimLines->isChecked())
        captureFileName += "_timeline";
    if (ui->ckFilt->isChecked()) {
        captureFileName +=
            QString("_filt_%1_%2_%3_%4")
                .arg(_f1)
                .arg(_f2)
                .arg(_f3)
                .arg(_f4);
    }
    if (ui->ckAgc->isChecked())
        captureFileName += "_agc";
    if (ui->ckNorm->isChecked())
        captureFileName += "_norm";
    if (ui->ckDly->isChecked())
        captureFileName += "_dly";
    if (exportHighResolution)
        captureFileName += QString("_%1").arg(requestedDpi);
    captureFileName += ".png";

    const QDir outputDirectory(fileInfo.absolutePath());
    QString outputPath = outputDirectory.filePath(captureFileName);
    const QFileInfo captureFileInfo(outputPath);
    const QString suffix = captureFileInfo.suffix();
    const QString baseName = captureFileInfo.completeBaseName();
    for (int number = 1; QFile::exists(outputPath); ++number) {
        outputPath = outputDirectory.filePath(
            QString("%1_%2.%3").arg(baseName).arg(number).arg(suffix));
    }

    if (!capturedImage.save(outputPath, "PNG")) {
        showScreenshotToast(
            this, tr("Screenshot failed"),
            tr("Could not save the screenshot to %1").arg(outputPath),
            ToastPreset::ERROR_NOTIFICATION);
        return;
    }

    const QString successTitle =
        exportHighResolution ? tr("Image exported") : tr("Screenshot saved");
    const QString successText =
        exportHighResolution
            ? tr("Image exported to %1")
                  .arg(QDir::toNativeSeparators(outputPath))
            : tr("Screenshot saved to %1")
                  .arg(QDir::toNativeSeparators(outputPath));
    showScreenshotToast(this, successTitle, successText,
                         ToastPreset::SUCCESS);
}

void MainWindow::on_actionAxes_Setup_triggered()
{
    on_axisBtn_pressed();
}

void MainWindow::axesDlgEvent()
{
    hdrsAxis.setHdrList(hdrAxisCk.List());
    hdrsBottomAxis.setHdrList(hdrAxisCk.List());
    hdrsLab.setHdrList(hdrAxisCk.List());

    double ti = AxisDlg.dT / 1000.;
    bool tl = AxisDlg.tL;

    timeAxis.setTi(ti);
    timeRightAxis.setTi(ti);
    seisSct.setTi(ti);
    seisSct.setTl(tl);

    ui->ckTimLines->setChecked(tl);
    ArrangeSections();
}

void MainWindow::FillHdrListGrids()
{
    int n;
    int l;

    TrHdrDefList *List = TraceHdrsSet.List();

    l = List->count();

    hdrListCkGrid.setRowCount(l - 1);
    trcHdrGrid.setRowCount(l - 1);

    trcDatGrid.setRowCount(0);

    for (n = 1; n < l; n++) {
        TrHdrDef *h = List->item(n);

        hdrListCkGrid.setTag(n - 1, 0, h->edit);
        hdrListCkGrid.setCell(n - 1, 2, h->name);
        hdrListCkGrid.setCell(n - 1, 3, h->desc);
        hdrListCkGrid.setCell(n - 1, 1, h->bytesStr());
        hdrListCkGrid.setCheck(n - 1, 0, hdrListCk[h->name]);
        trcHdrGrid.setCell(n - 1, 1, h->bytesStr());
        trcHdrGrid.setCell(n - 1, 2, h->name);
        trcHdrGrid.setCell(n - 1, 3, h->desc);
        trcHdrGrid.setCell(n - 1, 0, "");
    }

    SetHdrDatList();
}

void MainWindow::FillHdrElstGrids()
{
    int n;
    int l;

    TrHdrDefList *List = TraceHdrsSet.List();

    l = List->count();

    hdrElstCkGrid.setRowCount(l - 1);
    trcHdrGrid.setRowCount(l - 1);

    trcDatGrid.setRowCount(0);

    for (n = 1; n < l; n++) {
        TrHdrDef *h = List->item(n);

        hdrElstCkGrid.setTag(n - 1, 0, h->edit);
        hdrElstCkGrid.setCell(n - 1, 2, h->name);
        hdrElstCkGrid.setCell(n - 1, 3, h->desc);
        hdrElstCkGrid.setCell(n - 1, 1, h->bytesStr());
        hdrElstCkGrid.setCheck(n - 1, 0, hdrElstCk[h->name]);
        trcHdrGrid.setCell(n - 1, 1, h->bytesStr());
        trcHdrGrid.setCell(n - 1, 2, h->name);
        trcHdrGrid.setCell(n - 1, 3, h->desc);
        trcHdrGrid.setCell(n - 1, 0, "");
    }

    SetHdrDatElst();
}

void MainWindow::hdreDlgEvent()
{
    FillHdrListGrids();
    FillHdrElstGrids();
}

void MainWindow::on_actionParameters_triggered()
{
    on_procParmBtn_pressed();
}

void MainWindow::on_actionHeader_Editor_triggered()
{
    on_btnEdHdr_clicked();
}

void MainWindow::on_btnEdHdr_clicked()
{
    EdHdrDlg.show();
}

void MainWindow::checkTxtHdrChange()
{
    bool changed = false;

    SeisFile *sf = seisSrc.Sfile();
    QString etxt = ui->TxtHdrEdit->toPlainText();

    if (sf && sf->Active()) {
        if (etxt != sf->TxtHed())
            changed = true;
    }

    if (changed) {
        ui->btnTxtRst->show();
        ui->btnTxtUpd->show();
    } else {
        ui->btnTxtRst->hide();
        ui->btnTxtUpd->hide();
    }
}

void MainWindow::loadTxtHdr()
{
    FILE *inpf;

    int i, n;
    char s[128];

    QString S;

    ui->TxtHdrEdit->clear();

    inpf = fopen_q(txtHdrFile, "r");

    if (inpf == NULL) {
        QMessageBox::critical(this, "Error",
                              "Cannot open text file: " + txtHdrFile,
                              QMessageBox::Discard);
        return;
    }

    for (n = 0; n < 40; n++) {
        if (fgets(s, 80, inpf) == NULL)
            break;

        int l = strlen(s);

        for (i = 0; i < l; i++) {
            if (s[i] < 32)
                s[i] = ' ';
        }

        for (i = l - 1; i >= 0; i++) {
            if (s[i] == ' ')
                s[i] = 0;
            else
                break;
        }

        S = s;
        ui->TxtHdrEdit->insertPlainText(S + "\n");
    }

    //  Modified = CheckChanges();
    fclose(inpf);

    QTextCursor tmpCursor = ui->TxtHdrEdit->textCursor();
    tmpCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor, 1);
    ui->TxtHdrEdit->setTextCursor(tmpCursor);
    on_TxtHdrEdit_cursorPositionChanged();
}

void MainWindow::exportTxtHdr(QString fileName)
{
    SeisFile *sf = seisSrc.Sfile();
    if (!sf || !sf->Active())
        return;

    QFile file(fileName);
    bool ok = file.open(QIODevice::ReadWrite);

    if (!ok) {
        QMessageBox::critical(this, "Error",
                              "Cannot create text file: " + fileName,
                              QMessageBox::Discard);
        return;
    }

    QTextStream stream(&file);
    stream << sf->TxtHed();
    file.close();

    //  Modified = CheckChanges();
}

void MainWindow::on_btnTxtRd_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Load Text Header from File", txtHdrDir,
        "Text File (*.txt);;All Files (*)");
    if (fileName == "") {
        return;
    }

    QFileInfo fi(fileName);
    txtHdrDir = fi.dir().absolutePath();
    txtHdrFile = fi.absoluteFilePath();

    loadTxtHdr();
}

void MainWindow::on_actionLoad_Text_Header_from_File_triggered()
{
    on_btnTxtRd_clicked();
}

void MainWindow::on_actionExport_Text_Header_to_File_triggered()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Text Header from File", txtHdrDir, "Text File (*.txt)");
    if (fileName == "") {
        return;
    }

    QFileInfo fi(fileName);
    txtHdrDir = fi.dir().absolutePath();

    exportTxtHdr(fileName);
}

void MainWindow::on_btnTxtRdx_clicked()
{
    if (txtHdrFile != "") {
        loadTxtHdr();
    }
}

void MainWindow::on_btnTxtRst_clicked()
{
    SeisFile *sf = seisSrc.Sfile();
    ui->TxtHdrEdit->clear();
    ui->TxtHdrEdit->insertPlainText(sf->TxtHed());
    ui->TxtHdrEdit->document()->clearUndoRedoStacks();
    checkTxtHdrChange();
}

void MainWindow::on_btnTxtUpd_clicked()
{
    SeisFile *sf = seisSrc.Sfile();

    if (!sf->UpdateTxtHed(ui->TxtHdrEdit->toPlainText())) {
        QMessageBox::critical(this, "Error", "Cannot Update Text Hdr",
                              QMessageBox::Discard);
    }
    checkTxtHdrChange();
}

void MainWindow::on_ckTrEd_toggled(bool checked)
{
    Q_UNUSED(checked);
    FillBinHdrGrid();
}

void MainWindow::binHdrGridChangedEvent()
{
    if (busy)
        return;

    checkBinHdrChange();
}

void MainWindow::on_btnBinRst_clicked()
{
    FillBinHdrGrid();
    checkBinHdrChange();
}

void MainWindow::on_btnBinUpd_clicked()
{
    int n;
    QVector<QString> vals;
    int nr = binHdrGrid.RowCount();
    SeisFile *sf = seisSrc.Sfile();

    for (n = 0; n < nr; n++) {
        QString s = binHdrGrid.Cell(n, 0);
        vals.append(s);
    }

    sf->UpdateBinHed(vals);
    checkBinHdrChange();
}

void MainWindow::on_InfoTab_currentChanged(int index)
{
    Q_UNUSED(index);
    // ui->InfoTab;
}

void MainWindow::on_btnLastTr_clicked()
{
    hdrListDtGrid.setCurCell(hdrListDtGrid.rowCount() - 1);
}

void MainWindow::on_btnFirstTr_clicked()
{
    hdrListDtGrid.setCurCell(0);
}

void MainWindow::SaveSval()
{
    QString sval = ui->cbSval->currentText();

    if (ui->cbSval->findText(sval) < 0) {
        ui->cbSval->addItem(sval);
    }
}

void MainWindow::FindTrace(QString dir)
{
    QString sidx = ui->cbSidx->currentText();
    QString sval = ui->cbSval->currentText();
    QString sign = ui->cbSsign->currentText();

    bool ok;
    double val = sval.toDouble(&ok);
    if (!ok)
        return;

    SaveSval();

    if (sidx == "Trace" && val > 0) {
        hdrListDtGrid.setCurCell(val - 1);
    } else {
        SeisFile *sf = seisSrc.Sfile();
        int cidx = hdrListDtGrid.curRow();

        emit stop_find();

        SetSearchControls(false);

        QThread *thread = new QThread;

        TraceFindWorker *worker =
            new TraceFindWorker(sf, cidx, sidx, val, dir, sign);

        worker->moveToThread(thread);
        connect(thread, SIGNAL(started()), worker, SLOT(process()));
        connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
        connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
        connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
        connect(worker, SIGNAL(efind(int)), this, SLOT(x_find(int)));
        connect(worker, SIGNAL(eprogr(int, QString)), this,
                SLOT(x_progr(int, QString)));
        connect(worker, SIGNAL(efin(QString)), this, SLOT(x_find_fin(QString)));

        connect(this, SIGNAL(stop_find()), worker, SLOT(stop()),
                Qt::DirectConnection);

        thread->start();
    }
}

/*
void MainWindow::x_pers (int pers)
{
//    ShowProgress("",pers);
//    progressBar->setValue(pers);
//    int x = pers;
//    m_statMess->setText(QString::number(pers));
}
*/
void MainWindow::x_find(int tridx)
{
    hdrListDtGrid.setCurCell(tridx);
    ShowProgress("", -1);
}

void MainWindow::x_find_fin(QString mess)
{
    SetSearchControls(true);
    ShowProgress(mess, -1);
}

void MainWindow::SetTrEdCurVal()
{
    qint64 n = hdrListDtGrid.curRow();
    qint64 c = hdrListDtGrid.curCol();
    QString lab = ui->lbElab->text();

    SeisFile *sf = seisSrc.Sfile();

    if (lab == "Trace") {
        ui->cbEnval->setEnabled(false);
        ui->btnUpdTrh->setEnabled(false);
    } else {
        ui->cbEnval->setEnabled(true);
        //    ui->btnUpdTrh->setEnabled(true);
    }

    if (n < 0 || !sf || !sf->Active() || n >= sf->Nt()) {
        ui->edEcval->setText("");
    } else {
        bool ok;
        double vn = ui->cbEnval->currentText().toDouble(&ok);

        double vc = sf->Rh(n, lab);
        ui->edEcval->setText(QString::number(vc));

        if (ok && vc != vn) {
            ui->btnUpdTrh->setEnabled(true);
        } else {
            ui->btnUpdTrh->setEnabled(false);
        }
    }
}

void MainWindow::SetSearchControls(bool on)
{
    QString lab = ui->cbSidx->currentText();

    ui->lbElab->setText(lab);
    SetTrEdCurVal();

    if (on) {
        if (lab == "Trace") {
            ui->btnSfwd->setEnabled(false);
            ui->btnSbkw->setEnabled(false);
            ui->btnSbin->setEnabled(true);
        } else {
            ui->btnSfwd->setEnabled(true);
            ui->btnSbkw->setEnabled(true);
            ui->btnSbin->setEnabled(true);
        }
        ui->btnSstop->setEnabled(false);
    } else {
        ui->btnSfwd->setEnabled(false);
        ui->btnSbkw->setEnabled(false);
        ui->btnSbin->setEnabled(false);
        ui->btnSstop->setEnabled(true);
    }
}

void MainWindow::on_cbSval_editTextChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    QString txt = ui->cbSval->currentText();
}

void MainWindow::on_cbSidx_currentIndexChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    SetSearchControls(true);
}

void MainWindow::on_btnSbin_clicked()
{
    FindTrace("=");
}

void MainWindow::on_btnSfwd_clicked()
{
    FindTrace(">");
}

void MainWindow::on_btnSbkw_clicked()
{
    FindTrace("<");
}

void MainWindow::on_btnSstop_clicked()
{
    emit stop_find();
}

void MainWindow::on_btnUpdTrh_clicked()
{
    bool ok;
    double vn = ui->cbEnval->currentText().toDouble(&ok);
    if (!ok)
        return;

    long long n = hdrListDtGrid.curRow();
    QString lab = ui->lbElab->text();

    SeisFile *sf = seisSrc.Sfile();

    if (n < 0 || !sf || !sf->Active() || n >= sf->Nt())
        return;

    if (sf->Wh(n, lab, vn)) {
        SetTrEdCurVal();
        sf->Reset();
        SetHdrDatList();
        SelectTrace(n);

        QString enval = ui->cbEnval->currentText();

        if (ui->cbEnval->findText(enval) < 0) {
            ui->cbEnval->addItem(enval);
        }
    }
}

void MainWindow::on_cbEnval_editTextChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    SetTrEdCurVal();
}

void MainWindow::on_btnCkAll_clicked()
{
    hdrListCkGrid.setColCheck(0, 2);
    SetHdrDatList();
}

void MainWindow::on_btnCkNon_clicked()
{
    hdrListCkGrid.setColCheck(0, 0);
    SetHdrDatList();
}

void MainWindow::on_btnCkNonE_clicked()
{
    hdrElstCkGrid.setColCheck(0, 0);
    SetHdrDatElst();
}

void MainWindow::on_edExpr_textChanged(const QString &arg1)
{
    if (busy)
        return;

    if (eExpIdx < 2)
        return;

    hdrElstDtGrid.setColLabel(eExpIdx, arg1);
    QString name = hdrElstDtGrid.colLabel(eExpIdx - 1);

    SeisTrExpr expr(arg1);

    QPalette *palette = new QPalette();

    if (expr.Error())
        palette->setColor(QPalette::Text, Qt::red);
    else
        palette->setColor(QPalette::Text, Qt::black);

    ui->edExpr->setPalette(*palette);

    exprList[name] = expr;

    hdrElstDtGrid.update();
}

void MainWindow::hdrListDtGridHeaderEvent(int col)
{
    Q_UNUSED(col);
    /*
        QString lab = hdrListDtGrid.ColLabel(col);
        int i = ui->cbSidx->findText(lab);
        ui->cbSidx->setCurrentIndex(i);
    */
}

void MainWindow::hdrElstDtGridHeaderEvent(int col)
{
    if (col < 1)
        return;

    busy = true;

    hdrElstDtGrid.setColHdrBgc(-1); // reset all

    int n = (col - 1) / 2;
    n = n * 2 + 1;

    //  hdrElstDtGrid.setColLabBg(n,   Qt::cyan);
    //  hdrElstDtGrid.setColLabBg(n+1, Qt::cyan);

    hdrElstDtGrid.setColHdrBgc(n, Qt::cyan);
    hdrElstDtGrid.setColHdrBgc(n + 1, Qt::cyan);

    ui->edExpr->setEnabled(true);
    ui->edExpr->setText(hdrElstDtGrid.colLabel(n + 1));
    eExpIdx = n + 1;

    busy = false;
    //  qDebug()<< "n=" << n;
}

void MainWindow::on_btnNexp_clicked()
{
    if (ui->edExpr->isEnabled()) {
        ui->edExpr->setText(ui->edExpr->text() + "N");
    }
}

void MainWindow::on_btnLexp_clicked()
{
    if (ui->edExpr->isEnabled()) {
        ui->edExpr->setText(ui->edExpr->text() + "L");
    }
}

void MainWindow::on_btnHexp_clicked()
{
    if (!ui->edExpr->isEnabled())
        return;

    SeisFile *sf = seisSrc.Sfile();
    if (!sf)
        return;

    int r = hdrElstCkGrid.curRow();

    TrHdrDef *h = sf->THdef(r + 1);

    QString s;

    s.sprintf("H(%d,%d)", h->pos, h->frmt);

    ui->edExpr->setText(ui->edExpr->text() + s);
}

void MainWindow::on_btnClrExp_clicked()
{
    if (ui->edExpr->isEnabled()) {
        ui->edExpr->setText("");
    }
}

void MainWindow::setEnabledAllBtns(QWidget *box, bool enabled)
{
    QList<QWidget *> widgets = box->findChildren<QWidget *>();

    foreach (QWidget *widget, widgets) {
        QString c = widget->metaObject()->className();

        if (c == "QPushButton" || c == "QToolButton") {
            widget->setEnabled(enabled);
        }
    }
}

void MainWindow::ClrUndoE()
{
    unlink_q(undofn);
    ui->btnUndE->setEnabled(false);
}

void MainWindow::on_btnUpdE_clicked()
{
    if (expu_running) {
        emit stop_expu();
        return;
    }

    SeisFile *sf = seisSrc.Sfile();

    if (!sf || !sf->Active())
        return;

    ui->btnUpdE->setIcon(QIcon(":/images/stop.png"));

    expu_running = true;

    QThread *thread = new QThread;

    ChangeThExprWorker *worker =
        new ChangeThExprWorker(sf, exprList, undofn, 0);

    worker->moveToThread(thread);
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(eprogr(int, QString)), this,
            SLOT(x_progr(int, QString)));
    connect(worker, SIGNAL(efin(QString)), this, SLOT(x_fin_e(QString)));

    connect(this, SIGNAL(stop_expu()), worker, SLOT(stop()),
            Qt::DirectConnection);

    thread->start();

    ui->HdrBox->setEnabled(false);
    ui->dirGroup->setEnabled(false);
    ui->SeisTab->setTabEnabled(0, false);
    ui->hdrsLstTab->setTabEnabled(0, false);
    ui->HeBox->setEnabled(false);
    ui->edExpr->setEnabled(false);
    hdrElstDtGrid.setEnabled(false);
    setEnabledAllBtns(ui->EcBox, false);
    ui->btnUpdE->setEnabled(true);
}

void MainWindow::x_fin_e(QString mess)
{
    expu_running = false;
    ui->btnUpdE->setIcon(QIcon(":/images/FileSave.png"));
    ShowProgress(mess, -1);

    ui->HdrBox->setEnabled(true);
    ui->dirGroup->setEnabled(true);
    ui->SeisTab->setTabEnabled(0, true);
    ui->hdrsLstTab->setTabEnabled(0, true);
    ui->HeBox->setEnabled(true);
    ui->edExpr->setEnabled(true);
    hdrElstDtGrid.setEnabled(true);
    setEnabledAllBtns(ui->EcBox, true);

    if (mess == "Undone" || mess == "Undo error") {
        ClrUndoE();
    } else {
        ui->btnUndE->setEnabled(true);
        undoList = exprList;
    }
}

void MainWindow::on_btnUndE_clicked()
{
    SeisFile *sf = seisSrc.Sfile();

    if (!sf || !sf->Active())
        return;

    QThread *thread = new QThread;

    ChangeThExprWorker *worker =
        new ChangeThExprWorker(sf, undoList, undofn, 1);

    worker->moveToThread(thread);
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(eprogr(int, QString)), this,
            SLOT(x_progr(int, QString)));
    connect(worker, SIGNAL(efin(QString)), this, SLOT(x_fin_e(QString)));

    connect(this, SIGNAL(stop_expu()), worker, SLOT(stop()),
            Qt::DirectConnection);

    thread->start();

    ui->HdrBox->setEnabled(false);
    ui->dirGroup->setEnabled(false);
    ui->SeisTab->setTabEnabled(0, false);
    ui->hdrsLstTab->setTabEnabled(0, false);
    ui->HeBox->setEnabled(false);
    ui->edExpr->setEnabled(false);
    hdrElstDtGrid.setEnabled(false);
    setEnabledAllBtns(ui->EcBox, false);
}

void MainWindow::on_actionSave_As_triggered()
{
    SeisFile *sf = seisSrc.Sfile();

    if (!sf || !sf->Active())
        return;

    SaveAsDlg.sf = sf;
    SaveAsDlg.savDir = &savDir;
    SaveAsDlg.curDir = QDir(curDir).absolutePath();
    SaveAsDlg.show();
}

void MainWindow::on_actionDifference_triggered()
{
    SeisFile *sf = seisSrc.Sfile();

    if (!sf || !sf->Active())
        return;

    DiffDlg.sf = sf;
    DiffDlg.savDir = &savDir;
    DiffDlg.curDir = QDir(curDir).absolutePath();
    DiffDlg.show();
}

void MainWindow::on_btnLastTr_2_clicked()
{
    hdrElstDtGrid.setCurCell(hdrListDtGrid.rowCount() - 1);
}

void MainWindow::on_btnFirstTr_2_clicked()
{
    hdrElstDtGrid.setCurCell(0);
}

void MainWindow::on_autoGainBtn_clicked()
{
    seisSct.selTr();

    if (ui->ckNorm->isChecked() || ui->ckAgc->isChecked()) {
        seisSct.setGc(2);
        seisSct.setGw(2);
    } else {
        if (seisSct.selTr() >= 0) {
            float a = seisSct.getSelMaxAmp();
            if (a != 0) {
                double g = 2 / a;
                seisSct.setGc(g);
                seisSct.setGw(g);
            }
        }
    }

    FillEdits();
}

void MainWindow::on_rbDirNorm_toggled(bool checked)
{
    if (checked) {
        SeisFile *sf = seisSrc.Sfile();

        if (sf == NULL)
            return;

        seisSct.setX1(-1);
        seisSct.setX2(sf->Nt());
        hdrsAxis.setX1(-1);
        hdrsAxis.setX2(sf->Nt());
        hdrsBottomAxis.setX1(-1);
        hdrsBottomAxis.setX2(sf->Nt());
    }
}

void MainWindow::on_rbDirRev_toggled(bool checked)
{
    if (checked) {
        SeisFile *sf = seisSrc.Sfile();

        if (sf == NULL)
            return;

        seisSct.setX1(sf->Nt());
        seisSct.setX2(-1);
        hdrsAxis.setX1(sf->Nt());
        hdrsAxis.setX2(-1);
        hdrsBottomAxis.setX1(sf->Nt());
        hdrsBottomAxis.setX2(-1);
    }
}

void MainWindow::on_ckTimLines_toggled(bool checked)
{
    seisSct.setTl(checked);
}

void MainWindow::on_actionOpen_File_triggered()
{
    QString fileName =
        QFileDialog::getOpenFileName(this, "Load Text Header from File", curDir,
                                     "Any file (*);;"
                                     "SEG-Y file (*.sgy);;"
                                     "SEG-Y file (*.segy);;"
                                     "CST file (*cst)");
    if (fileName == "") {
        return;
    }

    QFileInfo fi(fileName);

    QByteArray qfn = fi.filePath().toLocal8Bit();
    const char *fname = qfn.data();

    setWindowTitle("");

    int frm = 2;
    int cgg, junk, swap;

    if (is_cst_f(fname)) {
        frm = 3;
    } else if (is_su_f(fname, swap)) {
        frm = 2;
    } else if (is_segy_f(fname, swap, cgg, junk)) {
        frm = 1;
    } else {
        QMessageBox::critical(
            this, "Error", "Unknown format for seismic file: " + QString(fname),
            QMessageBox::Discard);
        return;
    }

    curFileName = fname;

    OpenSeisFile(frm);

    ChangeDataDir(fi.dir().absolutePath(), fi.fileName());
}

void MainWindow::on_ckDly_toggled(bool checked)
{
    Q_UNUSED(checked);

    SeisFile *sf = seisSrc.Sfile();
    if (!sf)
        return;

    _dly_min = 0;
    _dly_max = 0;

    if (ui->ckDly->isChecked() && sf->ThIdx("DELRECT") >= 0) {
        ScanDelay();
        return;
    }

    InitSeisView();
}
