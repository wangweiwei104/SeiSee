

#include <stdio.h>

#include "cwp.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#else
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

/* Copyright (c) Colorado School of Mines, 2011.*/
/* All rights reserved.                       */

/*********************** self documentation **********************/
/*****************************************************************************
INTSINC8 - Functions to interpolate uniformly-sampled data via 8-coeff. sinc
        approximations:

ints8r	Interpolation of a uniformly-sampled real function y(x) via a
        table of 8-coefficient sinc approximations

******************************************************************************
Function Prototypes:
void ints8r (int nxin, float dxin, float fxin, float yin[],
    float yinl, float yinr, int nxout, float xout[], float yout[]);

******************************************************************************
Input:
nxin		number of x values at which y(x) is input
dxin		x sampling interval for input y(x)
fxin		x value of first sample input
yin		array[nxin] of input y(x) values:  yin[0] = y(fxin), etc.
yinl		value used to extrapolate yin values to left of yin[0]
yinr		value used to extrapolate yin values to right of yin[nxin-1]
nxout		number of x values a which y(x) is output
xout		array[nxout] of x values at which y(x) is output

Output:
yout		array[nxout] of output y(x):  yout[0] = y(xout[0]), etc.

******************************************************************************
Notes:
Because extrapolation of the input function y(x) is defined by the
left and right values yinl and yinr, the xout values are not restricted
to lie within the range of sample locations defined by nxin, dxin, and
fxin.

The maximum error for frequiencies less than 0.6 nyquist is less than
one percent.

******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
/**************** end self doc ********************************/

// #include "cwp.h"

/* these are used by both ints8c and ints8r */
#define LTABLE 8
#define NTABLE 513

void ints8r(int nxin, float dxin, float fxin, float yin[], float yinl,
            float yinr, int nxout, float xout[], float yout[])
/*****************************************************************************
Interpolation of a uniformly-sampled real function y(x) via a
table of 8-coefficient sinc approximations; maximum error for frequiencies
less than 0.6 nyquist is less than one percent.
******************************************************************************
Input:
nxin		number of x values at which y(x) is input
dxin		x sampling interval for input y(x)
fxin		x value of first sample input
yin		array[nxin] of input y(x) values:  yin[0] = y(fxin), etc.
yinl		value used to extrapolate yin values to left of yin[0]
yinr		value used to extrapolate yin values to right of yin[nxin-1]
nxout		number of x values a which y(x) is output
xout		array[nxout] of x values at which y(x) is output

Output:
yout		array[nxout] of output y(x):  yout[0] = y(xout[0]), etc.
******************************************************************************
Notes:
Because extrapolation of the input function y(x) is defined by the
left and right values yinl and yinr, the xout values are not restricted
to lie within the range of sample locations defined by nxin, dxin, and
fxin.
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/

{
    static float table[NTABLE][LTABLE];
    static int tabled = 0;
    int jtable;
    float frac;

    /* tabulate sinc interpolation coefficients if not already tabulated */
    if (!tabled) {
        for (jtable = 1; jtable < NTABLE - 1; jtable++) {
            frac = (float)jtable / (float)(NTABLE - 1);
            mksinc(frac, LTABLE, &table[jtable][0]);
        }
        for (jtable = 0; jtable < LTABLE; jtable++) {
            table[0][jtable] = 0.0;
            table[NTABLE - 1][jtable] = 0.0;
        }
        table[0][LTABLE / 2 - 1] = 1.0;
        table[NTABLE - 1][LTABLE / 2] = 1.0;
        tabled = 1;
    }

    /* interpolate using tabulated coefficients */
    intt8r(NTABLE, table, nxin, dxin, fxin, yin, yinl, yinr, nxout, xout, yout);
}

/* Copyright (c) Colorado School of Mines, 2011.*/
/* All rights reserved.                       */

/*********************** self documentation **********************/
/*****************************************************************************
MKSINC - Compute least-squares optimal sinc interpolation coefficients.

mksinc		Compute least-squares optimal sinc interpolation coefficients.

******************************************************************************
Function Prototype:
void mksinc (float d, int lsinc, float sinc[]);

******************************************************************************
Input:
d		fractional distance to interpolation point; 0.0<=d<=1.0
lsinc		length of sinc approximation; lsinc%2==0 and lsinc<=20

Output:
sinc		array[lsinc] containing interpolation coefficients

******************************************************************************
Notes:
The coefficients are a least-squares-best approximation to the ideal
sinc function for frequencies from zero up to a computed maximum
frequency.  For a given interpolator length, lsinc, mksinc computes
the maximum frequency, fmax (expressed as a fraction of the nyquist
frequency), using the following empirically derived relation (from
a Western Geophysical Technical Memorandum by Ken Larner):

    fmax = min(0.066+0.265*log(lsinc),1.0)

Note that fmax increases as lsinc increases, up to a maximum of 1.0.
Use the coefficients to interpolate a uniformly-sampled function y(i)
as follows:

            lsinc-1
    y(i+d) =  sum  sinc[j]*y(i+j+1-lsinc/2)
              j=0

Interpolation error is greatest for d=0.5, but for frequencies less
than fmax, the error should be less than 1.0 percent.

******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
/**************** end self doc ********************************/

// #include "cwp.h"

