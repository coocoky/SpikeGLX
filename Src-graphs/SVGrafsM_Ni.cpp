
#include "Util.h"
#include "MainApp.h"
#include "ConfigCtl.h"
#include "GraphsWindow.h"
#include "SVGrafsM_Ni.h"
#include "Biquad.h"

#include <QStatusBar>
#include <QSettings>


#define MAX16BIT    32768


/* ---------------------------------------------------------------- */
/* class SVGrafsM_Ni ---------------------------------------------- */
/* ---------------------------------------------------------------- */

SVGrafsM_Ni::SVGrafsM_Ni( GraphsWindow *gw, DAQ::Params &p )
    :   SVGrafsM( gw, p ), hipass(0), lopass(0)
{
}


SVGrafsM_Ni::~SVGrafsM_Ni()
{
    saveSettings();

    fltMtx.lock();

    if( hipass )
        delete hipass;

    if( lopass )
        delete lopass;

    fltMtx.unlock();
}


/*  Time Scaling
    ------------
    Each graph has its own wrapping data buffer (yval) but shares
    the time axis span. As fresh data arrive they wrap around such
    that the latest data are present as well as one span's worth of
    past data. We will draw the data using a wipe effect. Older data
    remain visible while they are progressively overwritten by the
    new from left to right. In this mode selection ranges do not
    make sense, nor do precise cursor readouts of time-coordinates.
    Rather, min_x and max_x suggest only the span of depicted data.
*/

void SVGrafsM_Ni::putScans( vec_i16 &data, quint64 headCt )
{
#if 0
    double  tProf = getTime();
#endif
    double      ysc     = 1.0 / MAX16BIT;
    const int   nC      = chanCount(),
                ntpts   = (int)data.size() / nC,
                dwnSmp  = theX->dwnSmp,
                dstep   = dwnSmp * nC;

/* ------------ */
/* Apply filter */
/* ------------ */

    fltMtx.lock();

    if( hipass ) {
        hipass->applyBlockwiseMem(
                    &data[0], MAX16BIT, ntpts, nC,
                    0, p.ni.niCumTypCnt[CniCfg::niSumNeural] );
    }

    if( lopass ) {
        lopass->applyBlockwiseMem(
                    &data[0], MAX16BIT, ntpts, nC,
                    0, p.ni.niCumTypCnt[CniCfg::niSumNeural] );
    }

    fltMtx.unlock();

// ---------------------
// Append data to graphs
// ---------------------

    drawMtx.lock();

    QVector<float>  ybuf( ntpts );  // append en masse

    for( int ic = 0; ic < nC; ++ic ) {

        // -----------------
        // For active graphs
        // -----------------

        if( ic2iy[ic] < 0 )
            continue;

        // Collect points, update mean, stddev

        GraphStats  &stat   = ic2stat[ic];
        qint16      *d      = &data[ic];
        int         ny      = 0;

        stat.clear();

        if( ic < p.ni.niCumTypCnt[CniCfg::niSumNeural] ) {

            // -------------------
            // Neural downsampling
            // -------------------

            // Withing each bin, report the greatest
            // amplitude (pos or neg) extremum. This
            // ensures spikes are not missed.

            if( set.bandSel == 2 || !set.binMaxOn || dwnSmp <= 1 )
                goto pickNth;

            int ndRem = ntpts;

            for( int it = 0; it < ntpts; it += dwnSmp ) {

                int binMin = *d,
                    binMax = binMin,
                    binWid = dwnSmp;

                    stat.add( *d );

                    d += nC;

                    if( ndRem < binWid )
                        binWid = ndRem;

                for( int ib = 1; ib < binWid; ++ib, d += nC ) {

                    int val = *d;

                    stat.add( *d );

                    if( val <= binMin )
                        binMin = val;
                    else if( val > binMax )
                        binMax = val;
                }

                ndRem -= binWid;

                if( abs( binMin ) > abs( binMax ) )
                    binMax = binMin;

                ybuf[ny++] = binMax * ysc;
            }
        }
        else if( ic < p.ni.niCumTypCnt[CniCfg::niSumAnalog] ) {

            // ----------
            // Aux analog
            // ----------

pickNth:
            for( int it = 0; it < ntpts; it += dwnSmp, d += dstep ) {

                stat.add( *d );
                ybuf[ny++] = *d * ysc;
            }
        }
        else {

            // -------
            // Digital
            // -------

            for( int it = 0; it < ntpts; it += dwnSmp, d += dstep )
                ybuf[ny++] = *d;
        }

        // Append points en masse
        // Renormalize x-coords -> consecutive indices.

        theX->dataMtx->lock();
        ic2Y[ic].yval.putData( &ybuf[0], ny );
        theX->dataMtx->unlock();
    }

// -----------------------
// Update pseudo time axis
// -----------------------

    double  span = theX->spanSecs();

    theX->max_x = (headCt + ntpts) / p.ni.srate;
    theX->min_x = theX->max_x - span;

// ----
// Draw
// ----

    QMetaObject::invokeMethod( theM, "update", Qt::QueuedConnection );

    drawMtx.unlock();

// ---------
// Profiling
// ---------

#if 0
    tProf = getTime() - tProf;
    Log() << "Graph milis " << 1000*tProf;
#endif
}


