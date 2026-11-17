#include "seisfile.h"
#include "gfxsrcfile.h"



#include "util2.h"

//---------------------------------------------------------------------------

DataStorage::DataStorage()
{
    _nobj = 0;
    _obji = NULL;
    _obuf = NULL;
    _stri = NULL;
    _odly = NULL;
    _hlen = 0;
    _nsmp = 0;
    _next = 0;
}

DataStorage::~DataStorage()
{
    if (_obji)
        delete[] _obji;
    if (_obuf)
        delete[] _obuf;
    if (_stri)
        delete[] _stri;
    if (_odly)
        delete[] _odly;
}

void DataStorage::Clear()
{
    if (_obji)
        delete[] _obji;
    if (_obuf)
        delete[] _obuf;
    if (_stri)
        delete[] _stri;
    if (_odly)
        delete[] _odly;
    _nobj = 0;
    _obji = NULL;
    _obuf = NULL;
    _odly = NULL;
    _stri = NULL;
    _hlen = 0;
    _nsmp = 0;
    _next = 0;
}

void DataStorage::Init(int nobj, int olen, int strl)
{
    _nobj = nobj;
    _olen = olen;
    _strl = strl;
    _hlen = 0;
    _nsmp = 0;

    _next = 0;

    if (_obji)
        delete[] _obji;
    if (_odly)
        delete[] _odly;
    if (_obuf)
        delete[] _obuf;
    if (_stri)
        delete[] _stri;

    _stri = new int[_strl];
    _obji = new int[_nobj];
    _odly = new int[_strl];
    _obuf = new byte[_strl * _olen];

    memset(_obji, 0xFF, _nobj * sizeof(int));
    memset(_stri, 0xFF, _strl * sizeof(int));
    memset(_odly, 0x00, _strl * sizeof(int));
}

void DataStorage::Init(int nobj, int nsmp, int hlen, int strl)
{
    _nsmp = nsmp;
    _hlen = hlen;

    _olen = hlen + (_nsmp) * 4;

    _nobj = nobj;
    _strl = strl;

    _next = 0;

    if (_obji)
        delete[] _obji;
    if (_odly)
        delete[] _odly;
    if (_obuf)
        delete[] _obuf;
    if (_stri)
        delete[] _stri;

    _stri = new int[_strl];
    _obji = new int[_nobj];
    _odly = new int[_strl];
    _obuf = new byte[_strl * _olen];

    memset(_obji, 0xFF, _nobj * sizeof(int));
    memset(_stri, 0xFF, _strl * sizeof(int));
    memset(_odly, 0x00, _strl * sizeof(int));
}

void DataStorage::Reset()
{
    memset(_obji, 0xFF, _nobj * sizeof(int));
    memset(_stri, 0xFF, _strl * sizeof(int));
    memset(_odly, 0x00, _strl * sizeof(int));
}

byte *DataStorage::getObject(int n, int *dly)
{
    if (dly)
        *dly = 0;

    if (n >= _nobj)
        return NULL;

    if (_obji[n] < 0)
        return NULL;

    if (dly)
        *dly = _odly[_obji[n]];

    return _obuf + _obji[n] * _olen;
}

/*
DataStorageTrace DataStorage::getTrace(int n)
{
    int dly;

    DataStorageTrace trc;

    byte*  buf = getObject(n, &dly);

    trc._buf = buf;
    trc._dly = dly;

    return trc;
}
*/

byte *DataStorage::putObject(int n, int *dly)
{
    if (n >= _nobj)
        return NULL;

    if (_obji[n] < 0) {
        if (_stri[_next] >= 0)
            _obji[_stri[_next]] = -1;

        _obji[n] = _next;
        _stri[_next] = n;
        _next++;
        if (_next >= _strl)
            _next = 0;
    }

    if (dly)
        _odly[_obji[n]] = *dly;
    else
        _odly[_obji[n]] = 0;

    return _obuf + _obji[n] * _olen;
}