void mksinc(float d, int lsinc, float sinc[])
/*****************************************************************************
Compute least-squares optimal sinc interpolation coefficients.
******************************************************************************
Input:
d		fractional distance to interpolation point; 0.0<=d<=1.0
lsinc		length of sinc approximation; lsinc%2==0 and lsinc<=20

Output:
sinc		array[lsinc] containing interpolation coefficients
******************************************************************************
Notes:
The coefficients are a least-squares-best approximation to the ideal
sinc function for frequencies from zero up to a computed maximum
frequency.  For a given interpolator length, lsinc, mksinc computes
the maximum frequency, fmax (expressed as a fraction of the nyquist
frequency), using the following empirically derived relation (from
a Western Geophysical Technical Memorandum by Ken Larner):

    fmax = min(0.066+0.265*log(lsinc),1.0)

Note that fmax increases as lsinc increases, up to a maximum of 1.0.
Use the coefficients to interpolate a uniformly-sampled function y(i)
as follows:

            lsinc-1
    y(i+d) =  sum  sinc[j]*y(i+j+1-lsinc/2)
              j=0

Interpolation error is greatest for d=0.5, but for frequencies less
than fmax, the error should be less than 1.0 percent.
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
{
    int j;
    double s[20], a[20], c[20], work[20], fmax;

    /* compute auto-correlation and cross-correlation arrays */
    fmax = 0.066 + 0.265 * log((double)lsinc);
    fmax = (fmax < 1.0) ? fmax : 1.0;
    for (j = 0; j < lsinc; j++) {
        a[j] = dsinc(fmax * j);
        c[j] = dsinc(fmax * (lsinc / 2 - j - 1 + d));
    }

    /* solve symmetric Toeplitz system for the sinc approximation */
    stoepd(lsinc, a, c, s, work);
    for (j = 0; j < lsinc; j++)
        sinc[j] = s[j];
}

/* Copyright (c) Colorado School of Mines, 2011.*/
/* All rights reserved.                       */

/*********************** self documentation **********************/
/*****************************************************************************
SINC - Return SINC(x) for as floats or as doubles

fsinc		return float value of sinc(x) for x input as a float
dsinc		return double precision sinc(x) for double precision x

******************************************************************************
Function Prototype:
float fsinc (float x);
double dsinc (double x);

******************************************************************************
Input:
x		value at which to evaluate sinc(x)

Returned: 	sinc(x)

******************************************************************************
Notes:
    sinc(x) = sin(PI*x)/(PI*x)

******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
/**************** end self doc ********************************/

// #include "cwp.h"

float fsinc(float x)
/*****************************************************************************
Return sinc(x) = sin(PI*x)/(PI*x) (float version)
******************************************************************************
Input:
x		value at which to evaluate sinc(x)

Returned: 	sinc(x)
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
{
    float pix;

    if (x == 0.0) {
        return 1.0;
    } else {
        pix = M_PI * x;
        return sin(pix) / pix;
    }
}

double dsinc(double x)
/*****************************************************************************
Return sinc(x) = sin(PI*x)/(PI*x) (double version)
******************************************************************************
Input:
x		value at which to evaluate sinc(x)

Returned:	sinc(x)
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
{
    double pix;

    if (x == 0.0) {
        return 1.0;
    } else {
        pix = M_PI * x;
        return sin(pix) / pix;
    }
}

/* Copyright (c) Colorado School of Mines, 2011.*/
/* All rights reserved.                       */

/*********************** self documentation **********************/
/*****************************************************************************
STOEP - Functions to solve a symmetric Toeplitz linear system of equations
     Rf=g for f

stoepd		solve a symmetric Toeplitz system - doubles
stoepf		solve a symmetric Toeplitz system - floats

******************************************************************************
Function Prototypes:
void stoepd (int n, double r[], double g[], double f[], double a[]);
void stoepf (int n, float r[], float g[], float f[], float a[]);

******************************************************************************
Input:
n		dimension of system
r		array[n] of top row of Toeplitz matrix
g		array[n] of right-hand-side column vector

Output:
f		array[n] of solution (left-hand-side) column vector
a		array[n] of solution to Ra=v (Claerbout, FGDP, p. 57)

******************************************************************************
Notes:
These routines do NOT solve the case when the main diagonal is zero, it
just silently returns.

The left column of the Toeplitz matrix is assumed to be equal to the top
row (as specified in r); i.e., the Toeplitz matrix is assumed symmetric.

******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
/**************** end self doc ********************************/

// #include "cwp.h"

void stoepd(int n, double r[], double g[], double f[], double a[])
/*****************************************************************************
Solve a symmetric Toeplitz linear system of equations Rf=g for f
(double version)
******************************************************************************
Input:
n		dimension of system
r		array[n] of top row of Toeplitz matrix
g		array[n] of right-hand-side column vector

Output:
f		array[n] of solution (left-hand-side) column vector
a		array[n] of solution to Ra=v (Claerbout, FGDP, p. 57)
******************************************************************************
Notes:
This routine does NOT solve the case when the main diagonal is zero, it
just silently returns.

The left column of the Toeplitz matrix is assumed to be equal to the top
row (as specified in r); i.e., the Toeplitz matrix is assumed symmetric.
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
{
    int i, j;
    double v, e, c, w, bot;

    if (r[0] == 0.0)
        return;

    a[0] = 1.0;
    v = r[0];
    f[0] = g[0] / r[0];

    for (j = 1; j < n; j++) {

        /* solve Ra=v as in Claerbout, FGDP, p. 57 */
        a[j] = 0.0;
        f[j] = 0.0;
        for (i = 0, e = 0.0; i < j; i++)
            e += a[i] * r[j - i];
        c = e / v;
        v -= c * e;
        for (i = 0; i <= j / 2; i++) {
            bot = a[j - i] - c * a[i];
            a[i] -= c * a[j - i];
            a[j - i] = bot;
        }

        /* use a and v above to get f[i], i = 0,1,2,...,j */
        for (i = 0, w = 0.0; i < j; i++)
            w += f[i] * r[j - i];
        c = (w - g[j]) / v;
        for (i = 0; i <= j; i++)
            f[i] -= c * a[j - i];
    }
}

