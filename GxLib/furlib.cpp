//---------------------------------------------------------------------------

// #include <vcl.h>

#include "furlib.h"

#include <math.h>
#include <string.h>

#include "util2.h"

//---------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4305 4244)
#endif

#define ormsby 1
#define butterworth 2

static float *MAlloc(long size)
{
    return (float *)malloc(size);
}

static void MFree(float *m)
{
    free(m);
}

void FastFourierTransformation(float *cxxr, float *cxxi, int lx, int isign)
{
    int i, j, il, m, l, istep;
    int ii, jj, ill;
    float sc, ctempr, ctempi, cargi, cwr, cwi;
    float *cxr = cxxr; //-1;
    float *cxi = cxxi; //-1;
    float signi = isign;

    j = 1;
    sc = sqrt(1. / lx);

    for (i = 1; i <= lx; i++) // do 30
    {
        if (i > j)
            goto l10;
        {
            jj = j - 1;
            ctempr = cxr[jj] * sc; // ctemp = sc*cx[j]
            ctempi = cxi[jj] * sc;

            ii = i - 1;
            cxr[jj] = cxr[ii] * sc; // cx[j] = sc*cx[i]
            cxi[jj] = cxi[ii] * sc;

            cxr[ii] = ctempr; // cx[i] = ctemp
            cxi[ii] = ctempi;
        }
    l10:
        m = lx >> 1;

    l20:
        if (j <= m)
            goto l30;

        j -= m;
        m >>= 1;

        if (m >= 1)
            goto l20;

    l30:
        j += m;
    } // 30

    l = 1;

l40:
    istep = l << 1;

    for (m = 1; m <= l; m++) // do 50 (1)
    {
        cargi = (M_PI * signi * (m - 1)) / l;

        //!!!   cwr = hcos(cargi);        //cexp(cwr) - Euler's formula
        //!!!   cwi = hsin(cargi);

        cwr = cos(cargi); // cexp(cwr) - Euler's formula
        cwi = sin(cargi);

        for (i = m; i <= lx; i += istep) // do 50 (2)
        {
            ii = i - 1;
            il = i + l;
            ill = il - 1;
            ctempr = cwr * cxr[ill] - cwi * cxi[ill]; // ctemp=cw*cx[i+l]
            ctempi = cwr * cxi[ill] + cwi * cxr[ill];

            cxr[ill] = cxr[ii] - ctempr; // cx[i+l]=cx[i]-ctemp
            cxi[ill] = cxi[ii] - ctempi;

            cxr[ii] = cxr[ii] + ctempr; // cx[i]=cx[i]+ctemp
            cxi[ii] = cxi[ii] + ctempi;
            // l50:
        }
    }
    l = istep;
    if (l < lx)
        goto l40;
}

int HilbertTransformation(int nn, float inp[], float outr[], float outi[])
{
    long nf;
    int j, k, n, n2, padlen;
    float xr, xi, qr, qi, re, im;

    if (nn <= 0)
        return -1;

    float *fftdatar = NULL;
    float *fftdatai = NULL;

    for (nf = 1; nf < nn; nf <<= 1)
        if (nf == 32768L)
            return -1;

    fftdatar = MAlloc(sizeof(float) * nf);
    if (fftdatar == NULL)
        goto ERREXIT;

    fftdatai = MAlloc(sizeof(float) * nf);
    if (fftdatai == NULL)
        goto ERREXIT;

    padlen = (long)(nf - nn);
    if (padlen > 0) {
        memset(fftdatar, 0, sizeof(float) * nf);
        memset(fftdatai, 0, sizeof(float) * nf);
    }

    n2 = nf / 2;
    for (n = 0; n < nn; n++) {
        fftdatar[n] = inp[n];
        fftdatai[n] = 0;
    }

    FastFourierTransformation(fftdatar, fftdatai, nf, 1);

    for (n = 0; n < nf; n++) {
        if (n < n2) {
            j = n + n2;
            k = 0;
        } else {
            j = n - n2;
            k = 2;
        }

        qr = k;
        qi = 0;

        xr = fftdatar[j];
        xi = fftdatai[j];

        fftdatar[j] = xr * qr - xi * qi;
        fftdatai[j] = xr * qi + xi * qr;
    }

    FastFourierTransformation(fftdatar, fftdatai, nf, -1);

    for (n = 0; n < nn; n++) {
        re = fftdatar[n];
        im = fftdatai[n];

        outr[n] = re;
        outi[n] = im;
    }

    MFree(fftdatar);
    MFree(fftdatai);

    return 0;

ERREXIT:

    if (fftdatar)
        MFree(fftdatar);
    if (fftdatai)
        MFree(fftdatai);

    return -1;
}

