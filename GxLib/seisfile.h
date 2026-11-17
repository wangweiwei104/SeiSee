#ifndef SEISFILE_H
#define SEISFILE_H

#include <string>

#include <QCache>

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QVector>

#include "hdrdef.h"
#include "util2.h"

#include "seistrexpr.h"

#include "gfx.h"

using std::string;

extern TrHdrsMap TraceHdrsSet;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(                                                               \
    disable                                                                    \
    : 4100 4244 4267 4305) // implicit conversion, possible loss of data
#else
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

class DataStorageTrace
{
public:
    byte *_buf;
    int _dly;
    float *_smp;
    int _len;

    float operator[](int n)
    {
        return _buf[n];
    }

    DataStorageTrace()
    {
    }
};

class DataStorage
{
private:
    int _nobj; // number of objects;
    int _strl; // number of real items in storage;
    int _olen; // object length
    int _hlen; // header length
    int _nsmp; // number of samples

    int *_obji;  // object index -1 (not in storage)
    int *_stri;  // index of object of this storage item
    int *_odly;  // delay recording time
    byte *_obuf; // storage memory

    int _next;

public:
    DataStorage &Assign(const DataStorage &src);

    DataStorage();
    DataStorage(DataStorage &src)
    {
        Assign(src);
    }

    ~DataStorage();

    DataStorage &operator=(const DataStorage &src)
    {
        return Assign(src);
    }

    void Init(int nobj, int olen, int strl);

    void Init(int nobj, int nsmp, int hlen, int strl);

    void Clear();
    void Reset();

    byte *getObject(int n, int *dly = NULL);
    byte *putObject(int n, int *dly = NULL);

    int nsmp()
    {
        return _nsmp;
    }

    //  DataStorageTrace getTrace(int n);
};

class BinHed
{
protected:
    byte _buf[400];
    int _swap;

public:
    int val(int idx);
    void setVal(int idx, int val);
    QString name(int idx);
    QString sval(int idx);
    QString bstr(int idx); // byte postion description
    int edit(int idx);     // editable
    int count();

    int check(QVector<QString> vals);

    void set(int swap, byte *buf);
    void set(int swap, int si, int ns, int frmt);
    void set(int swap, int frmt, BinHed *src);

    byte *buf()
    {
        return _buf;
    }

    bool update(QVector<QString> vals);

    BinHed();
};

class SeisFile;

class SeisFile : public QObject
{
    Q_OBJECT

protected:
    QMutex _mutex;
    DataStorage _data;

    QList<QObject *> _links;

    TrHdrDefList *HdrDefList;
    QMap<QString, int> Hidx;

    QVector<QString> FormatNames;

    QCache<int, QString> test;

    double _Co; // CDP on first trace;
    double _Po;
    double _Pm;

    double _Si;
    double _Ti;
    int _Nt;
    int _Ns;
    int _Format;

    int _dly; // Use delay recording time

    string _Fname;

    int _smpl;
    int _trl;
    int _tlb;
    int _rd_only;

    float *_inpsmp;
    byte *_inptrc;
    int _f;
    int _hlen; // length of trace header (bytes)
    int _swap;
    int _cgg;
    int _su;
    int _jnk; // junk bytes at the end

    int _txtcode; // 0 - ascii, 1 - ebcdic

    off64_t _flen;

    BinHed _binh;
    char _txthed[3200];

    virtual void Open(string /*fn*/)
    {
    }
    virtual void Close()
    {
    }

    void Invalidate();

public:
    explicit SeisFile(QObject *parent = 0);

    virtual QString typeName()
    {
        return "SeisFile";
    }
    virtual int typeIdx()
    {
        return 0;
    }

    virtual ~SeisFile();

    void RegisterLink(QObject *v);
    void UnRegisterLink(QObject *v);

    bool AddTrcHeader(QString n, QString d, int f, int p, QString expr);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4267 4100 4068 4244 4267 4305)
