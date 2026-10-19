#include "fkspectrumdialog.h"
#include "customcontrols.h"
#include "fkcolormaps.h"
#include "gfxstyle.h"
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
    bool showFilterOverlay=false,showZeroLine=false;
    bool velocity=true,gainView=false,selecting=false,dragging=false,fanSelecting=false;
    double firstFanVelocity=-1;
    QPolygonF polygon;
    FkFilterSettings overlay;
    QPoint dragStart;
    double dragFlo=0,dragFhi=0,dragKlo=0,dragKhi=0;
    std::function<void()> polygonChanged;
    std::function<void(double,double)> fanSelected;
    std::function<void(double,double,double,double)> viewChanged;
    explicit FkPlot(QWidget* parent):QWidget(parent) {
        setObjectName("fkPlot"); setMinimumSize(480,280);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        setFont(QFont(GfxStyle::LabelFontFamily,int(GfxStyle::LabelFontPointSize)));
        setMouseTracking(true);
    }
    QRectF dataRect() const {return QRectF(78,velocity?92:35,width()-230,height()-(velocity?165:108));}
    QPointF coordinate(QPointF p) const {auto b=dataRect();return QPointF(klo+(p.x()-b.left())/b.width()*(khi-klo),fhi-(p.y()-b.top())/b.height()*(fhi-flo));}
    QPointF pixel(QPointF p) const {auto b=dataRect();return QPointF(b.left()+(p.x()-klo)/(khi-klo)*b.width(),b.bottom()-(p.y()-flo)/(fhi-flo)*b.height());}
    void finishPolygon(){bool wasFan=fanSelecting;selecting=false;fanSelecting=false;firstFanVelocity=-1;setCursor(Qt::ArrowCursor);if(!wasFan&&polygonChanged)polygonChanged();update();}
    void setView(double f0,double f1,double k0,double k1){
        if(spectrum.amplitude.isEmpty())return;
        auto constrain=[](double& a,double& b,double lo,double hi){double span=qMin(b-a,hi-lo);span=qMax(span,(hi-lo)*1e-5);a=qBound(lo,a,hi-span);b=a+span;};
        constrain(f0,f1,0,(spectrum.nf-1)*spectrum.df);constrain(k0,k1,spectrum.k0,spectrum.k0+(spectrum.nk-1)*spectrum.dk);
        if(viewChanged)viewChanged(f0,f1,k0,k1);
    }
    void zoom(double factor){double f=(flo+fhi)/2,k=(klo+khi)/2;setView(f-(fhi-flo)*factor/2,f+(fhi-flo)*factor/2,k-(khi-klo)*factor/2,k+(khi-klo)*factor/2);}
    void mousePressEvent(QMouseEvent* e) override {
        if(!dataRect().contains(e->pos())||spectrum.amplitude.isEmpty())return;
        if(selecting){if(e->button()==Qt::RightButton)finishPolygon();else if(e->button()==Qt::LeftButton){
                if(fanSelecting){auto point=coordinate(e->pos());if(std::abs(point.x())<1e-14)return;double v=qMin(1e9,point.y()/std::abs(point.x()));
                    if(firstFanVelocity<0)firstFanVelocity=v;
                    else {double low=qMin(firstFanVelocity,v),high=qMax(firstFanVelocity,v);finishPolygon();if(fanSelected)fanSelected(low,high);}}
                else polygon<<coordinate(e->pos());update();}return;}
        if(e->button()==Qt::LeftButton){dragging=true;dragStart=e->pos();dragFlo=flo;dragFhi=fhi;dragKlo=klo;dragKhi=khi;setCursor(Qt::ClosedHandCursor);}
    }
    void mouseMoveEvent(QMouseEvent* e) override {if(!dragging)return;auto b=dataRect();double dx=(e->pos().x()-dragStart.x())/b.width()*(dragKhi-dragKlo),dy=(e->pos().y()-dragStart.y())/b.height()*(dragFhi-dragFlo);
        setView(dragFlo+dy,dragFhi+dy,dragKlo-dx,dragKhi-dx);}
    void mouseReleaseEvent(QMouseEvent*) override {dragging=false;setCursor(selecting?Qt::CrossCursor:Qt::ArrowCursor);}
    void mouseDoubleClickEvent(QMouseEvent*) override {if(selecting)finishPolygon();}
    void wheelEvent(QWheelEvent* e) override {if(dataRect().contains(e->pos())){zoom(e->angleDelta().y()>0?.8:1.25);e->accept();}else QWidget::wheelEvent(e);}
    double value(int f,int k) const {
        double a=spectrum.amplitude[f*spectrum.nk+k];
        return logarithmic && !gainView ? std::log10(a+1e-12) : a;
    }
    QRgb rgb(double v) const {
        double lo=gainView?0:clo,hi=gainView?1:chi;
        int index=qBound(0,int(255*qBound(0.0,(v-lo)/(hi-lo),1.0)),255);
        return fkColorMaps[color][index];
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.fillRect(rect(),Qt::white);
        QRectF box=dataRect();
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
        if(velocity) {
            p.drawText(QRectF(box.left(),31,box.width(),22),Qt::AlignCenter,QString("Velocity at f=%1 Hz (m/s)").arg(fhi,0,'g',4));
            for(int i=0;i<=4;++i){double k=klo+(khi-klo)*i/4,x=box.left()+box.width()*i/4;
                if(i==2&&klo<0&&khi>0){k=0;x=box.left()-klo/(khi-klo)*box.width();}
                p.drawText(QRectF(x-45,box.top()-28,90,22),Qt::AlignCenter,std::abs(k)>1e-14?QString::number(fhi/k,'g',4):QString("-inf/+inf"));}
        }
        if(!spectrum.amplitude.isEmpty()){
            p.save();p.setClipRect(box);p.setPen(QPen(palette().color(QPalette::Highlight),1.5,Qt::DashLine));
            if(showZeroLine&&klo<0&&khi>0){p.setPen(QPen(QColor(0,0,0,90),.8));double x=pixel(QPointF(0,0)).x();p.drawLine(QPointF(x,box.top()),QPointF(x,box.bottom()));p.setPen(QPen(palette().color(QPalette::Highlight),1.5,Qt::DashLine));}
            if(showFilterOverlay && overlay.polygon && !polygon.isEmpty()){QPolygonF screen;for(auto v:polygon)screen<<pixel(v);p.setBrush(QColor(60,120,220,30));
                if(selecting)p.drawPolyline(screen);else p.drawPolygon(screen);
                p.setBrush(palette().color(QPalette::Highlight));for(auto v:screen)p.drawEllipse(v,3,3);
            } else if(showFilterOverlay && !overlay.polygon){for(double v:QList<double>({fanSelecting?firstFanVelocity:overlay.velocityLow,fanSelecting?firstFanVelocity:overlay.velocityHigh}))if(v>0)for(int sign:{-1,1})
                if(overlay.direction==0||(overlay.direction==1&&sign==1)||(overlay.direction==2&&sign==-1))p.drawLine(pixel(QPointF(sign*flo/v,flo)),pixel(QPointF(sign*fhi/v,fhi)));}
            p.restore();
        }
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
            p.drawText(QRectF(bar.right()+5,y-12,83,24),Qt::AlignLeft|Qt::AlignVCenter,QString::number(gainView?i/4.:clo+(chi-clo)*i/4,'g',4));
        }
        p.save();p.translate(width()-17,bar.center().y());p.rotate(-90);
        p.drawText(QRectF(-bar.height()/2,-12,bar.height(),24),Qt::AlignCenter,
                   gainView?"Filter gain (before edge blending)":logarithmic ? "log10(amplitude + 1e-12)" : "FFT amplitude");p.restore();
    }
};