DataStorage &DataStorage::Assign(const DataStorage &src)
{
    if (this == &src)
        return *this;

    Clear();

    _nobj = src._nobj;
    _olen = src._olen;
    _strl = src._strl;
    _hlen = src._hlen;
    _nsmp = src._nsmp;

    _next = 0;

    if (_obji)
        delete[] _obji;
    if (_obuf)
        delete[] _obuf;
    if (_stri)
        delete[] _stri;
    if (_odly)
        delete[] _odly;

    _stri = new int[_strl];
    _odly = new int[_strl];
    _obji = new int[_nobj];
    _obuf = new byte[_strl * _olen];

    memset(_obji, 0xFF, _nobj * sizeof(int));
    memset(_stri, 0xFF, _strl * sizeof(int));
    memset(_odly, 0x00, _strl * sizeof(int));

    return *this;
}
//---------------------------------------------------------------------------

SeisFile::SeisFile(QObject *parent)
    : QObject(parent)
{
    _f = -1;
    _hlen = 0;
    _swap = 0;

    _Fname = "";
    _Nt = 0;
    _Ns = 0;

    _Co = 0;
    _Po = 0;
    _Pm = 1;

    _f = -1;
    _hlen = 0;
    _swap = 0;

    HdrDefList = NULL;

    _txtcode = 1;

    _cgg = 0;
    _jnk = 0;

    _dly = 0;
}

SeisFile::~SeisFile()
{
    int n;

    for (n = 0; n < _links.count(); n++) {
        GfxSrcFile *f = (GfxSrcFile *)_links[n];
        f->setSfile(NULL);
    }

    if (_f >= 0)
        close(_f);

    if (_inptrc)
        delete[] _inptrc;
    _inptrc = NULL;
}

void SeisFile::RegisterLink(QObject *v)
{
    _links.append(v);
    //    update();
}

void SeisFile::UnRegisterLink(QObject *v)
{
    _links.removeAll(v);
    //    update();
}

void SeisFile::Invalidate()
{
    for (int n = 0; n < _links.count(); n++) {
        GfxSrcFile *src = (GfxSrcFile *)_links[n];
        src->Invalidate();
    }
}

byte *SeisFile::Tb(long long ntr) // Get trace with header
{
    byte *buf;

    if (ntr < 0)
        return NULL;
    if (ntr >= _Nt)
        return NULL;

    buf = _data.getObject(ntr);

    if (buf)
        return buf;

    ReadTrace(ntr);

    buf = _data.getObject(ntr);

    if (buf) {
        float *trace = (float *)(buf + _hlen);

        execProc(this, _Ns, _Si, trace);

        return buf;
    }

    else
        return NULL;
}

Ttr SeisFile::Tt(long long ntr) // Get trace with header
{
    Ttr trc;

    byte *buf;
    int dly;

    if (ntr < 0)
        return trc;
    if (ntr >= _Nt)
        return trc;

    buf = _data.getObject(ntr, &dly);

    if (buf) {
        trc._buf = buf;
        if (_dly)
            trc._dly = dly;
        else
            trc._dly = 0;
        trc._smp = (float *)(buf + _hlen);
        trc._nsmp = _data.nsmp();
        return trc;
    }

    ReadTrace(ntr);

    buf = _data.getObject(ntr);

    if (buf) {
        float *trace = (float *)(buf + _hlen);

        execProc(this, _Ns, _Si, trace);

        trc._buf = buf;
        if (_dly)
            trc._dly = dly;
        else
            trc._dly = 0;
        //  trc._dly=dly;
        trc._smp = (float *)(buf + _hlen);
        trc._nsmp = _data.nsmp();
    }

    return trc;
}

double SeisFile::Th(long long tidx, QString hname)
{
    if (HdrDefList == NULL)
        return 0;
    int i = HdrDefList->index(hname);
    if (i < 0)
        return 0;
    return Th(tidx, i);
}

int SeisFile::ThIdx(QString hname)
{
    if (HdrDefList == NULL)
        return -1;
    return HdrDefList->index(hname);
}

double SeisFile::Th(byte *buf, long long tidx, QString hname)
{
    if (HdrDefList == NULL)
        return 0;
    int i = HdrDefList->index(hname);
    if (i < 0)
        return 0;
    return Th(buf, tidx, i);
}

double SeisFile::Th(byte *buf, long long tidx, int nitm)
{
    double v;

    if (HdrDefList == NULL)
        return 0;
    TrHdrDef *hi = HdrDefList->item(nitm);
    if (hi == NULL)
        return 0;

    if (buf == NULL)
        return 0;

    int frmt = hi->frmt;
    int hpos = hi->pos;

    SeisTrExpr *exp = &(hi->expr);

    if (exp->ExprString() != "") {
        if (exp->Error())
            return 0;

        exp->setHdr(buf, _hlen, _swap, tidx + 1, Nt());

        v = exp->Evaluate();
        if (exp->Error())
            return 0;
        return v;
    }

    if (frmt == 0)
        return tidx + 1;

    return iFmt(frmt, buf, hpos, _swap);
}

