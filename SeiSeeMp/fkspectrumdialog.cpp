#include "fkspectrumdialog.h"
#include "customcontrols.h"
#include "fkcolormaps.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>

class FkPlot : public QWidget {
public:
    FkSpectrum spectrum;
    QString caption;
    double flo=0,fhi=1,klo=-1,khi=1,clo=-12,chi=0;
    int color=0;
    bool logarithmic=true;
    bool smooth=true;
    explicit FkPlot(QWidget* parent):QWidget(parent) {
        setObjectName("fkPlot"); setMinimumSize(480,280);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    }
    double value(int f,int k) const {
        double a=spectrum.amplitude[f*spectrum.nk+k];
        return logarithmic ? std::log10(a+1e-12) : a;
    }
    QRgb rgb(double v) const {
        int index=qBound(0,int(255*qBound(0.0,(v-clo)/(chi-clo),1.0)),255);
        return fkColorMaps[color][index];
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.fillRect(rect(),Qt::white);
        QRectF box(78,35,width()-230,height()-108);
        p.setPen(Qt::black);
        p.drawText(QRectF(78,4,box.width(),25),Qt::AlignCenter,
                   p.fontMetrics().elidedText(caption.isEmpty()?QString("F-K spectrum"):caption,Qt::ElideMiddle,int(box.width())));
        if(spectrum.amplitude.isEmpty()) p.drawText(box,Qt::AlignCenter,"Select an input range and click Calculate");
        else {
            double ratio=p.device()->devicePixelRatioF();
            QImage image(qMax(1,int(box.width()*ratio)),qMax(1,int(box.height()*ratio)),QImage::Format_RGB32);
            image.setDevicePixelRatio(ratio);
            for(int y=0;y<image.height();++y) {
                double hz=fhi-(fhi-flo)*(y+.5)/image.height();
                double fi=qBound(0.0,hz/spectrum.df,double(spectrum.nf-1));
                int f0=int(fi),f1=qMin(f0+1,spectrum.nf-1);
                double fy=fi-f0;
                QRgb* row=reinterpret_cast<QRgb*>(image.scanLine(y));
                for(int x=0;x<image.width();++x) {
                    double k=klo+(khi-klo)*(x+.5)/image.width();
                    double ki=qBound(0.0,(k-spectrum.k0)/spectrum.dk,double(spectrum.nk-1));
                    if(smooth) {
                        int k0=int(ki),k1=qMin(k0+1,spectrum.nk-1);double fx=ki-k0;
                        double lower=value(f0,k0)*(1-fx)+value(f0,k1)*fx;
                        double upper=value(f1,k0)*(1-fx)+value(f1,k1)*fx;
                        row[x]=rgb(lower*(1-fy)+upper*fy);
                    } else row[x]=rgb(value(int(std::round(fi)),int(std::round(ki))));
                }
            }
            p.drawImage(box,image);
        }
        p.drawRect(box);
        for(int i=0;i<=4;++i) {
            double fraction=i/4.0,x=box.left()+fraction*box.width(),y=box.bottom()-fraction*box.height();
            p.drawLine(QPointF(x,box.bottom()),QPointF(x,box.bottom()+5));
            p.drawLine(QPointF(box.left()-5,y),QPointF(box.left(),y));
            p.drawText(QRectF(x-44,box.bottom()+7,88,22),Qt::AlignCenter,QString::number(klo+fraction*(khi-klo),'g',4));
            p.drawText(QRectF(2,y-12,67,24),Qt::AlignRight|Qt::AlignVCenter,QString::number(flo+fraction*(fhi-flo),'g',4));
        }
        p.drawText(QRectF(box.left(),height()-35,box.width(),25),Qt::AlignCenter,"Wavenumber (cycles/m)");
        p.save(); p.translate(16,box.center().y()); p.rotate(-90);
        p.drawText(QRectF(-90,-12,180,24),Qt::AlignCenter,"Frequency (Hz)"); p.restore();
        QRectF bar(box.right()+28,box.top(),20,box.height());
        QLinearGradient gradient(bar.bottomLeft(),bar.topLeft());
        for(int i=0;i<256;++i) gradient.setColorAt(i/255.0,QColor::fromRgb(fkColorMaps[color][i]));
        p.fillRect(bar,gradient); p.drawRect(bar);
        for(int i=0;i<=4;++i) {
            double y=bar.bottom()-i*bar.height()/4;
            p.drawText(QRectF(bar.right()+5,y-12,83,24),Qt::AlignLeft|Qt::AlignVCenter,QString::number(clo+(chi-clo)*i/4,'g',4));
        }
        p.save();p.translate(width()-17,bar.center().y());p.rotate(-90);
        p.drawText(QRectF(-bar.height()/2,-12,bar.height(),24),Qt::AlignCenter,
                   logarithmic ? "log10(amplitude + 1e-12)" : "FFT amplitude");p.restore();
    }
};