class FkTxPlot : public QWidget {
public:
    FkFilterResult data;
    explicit FkTxPlot(QWidget* parent):QWidget(parent){setObjectName("fkTxPlot");setMinimumSize(480,320);setFont(QFont(GfxStyle::LabelFontFamily,int(GfxStyle::LabelFontPointSize)));}
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.fillRect(rect(),Qt::white);p.setPen(Qt::black);
        if(data.raw.isEmpty()){p.drawText(rect(),Qt::AlignCenter,"Apply a filter to compare raw, filtered and removed data");return;}
        double peak=0;for(float v:data.raw)peak=qMax(peak,std::abs(double(v)));peak=qMax(peak,1e-20);
        for(int panel=0;panel<3;++panel){QRect box(55+panel*(width()-65)/3,45,(width()-95)/3,height()-100);
            p.drawText(QRect(box.left(),8,box.width(),30),Qt::AlignCenter,QStringList({"Raw","Filtered","Removed"})[panel]);
            const auto& values=panel==0?data.raw:panel==1?data.filtered:data.removed;
            QImage im(qMax(1,box.width()),qMax(1,box.height()),QImage::Format_RGB32);
            for(int y=0;y<im.height();++y){auto row=reinterpret_cast<QRgb*>(im.scanLine(y));int t=qMin(data.nt-1,y*data.nt/im.height());
                for(int x=0;x<im.width();++x){int ix=qMin(data.nx-1,x*data.nx/im.width());int gray=qBound(0,int(127.5+127.5*values[ix*data.nt+t]/peak),255);row[x]=qRgb(gray,gray,gray);}}
            p.drawImage(box,im);p.drawRect(box);
            p.drawText(QRect(box.left(),box.bottom()+8,box.width(),25),Qt::AlignCenter,QString("Traces %1-%2").arg(data.first+1).arg(data.first+data.nx));}
        p.save();p.translate(15,height()/2);p.rotate(-90);p.drawText(QRect(-150,-10,300,25),Qt::AlignCenter,
            QString("Time %1-%2 ms (same amplitude scale)").arg(data.sampleFirst*data.dt*1000).arg((data.sampleFirst+data.nt-1)*data.dt*1000));p.restore();
    }
};