double SeisFile::Th(long long tidx, int nitm)
{
    byte *buf;
    double v;

    if (HdrDefList == NULL)
        return 0;
    TrHdrDef *hi = HdrDefList->item(nitm);
    if (hi == NULL)
        return 0;

    buf = Tb(tidx);

    v = Th(buf, tidx, nitm);
    /*
        if(buf==NULL) return 0;



        int        frmt = hi->frmt;
        int        hpos = hi->pos;

    //  ExprEval*  e    = FTrHdrDef->Expr[nitm];

        if(frmt==0)
            v = tidx+1;
        else
            v = iFmt(frmt,buf,hpos,_swap);
    */
    return v;
}

// Raw Trace Header
double SeisFile::Rh(long long tidx, QString hname)
{
    if (HdrDefList == NULL)
        return 0;
    int i = HdrDefList->index(hname);
    if (i < 0)
        return 0;
    return Rh(tidx, i);
}

double SeisFile::Rh(long long tidx, int nitm)
{
    byte *buf;
    double v;

    if (HdrDefList == NULL)
        return 0;
    TrHdrDef *hi = HdrDefList->item(nitm);
    if (hi == NULL)
        return 0;

    int frmt = hi->frmt;
    int hpos = hi->pos;

    if (frmt == 0) {
        return tidx + 1;
    }

    int cnt = ReadRawTrace(buf, tidx, true);
    if (cnt <= 0)
        return 0;

    //  ExprEval*  e    = FTrHdrDef->Expr[nitm];

    /*
        if(e->Expression!="")
        {
            e->TraceHdr = buf;
            e->TraceIdx = tidx+1;//GetRawTraceNumber(ntr)+1;
            e->TraceCnt = FNt;
            v = e->Eval();
        }
    */
    v = iFmt(frmt, buf, hpos, _swap);

    delete[] buf;

    return v;
}

double SeisFile::Re(long long tidx, SeisTrExpr *exp)
{
    byte *buf;
    double v;

    int cnt = ReadRawTrace(buf, tidx, true);
    if (cnt <= 0)
        return 0;

    if (exp->Error())
        return 0;

    exp->setHdr(buf, _hlen, _swap, tidx + 1, Nt());

    v = exp->Evaluate();
    if (exp->Error())
        v = 0;

    delete[] buf;

    return v;
}

bool SeisFile::Welist(long long tidx, QMap<QString, SeisTrExpr> *elist)
{
    byte *buf;
    double v;
    double pv;

    if (HdrDefList == NULL)
        return false;

    int cnt = ReadRawTrace(buf, tidx, true);
    if (cnt <= 0)
        return false;

    QMap<QString, SeisTrExpr>::iterator i;

    for (i = elist->begin(); i != elist->end(); ++i) {
        QString name = i.key();
        SeisTrExpr *e = &(i.value());
        int nitm = HdrDefList->index(name);
        TrHdrDef *hi = HdrDefList->item(nitm); // if(hi==NULL) return false;

        e->setHdr(buf, _hlen, _swap, tidx + 1, Nt());
        v = e->Evaluate();

        if (hi) {
            int frmt = hi->frmt;
            int hpos = hi->pos;

            pv = iFmt(frmt, buf, hpos, _swap);
            e->V = pv;

            sFmt(frmt, buf, hpos, v, _swap);
        }
    }

    cnt = WriteRawTrace(buf, tidx, true);

    delete[] buf;

    if (cnt < 0)
        return false;

    return true;
}

bool SeisFile::Wvlist(long long tidx, QMap<QString, double> *vlist)
{
    byte *buf;
    double v;

    if (HdrDefList == NULL)
        return false;

    int cnt = ReadRawTrace(buf, tidx, true);
    if (cnt <= 0)
        return false;

    QMap<QString, double>::iterator i;

    for (i = vlist->begin(); i != vlist->end(); ++i) {
        QString name = i.key();
        double v = i.value();
        int nitm = HdrDefList->index(name);
        TrHdrDef *hi = HdrDefList->item(nitm); // if(hi==NULL) return false;

        if (hi) {
            int frmt = hi->frmt;
            int hpos = hi->pos;

            sFmt(frmt, buf, hpos, v, _swap);
        }
    }

    cnt = WriteRawTrace(buf, tidx, true);

    delete[] buf;

    if (cnt < 0)
        return false;

    return true;
}