void stoepf(int n, float r[], float g[], float f[], float a[])
/*****************************************************************************
Solve a symmetric Toeplitz linear system of equations Rf=g for f
(float version)
******************************************************************************
Input:
n		dimension of system
r		array[n] of top row of Toeplitz matrix
g		array[n] of right-hand-side column vector

Output:
f		array[n] of solution (left-hand-side) column vector
a		array[n] of solution to Ra=v (Claerbout, FGDP, p. 57)
******************************************************************************
Notes:
This routine does NOT solve the case when the main diagonal is zero, it
just silently returns.

The left column of the Toeplitz matrix is assumed to be equal to the top
row (as specified in r); i.e., the Toeplitz matrix is assumed symmetric.
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
{
    int i, j;
    float v, e, c, w, bot;

    if (r[0] == 0.0)
        return;

    a[0] = 1.0;
    v = r[0];
    f[0] = g[0] / r[0];

    for (j = 1; j < n; j++) {

        /* solve Ra=v as in Claerbout, FGDP, p. 57 */
        a[j] = 0.0;
        f[j] = 0.0;
        for (i = 0, e = 0.0; i < j; i++)
            e += a[i] * r[j - i];
        c = e / v;
        v -= c * e;
        for (i = 0; i <= j / 2; i++) {
            bot = a[j - i] - c * a[i];
            a[i] -= c * a[j - i];
            a[j - i] = bot;
        }

        /* use a and v above to get f[i], i = 0,1,2,...,j */
        for (i = 0, w = 0.0; i < j; i++)
            w += f[i] * r[j - i];
        c = (w - g[j]) / v;
        for (i = 0; i <= j; i++)
            f[i] -= c * a[j - i];
    }
}

/* Copyright (c) Colorado School of Mines, 2011.*/
/* All rights reserved.                       */

/*********************** self documentation **********************/
/*****************************************************************************
INTTABLE8 -  Interpolation of a uniformly-sampled complex function y(x)
        via a table of 8-coefficient interpolators

intt8r	interpolation of a uniformly-sampled real function y(x) via a
        table of 8-coefficient interpolators

******************************************************************************
Function Prototype:
void intt8r (int ntable, float table[][8],
    int nxin, float dxin, float fxin, float yin[],
    float yinl, float yinr, int nxout, float xout[], float yout[]);

******************************************************************************
Input:
ntable		number of tabulated interpolation operators; ntable>=2
table		array of tabulated 8-point interpolation operators
nxin		number of x values at which y(x) is input
dxin		x sampling interval for input y(x)
fxin		x value of first sample input
yin		array of input y(x) values:  yin[0] = y(fxin), etc.
yinl		value used to extrapolate yin values to left of yin[0]
yinr		value used to extrapolate yin values to right of yin[nxin-1]
nxout		number of x values a which y(x) is output
xout		array of x values at which y(x) is output

Output:
yout		array of output y(x) values:  yout[0] = y(xout[0]), etc.

******************************************************************************
NOTES:
ntable must not be less than 2.

The table of interpolation operators must be as follows:

Let d be the distance, expressed as a fraction of dxin, from a particular
xout value to the sampled location xin just to the left of xout.  Then,
for d = 0.0,

table[0][0:7] = 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0

are the weights applied to the 8 input samples nearest xout.
Likewise, for d = 1.0,

table[ntable-1][0:7] = 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0

are the weights applied to the 8 input samples nearest xout.  In general,
for d = (float)itable/(float)(ntable-1), table[itable][0:7] are the
weights applied to the 8 input samples nearest xout.  If the actual sample
distance d does not exactly equal one of the values for which interpolators
are tabulated, then the interpolator corresponding to the nearest value of
d is used.

Because extrapolation of the input function y(x) is defined by the left
and right values yinl and yinr, the xout values are not restricted to lie
within the range of sample locations defined by nxin, dxin, and fxin.

******************************************************************************
AUTHOR:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
/**************** end self doc ********************************/