FkSpectrumDialog::FkSpectrumDialog(SeisFile* file,const QString& dir,QWidget* parent)
    :QDialog(parent),path(QString::fromLocal8Bit(file->Fname().c_str())),directory(dir),
      fileType(file->typeName()=="SEG-Y"?1:file->typeIdx()),dt(file->Si())
{
    qRegisterMetaType<FkSpectrum>("FkSpectrum");
    qRegisterMetaType<FkFilterResult>("FkFilterResult");
    setObjectName("FkSpectrumDialog"); setWindowTitle(tr("F-K Spectrum and Filter"));
    setWindowModality(Qt::NonModal);setModal(false);setSizeGripEnabled(true);resize(1440,900);
    setFont(QApplication::font());setPalette(parent?parent->palette():QApplication::palette());
    auto layout=new QHBoxLayout(this);layout->setContentsMargins(8,8,8,8);
    auto splitter=new QSplitter(Qt::Horizontal,this);splitter->setObjectName("fkSplitter");layout->addWidget(splitter);
    auto left=new QWidget;left->setObjectName("fkParameterPanel");left->setMinimumWidth(360);
    auto leftLayout=new QVBoxLayout(left);leftLayout->setContentsMargins(0,0,0,0);leftLayout->setSpacing(6);
    auto parameterScroll=new QScrollArea;parameterScroll->setWidgetResizable(true);parameterScroll->setFrameShape(QFrame::NoFrame);
    auto parameters=new QWidget;parameters->setObjectName("fkParameters");auto settingsLayout=new QVBoxLayout(parameters);settingsLayout->setContentsMargins(0,0,4,0);settingsLayout->setSpacing(6);
    parameterScroll->setWidget(parameters);leftLayout->addWidget(parameterScroll,1);splitter->addWidget(left);
    auto right=new QWidget;auto rightLayout=new QVBoxLayout(right);rightLayout->setContentsMargins(0,0,0,0);rightLayout->setSpacing(6);splitter->addWidget(right);
    splitter->setChildrenCollapsible(false);splitter->setStretchFactor(0,0);splitter->setStretchFactor(1,1);splitter->setSizes({390,1030});
    auto group=[&](const QString& title){auto g=new QGroupBox(title);auto f=new QFormLayout(g);f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);f->setSpacing(6);settingsLayout->addWidget(g);return f;};
    auto fileGroup=group(tr("File"));auto source=new QLineEdit(path);source->setReadOnly(true);source->setObjectName("fkFile");source->setToolTip(path);fileGroup->addRow(source);
    auto integer=[&](const char* name,int maximum,int value){auto s=new QSpinBox;s->setObjectName(name);s->setRange(1,maximum);s->setValue(value);s->setMinimumWidth(110);s->setKeyboardTracking(false);return s;};
    auto real=[&](const char* name,double lo,double hi,double value,int decimals=6){auto s=new QDoubleSpinBox;s->setObjectName(name);s->setDecimals(decimals);s->setRange(lo,hi);s->setValue(value);s->setKeyboardTracking(false);s->setMinimumWidth(110);s->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);return s;};
    auto form=group(tr("Input range"));inputGroup=qobject_cast<QGroupBox*>(form->parentWidget());
    first=integer("fkFirst",file->Nt(),1);last=integer("fkLast",file->Nt(),qMin(file->Nt(),256));
    start=real("fkStart",0,(file->Ns()-1)*dt*1000,0,3);end=real("fkEnd",0,(file->Ns()-1)*dt*1000,(file->Ns()-1)*dt*1000,3);
    spacing=real("fkSpacing",.000001,1000000,12.5);
    form->addRow(tr("First trace (1-based)"),first);form->addRow(tr("Last trace (inclusive)"),last);
    form->addRow(tr("Start time (ms)"),start);form->addRow(tr("End time (ms)"),end);form->addRow(tr("Trace spacing (m)"),spacing);
    preprocess=new CustomCheckBox;preprocess->setText(tr("Remove means + 2-D Hann"));preprocess->setObjectName("fkPreprocess");preprocess->setChecked(true);
    preprocess->setToolTip(tr("Spectrum display only. Filtering uses untapered raw data and a separate edge blend."));
    auto display=group(tr("Spectrum display"));
    fmin=real("fkFmin",0,.5/dt,0);fmax=real("fkFmax",0,.5/dt,.5/dt);
    kmin=real("fkKmin",-1000000,1000000,-.04,9);kmax=real("fkKmax",-1000000,1000000,.04,9);
    cmin=real("fkCmin",-1e30,1e30,-12);cmax=real("fkCmax",-1e30,1e30,0);
    display->addRow(tr("Frequency min (Hz)"),fmin);display->addRow(tr("Frequency max (Hz)"),fmax);
    display->addRow(tr("Wavenumber min (cycles/m)"),kmin);display->addRow(tr("Wavenumber max (cycles/m)"),kmax);
    display->addRow(tr("Color minimum"),cmin);display->addRow(tr("Color maximum"),cmax);
    palette=new QComboBox;palette->setObjectName("fkPalette");palette->addItems({"Jet","Viridis","Inferno","Gray","RdBu"});
    scale=new QComboBox;scale->setObjectName("fkScale");scale->addItems({"log10(amplitude + 1e-12)","Linear amplitude"});
    interpolation=new QComboBox;interpolation->setObjectName("fkInterpolation");interpolation->addItems({tr("Smooth (bilinear)"),tr("Original bins (nearest)")});
    velocityAxis=new CustomCheckBox;velocityAxis->setObjectName("fkVelocityAxis");velocityAxis->setText(tr("Velocity auxiliary axis"));velocityAxis->setChecked(true);
    velocityAxis->setToolTip(tr("Top axis shows f/k at the top displayed frequency; it is not a velocity mapping for every frequency row."));
    filterOverlay=new CustomCheckBox;filterOverlay->setObjectName("fkShowFilterOverlay");filterOverlay->setText(tr("Show filter boundaries"));
    filterOverlay->setToolTip(tr("Display fan or polygon guides only; this does not apply a filter."));
    zeroLine=new CustomCheckBox;zeroLine->setObjectName("fkShowZeroLine");zeroLine->setText(tr("Show zero-wavenumber line"));
    previewMode=new QComboBox;previewMode->setObjectName("fkPreviewMode");previewMode->addItems({tr("Original spectrum"),tr("Filtered spectrum"),tr("Removed spectrum"),tr("Filter gain")});
    display->addRow(tr("Preview"),previewMode);display->addRow(tr("Color map"),palette);display->addRow(tr("Amplitude"),scale);display->addRow(tr("Interpolation"),interpolation);display->addRow(preprocess);display->addRow(velocityAxis);display->addRow(filterOverlay);display->addRow(zeroLine);
    auto apply=new QPushButton(tr("Apply range"));apply->setObjectName("fkApply");auto autoColor=new QPushButton(tr("Auto color (P1 / P99.5)"));autoColor->setObjectName("fkAutoColor");
    auto displayButtons=new QHBoxLayout;displayButtons->addWidget(apply);displayButtons->addWidget(autoColor);display->addRow(displayButtons);
    auto filterForm=group(tr("F-K filter"));
    filterType=new QComboBox;filterType->setObjectName("fkFilterType");filterType->addItems({tr("Fan (velocity)"),tr("Polygon")});
    filterResponse=new QComboBox;filterResponse->setObjectName("fkFilterResponse");filterResponse->addItems({tr("Reject selected region"),tr("Pass selected region")});
    filterDirection=new QComboBox;filterDirection->setObjectName("fkFilterDirection");filterDirection->addItems({tr("Both k directions"),tr("Positive k"),tr("Negative k")});
    velocityLow=real("fkVelocityLow",0,1e9,0,2);velocityHigh=real("fkVelocityHigh",.001,1e9,1500,2);velocityTaper=real("fkVelocityTaper",0,1e9,200,2);
    filterFmin=real("fkFilterFmin",0,.5/dt,0,3);filterFmax=real("fkFilterFmax",0,.5/dt,.5/dt,3);
    attenuation=real("fkAttenuation",-160,0,-60,1);taperBins=real("fkTaperBins",0,1000,2,1);
    timeBlend=real("fkTimeBlend",0,1e6,20,2);traceBlend=integer("fkTraceBlend",100000,2);traceBlend->setMinimum(0);
    filterForm->addRow(tr("Type"),filterType);filterForm->addRow(tr("Response"),filterResponse);filterForm->addRow(tr("Direction"),filterDirection);
    filterForm->addRow(tr("Low velocity (m/s)"),velocityLow);filterForm->addRow(tr("High velocity (m/s)"),velocityHigh);filterForm->addRow(tr("Velocity taper (m/s)"),velocityTaper);
    filterForm->addRow(tr("Filter frequency min (Hz)"),filterFmin);filterForm->addRow(tr("Filter frequency max (Hz)"),filterFmax);
    filterForm->addRow(tr("Attenuation (dB)"),attenuation);filterForm->addRow(tr("Boundary taper (bins)"),taperBins);
    filterForm->addRow(tr("Time edge blend (ms)"),timeBlend);filterForm->addRow(tr("Trace edge blend"),traceBlend);
    auto polygonButtons=new QHBoxLayout;auto fanDraw=new QPushButton(tr("Draw fan"));fanDraw->setObjectName("fkDrawFan");auto draw=new QPushButton(tr("Draw polygon"));draw->setObjectName("fkDrawPolygon");auto finish=new QPushButton(tr("Finish polygon"));finish->setObjectName("fkFinishPolygon");auto clear=new QPushButton(tr("Clear selection"));clear->setObjectName("fkClearPolygon");
    polygonButtons->addWidget(fanDraw);polygonButtons->addWidget(draw);filterForm->addRow(polygonButtons);
    auto finishButtons=new QHBoxLayout;finishButtons->addWidget(finish);finishButtons->addWidget(clear);filterForm->addRow(finishButtons);
    polygonStatus=new QLabel(tr("Pick two fan boundaries, or draw and finish a polygon."));polygonStatus->setWordWrap(true);filterForm->addRow(polygonStatus);
    realTime=new CustomCheckBox;realTime->setObjectName("fkRealTime");realTime->setText(tr("Real-time filter preview"));filterForm->addRow(realTime);
    filterButton=new QPushButton(QIcon(":/images/Procp.png"),tr("Apply filter / preview"));filterButton->setObjectName("fkApplyFilter");filterForm->addRow(filterButton);
    auto imageForm=group(tr("Image export"));
    imageWidth=integer("fkImageWidth",4096,1000);imageWidth->setMinimum(640);imageWidth->setKeyboardTracking(true);
    imageHeight=integer("fkImageHeight",4096,360);imageHeight->setMinimum(280);imageHeight->setKeyboardTracking(true);
    imageWidth->setSingleStep(50);imageHeight->setSingleStep(50);QSettings settings("WW","SeiSeeMp");
    bool widthOk=false,heightOk=false;int savedWidth=settings.value("FkSpectrum/imageWidth",1000).toInt(&widthOk),savedHeight=settings.value("FkSpectrum/imageHeight",360).toInt(&heightOk);
    imageWidth->setValue(widthOk?savedWidth:1000);imageHeight->setValue(heightOk?savedHeight:360);
    imageWidth->setToolTip(tr("640 to 4096 pixels; valid input is applied immediately."));imageHeight->setToolTip(tr("280 to 4096 pixels; valid input is applied immediately."));
    imageForm->addRow(tr("Image width (px)"),imageWidth);imageForm->addRow(tr("Image height (px)"),imageHeight);
    auto applySize=new QPushButton(tr("Apply size"));applySize->setObjectName("fkApplySize");imageForm->addRow(applySize);
    // Keep analysis and filtering together; display/export remain reachable via
    // section navigation without moving any controls out of the left panel.
    auto filterWidget=filterForm->parentWidget();settingsLayout->removeWidget(filterWidget);settingsLayout->insertWidget(2,filterWidget);
    auto section=new QComboBox;section->setObjectName("fkSection");section->addItems({tr("Input range"),tr("F-K filter"),tr("Spectrum display"),tr("Image export")});
    leftLayout->insertWidget(0,section);
    connect(section,QOverload<int>::of(&QComboBox::activated),this,[parameterScroll,form,filterForm,display,imageForm](int i){
        QWidget* target=QList<QWidget*>({form->parentWidget(),filterForm->parentWidget(),display->parentWidget(),imageForm->parentWidget()})[i];
        parameterScroll->verticalScrollBar()->setValue(target->y());});
    auto note=new QLabel(tr("Raw, equally spaced traces. Confirm dx. Filtering preserves samples outside the selected window; export creates a new SEG-Y."));note->setWordWrap(true);settingsLayout->addWidget(note);settingsLayout->addStretch();
    status=new QLabel(tr("Ready. Confirm dx and select the analysis range."));status->setObjectName("fkStatus");status->setWordWrap(true);leftLayout->addWidget(status);
    progress=new QProgressBar;progress->setValue(0);leftLayout->addWidget(progress);
    auto buttons=new QGridLayout;calculate=new QPushButton(QIcon(":/images/Refresh.png"),tr("Refresh spectrum"));calculate->setObjectName("fkCalculate");
    save=new QPushButton(QIcon(":/images/FileSave.png"),tr("Save PNG"));save->setObjectName("fkSave");save->setEnabled(false);
    exportFiltered=new QPushButton(tr("Save filtered SEG-Y"));exportFiltered->setObjectName("fkSaveFiltered");exportFiltered->setEnabled(false);auto close=new QPushButton(tr("Close"));
    buttons->addWidget(calculate,0,0);buttons->addWidget(save,0,1);buttons->addWidget(exportFiltered,1,0);buttons->addWidget(close,1,1);leftLayout->addLayout(buttons);
    auto navigation=new QHBoxLayout;auto zoomIn=new QPushButton(QIcon(":/images/ZoomI.png"),tr("Zoom in"));zoomIn->setObjectName("fkZoomIn");auto zoomOut=new QPushButton(QIcon(":/images/ZoomO.png"),tr("Zoom out"));zoomOut->setObjectName("fkZoomOut");auto reset=new QPushButton(QIcon(":/images/ZoomA.png"),tr("Reset view"));reset->setObjectName("fkResetView");
    fitImage=new QPushButton(tr("Fit to window"));fitImage->setObjectName("fkFitImage");fitImage->setCheckable(true);
    navigation->addWidget(zoomIn);navigation->addWidget(zoomOut);navigation->addWidget(reset);navigation->addWidget(fitImage);navigation->addStretch();rightLayout->addLayout(navigation);
    previewTabs=new QTabWidget;previewTabs->setObjectName("fkPreviewTabs");rightLayout->addWidget(previewTabs,1);
    plot=new FkPlot(this);plot->setFixedSize(imageWidth->value(),imageHeight->value());
    auto viewport=new QScrollArea;viewport->setObjectName("fkViewport");viewport->setWidget(plot);viewport->setWidgetResizable(false);viewport->setAlignment(Qt::AlignCenter);viewport->setMinimumSize(480,280);
    previewViewport=viewport;viewport->viewport()->installEventFilter(this);
    previewTabs->addTab(viewport,tr("F-K spectrum"));txPlot=new FkTxPlot(this);previewTabs->addTab(txPlot,tr("T-X comparison"));
    auto resizeImage=[this](int){resizePreview();saveImageSize();};
    connect(imageWidth,QOverload<int>::of(&QSpinBox::valueChanged),this,resizeImage);connect(imageHeight,QOverload<int>::of(&QSpinBox::valueChanged),this,resizeImage);
    connect(applySize,&QPushButton::clicked,this,[this,resizeImage](){imageWidth->interpretText();imageHeight->interpretText();resizeImage(0);});
    connect(interpolation,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int i){plot->smooth=i==0;plot->update();});
    connect(filterOverlay,&QCheckBox::toggled,this,[this](bool enabled){plot->showFilterOverlay=enabled;if(!enabled&&plot->selecting)plot->finishPolygon();plot->update();});
    connect(zeroLine,&QCheckBox::toggled,this,[this](bool enabled){plot->showZeroLine=enabled;plot->update();});
    connect(velocityAxis,&QCheckBox::toggled,this,[this](bool enabled){plot->velocity=enabled;plot->update();});
    connect(zoomIn,&QPushButton::clicked,this,[this](){plot->zoom(.8);});connect(zoomOut,&QPushButton::clicked,this,[this](){plot->zoom(1.25);});
    connect(fitImage,&QPushButton::toggled,this,[this](){resizePreview();});
    connect(reset,&QPushButton::clicked,this,[this](){auto s=plot->spectrum;if(!s.amplitude.isEmpty())plot->setView(0,(s.nf-1)*s.df,s.k0,s.k0+(s.nk-1)*s.dk);});
    plot->viewChanged=[this](double f0,double f1,double k0,double k1){fmin->setValue(f0);fmax->setValue(f1);kmin->setValue(k0);kmax->setValue(k1);applyDisplay(false);};
    plot->polygonChanged=[this](){polygonStatus->setText(tr("%1 vertices. Double-click or Finish to close the selection.").arg(plot->polygon.size()));filterChanged();};
    connect(fanDraw,&QPushButton::clicked,this,[this](){filterOverlay->setChecked(true);filterType->setCurrentIndex(0);plot->selecting=true;plot->fanSelecting=true;plot->firstFanVelocity=-1;plot->setCursor(Qt::CrossCursor);previewTabs->setCurrentIndex(0);polygonStatus->setText(tr("Click two velocity boundaries away from k=0. Right-click cancels."));filterChanged();});
    plot->fanSelected=[this](double low,double high){velocityLow->setValue(low);velocityHigh->setValue(high);polygonStatus->setText(tr("Fan boundaries: %1 to %2 m/s.").arg(low,0,'g',6).arg(high,0,'g',6));filterChanged();};
    connect(draw,&QPushButton::clicked,this,[this](){filterOverlay->setChecked(true);filterType->setCurrentIndex(1);plot->polygon.clear();plot->selecting=true;plot->fanSelecting=false;plot->setCursor(Qt::CrossCursor);previewTabs->setCurrentIndex(0);polygonStatus->setText(tr("Click vertices; double-click, right-click or Finish to close."));filterChanged();});
    connect(finish,&QPushButton::clicked,this,[this](){plot->finishPolygon();});connect(clear,&QPushButton::clicked,this,[this](){plot->polygon.clear();plot->finishPolygon();});
    connect(filterButton,&QPushButton::clicked,this,[this](){if(worker){worker->requestInterruption();}else begin(1);});connect(exportFiltered,&QPushButton::clicked,this,&FkSpectrumDialog::saveFiltered);
    connect(previewMode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,autoColor](int i){
        autoColor->setEnabled(i!=3);scale->setEnabled(i!=3);cmin->setEnabled(i!=3);cmax->setEnabled(i!=3);showPreview();});
    refreshTimer=new QTimer(this);refreshTimer->setSingleShot(true);refreshTimer->setInterval(350);
    connect(refreshTimer,&QTimer::timeout,this,[this](){if(!closing&&!worker)begin(1);});
    for(auto combo:{filterType,filterResponse,filterDirection})connect(combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](){filterChanged();});
    for(auto spin:{velocityLow,velocityHigh,velocityTaper,filterFmin,filterFmax,attenuation,taperBins,timeBlend})connect(spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](){filterChanged();});
    connect(traceBlend,QOverload<int>::of(&QSpinBox::valueChanged),this,[this](){filterChanged();});connect(realTime,&QCheckBox::toggled,this,[this](bool enabled){if(enabled)filterChanged();else refreshTimer->stop();});
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
    auto dirty=[this](){save->setEnabled(false);filterStale=true;++filterRevision;exportFiltered->setEnabled(false);status->setText(tr("Input changed. Refresh the spectrum or apply the filter."));};
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