bool SeisFile::Wh(long long tidx, QString hname, double v)
{
    if (HdrDefList == NULL)
        return false;
    int i = HdrDefList->index(hname);
    if (i < 0)
        return false;
    return Wh(tidx, i, v);
}

bool SeisFile::Wh(long long tidx, int nitm, double v)
{
    byte *buf;

    if (HdrDefList == NULL)
        return 0;
    TrHdrDef *hi = HdrDefList->item(nitm);
    if (hi == NULL)
        return false;

    int frmt = hi->frmt;
    int hpos = hi->pos;

    if (frmt == 0)
        return false;

    int cnt = ReadRawTrace(buf, tidx, true);
    if (cnt <= 0)
        return false;

    sFmt(frmt, buf, hpos, v, _swap);

    cnt = WriteRawTrace(buf, tidx, true);

    delete[] buf;

    if (cnt < 0)
        return false;

    return true;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) // implicit conversion, possible loss of data
#else
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

bool SeisFile::AddTrcHeader(QString n, QString d, int f, int p, QString expr)
{
    return false;
}

#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic warning "-Wunused-parameter"
#endif

TrHdrDef *SeisFile::THdef(int nitm)
{
    if (HdrDefList == NULL)
        return NULL;
    TrHdrDef *hi = HdrDefList->item(nitm);

    return hi;
}

int SeisFile::ThCount()
{
    if (HdrDefList == NULL)
        return 0;
    return HdrDefList->count();
}

byte *SeisFile::MakeTrace(int &len, long long ntr, int swap, int frmt, int t1,
                          int t2, bool isProc)
{
    int cnt;
    int hfrm;
    int hpos;

    int out_Ns = t2 - t1;

    len = TrcLen(240, frmt, out_Ns);
    byte *obuf = new byte[len];
    /*
        if(_swap==swap && _Format==frmt)
        {
            cnt = ReadRawTrace(obuf, ntr, false);
            return obuf;
        }
    */
    byte *ibuf = new byte[_trl];

    cnt = ReadRawTrace(ibuf, ntr, false);

    int i;
    float v;

    TrHdrDefList *OutHdrList = &TraceHdrsSet[1];

    memset(obuf, 0, 240);

    for (i = 0; i < HdrDefList->count(); i++) {
        QString name = HdrDefList->item(i)->name;

        int oidx = OutHdrList->index(name);

        if (oidx >= 0) {
            TrHdrDef *ih = HdrDefList->item(name);

            hfrm = ih->frmt;
            hpos = ih->pos;

            if (hfrm == 0)
                v = ntr + 1;
            else
                v = iFmt(hfrm, ibuf, hpos, _swap);

            TrHdrDef *oh = OutHdrList->item(oidx);
            hfrm = oh->frmt;
            hpos = oh->pos;

            if (hfrm == 0)
                continue;

            sFmt(hfrm, obuf, hpos, v, swap);
        }
    }

    TrHdrDef *oh = OutHdrList->item("NSMP");
    hfrm = oh->frmt;
    hpos = oh->pos;
    sFmt(hfrm, obuf, hpos, out_Ns, swap);

    float *dat = (float *)(obuf + 240);
    float *datin = (float *)(ibuf + 240);
    for (i = 0; i < _Ns; i++)
        datin[i] = GetSample(i, ibuf);

    if (isProc)
        execProc(this, _Ns, _Si, datin);

    for (i = 0; i < out_Ns; i++) {
        //        v = GetSample(i,ibuf);

        if (frmt == 5)
            dat[i] = swapf4(datin[i + t1], swap);
        else
            dat[i] = ieee2ibm(datin[i + t1]);
    }

    delete[] ibuf;

    return obuf;
}