void intt8r(int ntable, float table[][8], int nxin, float dxin, float fxin,
            float yin[], float yinl, float yinr, int nxout, float xout[],
            float yout[])
{
    int ioutb, nxinm8, ixout, ixoutn, kyin, ktable, itable;
    float xoutb, xoutf, xouts, xoutn, frac, fntablem1, yini, sum, *yin0,
        *table00, *pyin, *ptable;

    /* compute constants */
    ioutb = -3 - 8;
    xoutf = fxin;
    xouts = 1.0 / dxin;
    xoutb = 8.0 - xoutf * xouts;
    fntablem1 = (float)(ntable - 1);
    nxinm8 = nxin - 8;
    yin0 = &yin[0];
    table00 = &table[0][0];

    /* loop over output samples */
    for (ixout = 0; ixout < nxout; ixout++) {
        /* determine pointers into table and yin */
        xoutn = xoutb + xout[ixout] * xouts;
        ixoutn = (int)xoutn;
        kyin = ioutb + ixoutn;
        pyin = yin0 + kyin;
        frac = xoutn - (float)ixoutn;
        ktable = frac >= 0.0 ? frac * fntablem1 + 0.5
                             : (frac + 1.0) * fntablem1 - 0.5;
        ptable = table00 + ktable * 8;

        /* if totally within input array, use fast method */
        if (kyin >= 0 && kyin <= nxinm8) {
            yout[ixout] = pyin[0] * ptable[0] + pyin[1] * ptable[1] +
                          pyin[2] * ptable[2] + pyin[3] * ptable[3] +
                          pyin[4] * ptable[4] + pyin[5] * ptable[5] +
                          pyin[6] * ptable[6] + pyin[7] * ptable[7];

            /* else handle end effects with care */
        } else {

            /* sum over 8 tabulated coefficients */
            for (itable = 0, sum = 0.0; itable < 8; itable++, kyin++) {
                if (kyin < 0)
                    yini = yinl;
                else if (kyin >= nxin)
                    yini = yinr;
                else
                    yini = yin[kyin];
                sum += yini * (*ptable++);
            }
            yout[ixout] = sum;
        }
    }
}

//--------------------------------------------------------------

void polygonalFilter(float *f, float *amps, int npoly, int nfft, float dt,
                     float *filter)
/*************************************************************************
polygonalFilter -- polygonal filter with sin^2 tapering
**************************************************************************
Input:
f		array[npoly] of frequencies defining the filter
amps		array[npoly] of amplitude values
npoly		size of input f and amps arrays
dt		time sampling interval
nfft		number of points in the fft

Output:
filter		array[nfft] filter values
**************************************************************************
Notes: Filter is to be applied in the frequency domain
**************************************************************************
Author:  CWP: John Stockwell   1992
*************************************************************************/
#define PIBY2 1.57079632679490
{
    int *intfr;      /* .... integerizations of f		*/
    int icount, ifs; /* loop counting variables              */
    int taper = 0;   /* flag counter				*/
    int nf;          /* number of frequencies (incl Nyq)     */
    int nfm1;        /* nf-1                                 */
    float onfft;     /* reciprocal of nfft                   */
    float df;        /* frequency spacing (from dt)          */

    intfr = new int[npoly]; // alloc1int(npoly);

    nf = nfft / 2 + 1;
    nfm1 = nf - 1;
    onfft = 1.0 / nfft;

    /* Compute array of integerized frequencies that define the filter*/
    df = onfft / dt;
    for (ifs = 0; ifs < npoly; ++ifs) {
        intfr[ifs] = NINT(f[ifs] / df);
        if (intfr[ifs] > nfm1)
            intfr[ifs] = nfm1;
    }

    /* Build filter, with scale, and taper specified by amps[] values*/
    /* Do low frequency end first*/
    for (icount = 0; icount < intfr[0]; ++icount)
        filter[icount] = amps[0] * onfft;

    /* now do the middle frequencies */
    for (ifs = 0; ifs < npoly - 1; ++ifs) {
        if (amps[ifs] < amps[ifs + 1]) {
            ++taper;
            for (icount = intfr[ifs]; icount <= intfr[ifs + 1]; ++icount) {
                float c = PIBY2 / (intfr[ifs + 1] - intfr[ifs] + 2);
                float s = sin(c * (icount - intfr[ifs] + 1));
                float adiff = amps[ifs + 1] - amps[ifs];
                filter[icount] = (amps[ifs] + adiff * s * s) * onfft;
            }
        } else if (amps[ifs] > amps[ifs + 1]) {
            ++taper;
            for (icount = intfr[ifs]; icount <= intfr[ifs + 1]; ++icount) {
                float c = PIBY2 / (intfr[ifs + 1] - intfr[ifs] + 2);
                float s = sin(c * (intfr[ifs + 1] - icount + 1));
                float adiff = amps[ifs] - amps[ifs + 1];
                filter[icount] = (amps[ifs + 1] + adiff * s * s) * onfft;
            }
        } else if (!(taper)) {
            for (icount = intfr[ifs]; icount <= intfr[ifs + 1]; ++icount)
                filter[icount] = amps[ifs] * onfft;
        } else {
            for (icount = intfr[ifs] + 1; icount <= intfr[ifs + 1]; ++icount)
                filter[icount] = amps[ifs] * onfft;
        }
    }

    /* finally do the high frequency end */
    for (icount = intfr[npoly - 1] + 1; icount < nf; ++icount) {
        filter[icount] = amps[npoly - 1] * onfft;
    }

    delete[] intfr;
}

#define PIBY2 1.57079632679490
#define FRAC0 0.10     /* Ratio of default f1 to Nyquist */
#define FRAC1 0.15     /* Ratio of default f2 to Nyquist */
#define FRAC2 0.45     /* Ratio of default f3 to Nyquist */
#define FRAC3 0.50     /* Ratio of default f4 to Nyquist */
#define LOOKFAC 2      /* Look ahead factor for npfao    */
#define PFA_MAX 720720 /* Largest allowed nfft           */

