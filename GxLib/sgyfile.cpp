#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// #include <unistd.h>

#include "sgyfile.h"
#include "util2.h"

#include "math.h"



SgyFile::SgyFile(QObject *parent)
    : SeisFile(parent)
{
    const char *Formats[] = {
        "IBM Float (32 bit)",                                       // 1
        "Integer   (32 bit)",                                       // 2
        "Integer   (16 bit)",                                       // 3
        "Fixed point with gain (obsolete)(16 bit) - Not supported", // 4
        "IEEE Float(32 bit)",                                       // 5
        "Integer   (8 bit) - Non standard",                         // 6
        "Not used",                                                 // 7
        "Integer   (8 bit)"                                         // 8
    };

    int n;

    FormatNames.clear();

    for (n = 0; n < sizeof(Formats) / sizeof(char *); n++) {
        FormatNames.append(Formats[n]);
    }

    _inptrc = NULL;
    _inpsmp = NULL;
    _hlen = 240;
    HdrDefList = &TraceHdrsSet[1];
    _f = -1;
}

SgyFile::~SgyFile()
{
    Close();
}

//---------------------------------------------------------------------------
bool SgyFile::ReadTxtHed()
{
    memset(_txthed, ' ', 3200);
    _txtcode = 0;

    if (_f < 0)
        return false;

    char txthed[3200];

    _mutex.lock();

    lseek64(_f, 0, SEEK_SET);

    int cnt = read(_f, txthed, 3200);
    if (cnt != 3200)
        return false;

    _txtcode = is_txthdr_ebcdic(txthed);

    if (_txtcode)
        ebasdn(txthed, txthed, 3200);

    memcpy(_txthed, txthed, 3200);

    _mutex.unlock();

    return true;
}

bool SgyFile::ReadBinHed()
{
    if (_f < 0)
        return false;

    byte buf[400];
    memset(buf, 0, 400);

    _mutex.lock();

    lseek64(_f, 3200, SEEK_SET);
    int cnt = read(_f, buf, 400);
    if (cnt != 400)
        return false;
    _mutex.unlock();

    int si = ui2(buf, 17, _swap);
    int nsmp = ui2(buf, 21, _swap);
    int frmt = i2(buf, 25, _swap);

    int smpl;

    if (frmt == 3)
        smpl = 2;
    else if (frmt == 6)
        smpl = 1;
    else if (frmt == 8)
        smpl = 1;
    else
        smpl = 4;

    int trl = 240 + nsmp * smpl;

    _Nt = (_flen - 3600) / trl;
    _Format = frmt;
    _smpl = smpl;
    _trl = trl;
    _Ns = nsmp;
    _Si = si / 1000000.;

    _binh.set(_swap, buf);

    return true;
}

bool SgyFile::UpdateTxtHed(QString txt)
{
    if (_f < 0 || _rd_only)
        return false;

    char buf[3200];

    QByteArray qtxt = txt.toLocal8Bit();
    char *s = qtxt.data();

    memset(buf, ' ', 3200);

    int n, i, j;

    i = j = 0;

    for (n = 0; n < strlen(s); n++) {
        char c = s[n];
        if (c == '\n') {
            i++;
            j = 0;
            continue;
        } else {
            if (j < 80 && i < 40) {
                buf[i * 80 + j] = c;
                j++;
            }
        }
    }

    _mutex.lock();

    lseek64(_f, 0, SEEK_SET);

    int cnt;

    if (_txtcode) {
        char ebuf[3200];

        asebdn(ebuf, buf, 3200);
        cnt = write(_f, ebuf, 3200);
    } else {
        cnt = write(_f, buf, 3200);
    }

    _mutex.unlock();

    if (cnt != 3200)
        return false;

    memcpy(_txthed, buf, 3200);

    return true;
}