FkFilterSettings FkSpectrumDialog::filterSettings() const
{
    FkFilterSettings s;s.polygon=filterType->currentIndex()==1;s.reject=filterResponse->currentIndex()==0;
    s.direction=filterDirection->currentIndex();s.velocityLow=velocityLow->value();s.velocityHigh=velocityHigh->value();s.velocityTaper=velocityTaper->value();
    s.frequencyLow=filterFmin->value();s.frequencyHigh=filterFmax->value();s.attenuationDb=attenuation->value();
    s.taperBins=taperBins->value();s.timeBlendMs=timeBlend->value();s.traceBlend=traceBlend->value();s.vertices=plot->polygon;return s;
}

void FkSpectrumDialog::filterChanged()
{
    filterStale=true;++filterRevision;exportFiltered->setEnabled(false);
    if(!lastFilter.filtered.isEmpty())status->setText(tr("Filter settings changed. Apply filter to refresh the preview."));
    plot->overlay=filterSettings();plot->update();
    bool fan=filterType->currentIndex()==0;
    for(auto spin:{velocityLow,velocityHigh,velocityTaper})spin->setEnabled(fan);
    filterDirection->setEnabled(fan);
    if(realTime->isChecked() && !plot->selecting){if(worker&&worker->mode==1)worker->requestInterruption();refreshTimer->start();}
}