int CwpFilter::Init(int Nsmp, float Dt, int Nf, float *F, int Na, float *Amps)
{
    Clear();

    int icount, ifs, iamps; /* loop counting variables              */
    nt = Nsmp;
    dt = Dt;
    nyq = 0.5 / dt;

    // Set up FFT parameters
    nfft = npfaro(nt, LOOKFAC * nt);
    /*
        if (nfft >= SU_NFLTS || nfft >= PFA_MAX)
        {
           Err("Padded nt -- too big");
           return -1;
        }
    */
    nf = nfft / 2 + 1;

    if (Nf == 0) {
        npoly = 4;
        f = new float[npoly];
        f[0] = FRAC0 * nyq;
        f[1] = FRAC1 * nyq;
        f[2] = FRAC2 * nyq;
        f[3] = FRAC3 * nyq;
    } else {
        npoly = Nf;
        f = new float[npoly];
        memcpy(f, F, sizeof(float) * npoly);
    }

    // Check f values
    if (npoly < 2) {
        // warn("Only %d value defining filter",npoly);
    }

    for (ifs = 0; ifs < npoly - 1; ++ifs) {
        if (f[ifs] < 0.0 || f[ifs] > f[ifs + 1]) {
            Err("Bad filter parameters");
            return -2;
        }
    }

    if (Na != 0) {
        namps = Na;
        amps = new float[Na];
        memcpy(amps, Amps, sizeof(float) * namps);
    } else {
        namps = npoly;
        amps = new float[namps];

        // default is a trapezoidal bandpass filter
        for (iamps = 0; iamps < namps; ++iamps)
            amps[iamps] = 1.;
        amps[0] = 0.;
        amps[namps - 1] = 0.;
    }

    if (!(namps == npoly)) {
        Err("number of f values must = number of amps values");
        return -3;
    }

    // Check amps values
    for (iamps = 0, icount = 0; iamps < namps; ++iamps) {
        if (amps[iamps] > 0.)
            ++icount;

        if (amps[iamps] < 0.) {
            Err("amp values must be positive");
            return -4;
        }
    }

    if (icount == 0) {
        Err("All amps values are zero");
        return -5;
    }

    for (iamps = 0, icount = 0; iamps < namps - 1; ++iamps) {
        if (!(amps[iamps] == amps[iamps + 1]))
            ++icount;
    }

    if (icount == 0) {
        // warn("All amps values are the same");
    }

    // Allocate fft arrays
    rt = new float[nfft];   // ealloc1float(nfft);
    ct = new complex[nf];   // ealloc1complex(nf);
    filter = new float[nf]; // ealloc1float(nf);

    // Build the polygonal filter
    polygonalFilter(f, amps, npoly, nfft, dt, filter);

    ok = 1;

    return 1;
}

int CwpFilter::Apply(float *Inp, float *Out)
{
    int i;

    if (Out == NULL)
        Out = Inp;

    if (ok < 1) {
        if (Out != Inp)
            memcpy(Out, Inp, sizeof(float) * nt);
        return ok;
    }

    memcpy((void *)rt, (const void *)Inp, nt * sizeof(float));
    memset((void *)(rt + nt), 0, (nfft - nt) * sizeof(float));

    // FFT, filter, inverse FFT
    pfarc(1, nfft, rt, ct);
    for (i = 0; i < nf; ++i)
        ct[i] = crmul(ct[i], filter[i]);
    pfacr(-1, nfft, ct, rt);

    // Load traces back in, recall filter had nfft factor
    for (i = 0; i < nt; ++i)
        Out[i] = rt[i];

    return 1;
}

//---------------------------------------------------------------------------------------------

/* Copyright (c) Colorado School of Mines, 2011.*/
/* All rights reserved.                       */

/*********************** self documentation **********************/
/*****************************************************************************
YXTOXY - Compute a regularly-sampled, monotonically increasing function x(y)
    from a regularly-sampled, monotonically increasing function y(x) by
    inverse linear interpolation.

yxtoxy		compute a regularly sampled function x(y) from a regularly
        sampled, monotonically increasing function y(x)

******************************************************************************
Function Prototype:
void yxtoxy (int nx, float dx, float fx, float y[],
    int ny, float dy, float fy, float xylo, float xyhi, float x[]);

******************************************************************************
Input:
nx		number of samples of y(x)
dx		x sampling interval; dx>0.0 is required
fx		first x
y		array[nx] of y(x) values; y[0] < y[1] < ... < y[nx-1] required
ny		number of samples of x(y)
dy		y sampling interval; dy>0.0 is required
fy		first y
xylo		x value assigned to x(y) when y is less than smallest y(x)
xyhi		x value assigned to x(y) when y is greater than largest y(x)

Output:
x		array[ny] of x(y) values

******************************************************************************
Notes:
User must ensure that:
(1) dx>0.0 && dy>0.0
(2) y[0] < y[1] < ... < y[nx-1]

******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
/**************** end self doc ********************************/

void yxtoxy(int nx, float dx, float fx, float y[], int ny, float dy, float fy,
            float xylo, float xyhi, float x[])
