#ifndef FILEVIEWERWINDOW_H
#define FILEVIEWERWINDOW_H

#include <QMainWindow>

#include <QBitArray>

class FileViewerWindow;
class FVToolbar;
class FVScanGrp;
class DataFile;
class ChanMap;
class MGraphY;
class MGScroll;
class Biquad;
class ExportCtl;
class TaggableLabel;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class FileViewerWindow : public QMainWindow
{
    Q_OBJECT

    friend class FVScanGrp;

private:
    struct SaveSet {
        double      fArrowKey,
                    fPageKey,
                    xSpan,
                    ySclImAp,
                    ySclImLf,
                    ySclNiNeu,
                    ySclAux;
        int         yPix,
                    nDivs;
        bool        sortUserOrder;

        SaveSet()
        : fArrowKey(0.1), fPageKey(0.5) {}
    };

    struct GraphParams {
        // Copiable to other graphs of same type
        double  gain;
        bool    filter300Hz,
                dcFilter;

        GraphParams()
        : gain(1.0), filter300Hz(false), dcFilter(false) {}
    };

    FVToolbar               *tbar;
    FVScanGrp               *scanGrp;
    SaveSet                 sav;
    double                  tMouseOver,
                            yMouseOver;
    qint64                  dfCount,
                            dragAnchor,
                            dragL,              // or -1
                            dragR;
    DataFile                *df;
    ChanMap                 *chanMap;
    Biquad                  *hipass;
    ExportCtl               *exportCtl;
    QMenu                   *channelsMenu;
    MGScroll                *mscroll;
    QAction                 *exportAction;
    TaggableLabel           *closeLbl;
    QTimer                  *hideCloseTimer;
    QVector<MGraphY>        grfY;
    QVector<GraphParams>    grfParams;          // per-graph params
    QVector<QAction*>       grfActShowHide;
    QVector<int>            order2ig,           // sort order
                            ig2AcqChan;
    QBitArray               grfVisBits;
    int                     fType,              // {0=imap, 1=imlf, 2=ni}
                            igSelected,         // if >= 0
                            igMaximized,        // if >= 0
                            igMouseOver;        // if >= 0
    bool                    didLayout,
                            dragging;

public:
    FileViewerWindow();
    virtual ~FileViewerWindow();

    // Ok to call it multiple times to open new files using same window.
    bool viewFile( const QString &fileName, QString *errMsg_out = 0 );

    // Return currently open (.bin) path or null
    QString file() const;

// Toolbar
    double tbGetfileSecs() const;
    double tbGetxSpanSecs() const   {return sav.xSpan;}
    double tbGetyScl() const
        {
            switch( fType ) {
                case 0:  return sav.ySclImAp;
                case 1:  return sav.ySclImLf;
                default: return sav.ySclNiNeu;
            }
        }
    int    tbGetyPix() const        {return sav.yPix;}
    int    tbGetNDivs() const       {return sav.nDivs;}

// Export
    void getInverseGains(
        std::vector<double> &invGain,
        const QBitArray     &exportBits ) const;

public slots:
// Toolbar
    void tbToggleSort();
    void tbScrollToSelected();
    void tbSetXScale( double d );
    void tbSetYPix( int n );
    void tbSetYScale( double d );
    void tbSetMuxGain( double d );
    void tbSetNDivs( int n );
    void tbHipassClicked( bool b );
    void tbDcClicked( bool b );
    void tbApplyAll();

private slots:
// Menu
    void file_Open();
    void file_Options();
    void channels_ShowAll();

// CloseLabel
    void hideCloseLabel();
    void hideCloseTimeout();

// Export
    void doExport();

// Mouse
    void mouseOverGraph( double x, double y, int iy );
    void clickGraph( double x, double y, int iy );
    void dragDone();
    void dblClickGraph( double x, double y, int iy );
    void mouseOverLabel( int x, int y, int iy );

// Actions
    void menuShowHideGraph();
    void cursorHere( QPoint p );
    void clickedCloseLbl();

// Timer targets
    void layoutGraphs();

protected:
    virtual bool eventFilter( QObject *obj, QEvent *e );
    virtual void closeEvent( QCloseEvent *e );

private:
// Data-independent inits
    void initMenus();
    void initExport();
    void initCloseLbl();
    void initDataIndepStuff();

// Data-dependent inits
    bool openFile( const QString &fname, QString *errMsg );
    void initHipass();
    void killShowHideAction( int i );
    void killActions();
    void initGraphs();

    void loadSettings();
    void saveSettings() const;

    qint64 nScansPerGraph() const;
    void updateNDivText();

    double scalePlotValue( double v );

    QString nameGraph( int ig ) const;
    void hideGraph( int ig );
    void showGraph( int ig );
    void selectGraph( int ig, bool updateGraph = true );
    void toggleMaximized();
    void updateXSel();
    void updateGraphs();

    void printStatusMessage();
    bool queryCloseOK();

// Stream linking
    void linkRecvPos( double t0, double tSpan );
    void linkRecvSel( double tL, double tR );
};

#endif  // FILEVIEWERWINDOW_H