FkSpectrumDialog::FkSpectrumDialog(SeisFile* file,const QString& dir,QWidget* parent)
    :QDialog(parent),path(QString::fromLocal8Bit(file->Fname().c_str())),directory(dir),
      fileType(file->typeIdx()),dt(file->Si())
{
    qRegisterMetaType<FkSpectrum>("FkSpectrum");
    setObjectName("FkSpectrumDialog"); setWindowTitle(tr("F-K Spectrum"));
    setWindowModality(Qt::NonModal);setModal(false); setSizeGripEnabled(true); resize(1080,900);
    auto layout=new QVBoxLayout(this);
    auto source=new QLineEdit(path);source->setReadOnly(true);source->setObjectName("fkFile");
    source->setStyleSheet("QLineEdit { background: palette(alternate-base); }");
    layout->addWidget(source);
    inputGroup=new QGroupBox(tr("Input range (raw samples; consecutive, equally spaced traces)"));
    auto grid=new QGridLayout(inputGroup);
    auto integer=[&](const char* name,int max,int value) {
        auto s=new QSpinBox; s->setObjectName(name);s->setRange(1,max);s->setValue(value);return s;
    };
    auto real=[&](const char* name,double lo,double hi,double value,int decimals=6) {
        auto s=new QDoubleSpinBox;s->setObjectName(name);s->setDecimals(decimals);s->setRange(lo,hi);s->setValue(value);
        s->setKeyboardTracking(false);s->setMinimumWidth(115);return s;
    };
    first=integer("fkFirst",file->Nt(),1);last=integer("fkLast",file->Nt(),qMin(file->Nt(),256));
    start=real("fkStart",0,(file->Ns()-1)*dt*1000,0,3);
    end=real("fkEnd",0,(file->Ns()-1)*dt*1000,(file->Ns()-1)*dt*1000,3);
    spacing=real("fkSpacing",.000001,1000000,12.5);
    preprocess=new CustomCheckBox;preprocess->setText(tr("Remove trace/sample means + 2-D Hann"));
    preprocess->setObjectName("fkPreprocess");preprocess->setChecked(true);
    grid->addWidget(new QLabel(tr("First trace (1-based)")),0,0);grid->addWidget(first,0,1);
    grid->addWidget(new QLabel(tr("Last trace (inclusive)")),0,2);grid->addWidget(last,0,3);
    grid->addWidget(new QLabel(tr("Start time (ms)")),1,0);grid->addWidget(start,1,1);
    grid->addWidget(new QLabel(tr("End time (ms)")),1,2);grid->addWidget(end,1,3);
    grid->addWidget(new QLabel(tr("Trace spacing dx (m)")),2,0);grid->addWidget(spacing,2,1);
    grid->addWidget(preprocess,2,2,1,2);layout->addWidget(inputGroup);
    auto display=new QGroupBox(tr("Display range (Apply updates the plot without recalculating FFT)"));
    auto dg=new QGridLayout(display);
    fmin=real("fkFmin",0,.5/dt,0);fmax=real("fkFmax",0,.5/dt,.5/dt);
    kmin=real("fkKmin",-1000000,1000000,-.04,9);kmax=real("fkKmax",-1000000,1000000,.04,9);
    cmin=real("fkCmin",-1e30,1e30,-12);cmax=real("fkCmax",-1e30,1e30,0);
    palette=new QComboBox;palette->setObjectName("fkPalette");palette->addItems({"Jet","Viridis","Inferno","Gray","RdBu"});
    scale=new QComboBox;scale->setObjectName("fkScale");scale->addItems({"log10(amplitude + 1e-12)","Linear amplitude"});
    dg->addWidget(new QLabel(tr("Frequency min / max (Hz)")),0,0);dg->addWidget(fmin,0,1);dg->addWidget(fmax,0,2);
    dg->addWidget(new QLabel(tr("Wavenumber min / max (cycles/m)")),1,0);dg->addWidget(kmin,1,1);dg->addWidget(kmax,1,2);
    dg->addWidget(new QLabel(tr("Color min / max (selected scale)")),2,0);dg->addWidget(cmin,2,1);dg->addWidget(cmax,2,2);
    dg->addWidget(palette,0,3);dg->addWidget(scale,1,3);
    auto apply=new QPushButton(tr("Apply range"));apply->setObjectName("fkApply");
    auto autoColor=new QPushButton(tr("Auto color (P1 / P99.5)"));autoColor->setObjectName("fkAutoColor");
    dg->addWidget(apply,2,3);dg->addWidget(autoColor,3,3);layout->addWidget(display);
    auto sizeRow=new QHBoxLayout;
    interpolation=new QComboBox;interpolation->setObjectName("fkInterpolation");
    interpolation->addItems({tr("Smooth (bilinear)"),tr("Original bins (nearest)")});
    imageWidth=integer("fkImageWidth",4096,1000);imageWidth->setMinimum(640);imageWidth->setKeyboardTracking(true);
    imageHeight=integer("fkImageHeight",4096,360);imageHeight->setMinimum(280);imageHeight->setKeyboardTracking(true);
    imageWidth->setToolTip(tr("640 to 4096 pixels. Valid keyboard input takes effect immediately."));
    imageHeight->setToolTip(tr("280 to 4096 pixels. Valid keyboard input takes effect immediately."));
    imageWidth->setSingleStep(50);imageHeight->setSingleStep(50);
    QSettings settings("WW","SeiSeeMp");
    bool widthOk=false,heightOk=false;
    int savedWidth=settings.value("FkSpectrum/imageWidth",1000).toInt(&widthOk);
    int savedHeight=settings.value("FkSpectrum/imageHeight",360).toInt(&heightOk);
    imageWidth->setValue(widthOk?savedWidth:1000);
    imageHeight->setValue(heightOk?savedHeight:360);
    sizeRow->addWidget(new QLabel(tr("Interpolation")));sizeRow->addWidget(interpolation);
    sizeRow->addWidget(new QLabel(tr("Image width (px)")));sizeRow->addWidget(imageWidth);
    sizeRow->addWidget(new QLabel(tr("Height (px)")));sizeRow->addWidget(imageHeight);
    auto applySize=new QPushButton(tr("Apply size"));applySize->setObjectName("fkApplySize");sizeRow->addWidget(applySize);
    layout->addLayout(sizeRow);
    plot=new FkPlot(this);plot->setFixedSize(imageWidth->value(),imageHeight->value());
    auto viewport=new QScrollArea;viewport->setObjectName("fkViewport");viewport->setWidget(plot);
    viewport->setWidgetResizable(false);viewport->setAlignment(Qt::AlignCenter);
    viewport->setMinimumHeight(240);layout->addWidget(viewport,1);
    auto resizeImage=[this](int){plot->setFixedSize(imageWidth->value(),imageHeight->value());saveImageSize();};
    connect(imageWidth,QOverload<int>::of(&QSpinBox::valueChanged),this,resizeImage);
    connect(imageHeight,QOverload<int>::of(&QSpinBox::valueChanged),this,resizeImage);
    connect(applySize,&QPushButton::clicked,this,[this,resizeImage](){imageWidth->interpretText();imageHeight->interpretText();resizeImage(0);});
    connect(interpolation,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int i){plot->smooth=i==0;plot->update();});
    auto note=new QLabel(tr("Reference: fk_spectrum_fixed.py (Python mode). No FFT padding; positive frequencies; signed k. "
        "Time starts at sample 0. Enter the actual uniform dx; mixed gathers or irregular spacing give misleading k axes."));
    note->setWordWrap(true);layout->addWidget(note);
    status=new QLabel(tr("Ready. Confirm dx and select a range (up to 8 million samples)."));
    status->setWordWrap(true);status->setObjectName("fkStatus");layout->addWidget(status);
    progress=new QProgressBar;progress->setValue(0);layout->addWidget(progress);
    auto buttons=new QHBoxLayout;
    calculate=new QPushButton(tr("Calculate"));calculate->setObjectName("fkCalculate");
    save=new QPushButton(tr("Save PNG"));save->setObjectName("fkSave");save->setEnabled(false);
    auto close=new QPushButton(tr("Close"));
    buttons->addWidget(calculate);buttons->addWidget(save);buttons->addStretch();buttons->addWidget(close);layout->addLayout(buttons);
    connect(calculate,&QPushButton::clicked,this,[this](){
        if(worker){worker->requestInterruption();calculate->setEnabled(false);status->setText(tr("Cancelling..."));}
        else begin();
    });
    connect(close,&QPushButton::clicked,this,&QDialog::close);
    connect(save,&QPushButton::clicked,this,&FkSpectrumDialog::saveImage);
    connect(apply,&QPushButton::clicked,this,[this](){applyDisplay(false);});
    connect(autoColor,&QPushButton::clicked,this,[this](){applyDisplay(true);});
    connect(palette,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int i){plot->color=i;plot->update();});
    connect(scale,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](){applyDisplay(false);});
    for(auto s:{fmin,fmax})connect(s,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double){frequencyEdited=true;});
    for(auto s:{kmin,kmax})connect(s,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double){wavenumberEdited=true;});
    for(auto s:{cmin,cmax})connect(s,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double){colorsEdited=true;});
    auto dirty=[this](){save->setEnabled(false);status->setText(tr("Input changed. Click Calculate to update the spectrum."));};
    for(auto s:{first,last})connect(s,QOverload<int>::of(&QSpinBox::valueChanged),this,dirty);
    for(auto s:{start,end,spacing})connect(s,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,dirty);
    connect(preprocess,&QCheckBox::toggled,this,dirty);
    // Enter in a size/path editor must not implicitly start a calculation.
    for(auto button:findChildren<QPushButton*>())button->setAutoDefault(false);
}