#else
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

    BinHed *binHed()
    {
        return &_binh;
    }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    int TrLength()
    {
        return _trl;
    }

    QString TxtHed()
    {
        int i, n, l;
        QString hed;
        char s[81];

        s[80] = 0;

        l = 80;

        for (n = 0; n < 40; n++) {
            memcpy(s, _txthed + n * 80, 80);

            for (i = 0; i < l; i++) {
                if (s[i] < 32)
                    s[i] = ' ';
            }

            for (i = l - 1; i >= 0; i--) {
                if (s[i] == ' ')
                    s[i] = 0;
                else
                    break;
            }

            hed = hed + s + QString("\n");
        }

        return hed;
    }

    void Reset()
    {
        _data.Reset();
        Invalidate();
    }

    void setFname(string fname)
    {
        _Fname = fname;
    }

    void setFname(QString fname)
    {
        QByteArray qfn = fname.toLocal8Bit();
        _Fname = qfn.data();
    }

    void setDly(int on)
    {
        _dly = on;
    }

    int Dly()
    {
        return _dly;
    }

    string Fname()
    {
        return _Fname;
    }

    int Format()
    {
        return _Format;
    }

    int Swap()
    {
        return _swap;
    }

    virtual bool Active()
    {
        return false;
    }

    virtual void setActive(bool v)
    {
        if (!v)
            Close();
        else
            Open(_Fname);
        Invalidate();
    }

    virtual int Nt()
    {
        return _Nt;
    }

    virtual int Ns()
    {
        return _Ns;
    }

    virtual double Si()
    {
        return _Si;
    }

    virtual int Jnk()
    {
        return _jnk;
    }

    virtual double Ti()
    {
        return _Ti;
    }

    virtual double Co()
    {
        return _Co;
    }

    virtual double Po()
    {
        return _Po;
    }

    virtual void setPo(double v)
    {
        _Po = v;
        Invalidate();
    }

    virtual double Pm()
    {
        return _Pm;
    }

    virtual void setPm(double v)
    {
        if (v == 0)
            v = 1;
        _Pm = v;
        Invalidate();
    }

    virtual double Tp(long long tidx)
    {
        return tidx * _Pm + _Po;
    }

    virtual double Tmin()
    {
        return 0;
    }

    virtual double Tmax()
    {
        return _Si * (_Ns - 1);
    }

    virtual void ReadTrace(long long ntr)
    {
        Q_UNUSED(ntr);
    }

    virtual int ReadRawTrace(byte *&buf, long long ntr,
                             bool header_only = false)
    {
        Q_UNUSED(ntr);
        Q_UNUSED(header_only);
        buf = NULL;
        return 0;
    }

    byte *MakeTrace(int &len, long long ntr, int swap, int frmt, int t1, int t2,
                    bool isProc);
    float *GetTraceSample(int &len, long long ntr, int swap, int frmt, int t1,
                    int t2);
    byte *MakeTraceDiff(int &len, long long ntr, int swap, int frmt, int t1,
                        int t2, float *data2);

    virtual int WriteRawTrace(byte *, long long ntr, bool header_only = false)
    {
        Q_UNUSED(ntr);
        Q_UNUSED(header_only);
        return 0;
    }

    virtual bool ReadTxtHed()
    {
        memset(_txthed, ' ', 3200);
        _txtcode = 0;
        return true;
    }

    virtual bool ReadBinHed()
    {
        byte buf[400];
        memset(buf, 0, 400);
        _binh.set(_swap, buf);
        return true;
    }

    int TxtCode()
    {
        return _txtcode;
    }

    virtual bool UpdateTxtHed(QString v)
    {
        Q_UNUSED(v);
        return true;
    }
    virtual bool UpdateBinHed(QVector<QString> vals)
    {
        Q_UNUSED(vals);
        return true;
    }

    double Re(long long tidx, SeisTrExpr *exp);
    bool Welist(long long tidx, QMap<QString, SeisTrExpr> *elist);
    bool Wvlist(long long tidx, QMap<QString, double> *vlist);

    double Rh(long long tidx, int nitm);
    double Rh(long long tidx, QString hname);
    bool Wh(long long tidx, int nitm, double v);
    bool Wh(long long tidx, QString hname, double v);

    virtual float *Rs(long long ntr)
    {
        Q_UNUSED(ntr);
        return NULL;
    }

    virtual float *Ts(long long tidx)
    {
        Q_UNUSED(tidx);
        return NULL;
    }

    virtual double Tx(double p)
    {
        double ip = (p - _Po) / _Pm;

        if (ip <= 0)
            return 0;
        if (ip > _Nt - 1)
            return _Nt - 1;
        return ip;
    }

    QString FormatName()
    {
        int idx = _Format - 1;

        if (idx < 0 || idx >= FormatNames.count())
            return "Unknown";

        return FormatNames[idx];
    }

    static int SmpLen(int type, int frmt)
    {
        int smpl = 0;

        if (type == 3) {
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
        } else {
            if (frmt == 1)
                smpl = 4;
            else if (frmt == 2)
                smpl = 4;
            else if (frmt == 3)
                smpl = 2;
            else if (frmt == 4)
                smpl = 4;
            else if (frmt == 5)
                smpl = 4;
            else if (frmt == 6)
                smpl = 1;
            else if (frmt == 7)
                smpl = 4;
            else if (frmt == 8)
                smpl = 1;
            else
                smpl = 4;
        }

        return smpl;
    }

    static int TrcLen(int hdrlen, int frmt, int nsmp)
    {
        int len = SmpLen(1, frmt) * nsmp + hdrlen;
        return len;
    }

    virtual double Th(byte *buf, long long tidx, int nitm);
    virtual double Th(byte *buf, long long tidx, QString hname);
    virtual double Th(long long tidx, int nitm);
    virtual double Th(long long tidx, QString hname);

    virtual int ThIdx(QString hname);

    virtual TrHdrDef *THdef(int nitm);
    virtual int ThCount();

    virtual float GetSample(int idx, byte *inptrc)
    {
        Q_UNUSED(idx);
        Q_UNUSED(inptrc);
        return 0;
    }

    byte *Tb(long long ntr); // Get trace with header

    Ttr Tt(long long ntr); // Get trace with header

signals:
    void execProc(SeisFile *sf, int ns, double si, float *smp);

public slots:
};

//---------------------------------------------------------------------------

#endif // SEISFILE_H