void SgyFile::Open(string fn)
{
    if (_f > 0)
        Close();

    const char *fname = fn.c_str();

    int cgg, junk, swap;

    if (!is_segy_f(fname, swap, cgg, junk)) {
        //_Active=false;
        return;
    }

    _swap = swap;
    _rd_only = 0;

    _f = open(fname, O_RDWR | O_BINARY);
    if (_f < 0) // try read only
    {
        _f = open(fname, O_RDONLY | O_BINARY);
        _rd_only = 1;
    }

    if (_f < 0) {
        //_Active=false;
        return;
    }

    _flen = getfilesize64(_f);

    bool errb = !ReadBinHed();
    bool errt = !ReadTxtHed();

    _jnk = (_flen - 3200 - 400 - 16 * _cgg) % _trl;

    if (errb || errt)
        goto ERR;

    if (_Nt < 2048)
        _data.Init(_Nt, _Ns, 240, _Nt);
    //      _data.Init(_Nt, 240+(_Ns)*4, _Nt);

    else
        _data.Init(_Nt, _Ns, 240, 2048);
    //      _data.Init(_Nt, 240+(_Ns)*4, 2048);

    //    ReadRawHeader(0,       _trhedo, false);
    //    ReadRawHeader((_ntr-1),_trhedn, false);

    lseek(_f, 3600, SEEK_SET);

    if (_inptrc)
        delete[] _inptrc;
    if (_inpsmp)
        delete[] _inpsmp;
    //    _inptrc =   new char[_trl+_dlymax];
    _inptrc = new byte[_trl];
    _inpsmp = new float[_Ns];

    //    if(!_rd_only)  rc|=1;
    //    if(_cgg)       rc|=2;
    //    if(!getswap()) rc|=4;

    //_Active=true;

    _Co = Th(0, 6);

    return;

ERR:
    _data.Clear();
    _Nt = 0; //_Active=false;

    return;
}

void SgyFile::Close()
{
    //    qDebug() << "SgyFile::Close()";

    if (_f > 0)
        close(_f);
    _f = -1;

    _data.Clear();

    if (_inptrc)
        delete[] _inptrc;
    _inptrc = NULL;
    if (_inpsmp)
        delete[] _inpsmp;
    _inpsmp = NULL;

    _Ns = _Nt = 0; //_Active=false;
}

void SgyFile::ReadTrace(long long ntr)
{
    if (!_f)
        return;

    int n;

    _mutex.lock();

    lseek64(_f, 3600 + ntr * _trl, SEEK_SET);
    n = read(_f, _inptrc, _trl);

    int dly = i2(_inptrc, 109, _swap) / (_Si * 1000) + 0.5;

    byte *buf = _data.putObject(ntr, &dly);
    if (buf == NULL)
        return;

    //    if(_recdly) delay=i2(_inptrc,109)/(_si*1000)+0.5;
    //    if(_recdly) delay=i2(_inptrc,109)/(_si*1000)+0.5;

    float *sample = (float *)(buf + 240);

    //    if(_recdly) memset(sample,0,_nsmp*4);
    //    for(n=0;n<_nsmp-_dlymax;n++) sample[n+delay] = GetSample(n, _inptrc);
    for (n = 0; n < _Ns; n++)
        sample[n] = GetSample(n, _inptrc);

    //    DoProcessing(sample);

    memcpy(buf, _inptrc, 240);

    _mutex.unlock();
}

int SgyFile::ReadRawTrace(byte *&buf, long long ntr, bool header_only)
{
    if (!_f)
        return -3;
    if (ntr < 0 || ntr >= _Nt)
        return -2;

    int len;

    if (header_only)
        len = 240;
    else
        len = _trl;

    buf = new byte[len];

    _mutex.lock();
    lseek64(_f, 3600 + ntr * _trl, SEEK_SET);
    int cnt = read(_f, buf, len);
    _mutex.unlock();

    if (cnt != len) {
        delete[] buf;
        buf = NULL;
        return -1;
    }

    return len;
}

int SgyFile::WriteRawTrace(byte *buf, long long ntr, bool header_only)
{
    if (!_f)
        return -1;

    if (ntr < 0 || ntr >= _Nt)
        return -1;

    int len;

    if (header_only)
        len = 240;
    else
        len = _trl;

    _mutex.lock();
    lseek64(_f, 3600 + ntr * _trl, SEEK_SET);
    int cnt = write(_f, buf, len);
    _mutex.unlock();

    if (cnt != len)
        return -1;

    return 0;
}

float *SgyFile::Rs(long long ntr)
{
    int n;

    if (!_f)
        return NULL;

    _mutex.lock();
    lseek64(_f, 3600 + ntr * _trl, SEEK_SET);
    n = read(_f, _inptrc, _trl);

    for (n = 0; n < _Ns; n++)
        _inpsmp[n] = GetSample(n, _inptrc);
    _mutex.unlock();

    return _inpsmp;
}

float *SgyFile::Rs(long long ntr, byte *hed)
{
    float *smp = Rs(ntr);

    if (smp && hed)
        memcpy(hed, _inptrc, 240);

    return smp;
}