int Spectrum1(float *inp, float *out, float si, int nn)
{
    long nf;
    int n, padlen;
    int nm;
    float re;

    if (nn <= 0 || si <= 0)
        return -1;

    float *fftdatar = NULL;
    float *fftdatai = NULL;

    for (nf = 1; nf < nn; nf <<= 1)
        if (nf == 32768L)
            return -1;

    fftdatar = MAlloc(sizeof(float) * nf);
    if (fftdatar == NULL)
        goto ERREXIT;

    fftdatai = MAlloc(sizeof(float) * nf);
    if (fftdatai == NULL)
        goto ERREXIT;

    padlen = nf - nn;
    if (padlen > 0) {
        memset(fftdatar, 0, sizeof(float) * nf);
        memset(fftdatai, 0, sizeof(float) * nf);
    }

    nm = nf / 2;

    for (n = 0; n < nn; n++) {
        fftdatar[n] = inp[n];
        fftdatai[n] = 0;
    }

    FastFourierTransformation(fftdatar, fftdatai, nf, 1);

    for (n = 0; n < nn; n++) {
        if (n < nm)
            re = sqrt(fftdatar[n] * fftdatar[n] + fftdatai[n] * fftdatai[n]);
        else
            re = 0;

        out[n] = re;
    }

    MFree(fftdatar);
    MFree(fftdatai);

    return 0;

ERREXIT:

    if (fftdatar)
        MFree(fftdatar);
    if (fftdatai)
        MFree(fftdatai);

    return -1;
}

int BandPassFilter(float *inp, float *out, float f1, float f2, float f3,
                   float f4, float si, int nn)
{
    long nf;
    int n, padlen;
    int n1, n2, n3, n4, nm;
    float k1, k2, a;

    if (nn <= 0 || si <= 0)
        return -1;

    float *fftdatar = NULL;
    float *fftdatai = NULL;

    //  for(nf=1; nf<nn; nf<<=1) if(nf==32768L) return -1;
    for (nf = 1; nf < nn; nf <<= 1)
        if ((unsigned)nf == 0x80000000)
            return -1;

    n1 = f1 * (nf * si);
    n2 = f2 * (nf * si);
    n3 = f3 * (nf * si);
    n4 = f4 * (nf * si);

    if (!(n1 < n2 && n2 < n3 && n3 < n4))
        return -1;

    k1 = M_PI / (n2 - n1);
    k2 = M_PI / (n4 - n3);

    fftdatar = MAlloc(sizeof(float) * nf);
    if (fftdatar == NULL)
        goto ERREXIT;

    fftdatai = MAlloc(sizeof(float) * nf);
    if (fftdatai == NULL)
        goto ERREXIT;

    padlen = nf - nn;
    if (padlen > 0) {
        memset(fftdatar, 0, sizeof(float) * nf);
        memset(fftdatai, 0, sizeof(float) * nf);
    }

    memcpy(fftdatar, inp, nn * sizeof(float));
    memset(fftdatai, 0, nn * sizeof(float));

    /*
        for(n=0;n<nn;n++)
         {
            fftdatar[n]=inp[n];
            fftdatai[n]=0;
         }
    */

    FastFourierTransformation(fftdatar, fftdatai, nf, 1);

    nm = nf / 2;

    for (n = 0; n <= nm; n++) {
        a = 1;

        if (n >= n1 && n < n2) {
            a = 0.5 * (1 - hcos(k1 * (n - n1)));
        } else if (n >= n3 && n < n4) {
            a = 0.5 * (1 + hcos(k2 * (n - n3)));
        } else if (n >= n2 && n < n3) {
            a = 1;
        } else {
            a = 0;
        }

        if (n != nm) {
            fftdatar[n] *= a;
            fftdatai[n] *= a;
        }

        if (n != 0) {
            fftdatar[nf - n] *= a;
            fftdatai[nf - n] *= a;
        }
    }
    FastFourierTransformation(fftdatar, fftdatai, nf, -1);

    memcpy(out, fftdatar, nn * sizeof(float));
    /*
        for(n=0;n<nn;n++)
         {
            re=fftdatar[n];
            out[n]=re;
         }
    */

    MFree(fftdatar);
    MFree(fftdatai);

    return 0;

ERREXIT:

    if (fftdatar)
        MFree(fftdatar);
    if (fftdatai)
        MFree(fftdatai);

    return -1;
}