float *SeisFile::GetTraceSample(int &len, long long ntr, int swap, int frmt,
                                int t1, int t2)
{
    int cnt;
    //    int hfrm;
    //    int hpos;

    //    int out_Ns = t2 - t1;

    //    len = TrcLen(240,frmt,out_Ns);
    //    byte* obuf = new byte [len];
    /*
        if(_swap==swap && _Format==frmt)
        {
            cnt = ReadRawTrace(obuf, ntr, false);
            return obuf;
        }
    */
    byte *ibuf = new byte[_trl];

    cnt = ReadRawTrace(ibuf, ntr, false);

    int i;
    float v;

    //    memset(obuf,0,240);

    float *datin = (float *)(ibuf + 240);
    for (i = 0; i < _Ns; i++)
        datin[i] = GetSample(i, ibuf);
    return datin;
}

byte *SeisFile::MakeTraceDiff(int &len, long long ntr, int swap, int frmt,
                              int t1, int t2, float *data2)
{
    int cnt;
    int hfrm;
    int hpos;

    int out_Ns = t2 - t1;

    len = TrcLen(240, frmt, out_Ns);
    byte *obuf = new byte[len];
    /*
        if(_swap==swap && _Format==frmt)
        {
            cnt = ReadRawTrace(obuf, ntr, false);
            return obuf;
        }
    */
    byte *ibuf = new byte[_trl];

    cnt = ReadRawTrace(ibuf, ntr, false);

    int i;
    float v;

    TrHdrDefList *OutHdrList = &TraceHdrsSet[1];

    memset(obuf, 0, 240);

    for (i = 0; i < HdrDefList->count(); i++) {
        QString name = HdrDefList->item(i)->name;

        int oidx = OutHdrList->index(name);

        if (oidx >= 0) {
            TrHdrDef *ih = HdrDefList->item(name);

            hfrm = ih->frmt;
            hpos = ih->pos;

            if (hfrm == 0)
                v = ntr + 1;
            else
                v = iFmt(hfrm, ibuf, hpos, _swap);

            TrHdrDef *oh = OutHdrList->item(oidx);
            hfrm = oh->frmt;
            hpos = oh->pos;

            if (hfrm == 0)
                continue;

            sFmt(hfrm, obuf, hpos, v, swap);
        }
    }

    TrHdrDef *oh = OutHdrList->item("NSMP");
    hfrm = oh->frmt;
    hpos = oh->pos;
    sFmt(hfrm, obuf, hpos, out_Ns, swap);

    float *dat = (float *)(obuf + 240);
    float *datin = (float *)(ibuf + 240);
    for (i = 0; i < _Ns; i++)
        datin[i] = GetSample(i, ibuf) - data2[i]; // calculate difference

    //    if (isProc) execProc(this,_Ns, _Si, datin);

    for (i = 0; i < out_Ns; i++) {
        //        v = GetSample(i,ibuf);

        if (frmt == 5)
            dat[i] = swapf4(datin[i + t1], swap);
        else
            dat[i] = ieee2ibm(datin[i + t1]);
    }

    delete[] ibuf;

    return obuf;
}

//---------------------------------------------------------------------------

static int __ff[] = {4, 4, 4, 2, 2, 12, 12, 12, 2, 2, 2, 2, 2, 2, 2, 2,
                     2, 2, 2, 2, 2, 2,  2,  2,  2, 2, 2, 4, 2, 2, 0};

static int __fe[] = {1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};

static int __fp[] = {1,  5,  9,  13, 15, 17, 19,  21,  23,  25, 27,
                     29, 31, 33, 35, 37, 39, 41,  43,  45,  47, 49,
                     51, 53, 55, 57, 59, 63, 301, 303, 305, 0};

const char *__fs[] = {"  Job identification number",                // 0
                      "* Line number",                              // 1
                      "* Reel number",                              // 2
                      "* # data traces per record",                 // 3
                      "* # aux  traces per record",                 // 4
                      "* Sample interval (microseconds) for reel",  // 5
                      "  Sample interval (microseconds) for field", // 6
                      "* Number samples per data trace for reel",   // 7
                      "  Number samples per data trace for field",  // 8
                      "* Data sample format code",                  // 9
                      "* CDP fold",                                 // 10
                      "* Trace sorting code",                       // 11
                      "  Vertical sum code",                        // 12
                      "  Sweep frequency at start",                 // 13
                      "  Sweep frequency at end",                   // 14
                      "  Sweep length (milliseconds)",              // 15
                      "  Sweep type code",                          // 16
                      "  Trace number of sweep channel",            // 17
                      "  Sweep trace taper length at start (ms)",   // 18
                      "  Sweep trace taper length at end   (ms)",   // 19
                      "  Taper type",                               // 20
                      "  Correlated data traces",                   // 21
                      "  Binary gain recovered",                    // 22
                      "  Amplitude recovery method",                // 23
                      "  Measurement system (1-m / 2-feet)",        // 24
                      "  Impulse signal",                           // 25
                      "  Vibratory polarity code",                  // 26
                      "- SSC Co SEG-Y samples number",              // 27
                      "  SEG-Y Rev",                                // 28
                      "  Fixed trace flag",                         // 29
                      "  Number of Extended Textual Headers",       // 30
                      ""};