float SgyFile::GetSample(int idx, byte *inptrc)
{
    float smp;
    int *xmp = (int *)&smp;
    float *dat = (float *)(inptrc + 240);

    short *int2ptr = (short *)dat;
    int *int4ptr = (int *)dat;
    signed char *int1ptr = (signed char *)dat;

    if (idx < 0 && idx > _Ns)
        return 0;

    if (_Format == 1) {
        smp = dat[idx];
        smp = ibm2ieee(smp, _swap);
    } else if (_Format == 2) {
        smp = swapi4(int4ptr[idx], _swap);
    } else if (_Format == 3) {
        smp = swapi2(int2ptr[idx], _swap);
    } else if (_Format == 4) {
        smp = dat[idx];
    } else if (_Format == 5) {
        *xmp = swapi4(int4ptr[idx], _swap);
    } else if (_Format == 6 || _Format == 8) {
        smp = int1ptr[idx];
    } else
        smp = 0;

    int wgti = i2((byte *)_inptrc, 169, _swap);

    if (wgti == 0)
        return smp;

    double wgt = pow(2., wgti);

    return smp / wgt;
}

float *SgyFile::Ts(long long ntr)
{
    byte *buf = Tb(ntr);
    if (buf)
        return (float *)(buf + 240);
    else
        return NULL;
}

bool SgyFile::UpdateBinHed(QVector<QString> vals)
{
    // return:
    //  true  - updated
    //  false - not updated

    if (_f < 0)
        return true;
    int changed = binHed()->update(vals);

    if (changed) {
        int cnt;

        lseek64(_f, 3200, SEEK_SET);

        cnt = write(_f, _binh.buf(), 400);

        if (cnt != 400)
            return false;

        // memcpy(_binhed,buf,400);
    }

    return changed;
}

//---------------------------------------------------------------------------

SuFile::SuFile(QObject *parent)
    : SgyFile(parent)
{
    HdrDefList = &TraceHdrsSet[1];
}

SuFile::~SuFile()
{
    //  qDebug() << "~SgyFile()";

    Close();
}

void SuFile::Close()
{
    if (_f > 0)
        close(_f);
    _f = -1;

    _data.Clear();

    if (_inptrc)
        delete[] _inptrc;
    _inptrc = NULL;
    if (_inpsmp)
        delete[] _inpsmp;
    _inpsmp = NULL;

    _Ns = _Nt = 0; //_Active=false;
}

void SuFile::Open(string fn)
{
    memset(_txthed, ' ', 3200);
    _txtcode = 0;

    if (_f > 0)
        Close();

    const char *fname = fn.c_str();

    int swap;

    if (!is_su_f(fname, swap)) {
        //_Active=false;
        return;
    }

    _swap = swap;
    _rd_only = 1;

    _f = open(fname, O_RDWR | O_BINARY);
    if (_f < 0) // try read only
    {
        _f = open(fname, O_RDONLY | O_BINARY);
        _rd_only = 1;
    }

    if (_f < 0) {
        //_Active=false;
        return;
    }

    _flen = getfilesize64(_f);

    bool errb = !ReadBinHed();

    if (errb)
        goto ERR;

    if (_Nt < 2048)
        _data.Init(_Nt, _Ns, 240, _Nt);
    //      _data.Init(_Nt, 240+(_Ns)*4, _Nt);
    else
        _data.Init(_Nt, _Ns, 240, 2048);
    //      _data.Init(_Nt, 240+(_Ns)*4, 2048);

    //    ReadRawHeader(0,       _trhedo, false);
    //    ReadRawHeader((_ntr-1),_trhedn, false);

    lseek(_f, 0, SEEK_SET);

    if (_inptrc)
        delete[] _inptrc;
    if (_inpsmp)
        delete[] _inpsmp;

    _inptrc = new byte[_trl];
    _inpsmp = new float[_Ns];

    //_Active=true;

    _Co = Th(0, 6);

    return;

ERR:
    _data.Clear();
    _Nt = 0; //_Active=false;

    return;
}

bool SuFile::ReadBinHed()
{
    // SU file doesn't have Bin Header, get data from the first trace

    if (_f < 0)
        return false;

    byte buf[240];

    _mutex.lock();

    lseek64(_f, 0, SEEK_SET);
    int cnt = read(_f, buf, 240);
    if (cnt != 240)
        return false;

    _mutex.unlock();

    int si = ui2(buf, 117, _swap);
    int nsmp = ui2(buf, 115, _swap);

    int smpl = 4;

    int trl = 240 + nsmp * smpl;

    _Nt = (_flen) / trl;
    _Format = 5;
    _smpl = smpl;
    _trl = trl;
    _Ns = nsmp;
    _Si = si / 1000000.;

    _binh.set(_swap, si, nsmp, 5);

    return true;
}