void FkSpectrumDialog::begin(int mode)
{
    if(worker||closing)return;
    int a=int(std::ceil(start->value()/(dt*1000)-1e-7));
    int b=int(std::floor(end->value()/(dt*1000)+1e-7));
    if(first->value()>=last->value() || b-a+1<4 || (preprocess->isChecked() && last->value()-first->value()+1<3) ||
       qint64(last->value()-first->value()+1)*(b-a+1)>8000000) {
        status->setText(tr("Choose an ordered range with 2+ traces (3+ with Hann), 4+ time samples and at most 8 million cells."));return;
    }
    if(mode==1&&plot->selecting){status->setText(tr("Complete the current fan or polygon selection first."));return;}
    if(mode==1 && filterType->currentIndex()==1 && plot->polygon.size()<3){
        status->setText(tr("Finish a polygon with at least 3 vertices before applying the filter."));return;}
    auto job=new FkWorker(this);job->path=path;job->fileType=fileType;job->mode=mode;job->revision=filterRevision;
    job->first=first->value()-1;job->last=last->value()-1;
    job->sampleFirst=a;job->sampleLast=b;job->dx=spacing->value();job->preprocess=preprocess->isChecked();job->filter=filterSettings();
    filterStale=true;exportFiltered->setEnabled(false);
    if(mode==0){lastFilter=FkFilterResult();txPlot->data=FkFilterResult();txPlot->update();previewMode->setCurrentIndex(0);plot->spectrum=FkSpectrum();plot->update();}
    status->setText(mode==1?tr("Filtering raw data and computing comparison spectra..."):tr("Reading raw traces and computing F-K spectrum..."));
    launch(job);
}