/*****************************************************************************
Compute a regularly-sampled, monotonically increasing function x(y) from a
regularly-sampled, monotonically increasing function y(x) by inverse linear
interpolation.
******************************************************************************
Input:
nx		number of samples of y(x)
dx		x sampling interval; dx>0.0 is required
fx		first x
y		array[nx] of y(x) values; y[0] < y[1] < ... < y[nx-1] required
ny		number of samples of x(y)
dy		y sampling interval; dy>0.0 is required
fy		first y
xylo		x value assigned to x(y) when y is less than smallest y(x)
xyhi		x value assigned to x(y) when y is greater than largest y(x)

Output:
x		array[ny] of x(y) values
******************************************************************************
Notes:
User must ensure that:
(1) dx>0.0 && dy>0.0
(2) y[0] < y[1] < ... < y[nx-1]
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
{
    int nxi, nyo, jxi1, jxi2, jyo;
    float dxi, fxi, dyo, fyo, fyi, yo, xi1, yi1, yi2;

    nxi = nx;
    dxi = dx;
    fxi = fx;
    nyo = ny;
    dyo = dy;
    fyo = fy;
    fyi = y[0];

    /* loop over output y less than smallest input y */
    for (jyo = 0, yo = fyo; jyo < nyo; jyo++, yo += dyo) {
        if (yo >= fyi)
            break;
        x[jyo] = xylo;
    }

    /* loop over output y between smallest and largest input y */
    if (jyo == nyo - 1 && yo == fyi) {
        x[jyo++] = fxi;
        yo += dyo;
    }
    jxi1 = 0;
    jxi2 = 1;
    xi1 = fxi;
    while (jxi2 < nxi && jyo < nyo) {
        yi1 = y[jxi1];
        yi2 = y[jxi2];
        if (yi1 <= yo && yo <= yi2) {
            x[jyo++] = xi1 + dxi * (yo - yi1) / (yi2 - yi1);
            yo += dyo;
        } else {
            jxi1++;
            jxi2++;
            xi1 += dxi;
        }
    }

    /* loop over output y greater than largest input y */
    while (jyo < nyo)
        x[jyo++] = xyhi;
}

static void zttz(int nt, float dt, float ft, float zt[], float vft, float vlt,
                 int nz, float dz, float fz, float tz[])
/************************************************************************
zttz - compute t(z) from z(t)
*************************************************************************
Input:
nt	number of time samples
dt	time sampling interval
ft	first time value
zt[]	array of z values as a function of time
vft	velocity at first time sample
vlt	velocity at last time sample
nz	number of z values  (output)
dz	depth sampling interval (output)
fz	first z value
Output:
tz[] 	array of time values as a function of depth
*************************************************************************
Author: CWP: Dave Hale (c. 1992)
************************************************************************/
{
    int iz;                        /* depth counter */
    float z;                       /* depth */
    float lt = ft + (nt - 1) * dt; /* last time */
    float lz = fz + (nz - 1) * dz; /* last depth */

    /* switch from z(t) to t(z) */
    yxtoxy(nt, dt, ft, zt, nz, dz, fz, 0.0, 0.0, tz);

    /* for z values before fz, use first velocity to calculate t(z) */
    for (iz = 0, z = fz; z <= zt[0]; iz++, z += dz)
        tz[iz] = 2.0 * z / vft;

    /* for z values from lz down to fz, calculate t(z) */
    for (iz = nz - 1, z = lz; z >= zt[nt - 1]; iz--, z -= dz)
        tz[iz] = lt + 2.0 * (z - zt[nt - 1]) / vlt;
}

void maketz(int nt, float dt, float ft, float v[], int nz, float dz, float fz,
            float t[])
/************************************************************************
maketz - compute t(z) from v(t)
*************************************************************************
Input:
nt	number of time samples
dt	time sampling interval
ft	first time sample
v[]	array of velocities as a function of time t
nz	number of z values (output)
dz	depth sampling interval (output)
fz	first depth value (output)
Output:
t[]	array of t values as a function of z
*************************************************************************
Author: CWP: Dave Hale (c. 1992)
*************************************************************************/
{
    int it;          /* counter */
    float vft;       /* velocity at the first time sample */
    float vlt;       /* velocity at the last time sample */
    float *z = NULL; /* array of depth values as a function of t */

    /* allocate space */
    z = new float[nt];

    /* calculate z(t) from v(t) */
    z[0] = 0.5 * ft * v[0];

    for (it = 1; it < nt; it++) {
        z[it] = z[it - 1] + 0.5 * dt * v[it - 1];
    }

    vft = v[0];
    vlt = v[nt - 1];

    /* compute t(z) from z(t) */
    zttz(nt, dt, ft, z, vft, vlt, nz, dz, fz, t);

    delete[] z;
}

void maketz2(int nt, float dt, float ft, float v[], int nz, float dz, float fz,
             float t[])
/************************************************************************
maketz - compute t(z) from v(t)
*************************************************************************
Input:
nt	number of time samples
dt	time sampling interval
ft	first time sample
v[]	array of velocities as a function of time t
nz	number of z values (output)
dz	depth sampling interval (output)
fz	first depth value (output)
Output:
t[]	array of t values as a function of z
*************************************************************************
Глубины вычисляются как t*v*0.5 (PSI 2017)
*************************************************************************/
{
    int it;          /* counter */
    float vft;       /* velocity at the first time sample */
    float vlt;       /* velocity at the last time sample */
    float *z = NULL; /* array of depth values as a function of t */

    /* allocate space */
    z = new float[nt];

    /* calculate z(t) from v(t) */
    z[0] = 0.5 * ft * v[0];

    for (it = 1; it < nt; it++) {
        z[it] = (it * dt * 0.5 + ft) * v[it];
    }

    vft = v[0];
    vlt = v[nt - 1];

    /* compute t(z) from z(t) */
    zttz(nt, dt, ft, z, vft, vlt, nz, dz, fz, t);

    delete[] z;
}