void SuFile::ReadTrace(long long ntr)
{
    if (!_f)
        return;

    int n;

    _mutex.lock();

    lseek64(_f, ntr * _trl, SEEK_SET);
    n = read(_f, _inptrc, _trl);

    int dly = i2(_inptrc, 109, _swap) / (_Si * 1000) + 0.5;

    byte *buf = _data.putObject(ntr, &dly);
    if (buf == NULL)
        return;

    float *sample = (float *)(buf + 240);

    for (n = 0; n < _Ns; n++)
        sample[n] = GetSample(n, _inptrc);

    memcpy(buf, _inptrc, 240);

    _mutex.unlock();
}

int SuFile::ReadRawTrace(byte *&buf, long long ntr, bool header_only)
{
    if (!_f)
        return -3;

    if (ntr < 0 || ntr >= _Nt)
        return -2;

    int len;

    if (header_only)
        len = 240;
    else
        len = _trl;

    buf = new byte[len];

    _mutex.lock();
    lseek64(_f, ntr * _trl, SEEK_SET);
    int cnt = read(_f, buf, len);
    _mutex.unlock();

    if (cnt != len) {
        delete[] buf;
        buf = NULL;
        return -1;
    }

    return len;
}

int SuFile::WriteRawTrace(byte *buf, long long ntr, bool header_only)
{
    if (!_f)
        return -1;

    if (ntr < 0 || ntr >= _Nt)
        return -1;

    int len;

    if (header_only)
        len = 240;
    else
        len = _trl;

    _mutex.lock();
    lseek64(_f, ntr * _trl, SEEK_SET);
    int cnt = write(_f, buf, len);
    _mutex.unlock();

    if (cnt != len)
        return -1;

    return len;
}

float *SuFile::Rs(long long ntr)
{
    int n;

    if (!_f)
        return NULL;

    _mutex.lock();
    lseek64(_f, ntr * _trl, SEEK_SET);
    n = read(_f, _inptrc, _trl);

    for (n = 0; n < _Ns; n++)
        _inpsmp[n] = GetSample(n, _inptrc);
    _mutex.unlock();

    return _inpsmp;
}

//---------------------------------------------------------------------------

CstFile::CstFile(QObject *parent)
    : SgyFile(parent)
{
    static const char *Formats[] = {
        "Integer   (16 bit)",                  // 1
        "BGN (16-bit)      ",                  // 2
        "Native - Use IEEE folat 32 bit",      // 3
        "SEGD8022 format",                     // 4
        "SEGD8042 format",                     // 5
        "INT8 format",                         // 6
        "VAX format",                          // 7
        "IEEE format (32-bit floating point)", // 8
        "I3EV format",                         // 9
        "CVX format (Convex)",                 // 10
    };

    int n;

    FormatNames.clear();

    for (n = 0; n < sizeof(Formats) / sizeof(char *); n++) {
        FormatNames.append(Formats[n]);
    }

    _hlen = 256;
    HdrDefList = &TraceHdrsSet[2];
}

CstFile::~CstFile()
{
    //  qDebug() << "~SgyFile()";

    Close();
}

void CstFile::Close()
{
    if (_f >= 0)
        close(_f);
    _f = -1;

    _data.Clear();

    if (_inptrc)
        delete[] _inptrc;
    _inptrc = NULL;
    if (_inpsmp)
        delete[] _inpsmp;
    _inpsmp = NULL;

    _Ns = _Nt = 0; //_Active=false;
}

void CstFile::Open(string fn)
{
    memset(_txthed, ' ', 3200);
    _txtcode = 0;

    if (_f > 0)
        Close();

    const char *fname = fn.c_str();

    if (!is_cst_f(fname)) {
        return;
    }

    _swap = 0;
    _rd_only = 1;

    _f = open(fname, O_RDWR | O_BINARY);
    if (_f < 0) // try read only
    {
        _f = open(fname, O_RDONLY | O_BINARY);
        _rd_only = 1;
    }

    if (_f < 0) {
        return;
    }

    _flen = getfilesize64(_f);

    bool errb = !ReadBinHed();

    if (errb)
        goto ERR;

    if (_Nt < 2048)
        _data.Init(_Nt, _Ns, 256, _Nt);
    //_data.Init(_Nt, 256+(_Ns)*4, _Nt);
    else
        _data.Init(_Nt, _Ns, 256, 2048);
    //_data.Init(_Nt, 256+(_Ns)*4, 2048);

    lseek(_f, 0, SEEK_SET);

    if (_inptrc)
        delete[] _inptrc;
    if (_inpsmp)
        delete[] _inpsmp;

    _inptrc = new byte[_trl];
    _inpsmp = new float[_Ns];

    _Co = Th(0, 2);

    return;

ERR:
    _data.Clear();
    _Nt = 0; //_Active=false;

    return;
}