void FkSpectrumDialog::launch(FkWorker* job)
{
    worker=job;inputGroup->setEnabled(false);preprocess->setEnabled(false);calculate->setText(tr("Cancel"));filterButton->setText(tr("Cancel"));save->setEnabled(false);exportFiltered->setEnabled(false);progress->setValue(0);
    connect(worker,&FkWorker::progress,progress,&QProgressBar::setValue);
    connect(worker,&FkWorker::spectrumReady,this,&FkSpectrumDialog::receive);
    connect(worker,&FkWorker::filterReady,this,&FkSpectrumDialog::receiveFilter);
    connect(worker,&FkWorker::exportReady,this,[this](const QString& filename){status->setText(tr("Saved filtered SEG-Y: %1").arg(filename));});
    connect(worker,&FkWorker::failed,status,&QLabel::setText);
    connect(worker,&QThread::finished,this,[this](){
        bool cancelled=worker->isInterruptionRequested();bool exporting=worker->mode==2;bool rerun=!exporting&&worker->revision!=filterRevision;
        worker->deleteLater();worker=nullptr;
        inputGroup->setEnabled(true);preprocess->setEnabled(true);calculate->setText(tr("Refresh spectrum"));calculate->setEnabled(true);filterButton->setText(tr("Apply filter / preview"));
        save->setEnabled(!plot->spectrum.amplitude.isEmpty());exportFiltered->setEnabled(hasFilterPreview()&&fileType==1);
        if(cancelled)status->setText(exporting?tr("Cancelled; unfinished output discarded."):tr("Cancelled; incomplete preview discarded."));
        if(realTime->isChecked() && rerun && !closing)refreshTimer->start();
    });
    worker->start();
}