int BinHed::count()
{
    int n = sizeof(__ff) / sizeof(int);
    return n;
}

int BinHed::val(int idx)
{
    if (idx < 0 || idx >= count())
        return 0;

    int v;

    int p = __fp[idx];
    int f = __ff[idx];

    v = iFmt(f, _buf, p, _swap);

    return v;
}

void BinHed::setVal(int idx, int val)
{
    if (idx < 0 || idx >= count())
        return;

    int p = __fp[idx];
    int f = __ff[idx];

    sFmt(f, _buf, p, val, _swap);
}

QString BinHed::sval(int idx)
{
    if (idx < 0 || idx >= count())
        return 0;

    QString vs;
    int v;

    int p = __fp[idx];
    int f = __ff[idx];

    v = iFmt(f, _buf, p, _swap);

    if (p == 301) {
        int maj = _buf[300];
        int min = _buf[301];
        vs.sprintf("%d.%d", maj, min);
    } else {
        vs.sprintf("%d", v);
    }

    return vs;
}

QString BinHed::name(int idx)
{
    if (idx < 0 || idx >= count())
        return "";
    return __fs[idx];
}

QString BinHed::bstr(int idx)
{
    if (idx < 0 || idx >= count())
        return 0;

    QString s;
    s.sprintf("%d-%d", __fp[idx], __fp[idx] + __ff[idx] - 1);

    return s;
}

int BinHed::edit(int idx)
{
    if (idx < 0 || idx >= count())
        return 0;
    return __fe[idx];
}

int BinHed::check(QVector<QString> vals)
{
    // return:
    //  0 - not changed
    // -1 - data error
    //  1 - changed
    // -2 - index error

    int idx;
    int changed = 0;

    for (idx = 0; idx < vals.count(); idx++) {
        QString vs = vals[idx];

        if (idx < 0 || idx >= count())
            return -2;

        int p = __fp[idx];
        int f = __ff[idx];

        bool ok;
        int vv;
        int v;

        if (p == 301) {
        } else {
            vv = vs.toInt(&ok);

            if (!ok) {
                return -1;
            }

            v = iFmt(f, _buf, p, _swap);

            if (vv != v)
                changed = 1;
        }
    }

    return changed;
}

bool BinHed::update(QVector<QString> vals)
{
    int idx;
    int changed = 0;

    for (idx = 0; idx < vals.count(); idx++) {
        QString vs = vals[idx];

        if (idx < 0 || idx >= count())
            return false;

        int p = __fp[idx];
        int f = __ff[idx];

        bool ok;
        int vv;
        int v;

        if (p == 301) {
        } else {
            vv = vs.toInt(&ok);

            if (!ok) {
                return false;
            }

            v = iFmt(f, _buf, p, _swap);

            if (vv != v) {
                sFmt(f, _buf, p, vv, _swap);
                changed = 1;
            }
        }
    }

    return changed;
}

void BinHed::set(int swap, byte *buf)
{
    _swap = swap;
    memcpy(_buf, buf, 400);
}

void BinHed::set(int swap, int si, int ns, int frmt)
{
    _swap = swap;
    memset(_buf, 0, 400);

    setVal(5, si);
    setVal(7, ns);
    setVal(9, frmt);
}

void BinHed::set(int swap, int frmt, BinHed *src)
{
    _swap = swap;

    if (src->_swap == _swap) {
        memcpy(_buf, src->_buf, 400);
    } else {
        int n;

        memset(_buf, 0, 400);

        for (n = 0; n < src->count(); n++) {
            setVal(n, src->val(n));
        }
    }

    setVal(9, frmt);
}

BinHed::BinHed()
{
    _swap = 1;
    memset(_buf, 0, 400);
}

//---------------------------------------------------------------------------