void FkSpectrumDialog::setInitialRange(int firstTrace,int lastTrace,double startMs,double endMs)
{
    first->setValue(firstTrace);last->setValue(lastTrace);
    start->setValue(startMs);end->setValue(endMs);
    status->setText(tr("Range captured from the main window. Confirm dx and click Calculate."));
}

void FkSpectrumDialog::begin()
{
    int a=int(std::ceil(start->value()/(dt*1000)-1e-7));
    int b=int(std::floor(end->value()/(dt*1000)+1e-7));
    if(first->value()>=last->value() || b-a+1<4 || (preprocess->isChecked() && last->value()-first->value()+1<3) ||
       qint64(last->value()-first->value()+1)*(b-a+1)>8000000) {
        status->setText(tr("Choose an ordered range with 2+ traces (3+ with Hann), 4+ time samples and at most 8 million cells."));return;
    }
    worker=new FkWorker(this);worker->path=path;worker->fileType=fileType;
    worker->first=first->value()-1;worker->last=last->value()-1;
    worker->sampleFirst=a;worker->sampleLast=b;worker->dx=spacing->value();worker->preprocess=preprocess->isChecked();
    inputGroup->setEnabled(false);calculate->setText(tr("Cancel"));save->setEnabled(false);progress->setValue(0);
    plot->spectrum=FkSpectrum();plot->update();status->setText(tr("Reading raw traces and computing F-K spectrum..."));
    connect(worker,&FkWorker::progress,progress,&QProgressBar::setValue);
    connect(worker,&FkWorker::spectrumReady,this,&FkSpectrumDialog::receive);
    connect(worker,&FkWorker::failed,status,&QLabel::setText);
    connect(worker,&QThread::finished,this,[this](){
        bool cancelled=worker->isInterruptionRequested();worker->deleteLater();worker=nullptr;
        inputGroup->setEnabled(true);calculate->setText(tr("Calculate"));calculate->setEnabled(true);
        if(cancelled)status->setText(tr("Cancelled; incomplete spectrum discarded."));
    });
    worker->start();
}

