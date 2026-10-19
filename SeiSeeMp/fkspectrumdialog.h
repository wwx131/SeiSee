#ifndef FKSPECTRUMDIALOG_H
#define FKSPECTRUMDIALOG_H

#include <QDialog>
#include "workthread.h"
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QGroupBox;
class CustomCheckBox;
class FkPlot;
class FkTxPlot;
class QTimer;
class QTabWidget;
class QScrollArea;

class FkSpectrumDialog : public QDialog {
    Q_OBJECT
public:
    explicit FkSpectrumDialog(SeisFile* file, const QString& directory, QWidget* parent=nullptr);
    ~FkSpectrumDialog() override;
    void shutdown();
    void reject() override;
    bool isCalculating() const { return worker!=nullptr; }
    bool savePng(const QString& filename);
    bool hasFilterPreview() const { return !lastFilter.filtered.isEmpty() && !filterStale; }
    void setInitialRange(int firstTrace, int lastTrace, double startMs, double endMs);
private:
    QString path, directory;
    int fileType;
    double dt;
    FkWorker* worker=nullptr;
    bool haveDisplayRange=false;
    bool frequencyEdited=false, wavenumberEdited=false, colorsEdited=false;
    bool filterStale=true,closing=false;
    int filterRevision=0;
    FkSpectrum originalSpectrum;
    FkFilterResult lastFilter;
    QGroupBox* inputGroup;
    QSpinBox *first, *last, *imageWidth, *imageHeight;
    QDoubleSpinBox *start, *end, *spacing, *fmin, *fmax, *kmin, *kmax, *cmin, *cmax;
    QComboBox *palette, *scale, *interpolation;
    CustomCheckBox* preprocess;
    QPushButton *calculate, *save;
    QLabel* status;
    QProgressBar* progress;
    FkPlot* plot;
    FkTxPlot* txPlot;
    QTabWidget* previewTabs;
    QComboBox *filterType,*filterResponse,*filterDirection,*previewMode;
    QDoubleSpinBox *velocityLow,*velocityHigh,*velocityTaper,*filterFmin,*filterFmax,*attenuation,*taperBins,*timeBlend;
    QSpinBox* traceBlend;
    CustomCheckBox *realTime,*velocityAxis,*filterOverlay,*zeroLine;
    QPushButton *filterButton,*exportFiltered;
    QLabel* polygonStatus;
    QTimer* refreshTimer;
    QScrollArea* previewViewport;
    QPushButton* fitImage;
    void resizePreview();
    bool eventFilter(QObject* object,QEvent* event) override;
    void begin(int mode=0);
    void launch(FkWorker* job);
    FkFilterSettings filterSettings() const;
    void filterChanged();
    void receiveFilter(const FkFilterResult& result);
    void showPreview();
    void saveFiltered();
    void receive(const FkSpectrum& result);
    bool applyDisplay(bool autoColors);
    void saveImage();
    void saveImageSize();
    void closeEvent(QCloseEvent* event) override;
};
#endif