int LowPassFilter(float *inp, float *out, float f3, float f4, float si, int nn)
{
    long nf;
    int n, padlen;
    int n3, n4, nm;
    float k2, a;

    if (nn <= 0 || si <= 0)
        return -1;

    float *fftdatar = NULL;
    float *fftdatai = NULL;

    //  for(nf=1; nf<nn; nf<<=1) if(nf==32768L) return -1;
    for (nf = 1; nf < nn; nf <<= 1)
        if ((unsigned)nf == 0x80000000)
            return -1;

    n3 = f3 * (nf * si);
    n4 = f4 * (nf * si);

    if (!(n3 < n4))
        return -1;

    k2 = M_PI / (n4 - n3);

    fftdatar = MAlloc(sizeof(float) * nf);
    if (fftdatar == NULL)
        goto ERREXIT;

    fftdatai = MAlloc(sizeof(float) * nf);
    if (fftdatai == NULL)
        goto ERREXIT;

    padlen = nf - nn;
    if (padlen > 0) {
        memset(fftdatar, 0, sizeof(float) * nf);
        memset(fftdatai, 0, sizeof(float) * nf);
    }

    memcpy(fftdatar, inp, nn * sizeof(float));
    memset(fftdatai, 0, nn * sizeof(float));

    FastFourierTransformation(fftdatar, fftdatai, nf, 1);

    nm = nf / 2;

    for (n = 0; n <= nm; n++) {
        a = 1;

        if (n >= n3 && n < n4) {
            a = 0.5 * (1 + hcos(k2 * (n - n3)));
        } else if (n < n3) {
            a = 1;
        } else {
            a = 0;
        }

        if (n != nm) {
            fftdatar[n] *= a;
            fftdatai[n] *= a;
        }

        if (n != 0) {
            fftdatar[nf - n] *= a;
            fftdatai[nf - n] *= a;
        }
    }
    FastFourierTransformation(fftdatar, fftdatai, nf, -1);

    memcpy(out, fftdatar, nn * sizeof(float));

    MFree(fftdatar);
    MFree(fftdatai);

    return 0;

ERREXIT:

    if (fftdatar)
        MFree(fftdatar);
    if (fftdatai)
        MFree(fftdatai);

    return -1;
}

//----------------------------------------------------------------

int Spectrum(float *inp, int *out, int StartFreq, int EndFreq)
{
    /*
    Input Parameters
    ----------------
    inp       - input trace
    out[0]    - length of input and output trace (# of samples)
    out[1]    - sample interval (microseconds)
    out[2]    - length of output array (# of samples)
    StartFreq - lowest  frequency in output spectrum (Hz)
    EndFreq   - highest frequency in output spectrum (Hz)

    Output Parameters
    -----------------
    out     - output amplitude sectrum

    Return code
    -----------------
    0 - OK
    -1 - Can't allocate memory for working arrays or bad parameters
    */

    long nf;
    int i, nn, n, padlen, n2, no;
    float re, im, df, f, f1, f2, si, ff, k, amax;

    float *fftdatar = NULL;
    float *fftdatai = NULL;
    float *ampspec = NULL;

    nn = out[0];
    si = out[1] / 1.0E6;
    no = out[2];

    f1 = StartFreq;
    f2 = EndFreq;

    df = (f2 - f1) / (no - 1);

    for (nf = 1; nf < nn; nf <<= 1)
        if (nf == 32768L)
            goto ERREXIT;

    fftdatar = MAlloc(sizeof(float) * nf);
    if (fftdatar == NULL)
        goto ERREXIT;

    fftdatai = MAlloc(sizeof(float) * nf);
    if (fftdatai == NULL)
        goto ERREXIT;

    ampspec = MAlloc(sizeof(float) * nf / 2);
    if (ampspec == 0)
        goto ERREXIT;

    padlen = nf - nn;
    if (padlen > 0) {
        memset(fftdatar, 0, sizeof(float) * nf);
        memset(fftdatai, 0, sizeof(float) * nf);
    }

    for (n = 0; n < nn; n++) {
        fftdatar[n] = inp[n];
        fftdatai[n] = 0;
    }

    FastFourierTransformation(fftdatar, fftdatai, nf, 1);

    n2 = nf / 2;
    amax = 0;
    for (n = 0; n < n2; n++) {
        re = fftdatar[n];
        im = fftdatai[n];
        ampspec[n] = sqrt(re * re + im * im);
        if (ampspec[n] > amax)
            amax = ampspec[n];
    }

    for (n = 0; n < n2; n++) {
        ampspec[n] *= (100. / amax);
    }

    ff = 1. / (nf * si);

    for (i = 0; i < no; i++) {
        f = f1 + i * df;
        n = f / ff;

        if (n >= n2) {
            out[i] = 0;
        } else {
            k = (ampspec[n + 1] - ampspec[n]) / ff;
            out[i] = ampspec[n] + k * (f - ff * n);
        }
    }

    MFree(fftdatar);
    MFree(fftdatai);
    MFree(ampspec);
    return 0;

ERREXIT:
    if (fftdatar)
        MFree(fftdatar);
    if (fftdatai)
        MFree(fftdatai);
    if (ampspec)
        MFree(ampspec);
    return -1;
}