bool CstFile::ReadBinHed()
{
    // CST file desn't have Bin Header, get data from the first trace

    if (_f < 0)
        return false;

    byte buf[400];
    int *hed = (int *)buf;

    _mutex.lock();
    lseek64(_f, 0, SEEK_END);
    long long l = tell64(_f);

    lseek64(_f, 0, SEEK_SET);

    int cnt = read(_f, buf, 256);
    if (cnt != 256)
        return false;
    _mutex.unlock();

    int frmt = ((unsigned)(hed[10])) >> 20;

    int smpl;

    if (frmt == 1)
        smpl = 2;
    else if (frmt == 2)
        smpl = 2;
    else if (frmt == 3)
        smpl = 4;
    else if (frmt == 4)
        smpl = 4;
    else if (frmt == 7)
        smpl = 4;
    else if (frmt == 8)
        smpl = 4;
    else {
        // printf("Data fomat is unknown\n");
        return false;
    }

    int nsmp = hed[9] / (hed[8] / 1000);
    int si = hed[8];

    int trl = (nsmp * smpl) + 256;
    int tlb = trl / 512;
    if (trl % 512)
        tlb++;
    tlb *= 512;

    _Nt = l / tlb;

    _Format = frmt;
    _smpl = smpl;
    _trl = trl;
    _tlb = tlb;
    _Ns = nsmp;
    _Si = si / 1000000.;

    // qDebug() << "+swap=" << _swap;

    _binh.set(_swap, si, nsmp, 5);

    return true;
}

void CstFile::ReadTrace(long long ntr)
{
    int n;

    if (_f < 0)
        return;

    _mutex.lock();
    lseek64(_f, ntr * _tlb, SEEK_SET);
    n = read(_f, _inptrc, _trl);

    byte *buf = _data.putObject(ntr);
    if (buf == NULL)
        return;

    float *sample = (float *)(buf + 256);

    for (n = 0; n < _Ns; n++)
        sample[n] = GetSample(n, _inptrc);

    memcpy(buf, _inptrc, 256);
    _mutex.unlock();
}

float CstFile::GetSample(int idx, byte *inptrc)
{
    int *hed = (int *)inptrc;
    float smp;
    float *dat = (float *)(inptrc + 256);

    short *int2ptr = (short *)dat;
    signed char *int1ptr = (signed char *)dat;

    int shift = hed[12];

    if (idx < 0 && idx > _Ns)
        return 0;

    if (_Format == 1) {
        smp = int2ptr[idx] * pow(2, shift);
    } else if (_Format == 3) {
        smp = ibm2ieee(dat[idx], 0);
        smp = smp * pow(2, shift);
    } else if (_Format == 8) {
        smp = dat[idx];
    } else if (_Format == 5) {
        smp = int1ptr[idx] * pow(2, shift);
    } else if (_Format == 2) {
        unsigned short v = int2ptr[idx];

        short d = v & 0xFFF0;
        d = d >> 4;
        short g = v & 0xF;

        try {
            smp = d * pow(2, 15 - g + shift); // * pow(2,shift);
        } catch (...) {
        }
    } else
        smp = 0;

    return smp;
}

int CstFile::ReadRawTrace(byte *&buf, long long ntr, bool header_only)
{
    if (!_f)
        return -3;

    if (ntr < 0 || ntr >= _Nt)
        return -2;

    int len;

    if (header_only)
        len = 256;
    else
        len = _trl;

    buf = new byte[len];

    _mutex.lock();
    lseek64(_f, ntr * _tlb, SEEK_SET);
    int cnt = read(_f, buf, len);
    _mutex.unlock();

    if (cnt != len) {
        delete[] buf;
        buf = NULL;
        return -1;
    }

    return len;
}

int CstFile::WriteRawTrace(byte *buf, long long ntr, bool header_only)
{
    if (!_f)
        return -1;
    if (ntr < 0 || ntr >= _Nt)
        return -1;

    int len;

    if (header_only)
        len = 256;
    else
        len = _trl;

    _mutex.lock();
    lseek64(_f, ntr * _tlb, SEEK_SET);
    int cnt = write(_f, buf, len);
    _mutex.unlock();

    if (cnt != len)
        return -1;

    return len;
}

float *CstFile::Rs(long long ntr)
{
    int n;

    if (!_f)
        return NULL;

    _mutex.lock();

    lseek64(_f, ntr * _tlb, SEEK_SET);
    n = read(_f, _inptrc, _trl);

    for (n = 0; n < _Ns; n++)
        _inpsmp[n] = GetSample(n, _inptrc);

    _mutex.unlock();

    return _inpsmp;
}