void FkSpectrumDialog::receive(const FkSpectrum& result)
{
    originalSpectrum=result;
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
    showPreview();
}

void FkSpectrumDialog::receiveFilter(const FkFilterResult& result)
{
    if(!worker || worker->revision!=filterRevision)return;
    lastFilter=result;filterStale=false;receive(result.originalSpectrum);
    txPlot->data=result;txPlot->update();previewMode->setCurrentIndex(1);showPreview();
    exportFiltered->setEnabled(fileType==1);
    status->setText(tr("Filter preview ready. Compare F-K / T-X tabs; save to a new SEG-Y when satisfied."));
}

void FkSpectrumDialog::showPreview()
{
    int index=previewMode->currentIndex();plot->gainView=index==3;plot->overlay=filterSettings();
    if(index==0)plot->spectrum=originalSpectrum;
    else if(!lastFilter.filtered.isEmpty())plot->spectrum=index==1?lastFilter.filteredSpectrum:index==2?lastFilter.removedSpectrum:lastFilter.gain;
    else plot->spectrum=FkSpectrum();
    plot->update();
}

void FkSpectrumDialog::saveFiltered()
{
    if(worker||!hasFilterPreview())return;
    if(fileType!=1){status->setText(tr("Filtered file export currently supports standard SEG-Y inputs."));return;}
    QString output=QFileDialog::getSaveFileName(this,tr("Save filtered SEG-Y"),QDir(directory).filePath(QFileInfo(path).completeBaseName()+"-fk-filtered.sgy"),tr("SEG-Y (*.sgy *.segy)"));
    if(output.isEmpty())return;
    auto job=new FkWorker(this);job->mode=2;job->outputPath=output;job->exportResult=lastFilter;
    status->setText(tr("Saving filtered traces to a new SEG-Y..."));launch(job);
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
    QSize target(imageWidth->value(),imageHeight->value()),previous=plot->size();
    QImage image(target,QImage::Format_RGB32);image.fill(Qt::white);
    QPainter painter(&image);
    if(previewTabs->currentIndex()==1){painter.scale(double(target.width())/txPlot->width(),double(target.height())/txPlot->height());txPlot->render(&painter);}
    else {plot->setFixedSize(target);plot->render(&painter);plot->setFixedSize(previous);}
    painter.end();
    return image.save(filename,"PNG");
}

void FkSpectrumDialog::resizePreview()
{
    QSize target(imageWidth->value(),imageHeight->value());
    if(fitImage->isChecked()){
        QSize available=previewViewport->viewport()->size()-QSize(8,8);
        double factor=qMin(double(available.width())/target.width(),double(available.height())/target.height());
        factor=qMax(factor,qMax(480./target.width(),280./target.height()));
        target=QSize(qRound(target.width()*factor),qRound(target.height()*factor));
    }
    plot->setFixedSize(target);
}

bool FkSpectrumDialog::eventFilter(QObject* object,QEvent* event)
{
    if(object==previewViewport->viewport()&&event->type()==QEvent::Resize)
        QTimer::singleShot(0,this,[this](){if(!closing&&fitImage->isChecked())resizePreview();});
    return QDialog::eventFilter(object,event);
}

void FkSpectrumDialog::shutdown()
{
    closing=true;if(refreshTimer)refreshTimer->stop();
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