int SVGrafsM_Ni::chanCount() const
{
    return p.ni.niCumTypCnt[CniCfg::niSumAll];
}


bool SVGrafsM_Ni::isSelAnalog() const
{
    return selected < p.ni.niCumTypCnt[CniCfg::niSumAnalog];
}


void SVGrafsM_Ni::bandSelChanged( int sel )
{
    fltMtx.lock();

    if( hipass ) {
        delete hipass;
        hipass = 0;
    }

    if( lopass ) {
        delete lopass;
        lopass = 0;
    }

    if( !sel )
        ;
    else if( sel == 1 )
        hipass = new Biquad( bq_type_highpass, 300/p.ni.srate );
    else {
        hipass = new Biquad( bq_type_highpass, 0.1/p.ni.srate );
        lopass = new Biquad( bq_type_lowpass,  300/p.ni.srate );
    }


    fltMtx.unlock();

    drawMtx.lock();
    set.bandSel = sel;
    drawMtx.unlock();

    saveSettings();
}


void SVGrafsM_Ni::binMaxChkClicked( bool checked )
{
    drawMtx.lock();
    set.binMaxOn = checked;
    drawMtx.unlock();

    saveSettings();
}


void SVGrafsM_Ni::mySaveGraphClicked( bool checked )
{
    Q_UNUSED( checked )
}


void SVGrafsM_Ni::myMouseOverGraph( double x, double y, int iy )
{
    int     ic          = lastMouseOverChan = theX->Y[iy]->usrChan;
    bool    isNowOver   = true;

    if( ic < 0 || ic >= chanCount() ) {
        gw->statusBar()->clearMessage();
        return;
    }

    QWidget *w = QApplication::widgetAt( QCursor::pos() );

    if( !w || !dynamic_cast<MGraph*>(w) )
        isNowOver = false;

    double      mean, rms, stdev;
    QString     msg;
    const char  *unit,
                *swhere = (isNowOver ? "Mouse over" : "Last mouse-over");
    int         h,
                m;

    h = int(x / 3600);
    x = x - h * 3600;
    m = x / 60;
    x = x - m * 60;

    if( ic < p.ni.niCumTypCnt[CniCfg::niSumAnalog] ) {

        // neural readout

        computeGraphMouseOverVars( ic, y, mean, stdev, rms, unit );

        msg = QString(
            "%1 %2 @ pos (%3h%4m%5s, %6 %7)"
            " -- {mean, rms, stdv} %7: {%8, %9, %10}")
            .arg( swhere )
            .arg( STR2CHR( myChanName( ic ) ) )
            .arg( h, 2, 10, QChar('0') )
            .arg( m, 2, 10, QChar('0') )
            .arg( x, 0, 'f', 3 )
            .arg( y, 0, 'f', 4 )
            .arg( unit )
            .arg( mean, 0, 'f', 4 )
            .arg( rms, 0, 'f', 4 )
            .arg( stdev, 0, 'f', 4 );
    }
    else {

        // digital readout

        msg = QString(
            "%1 %2 @ pos %3h%4m%5s")
            .arg( swhere )
            .arg( STR2CHR( myChanName( ic ) ) )
            .arg( h, 2, 10, QChar('0') )
            .arg( m, 2, 10, QChar('0') )
            .arg( x, 0, 'f', 3 );
    }

    gw->statusBar()->showMessage( msg );
}


void SVGrafsM_Ni::myClickGraph( double x, double y, int iy )
{
    myMouseOverGraph( x, y, iy );
    selectChan( lastMouseOverChan );
}


double SVGrafsM_Ni::mySampRate()
{
    return p.ni.srate;
}


void SVGrafsM_Ni::mySort_ig2ic()
{
    if( set.usrOrder )
        p.sns.niChans.chanMap.userOrder( ig2ic );
    else
        p.sns.niChans.chanMap.defaultOrder( ig2ic );
}


QString SVGrafsM_Ni::myChanName( int ic ) const
{
    return p.sns.niChans.chanMap.name( ic, p.isTrigChan( "nidq", ic ) );
}


QBitArray& SVGrafsM_Ni::mySaveBits()
{
    return p.sns.niChans.saveBits;
}