void FkSpectrumDialog::receive(const FkSpectrum& result)
{
    plot->spectrum=result;
    plot->caption=tr("F-K spectrum | %1 | traces %2-%3 | %4-%5 ms | dx=%6 m")
        .arg(QFileInfo(path).fileName()).arg(first->value()).arg(last->value())
        .arg(start->value(),0,'g',6).arg(end->value(),0,'g',6).arg(spacing->value(),0,'g',6);
    const bool initialize=!haveDisplayRange;
    const QSignalBlocker bf0(fmin),bf1(fmax),bk0(kmin),bk1(kmax),bc0(cmin),bc1(cmax);
    fmin->setRange(0,(result.nf-1)*result.df);fmax->setRange(0,(result.nf-1)*result.df);
    kmin->setRange(result.k0,result.k0+(result.nk-1)*result.dk);kmax->setRange(kmin->minimum(),kmin->maximum());
    // Retain user display choices on recalculation. Reset only an axis whose
    // whole interval falls outside the new physical bounds and collapses.
    if((initialize && !frequencyEdited) || fmin->value()>=fmax->value()) {fmin->setValue(0);fmax->setValue(fmax->maximum());}
    if((initialize && !wavenumberEdited) || kmin->value()>=kmax->value()) {kmin->setValue(kmin->minimum());kmax->setValue(kmax->maximum());}
    bool displayOk=applyDisplay(initialize && !colorsEdited);haveDisplayRange=true;save->setEnabled(displayOk);
    if(!displayOk)return;
    status->setText(tr("Ready | %1 traces x %2 samples | df=%3 Hz | dk=%4 cycles/m | raw samples%5")
        .arg(result.nk).arg(worker->sampleLast-worker->sampleFirst+1).arg(result.df,0,'g',6).arg(result.dk,0,'g',6)
        .arg(worker->preprocess?tr(" + mean removal / Hann"):QString()));
}