int PhaseRotate(float *inp, float *out, int degrees)
{
    /*
    Input Parameters
    ----------------
    inp     - input trace
    out[0]  - length of input and output trace (# of samples)
    degrees - angle of rotation

    Output Parameters
    -----------------
    out     - rotated trace

    Return code
    -----------------
    0 - OK
    -1 - Can't allocate memory for working arrays or bad parameters
    */
    int n;
    float amp;
    float phi, dphi;

    int nn = out[0];

    float *ctr_re = MAlloc(sizeof(float) * nn);
    float *ctr_im = MAlloc(sizeof(float) * nn);

    if (ctr_re == NULL || ctr_im == 0)
        goto ERREXIT;

    if (HilbertTransformation(nn, inp, ctr_re, ctr_im) != 0)
        goto ERREXIT;

    dphi = degrees / 180. * M_PI;

    for (n = 0; n < nn; n++) {
        amp = sqrt(ctr_re[n] * ctr_re[n] + ctr_im[n] * ctr_im[n]);
        phi = atan2(ctr_im[n], ctr_re[n]) + dphi;

        out[n] = amp * hcos(phi);
    }

    MFree(ctr_re);
    MFree(ctr_im);
    return 0;

ERREXIT:

    if (ctr_re)
        MFree(ctr_re);
    if (ctr_im)
        MFree(ctr_im);

    return -1;
}

// Zero phase bandpass filter
int BandPass(float *inp, float *out, int hz1, int hz2, int hz3, int hz4,
             int simks, int nn)
{
    /*
    Input Parameters
    ----------------
    inp     - input trace

    hz1\
    hz2 \   - filter cut
    hz3 /     frequences (Hz)
    hz4/

    simks   - sample interval (microseconds)

    Output Parameters
    -----------------
    out     - filtered trace

    Return code
    -----------------
    0 - OK
    -1 - Can't allocate memory for working arrays or bad parameters
    */

    return BandPassFilter(inp, out, hz1, hz2, hz3, hz4, simks * 1e6, nn);
}

// Automatic Gain Control--standard box
/*
Input Parameters
----------------
data    - input trace; the AGC result is written back to this array
iwagc   - half-length of the sliding statistical window (samples)
nt      - number of samples in the trace
typeagc - statistical type: 0 for mean absolute amplitude, 1 for RMS

For each sample, the gain is calculated from the selected statistic over its
sliding window. At the trace boundaries, the statistic uses the available
samples only.
*/
void Agc(float *data, int iwagc, int nt, int typeagc)
{
    float *agcdata;
    register int i;
    register double val;
    register double sum;
    register int nwin;
    register double statistic;
    const bool useAbsoluteMean = typeagc == 0;

    const auto sampleStatistic = [useAbsoluteMean](double sample) {
        return useAbsoluteMean ? fabs(sample) : sample * sample;
    };
    const auto windowAmplitude = [useAbsoluteMean](double sum, int count) {
        const double average = sum / count;
        if (average <= 0.0)
            return 0.0;
        return useAbsoluteMean ? average : sqrt(average);
    };

    if (nt < 2)
        return;

    if (iwagc >= nt / 2)
        iwagc = nt / 2 - 1;

    if (iwagc < 2)
        iwagc = 2;

    // allocate room for agc'd data
    agcdata = new float[nt];

    // compute initial window for first datum
    sum = 0.0;
    for (i = 0; i < iwagc; ++i) {
        val = data[i];
        sum += sampleStatistic(val);
    }
    nwin = iwagc;
    statistic = windowAmplitude(sum, nwin);
    agcdata[0] = (!statistic) ? 0.0 : data[0] / statistic;

    // ramping on
    for (i = 1; i < iwagc; ++i) {
        val = data[i + iwagc - 1];
        sum += sampleStatistic(val);
        ++nwin;
        statistic = windowAmplitude(sum, nwin);
        agcdata[i] = (!statistic) ? 0.0 : data[i] / statistic;
    }

    // middle range -- full statistical window
    for (i = iwagc; i < nt - iwagc; ++i) {
        val = data[i + iwagc - 1];
        sum += sampleStatistic(val);
        val = data[i - iwagc];
        sum -= sampleStatistic(val);
        statistic = windowAmplitude(sum, nwin);
        agcdata[i] =
            (statistic <= 0.0) ? 0.0 : data[i] / statistic;
    }

    // ramping off
    for (i = nt - iwagc; i < nt; ++i) {
        val = data[i - iwagc];
        sum -= sampleStatistic(val);
        --nwin;
        statistic = windowAmplitude(sum, nwin);
        agcdata[i] =
            (statistic <= 0.0) ? 0.0 : data[i] / statistic;
    }

    // copy data back into trace
    memcpy((void *)data, (const void *)agcdata, nt * sizeof(float));

    delete[] agcdata;

    return;
}