// Return type number of digital channels, or -1 if none.
//
int SVGrafsM_Ni::mySetUsrTypes()
{
    int c0, cLim;

    c0      = 0;
    cLim    = p.ni.niCumTypCnt[CniCfg::niSumNeural];

    for( int ic = c0; ic < cLim; ++ic )
        ic2Y[ic].usrType = 0;

    c0      = p.ni.niCumTypCnt[CniCfg::niSumNeural];
    cLim    = p.ni.niCumTypCnt[CniCfg::niSumAnalog];

    for( int ic = c0; ic < cLim; ++ic )
        ic2Y[ic].usrType = 1;

    c0      = p.ni.niCumTypCnt[CniCfg::niSumAnalog];
    cLim    = p.ni.niCumTypCnt[CniCfg::niSumAll];

    for( int ic = c0; ic < cLim; ++ic )
        ic2Y[c0].usrType = 2;

    return 2;
}


void SVGrafsM_Ni::saveSettings()
{
// -------------------
// Appearance defaults
// -------------------

    STDSETTINGS( settings, "graphs_M_Ni" );

    settings.beginGroup( "All" );
    settings.setValue( "secs", set.secs );
    settings.setValue( "yscl0", set.yscl0 );
    settings.setValue( "yscl1", set.yscl1 );
    settings.setValue( "yscl2", set.yscl2 );
    settings.setValue( "clr0", clrToString( set.clr0 ) );
    settings.setValue( "clr1", clrToString( set.clr1 ) );
    settings.setValue( "clr2", clrToString( set.clr2 ) );
    settings.setValue( "navNChan", set.navNChan );
    settings.setValue( "bandSel", set.bandSel );
    settings.setValue( "filterChkOn", set.filterChkOn );
    settings.setValue( "dcChkOn", false );
    settings.setValue( "binMaxOn", set.binMaxOn );
    settings.setValue( "usrOrder", set.usrOrder );
    settings.endGroup();
}


// Called only from init().
//
void SVGrafsM_Ni::loadSettings()
{
// -------------------
// Appearance defaults
// -------------------

    STDSETTINGS( settings, "graphs_M_Ni" );

    settings.beginGroup( "All" );
    set.secs        = settings.value( "secs", 4.0 ).toDouble();
    set.yscl0       = settings.value( "yscl0", 1.0 ).toDouble();
    set.yscl1       = settings.value( "yscl1", 1.0 ).toDouble();
    set.yscl2       = settings.value( "yscl2", 1.0 ).toDouble();
    set.clr0        = clrFromString( settings.value( "clr0", "ffeedd82" ).toString() );
    set.clr1        = clrFromString( settings.value( "clr1", "ff44eeff" ).toString() );
    set.clr2        = clrFromString( settings.value( "clr2", "ff44eeff" ).toString() );
    set.navNChan    = settings.value( "navNChan", 32 ).toInt();
    set.bandSel     = settings.value( "bandSel", 0 ).toInt();
    set.filterChkOn = settings.value( "filterChkOn", false ).toBool();
    set.dcChkOn     = settings.value( "dcChkOn", false ).toBool();
    set.binMaxOn    = settings.value( "binMaxOn", true ).toBool();
    set.usrOrder    = settings.value( "usrOrder", false ).toBool();
    settings.endGroup();
}


// Values (v) are in range [-1,1].
// (v+1)/2 is in range [0,1].
// This is mapped to range [rmin,rmax].
//
double SVGrafsM_Ni::scalePlotValue( double v, double gain )
{
    return p.ni.range.unityToVolts( (v+1)/2 ) / gain;
}


// Call this only for neural channels!
//
void SVGrafsM_Ni::computeGraphMouseOverVars(
    int         ic,
    double      &y,
    double      &mean,
    double      &stdev,
    double      &rms,
    const char* &unit )
{
    double  gain = p.ni.chanGain( ic );

    y       = scalePlotValue( y, gain );

    drawMtx.lock();

    mean    = scalePlotValue( ic2stat[ic].mean() / MAX16BIT, gain );
    stdev   = scalePlotValue( ic2stat[ic].stdDev() / MAX16BIT, gain );
    rms     = scalePlotValue( ic2stat[ic].rms() / MAX16BIT, gain );

    drawMtx.unlock();

    double  vmax = p.ni.range.rmax / (ic2Y[ic].yscl * gain);

    unit = "V";

    if( vmax < 0.001 ) {
        y       *= 1e6;
        mean    *= 1e6;
        stdev   *= 1e6;
        rms     *= 1e6;
        unit     = "uV";
    }
    else if( vmax < 1.0 ) {
        y       *= 1e3;
        mean    *= 1e3;
        stdev   *= 1e3;
        rms     *= 1e3;
        unit     = "mV";
    }
}