/*********************** self documentation **********************/
/*****************************************************************************
INTLIN - evaluate y(x) via linear interpolation of y(x[0]), y(x[1]), ...

intlin		evaluate y(x) via linear interpolation of y(x[0]), y(x[1]), ...

******************************************************************************
Function Prototype:
void intlin (int nin, float xin[], float yin[], float yinl, float yinr,
    int nout, float xout[], float yout[]);

******************************************************************************
Input:
nin		length of xin and yin arrays
xin		array[nin] of monotonically increasing or decreasing x values
yin		array[nin] of input y(x) values
yinl		value used to extraplate y(x) to left of input yin values
yinr		value used to extraplate y(x) to right of input yin values
nout		length of xout and yout arrays
xout		array[nout] of x values at which to evaluate y(x)

Output:
yout		array[nout] of linearly interpolated y(x) values

******************************************************************************
Notes:
xin values must be monotonically increasing or decreasing.

Extrapolation of the function y(x) for xout values outside the range
spanned by the xin values in performed as follows:

    For monotonically increasing xin values,
        yout=yinl if xout<xin[0], and yout=yinr if xout>xin[nin-1].

    For monotonically decreasing xin values,
        yout=yinl if xout>xin[0], and yout=yinr if xout<xin[nin-1].

If nin==1, then the monotonically increasing case is used.

******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
/**************** end self doc ********************************/

/*********************** self documentation **********************/
/*****************************************************************************
XINDEX - determine index of x with respect to an array of x values

xindex		determine index of x with respect to an array of x values

******************************************************************************
Input:
nx		number of x values in array ax
ax		array[nx] of monotonically increasing or decreasing x values
x		the value for which index is to be determined
index		index determined previously (used to begin search)

Output:
index		for monotonically increasing ax values, the largest index
        for which ax[index]<=x, except index=0 if ax[0]>x;
        for monotonically decreasing ax values, the largest index
        for which ax[index]>=x, except index=0 if ax[0]<x

******************************************************************************
Notes:
This function is designed to be particularly efficient when called
repeatedly for slightly changing x values; in such cases, the index
returned from one call should be used in the next.

******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 12/25/89
*****************************************************************************/
/**************** end self doc ********************************/

void xindex(int nx, float ax[], float x, int *index)
/*****************************************************************************
determine index of x with respect to an array of x values
******************************************************************************
Input:
nx		number of x values in array ax
ax		array[nx] of monotonically increasing or decreasing x values
x		the value for which index is to be determined
index		index determined previously (used to begin search)

Output:
index		for monotonically increasing ax values, the largest index
        for which ax[index]<=x, except index=0 if ax[0]>x;
        for monotonically decreasing ax values, the largest index
        for which ax[index]>=x, except index=0 if ax[0]<x
******************************************************************************
Notes:
This function is designed to be particularly efficient when called
repeatedly for slightly changing x values; in such cases, the index
returned from one call should be used in the next.
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 12/25/89
*****************************************************************************/
{
    int lower, upper, middle, step;

    /* initialize lower and upper indices and step */
    lower = *index;
    if (lower < 0)
        lower = 0;
    if (lower >= nx)
        lower = nx - 1;
    upper = lower + 1;
    step = 1;

    /* if x values increasing */
    if (ax[nx - 1] > ax[0]) {

        /* find indices such that ax[lower] <= x < ax[upper] */
        while (lower > 0 && ax[lower] > x) {
            upper = lower;
            lower -= step;
            step += step;
        }
        if (lower < 0)
            lower = 0;
        while (upper < nx && ax[upper] <= x) {
            lower = upper;
            upper += step;
            step += step;
        }
        if (upper > nx)
            upper = nx;

        /* find index via bisection */
        while ((middle = (lower + upper) >> 1) != lower) {
            if (x >= ax[middle])
                lower = middle;
            else
                upper = middle;
        }

        /* else, if not increasing */
    } else {

        /* find indices such that ax[lower] >= x > ax[upper] */
        while (lower > 0 && ax[lower] < x) {
            upper = lower;
            lower -= step;
            step += step;
        }
        if (lower < 0)
            lower = 0;
        while (upper < nx && ax[upper] >= x) {
            lower = upper;
            upper += step;
            step += step;
        }
        if (upper > nx)
            upper = nx;

        /* find index via bisection */
        while ((middle = (lower + upper) >> 1) != lower) {
            if (x <= ax[middle])
                lower = middle;
            else
                upper = middle;
        }
    }

    /* return lower index */
    *index = lower;
}

#include "cwp.h"

void intlin(int nin, float xin[], float yin[], float yinl, float yinr, int nout,
            float xout[], float yout[])