float MaxAmp(float *data, int nt)
{
    int n;
    float mx = 0;
    float v;

    for (n = 0; n < nt; n++) {
        v = fabs(data[n]);
        if (mx < v)
            mx = v;
    }

    return mx;
}

void Normalize(float *data, float mx, int nt)
{
    int n;

    if (mx == 0)
        for (n = 0; n < nt; n++)
            data[n] = 0;
    else
        for (n = 0; n < nt; n++)
            data[n] /= mx;
}
//---------------------------------------------------------------------------

int Convolution(float *inp, float *out1, float *flt, int datl, int fltl)
{
    int di, fi, i;
    float ia;

    float *out = new float[datl];
    memset(out, 0, datl * sizeof(float));

    for (di = 0; di < datl; di++) {
        for (fi = 1 - fltl; fi < fltl; fi++) {
            i = di + fi;
            if (i < 0 || i >= datl)
                ia = 0;
            else
                ia = inp[i];

            out[di] = out[di] + ia * flt[abs(fi)];
        }
    }

    memcpy(out1, out, datl * sizeof(float));

    delete[] out;

    return 0;
}

// Low precision fast sine/cosine (~14x faster)
float lsin(float x)
{
    float sin;
    // always wrap input angle to -PI..PI
    if (x < -3.14159265)
        x += 6.28318531;
    else if (x > 3.14159265)
        x -= 6.28318531;

    // compute sine
    if (x < 0)
        sin = 1.27323954 * x + .405284735 * x * x;
    else
        sin = 1.27323954 * x - 0.405284735 * x * x;
    return sin;
}

// compute cosine: sin(x + PI/2) = cos(x)
float lcos(float x)
{
    float cos;
    // always wrap input angle to -PI..PI
    if (x < -3.14159265)
        x += 6.28318531;
    else if (x > 3.14159265)
        x -= 6.28318531;

    x += 1.57079632;
    if (x > 3.14159265)
        x -= 6.28318531;

    if (x < 0)
        cos = 1.27323954 * x + 0.405284735 * x * x;
    else
        cos = 1.27323954 * x - 0.405284735 * x * x;
    return cos;
}

// High precision sine/cosine (~8x faster)
float hsin(float x)
{
    // return sin(x); //!!!

    float sin;
    // always wrap input angle to -PI..PI
    if (x < -3.14159265)
        x += 6.28318531;
    else if (x > 3.14159265)
        x -= 6.28318531;

    // compute sine
    if (x < 0) {
        sin = 1.27323954 * x + .405284735 * x * x;

        if (sin < 0)
            sin = .225 * (sin * -sin - sin) + sin;
        else
            sin = .225 * (sin * sin - sin) + sin;
    } else {
        sin = 1.27323954 * x - 0.405284735 * x * x;

        if (sin < 0)
            sin = .225 * (sin * -sin - sin) + sin;
        else
            sin = .225 * (sin * sin - sin) + sin;
    }
    return sin;
}

float hcos(float x)
{
    // return cos(x); //|!!!

    float cos;
    // always wrap input angle to -PI..PI
    if (x < -3.14159265)
        x += 6.28318531;
    else if (x > 3.14159265)
        x -= 6.28318531;

    // compute cosine: sin(x + PI/2) = cos(x)
    x += 1.57079632;
    if (x > 3.14159265)
        x -= 6.28318531;

    if (x < 0) {
        cos = 1.27323954 * x + 0.405284735 * x * x;

        if (cos < 0)
            cos = .225 * (cos * -cos - cos) + cos;
        else
            cos = .225 * (cos * cos - cos) + cos;
    } else {
        cos = 1.27323954 * x - 0.405284735 * x * x;

        if (cos < 0)
            cos = .225 * (cos * -cos - cos) + cos;
        else
            cos = .225 * (cos * cos - cos) + cos;
    }
    return cos;
}