bool FkSpectrumDialog::applyDisplay(bool autoColors)
{
    const auto& s=plot->spectrum;
    if(s.amplitude.isEmpty())return false;
    if(fmin->value()>=fmax->value() || kmin->value()>=kmax->value() || (!autoColors && cmin->value()>=cmax->value())) {
        status->setText(tr("Each minimum must be smaller than its maximum."));return false;
    }
    bool log=scale->currentIndex()==0;
    if(autoColors) {
        std::vector<double> values;
        for(int f=0;f<s.nf;++f)if(f*s.df>=fmin->value()-1e-9 && f*s.df<=fmax->value()+1e-9)
            for(int k=0;k<s.nk;++k)if(s.k0+k*s.dk>=kmin->value()-1e-9 && s.k0+k*s.dk<=kmax->value()+1e-9) {
                double a=s.amplitude[f*s.nk+k];values.push_back(log?std::log10(a+1e-12):a);
            }
        if(values.empty()){status->setText(tr("The selected range contains no FFT bins."));return false;}
        std::sort(values.begin(),values.end());
        auto quantile=[&](double p){double i=p*(values.size()-1);size_t j=size_t(i);return values[j]+(values[qMin(j+1,values.size()-1)]-values[j])*(i-j);};
        double lo=quantile(.01),hi=quantile(.995);
        if(hi-lo<1e-6)hi=lo+1e-6;
        // Six significant decimals in the controls should not collapse tiny ranges.
        int decimals=log?6:qBound(6,int(-std::floor(std::log10(qMax(hi-lo,1e-20))))+4,20);
        cmin->setDecimals(decimals);cmax->setDecimals(decimals);cmin->setValue(lo);cmax->setValue(hi);
    }
    plot->flo=fmin->value();plot->fhi=fmax->value();plot->klo=kmin->value();plot->khi=kmax->value();
    plot->clo=cmin->value();plot->chi=cmax->value();plot->logarithmic=log;plot->color=palette->currentIndex();plot->update();return true;
}

void FkSpectrumDialog::saveImage()
{
    if(!applyDisplay(false))return;
    QString name=QDir(directory).filePath(QFileInfo(path).completeBaseName()+"-fk.png");
    name=QFileDialog::getSaveFileName(this,tr("Save F-K spectrum"),name,tr("PNG image (*.png)"));
    if(name.isEmpty())return;
    if(!name.endsWith(".png",Qt::CaseInsensitive))name+=".png";
    if(!savePng(name))QMessageBox::warning(this,tr("Save F-K spectrum"),tr("Could not save the image."));
}

bool FkSpectrumDialog::savePng(const QString& filename)
{
    if(plot->spectrum.amplitude.isEmpty())return false;
    // Render the entire canvas, including portions outside the scroll viewport.
    // DPR=1 guarantees that the exported pixel dimensions equal the controls.
    QImage image(plot->size(),QImage::Format_RGB32);image.fill(Qt::white);
    QPainter painter(&image);plot->render(&painter);painter.end();
    return image.save(filename,"PNG");
}

void FkSpectrumDialog::shutdown()
{
    imageWidth->interpretText();imageHeight->interpretText();saveImageSize();
    if(worker) {
        worker->disconnect(this);worker->requestInterruption();worker->wait();delete worker;worker=nullptr;
        inputGroup->setEnabled(true);calculate->setText(tr("Calculate"));calculate->setEnabled(true);
    }
}
void FkSpectrumDialog::saveImageSize()
{
    QSettings settings("WW","SeiSeeMp");
    settings.setValue("FkSpectrum/imageWidth",imageWidth->value());
    settings.setValue("FkSpectrum/imageHeight",imageHeight->value());
    settings.sync();
}
FkSpectrumDialog::~FkSpectrumDialog(){shutdown();}
void FkSpectrumDialog::reject(){shutdown();QDialog::reject();}
void FkSpectrumDialog::closeEvent(QCloseEvent* event){shutdown();QDialog::closeEvent(event);}