/*****************************************************************************
evaluate y(x) via linear interpolation of y(x[0]), y(x[1]), ...
******************************************************************************
Input:
nin		length of xin and yin arrays
xin		array[nin] of monotonically increasing or decreasing x values
yin		array[nin] of input y(x) values
yinl		value used to extraplate y(x) to left of input yin values
yinr		value used to extraplate y(x) to right of input yin values
nout		length of xout and yout arrays
xout		array[nout] of x values at which to evaluate y(x)

Output:
yout		array[nout] of linearly interpolated y(x) values
******************************************************************************
Notes:
xin values must be monotonically increasing or decreasing.

Extrapolation of the function y(x) for xout values outside the range
spanned by the xin values in performed as follows:

    For monotonically increasing xin values,
        yout=yinl if xout<xin[0], and yout=yinr if xout>xin[nin-1].

    For monotonically decreasing xin values,
        yout=yinl if xout>xin[0], and yout=yinr if xout<xin[nin-1].

If nin==1, then the monotonically increasing case is used.
******************************************************************************
Author:  Dave Hale, Colorado School of Mines, 06/02/89
*****************************************************************************/
{
    static int idx;
    int jout;
    float x;

    /* if input x values are monotonically increasing, then */
    if (xin[0] <= xin[nin - 1]) {
        for (jout = 0; jout < nout; jout++) {
            x = xout[jout];
            if (x < xin[0])
                yout[jout] = yinl;
            else if (x > xin[nin - 1])
                yout[jout] = yinr;
            else if (x == xin[nin - 1] || nin == 1)
                yout[jout] = yin[nin - 1];
            else {
                xindex(nin, xin, x, &idx);
                yout[jout] = yin[idx] + (x - xin[idx]) *
                                            (yin[idx + 1] - yin[idx]) /
                                            (xin[idx + 1] - xin[idx]);
            }
        }

        /* else, if input x values are monotonically decreasing, then */
    } else {
        for (jout = 0; jout < nout; jout++) {
            x = xout[jout];
            if (x > xin[0])
                yout[jout] = yinl;
            else if (x < xin[nin - 1])
                yout[jout] = yinr;
            else if (x == xin[nin - 1] || nin == 1)
                yout[jout] = yin[nin - 1];
            else {
                xindex(nin, xin, x, &idx);
                yout[jout] = yin[idx] + (x - xin[idx]) *
                                            (yin[idx + 1] - yin[idx]) /
                                            (xin[idx + 1] - xin[idx]);
            }
        }
    }
}

void suttoz()
{
    int nt; // number of time samples
    int it; // counter
    int nz; // numer of depth samples

    int ntpar; // number of getparred time values for velocities
    int nvpar; // number of getparred velocity values
    int itpar; // counter

    float dt;   // time sampling interval
    float ft;   // first time value
    float dz;   // depth sampling interval for velocities
    float fz;   // first depth value
    float t;    // timevalues for velocities
    float vmin; // minimum velocity
    float vmax; // maximum velocity

    float *tpar = NULL; // values of t getparred
    float *vpar = NULL; // values of v getparred

    float *vt = NULL;   // v(t) velocity as a function of t
    float *tz = NULL;   // t(z) time as a function of z
    float *temp = NULL; // temporary storage array

    float *trs; // trace samples

    //    v=0.0,.15,.2 v=1500,2000,3000

    ntpar = nvpar = 3;

    vpar = new float[nvpar];
    tpar = new float[ntpar];

    nt = 1000; // tr.ns;

    vt = new float[nt];

    tpar[0] = 0;
    tpar[1] = 0.15;
    tpar[2] = 0.2;

    vpar[0] = 1500;
    vpar[1] = 2000;
    vpar[2] = 3000;

    for (it = 0, t = 0.0; it < nt; ++it, t += dt)
        intlin(ntpar, tpar, vpar, vpar[0], vpar[ntpar - 1], 1, &t, &vt[it]);

    delete[] vpar;
    delete[] tpar;
    delete[] vt;

    return;

    /* determine minimum and maximum velocities */
    for (it = 1, vmin = vmax = vt[0]; it < nt; ++it) {
        if (vt[it] < vmin)
            vmin = vt[it];
        if (vt[it] > vmax)
            vmax = vt[it];
    }

    // allocate workspace
    tz = new float[nz];
    temp = new float[nt];

    // make t(z) function
    maketz(nt, dt, ft, vt, nz, dz, fz, tz);

    /*
        tr.trid = TRID_DEPTH;
        tr.ns = nz;
        tr.d1 = dz;
        tr.f1 = fz;
    */
    // resample //
    memcpy((void *)temp, (const void *)trs, nt * sizeof(float));
    ints8r(nt, dt, ft, temp, 0.0, 0.0, nz, tz, trs);

    delete[] vpar;
    delete[] tpar;
}

int CwpTimeToDepth::Exec(int Nt, float Dt, int Nv, float *V, float *T, float Dz,
                         float *Ttr, float *Dtr)
{
    int it;
    float t;

    Clear();

    nt = Nt;
    dt = Dt;
    dz = Dz;

    vt = new float[nt];

    for (it = 0, t = 0.0; it < nt; ++it, t += dt)
        intlin(Nv, T, V, V[0], V[Nv - 1], 1, &t, &vt[it]);

    // determine minimum and maximum velocities
    for (it = 1, vmin = vmax = vt[0]; it < nt; ++it) {
        if (vt[it] < vmin)
            vmin = vt[it];
        if (vt[it] > vmax)
            vmax = vt[it];
    }

    // ft = tr.delrt/1000.0;

    ft = 0;
    dz = vmin * dt / 2.0;
    fz = vt[0] * ft / 2.0;
    nz = 1 + (nt - 1) * dt * vmax / (2.0 * dz);

    // allocate workspace
    tz = new float[nz];

    ints8r(nt, dt, ft, Ttr, 0.0, 0.0, nz, tz, Dtr);

    return nz;
}
