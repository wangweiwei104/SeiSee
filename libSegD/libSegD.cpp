#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util2.h"



// #include <vld.h>

#if defined(__WIN32__)
#include <io.h>
#endif

#include "libSegD.h"

#ifdef _MSC_VER
#pragma warning(disable : 4267 4100 4068 4244 4267 4305 4068)
#else
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wcast-qual"

using namespace std;

//---------------------------------------------------------------------------

static const char *unknown[] = {"Unknown", "Illegal", "User defined"};

static const char *baux = "Aux  Chan Set";
static const char *bseis = "Seis Chan Set";

static const char *ChanTypeNam3[] = {
    "00:Unused",
    "10:Seis",
    "11:Electromagnetic (EM)",
    "20:Time break",
    "21:Clock timebreak",
    "22:Field timebreak",
    "30:Up hole",
    "40:Water break",
    "50:Timecounter",
    "60:External Data",
    "61:Acoustic range measurement",
    "62:Acoustic reference measured (correlation reference)",
    "63:Acoustic reference nominal (correlation reference)",
    "70:Other",
    "80:Signature/unfiltered",
    "90:Signature/filtered",
    "91:Source signature/unfiltered",
    "92:Source signature/filtered",
    "93:Source signature/estimated",
    "94:Source signature/measured",
    "95:Source base plate",
    "96:Source reference sweep",
    "97:Source other",
    "98:Source reference pilot",
    "99:Source mass",
    "A0:Auxiliary Data Trailer (no longer used)",
    "B0:True reference sweep (correlation reference)",
    "B1:Radio reference sweep",
    "B2:Radio similarity signal",
    "B3:Wireline reference sweep",
    "C0:Depth",
    "C1:Wind",
    "C2:Current",
    "C3:Voltage",
    "C4:Velocity",
    "C5:Acceleration",
    "C6:Pressure",
    "C7:Tension",
    "C8:Tilt measurement",
    "C9:Angle measurement",
    "F0:Calibration trace (time series)"};

static const char *ChanTypeNam2[] = {"0:Undefined",
                                     "1:Seis",
                                     "2:Time break",
                                     "3:Up hole",
                                     "4:Water break",
                                     "5:Time counter",
                                     "6:External Data",
                                     "7:Other",
                                     "8:Signature/unfiltered",
                                     "9:Signature/filtered",
                                     "C:Auxiliary Data Trailer"};

static const char *FormatNam[] = {
    "0015:20 bit binary multiplexed",
    "0022: 8 bit quaternary multiplexed",
    "0024:16 bit quaternary multiplexed",
    "0036:24 bit 2's compliment integer multiplexed",
    "0038:32 bit 2's compliment integer multiplexed",
    "0042: 8 bit hexadecimal multiplexed",
    "0044:16 bit hexadecimal multiplexed",
    "0048:32 bit hexadecimal multiplexed",
    "0058:32 bit IEEE multiplexed",
    "8015:20 bit binary demultiplexed",
    "8022: 8 bit quaternary demultiplexed",
    "8024:16 bit quaternary demultiplexed",
    "8036:24 bit 2's compliment integer demultiplexed",
    "8038:32 bit 2's compliment integer demultiplexed",
    "8042: 8 bit hexadecimal demultiplexed",
    "8044:16 bit hexadecimal demultiplexed",
    "8048:32 bit hexadecimal demultiplexed",
    "8058:32 bit IEEE demultiplexed",
    "8080:64 bit IEEE demultiplexed",
    "9036:24 bit 2's complement integer Little endian",
    "9038:32 bit 2's complement integer Little endian",
    "9058:32 bit IEEE Little endian ",
    "9080:64 bit IEEE Little endian ",
    "0200:Illegal, do not use",
    "0000:Illegal, do not use"};

static const char *PolarityNam[] = {"0:Untested", "1:Zero", "2:45 degrees",
                                    "3:90 degrees",
                                    "4:135 degrees"
                                    "5:180 degrees"
                                    "6:225 degrees"
                                    "7:270 degrees"
                                    "8:315 degrees"
                                    "C:Unassigned"};

static const char *McNam[] = {
    "01:Alpine Geophysical Associates, Inc. (Obsolete)",
    "02:Applied Magnetics Corporation (See 09)",
    "03:Western Geophysical Exploration Products (Litton Resources Systems)",
    "04:SIE, Inc. (Obsolete)",
    "05:Dyna-Tronics Mfg. Corporation (Obsolete)",
    "06:Electronic Instrumentation, Inc. (Obsolete)",
    "07:Halliburton Geophysical Services, Inc. (Electro-Technical Labs)",
    "08:Fortune Electronics, Inc. (Obsolete)",
    "09:Geo Space Corporation ",
    "10:Leach Corporation (Obsolete)",
    "11:Metrix Instrument Co. (Obsolete)",
    "12:Redcor Corporation (Obsolete)",
    "13:Sercel (Societe d'Etudes, Recherches Et Constructions Electroniques), "
    "France",
    "14:Scientific Data Systems (SDS), (Obsolete)",
    "15:Texas Instruments, Inc.",
    "17:GUS Manufacturing, Inc.",
    "18:Input/Output, Inc.",
    "19:Geco-Prakla (Terra Marine Engineering)",
    "20:Fairfield Industries, Inc.",
    "22:Geco-Prakla, UK",
    "31:Japex Geoscience Institute, Japan",
    "32:Halliburton Geophysical Services, Inc.",
    "33:Compuseis, Inc.",
    "34:Syntron, Inc.",
    "35:Syntron Europe, UK",
    "39:Grant Geophysical",
    "40:Geo-X, Canada",
    "41:PGS Inc.",
    "42:SeaMap, UK ",
    "43:Hydroscience Inc.",
    "44:JSC Special Design Bureau For Seismic Instrumentation, Russia",
    "45:Fugro, Norway",
    "46:ProFocus Systems AS, Norway",
    "47:Optoplan AS, Norway",
    "48:Wireless Seismic Inc.",
    "49:AutoSeis",
    "50:INOVA Geophysical, Inc.",
    "51:Verif-i Ltd., UK",
    "52:Troika International, UK",
    "53:MagSeis AS, Norway",
    "54:Seismic Instruments, Inc",
    "55:TGS, UK",
    "56:Hewlett Packard Co",
    "57:Modern Seismic Technology, LLC, Russia",
    "58:OCTIO,Norway",
    "58:inApril AS,Norway"};

static const char *RecTypeNam[] = {"2:Test record", "4:Parallel channel test",
                                   "6:Direct channel test", "8:Normal record",
                                   "1:Other"};

static const char *PhaseCtrlNam[] = {
    "0:Phase Control not recorded", "1:Baseplate accelerometer",
    "2:Reaction Mass", "3:Weighted sum", "4:Direct force measurement"};

static const char *VibratorNam[] = {"0:Type not recorded", "1:P wave vibrator",
                                    "2:Shear wave vibrator",
                                    "3:Marine vibrator"};

static const char *SoilTypeNam[] = {
    "00:Water",      "01:Alluvium",   "02:Dry sand", "03: Weathering",
    "04: Mud",       "05: Glacial",   "06: Shale",   "07: Sand",
    "08: Sandstone", "09: Limestone", "0A: Granite", "0B: Chalk",
    "0C: Gypsum",    "0D: Salt",      "0E: Gabbro",  "FF:Other"};

static const char *EmSrcTypeNam[] = {
    "00:Unknown",   "01:Dipole",      "02:Loop Coil", "03:Airborne",
    "04:Open Loop", "05:Closed loop", "0F:Other"};

static const char *SrcStatusNam[] = {
    "00:Untested/unknown",   "01:Source fired OK", "12:Timing error",
    "13:Mechanical failure", "14:Software error",  "15:Hydraulics failure",
    "16:Air leakage",        "17:Power failure",   "18:Communication failure",
    "FF:Unknown error"};

static const char *PosTypeNam[] = {"01:Planned/preplot", "02:Measured",
                                   "03:Processed", "04:Final", "0F:Unknown"};

static const char *PosQualNam[] = {
    "00:Position not present", "01:Position is good", "02:Quality uncertain",
    "03:Position is bad, not to be used"};

static const char *ArrayFormNam[] = {
    "01:No array forming.",
    "02: 2 groups summed, no weighting.",
    "03: 3 groups summed, no weighting.",
    "04: 4 groups summed, no weighting.",
    "05: 5 groups summed, no weighting.",
    "06: 6 groups summed, no weighting.",
    "07: 7 groups summed, no weighting.",
    "08: 2 groups summed, no weighting.",
    "09: 3 groups summed, no weighting.",
    "0A:10 groups summed, no weighting.",
    "0B:11 groups summed, no weighting.",
    "0C:12 groups summed, no weighting.",
    "0D:13 groups summed, no weighting.",
    "0E:14 groups summed, no weighting.",
    "0F:15 groups summed, no weighting.",
    "10: 0 groups weighted, overlapping, summation",
    "11: 1 groups weighted, overlapping, summation",
    "12: 2 groups weighted, overlapping, summation",
    "13: 3 groups weighted, overlapping, summation",
    "14: 4 groups weighted, overlapping, summation",
    "15: 5 groups weighted, overlapping, summation",
    "16: 6 groups weighted, overlapping, summation",
    "17: 7 groups weighted, overlapping, summation",
    "18: 8 groups weighted, overlapping, summation",
    "19: 9 groups weighted, overlapping, summation",
    "1A:10 groups weighted, overlapping, summation",
    "1B:11 groups weighted, overlapping, summation",
    "1C:12 groups weighted, overlapping, summation",
    "1D:13 groups weighted, overlapping, summation",
    "1E:14 groups weighted, overlapping, summation",
    "1F:15 groups weighted, overlapping, summation"};

static const char *ChanGCNam[] = {
    "1:Individual AGC",  "2:Ganged AGC",          "3:Fixed gain",
    "4:Programmed gain", "8:Binary gain control", "9:IFP gain control"};

static const char *FiltPhaseNam[] = {
    "00:Unknown", "01:Minimum", "02:Linear",
    "03:Zero",    "04:Mixed",   "05:Maximum",
};

static const char *PhysUnitNam[] = {"00:Unknown",
                                    "01:Millibar",
                                    "02:Bar",
                                    "03:Millimeter/second",
                                    "04:Meter/second",
                                    "05:Millimeter/second/second",
                                    "06:Meter/second/second",
                                    "07:Newton",
                                    "08:Kelvin",
                                    "09:Hertz",
                                    "0A:Second",
                                    "0B:Tesla",
                                    "0C:Volt/meter",
                                    "0D:Volt meter",
                                    "0E:Ampere/meter",
                                    "0F:Volt",
                                    "10:Ampere",
                                    "11:Radians(angle)",
                                    "12:Pascal",
                                    "13:Micropascal"};

static const char *SensorTypNam[] = {"00:Not defined",
                                     "01:Hydrophone(pressure sensor)",
                                     "02:Geophone (velocity sensor) Vertical",
                                     "03:Geophone, Horizontal, inline",
                                     "04:Geophone, Horizontal, cross-line",
                                     "05:Geophone, Horizontal, other",
                                     "06:Accelerometer, Vertical",
                                     "07:Accelerometer, Horizontal, inline",
                                     "08:Accelerometer, Horizontal, crossline",
                                     "09:Accelerometer, Horizontal, other",
                                     "15:Electric Dipole",
                                     "16:Magnetic coil"};

static const char *EquipTestNam[] = {"00:Unknown (untested)", "01:Passed",
                                     "02:Failed",
                                     "03:Uncertain (warning during IT)"};

static const char *TimeDriftNam[] = {
    "00:Uncorrected", "01:Linear correction (values in this header used)",
    "FF:Other, manufacturer method"};

static const char *OrientTypNam[] = {
    "00:None/Unknown",
    "01:Earth true north",
    "02:Cable direction (e.g. marine/seabedstreamer)",
    "03:Sensor housing orientation ",
    "04:Survey orientation",
    "05:Magnetic north",
    "09:Other"};

static const char *HeadBlkNam[] = {
    "02:General Header 2",
    "03:Timestamp and size header",
    "10:Vessel/Crew identification",
    "11:Survey Area Name",
    "12:Client Name",
    "13:Job identification",
    "14:Line Identification",
    "15:Vibrator Source Information",
    "16:Explosive source information",
    "17:Airgun source information",
    "18:Watergun source information",
    "19:Electromagnetic source",
    "1F:Other source type information",
    "20:Additional source information",
    "21:Source Auxiliary Channel Reference",
    "30:Channel set description block 1",
    "31:Channel set description block 2",
    "32:Channel set description block 3",
    "40:Demux Trace header extension 1",
    "41:Sensor Info header extension",
    "42:Time Stamp Header",
    "43:Sensor calibration",
    "44:Time drift",
    "45:Electromagnetic Src/Recv desc",
    "50:Position block 1",
    "51:Position block 2",
    "52:Position block 3",
    "55:Coordinate Reference System Identification",
    "56:Relative position",
    "60:Orientation Header",
    "61:Measurement",
    "70:General Trailer Description",
    "B0:User defined",
    "FF:User defined"};

IdxNamMap::IdxNamMap()
{
    unki = 0;
    inri = -1;
    idx1 = 0;
    idx2 = 0;
}

IdxNamMap::IdxNamMap(const char *data[], int datacnt, int idxbase, int unk,
                     int i1, int i2, int inr)
{
    int i, idx;
    const char *pidx;
    const char *pnam;
    const char *psep;
    char *pend;

    unki = unk;

    idx1 = i1;
    idx2 = i2;
    inri = inr;

    for (i = 0; i < datacnt; i++) {
        pidx = data[i];
        psep = strchr(pidx, ':');

        if (psep == NULL)
            continue;

        idx = strtol(pidx, &pend, idxbase);

        if (errno == ERANGE)
            continue;

        pnam = psep + 1;

        map[idx] = pnam;

        //      qDebug() << "idx=" << idx << "name="  << pnam;
    }
}

const char *IdxNamMap::operator[](int idx)
{
    if (inri >= 0 && idx >= idx1 && idx <= idx2)
        return unknown[inri];

    if (!map.contains(idx))
        return unknown[unki];

    return map[idx];
}

static IdxNamMap ChanTypeName2(ChanTypeNam2,
                               sizeof(ChanTypeNam2) / sizeof(char *), 16);
static IdxNamMap ChanTypeName3(ChanTypeNam3,
                               sizeof(ChanTypeNam3) / sizeof(char *), 16);
static IdxNamMap PolarityName(PolarityNam, sizeof(PolarityNam) / sizeof(char *),
                              16);
static IdxNamMap ManafactName(McNam, sizeof(McNam) / sizeof(char *), 10);
static IdxNamMap FormatName(FormatNam, sizeof(FormatNam) / sizeof(char *), 10);
static IdxNamMap RecTypeName(RecTypeNam, sizeof(RecTypeNam) / sizeof(char *),
                             10);
static IdxNamMap PhaseCtrlName(PhaseCtrlNam,
                               sizeof(PhaseCtrlNam) / sizeof(char *), 10);
static IdxNamMap VibratorName(VibratorNam, sizeof(VibratorNam) / sizeof(char *),
                              10);
static IdxNamMap SoilTypeName(SoilTypeNam, sizeof(SoilTypeNam) / sizeof(char *),
                              16);
static IdxNamMap EmSrcTypeName(EmSrcTypeNam,
                               sizeof(EmSrcTypeNam) / sizeof(char *), 16);
static IdxNamMap SrcStatusName(SrcStatusNam,
                               sizeof(SrcStatusNam) / sizeof(char *), 16);
static IdxNamMap PosTypeName(PosTypeNam, sizeof(PosTypeNam) / sizeof(char *),
                             16);
static IdxNamMap PosQualName(PosQualNam, sizeof(PosQualNam) / sizeof(char *),
                             16);
static IdxNamMap ArrayFormName(ArrayFormNam,
                               sizeof(ArrayFormNam) / sizeof(char *), 16);
static IdxNamMap ChanGCName(ChanGCNam, sizeof(ChanGCNam) / sizeof(char *), 16);
static IdxNamMap FiltPhaseName(FiltPhaseNam,
                               sizeof(FiltPhaseNam) / sizeof(char *), 16);
static IdxNamMap PhysUnitName(PhysUnitNam, sizeof(PhysUnitNam) / sizeof(char *),
                              16);
static IdxNamMap SensorTypName(SensorTypNam,
                               sizeof(SensorTypNam) / sizeof(char *), 10);
static IdxNamMap EquipTestName(EquipTestNam,
                               sizeof(EquipTestNam) / sizeof(char *), 16);
static IdxNamMap TimeDriftName(TimeDriftNam,
                               sizeof(TimeDriftNam) / sizeof(char *), 16);
static IdxNamMap OrientTypName(OrientTypNam,
                               sizeof(OrientTypNam) / sizeof(char *), 16);
static IdxNamMap HeadBlkName(HeadBlkNam, sizeof(HeadBlkNam) / sizeof(char *),
                             16, 0, 0xB0, 0xFF, 2);

string SegdTimeToStr(long long ts)
{
    //---------------------------------------------------------------------------
    QDateTime timeConvertor;
    QString dateTime = "06-01-1980 00:00:00";

    double t = round(ts / 1e6);

    // Mark this QDateTime as one with a certain offset from UTC, and set that
    // offset.
    timeConvertor.setTimeSpec(Qt::OffsetFromUTC);
    timeConvertor.setUtcOffset(0);

    timeConvertor = QDateTime::fromString(dateTime, "dd-MM-yyyy HH:mm:ss");

    dateTime = timeConvertor.toString("yyyy-MM-dd hh:mm:ss");

    // Store the UTC timestamp.
    int timestamp = timeConvertor.toTime_t();

    timestamp = round(t) + timestamp;

    timeConvertor.setTime_t(timestamp);

    dateTime = timeConvertor.toString("yyyy-MM-dd hh:mm:ss");

    QByteArray ba = dateTime.toLocal8Bit();

    return ba.data();
}

//---------------------------------------------------------------------------
ptrAry::ptrAry()
{
    _nalloc = 1;
    _data = new void *[1];
    _nitems = 0;
}

ptrAry::ptrAry(ptrAry &src)
{
    _nitems = src._nitems;
    _nalloc = src._nalloc;

    _data = new void *[_nalloc];

    if (_nalloc)
        memcpy(_data, src._data, _nalloc * sizeof(void *));
}

ptrAry::~ptrAry()
{
    delete[] _data;
}

void *&ptrAry::operator[](int i)
{
    if (i >= _nitems)
        throw "ptrAry: index error";

    return _data[i];
}

ptrAry &ptrAry::operator=(const ptrAry &src)
{
    if (this == &src)
        return *this;

    _nitems = src._nitems;
    _nalloc = src._nalloc;

    delete[] _data;
    _data = new void *[_nalloc];

    if (_nalloc)
        memcpy(_data, src._data, _nalloc * sizeof(void *));

    return *this;
}

int ptrAry::IndexOf(void *v)
{
    int n;
    for (n = 0; n < _nitems; n++)
        if (_data[n] == v)
            return n;
    return -1;
}

void ptrAry::Del(void *v)
{
    Del(IndexOf(v));
}

void ptrAry::Del(int i)
{
    if (_nitems < 1 || i < 0 || i >= _nitems)
        return;

    int n;

    for (n = i + 1; n < _nitems; n++)
        _data[n - 1] = _data[n];

    _nitems--;
}

void ptrAry::Add(void *v)
{
    int n;

    if (_nitems >= _nalloc) {
        _nalloc += 1000;

        void **tmp = new void *[_nalloc];

        for (n = 0; n < _nitems; n++)
            tmp[n] = _data[n];

        delete[] _data;
        _data = tmp;
    }

    _data[_nitems++] = v;
}

void ptrAry::Clear()
{
    delete[] _data;
    _nalloc = 1000;
    _data = new void *[1000];
    _nitems = 0;
}

int ptrAry::SetLength(int len)
{
    int n;
    int na;

    na = len;
    if (na < 1)
        na = 1;

    void **tmp = new void *[na];

    memset(tmp, 0, sizeof(void *) * na);

    for (n = 0; n < _nitems && n < len; n++)
        tmp[n] = _data[n];

    delete[] _data;
    _data = tmp;

    _nitems = len;
    _nalloc = na;

    return _nitems;
}
//---------------------------------------------------------------------------

byteAry::byteAry()
{
    _nitems = 0;
    _data = NULL;
}

byteAry::byteAry(byteAry &src)
{
    _nitems = src._nitems;

    if (_nitems) {
        _data = new byte[_nitems];
        memcpy(_data, src._data, _nitems);
    } else {
        _data = NULL;
    }
}

byteAry::~byteAry()
{
    if (_data)
        delete[] _data;
}

byte &byteAry::operator[](int i)
{
    if (i >= _nitems || i < 0)
        throw "byteAry: index error";

    return _data[i];
}

byteAry &byteAry::operator=(const byteAry &src)
{
    if (this == &src)
        return *this;

    _nitems = src._nitems;

    if (_nitems) {
        _data = new byte[_nitems];
        memcpy(_data, src._data, _nitems);
    } else {
        _data = NULL;
    }

    return *this;
}

void byteAry::Clear()
{
    if (_data)
        delete[] _data;
    _nitems = 0;
    _data = NULL;
}

int byteAry::SetLength(int len)
{
    if (len == _nitems)
        return _nitems;

    if (len <= 0) {
        Clear();
        return 0;
    }

    byte *tmp = new byte[len];

    if (_nitems > len)
        _nitems = len;

    memset(tmp, 0, len);

    if (_nitems)
        memcpy(tmp, _data, _nitems);

    delete[] _data;
    _data = tmp;

    _nitems = len;

    return _nitems;
}

//---------------------------------------------------------------------------

TSegD::TSegD()
{
    objs = new TMyObjList;
    csl = new TCsList;
    agh = new TGenHedAll(this);

    _buf = NULL;
    _pos = 0;
    _len = 0;

    Clear();
}

void TSegD::Clear()
{
    if (!_keep_buf && _buf)
        delete[] _buf;

    _buf = NULL;
    _pos = 0;
    _len = 0;

    RevMin = 0;
    RevMaj = 0;

    K[0] = 0;

    Format = 0;
    R = 0;
    ST = 0;
    CS = 0;
    SK = 0;
    EC = 0;
    EX = 0;
    EF = 0;
    GH = 0;
    EGH = 0;
    I = 0;
    Mc = 0;
    Ms = 0;
    P = 0;
    THE = 0;
    SKK = 0;
    SP = -999999;
    XX = 0;
    XF = 0;

    R_ms = 0;

    NavStr = "";

    Client = "";
    Contra = "";
    LineNm = "";

    ReelNo = 0;

    hdr_len = 0;
    trc_cnt = 0;

    atn = 0;

    objs->Clear();
    csl->Clear();

    _err = "";

    _keep_buf = false;
}

TSegD::~TSegD()
{
    delete objs;
    delete csl;
    delete agh;
    if (!_keep_buf && _buf)
        delete[] _buf;
}

int TSegD::get_nobj(int type)
{
    int l = objs->getCount();

    if (type < 0)
        return l;

    int n, i;

    for (n = i = 0; i < l; i++) {
        TSegDObj *obj = get_obj(i);

        if (obj->type == type)
            n++;
    }

    return n;
}

TCs *TSegD::get_csi(int idx)
{
    return (TCs *)(csl->getObj(idx));
}

int TSegD::find_csi(int csn, int stn)
{
    int i;
    for (i = 0; i < get_ncs(); i++) {
        TCs *Cs = get_csi(i);
        if (Cs->csn == csn && Cs->stn == stn)
            return i;
    }
    return -1;
}

TSegDObj *TSegD::get_obj(int idx)
{
    if (idx < 0)
        return (TSegDObj *)agh;
    return (TSegDObj *)(objs->getObj(idx));
}

int TSegD::get_ncs()
{
    return csl->getCount();
}

int TSegD::Parse(byte *buf, int len, bool HeadersOnly, bool keep_buf)

{
    int n, i;
    long long tpos = 0;
    int scnlen = 0;
    int lenok;
    byte *hed;
    int nGH = 0;

    Clear();

    _keep_buf = keep_buf;

    TSegDObj *obj = NULL;

    hdr_len = 0;
    trc_cnt = 0;

    _buf = buf;
    _len = len;

    new TSegdInfo(this);

    hed = read(128);

    if (hed) {
        seek(0, SEEK_SET);
        if (strncmp((const char *)hed + 4, (const char *)"SD2.", 4) == 0) {
            new TTapeHdr(this);
            seek(128, SEEK_SET);
        }

        if (strncmp((const char *)hed + 4, (const char *)"SD3.", 4) == 0) {
            new TTapeHdr(this);
            seek(128, SEEK_SET);
        }

        if (strncmp((const char *)hed, (const char *)"SS36", 4) == 0) {
            int ffid;
            if (is_segd_m(hed + 32, ffid)) {
                new TSsHdr(this);
                seek(32, SEEK_SET);
            }
        }

        if (len == 128) {
            Type = 21;
            goto END;
        }
    }

    obj = new TGenHdr1(this);
    if (obj->get_err() != "") {
        objs->Del(obj);
        obj = NULL;
        goto END;
    }

    if (Format == 200) {
        //  SEG-B parsing

        objs->Del(obj);
        obj = NULL;
        seek(0, SEEK_SET);
        obj = new TGenHdrB(this);

        int ntrs = ((TGenHdrB *)obj)->NTRS;
        int naux = ((TGenHdrB *)obj)->NAUX;

        trc_cnt = ntrs + naux;

        obj = new TCsB(this, 0, naux);
        obj = new TCsB(this, 1, ntrs);

        obj = new TMux(this);
        if (obj->get_err() != "")
            goto END;

        goto END;
    }

    //  SEG-D parsing

    if (GH > 0 || GH < 0) {
        obj = new TGenHdr2(this);
        if (obj->get_err() != "")
            goto END;

        if (EF) // rg16 fix
        {
            unsigned short ex = EF;
            swap2(&ex);

            memcpy(obj->get_buf() + 7, &ex, 4);
        }
    }

    if (GH > 1 || EGH > 1) {
        obj = new TGenHdr3(this);
        if (obj->get_err() != "")
            goto END;
    }

    nGH = GH;

    if (RevMaj == 3) {
        if (nGH < 0)
            nGH = EGH;
    }

    for (n = 2; n < nGH; n++) {
        obj = new TGenHdrN(this, n + 2);
        if (obj->get_err() != "")
            goto END;
    }

    for (i = 0; i < ST; i++) {
        for (n = 0; n < CS; n++) {
            obj = new TCsD(this);
            if (obj->get_err() != "")
                goto END;
        }

        if (SK > 0) {
            obj = new TSkew(this);
            if (obj->get_err() != "")
                goto END;
        }
    }

    if (EC > 0) {
        obj = new TEcHdr(this);
        if (obj->get_err() != "")
            goto END;
    }

    if (EX > 0) {
        obj = new TExHdr(this);
        if (obj->get_err() != "")
            goto END;
    }

    if (XX > 0) {
        obj = new TXxHdr(this);
        if (obj->get_err() != "")
            goto END;
    }

    // Fix for rg16
    if (XF && csl->getCount() == 1) {
        TCs *cs = get_csi(0);
        cs->nch = XF;

        int2hexd(XF, cs->get_buf() + 8, 2);
    }

    for (n = 0; n < csl->getCount(); n++) {
        TCs *cs = get_csi(n);
        trc_cnt = trc_cnt + cs->nch;
    }

    if (RevMaj == 3) {
        hdr_len = 32 * (ST * (CS * 3 + SK) + 1 + nGH + EC + EX);
    } else {
        hdr_len = 32 * (ST * (CS + SK) + 1 + nGH + EC + EX);
    }

    if (HeadersOnly)
        goto END;

    if (Si > 0 && R_ms > 0)
        TrcSmpNum = (int)(R_ms / Si) + 1;
    else
        TrcSmpNum = 0;
    if (Format / 100) // demultiplexed
    {
        for (;;) {
            int tpc = tell();
            int tp = find_trcx(tpc, FFFF, F);
            if (tp < 0)
                tp = _len;

            if (tp != tpc) {
                new TJunk(this, tp - tpc);
                seek(tp, SEEK_SET);
            }

            TTrInf ti = chck_trcx(tp, FFFF, F);
            TTrInf tn;

            if (ti.stat == 0) {
                new TSegdEof(this, "End of file!");
                break;
            }
            if (ti.stat < 0) {
                new TTr(this, ti);
                break;
            }
            lenok = 0;

            // printf("stat=%d stn=%d csn=%d chn=%d\n", ti.stat,
            // ti.stn,ti.csn,ti.chn); fflush(stdout);

            int csidx = find_csi(ti.csn, ti.stn);
            TCs *Cs = get_csi(csidx);

            int Ns = 0;
            int Nso = (int)(Cs->tml / Si);
            int Nss = Cs->nsscan;
            int Hl = 20 + 32 * ti.the;
            int Ln = 0;
            int Lj = 0;

            if (Cs->ns > 0) {
                Ln = Cs->ln;
                Ns = Cs->ns;
                tn = chck_trcx(tp + Ln, FFFF, F);
                if (tn.stat >= 0)
                    lenok = 1;
            }

            if (!lenok && ti.exns > 0) {
                // printf("exns=%d\n", ti.exns); fflush(stdout);
                Ns = ti.exns;
                Ln = Hl + Ns * SmplB;
                tn = chck_trcx(tp + Ln, FFFF, F);
                if (tn.stat >= 0)
                    lenok = 1;
            }

            if (!lenok) {
                Ns = Nso * Nss + 1;
                Ln = Hl + Ns * SmplB;
                tn = chck_trcx(tp + Ln, FFFF, F);
                // printf("next1=%d stn=%d csn=%d chn=%d len=%d\n", tn.stat,
                // tn.stn,tn.csn,tn.chn,Ln); fflush(stdout);
                if (tn.stat >= 0)
                    lenok = 1;
            }

            if (!lenok) {
                Ns = Nso * Nss;
                Ln = Hl + Ns * SmplB;
                tn = chck_trcx(tp + Ln, FFFF, F);
                // printf("next1=%d stn=%d csn=%d chn=%d len=%d\n", tn.stat,
                // tn.stn,tn.csn,tn.chn,Ln); fflush(stdout);
                if (tn.stat >= 0)
                    lenok = 1;
            }

            if (!lenok) {
                Nss = 1;
                Ns = Nso;
                Ln = Hl + Ns * SmplB;
                tn = chck_trcx(tp + Ln, FFFF, F);
                // printf("next2=%d stn=%d csn=%d chn=%d len=%d\n", tn.stat,
                // tn.stn,tn.csn,tn.chn,Ln); fflush(stdout);
                if (tn.stat >= 0)
                    lenok = 1;
            }

            if (!lenok) {
                Nss = 1;
                Ns = Nso + 1;
                Ln = Hl + (Ns + 1) * SmplB;
                tn = chck_trcx(tp + Ln, FFFF, F);
                // printf("next2=%d stn=%d csn=%d chn=%d len=%d\n", tn.stat,
                // tn.stn,tn.csn,tn.chn,Ln); fflush(stdout);
                if (tn.stat >= 0)
                    lenok = 1;
            }

            if (!lenok) {
                // После текущей трассы не найдена следующая или конец файла
                // Пытаемся определить длину трассы
                // ориентируясь на конец файла

                Nss = Cs->nsscan;

                int ln = Hl + Nso * SmplB; // minimal possible length
                int pos = find_trcx(tp + ln, FFFF, F);

                if (pos < 0)
                    pos = _len;

                ln = pos - tp;

                if (ti.exns) {
                    Ns = ti.exns;
                } else if (ln >= Hl + Nso * Nss + 1 * SmplB) {
                    Ns = Nso * Nss + 1;
                } else if (ln >= Hl + Nso * Nss * SmplB) {
                    Ns = Nso * Nss;
                } else if (ln >= Hl + (Nso + 1) * SmplB) {
                    Ns = Nso + 1;
                    Nss = 1;
                } else if (ln >= Hl + Nso * SmplB) {
                    Ns = Nso;
                    Nss = 1;
                }

                Ln = Hl + Ns * SmplB;

                if (ln >= Ln) {
                    // Lj = ln - Ln;
                    lenok = 1;
                }

                // printf("fixl=%d junk=%d\n",Ln, Lj); fflush(stdout);
                // nextok=1;
            }

            // printf("this=%d stn=%d csn=%d chn=%d len=%d junk=%d\n", ti.stat,
            // ti.stn,ti.csn,ti.chn,Ln,Lj); fflush(stdout); printf("next=%d
            // stn=%d csn=%d chn=%d len=%d junk=%d\n", tn.stat,
            // tn.stn,tn.csn,tn.chn,Ln,Lj); fflush(stdout);

            if (lenok) {
                if (Cs->ns < 1) {
                    Cs->ns = Ns;
                    Cs->ln = Ln;
                    Cs->nsscan = Nss;
                    Cs->si = Si / (Nss);
                }

                TTr *tr = new TTr(this, ti);
                if (tr->get_err() != "") {
                    break;
                }

                if (Lj > 0) {
                    seek(Ln + tp, SEEK_SET);
                    new TJunk(this, Lj);
                }

                seek(Ln + Lj + tp, SEEK_SET);
            } else {
                break;
            }
        }

        int csi;

        for (ntrs = csi = 0; csi < csl->getCount(); csi++) {
            TCs *cs = get_csi(csi);
            ntrs += cs->nch;
        }

        if (atn < ntrs) {
            new TSegdWarn(this, "Unexpected end of traces!");
        }
    } else // multiplexed
    {
        scnlen = 0;
        for (n = 0; n < csl->getCount(); n++) {
            TCs *cs = get_csi(n);

            int nss = cs->nsscan;
            int nch = cs->nch;
            scnlen += nch * nss;
        }

        scnlen = (int)(scnlen * Smpl / 8.) + 8;

        obj = new TMux(this);
        if (obj->get_err() != "")
            goto END;
    }

END:

    if (obj)
        _err = obj->get_err();
    else
        _err = "Unknown data";

    if (_err != "" || tpos < 0)
        return -1;

    if (Format != 200) {
        if (Si > 0 && R_ms > 0)
            TrcSmpNum = (int)(R_ms / Si) + 1;
        else
            TrcSmpNum = 0;

        int samples_len = (TrcSmpNum * Smpl) / 8;
        int trheadr_len = 20 + 32 * THE;

        int fheader_len = 0;

        if (RevMaj == 3) {
            fheader_len = 32 * (ST * (CS * 3 + SK) + 1 + nGH + EC + EX + XX);
        } else {
            fheader_len = 32 * (ST * (CS + SK) + 1 + nGH + EC + EX + XX);
        }

        int trace_len = trheadr_len + samples_len;

        int trblock_len = trc_cnt * (trace_len);

        hdr_len = fheader_len;
        trc_len = trblock_len;
    }

    if (!_keep_buf && _buf) {
        delete[] _buf;
        _buf = NULL;
    }

    return 1;
}
//---------------------------------------------------------------------------

int TSegDObj::xprintf(int out, const char *fmt, ...)
{
    char buffer[1024];
    va_list argptr;
    int cnt;

    va_start(argptr, fmt);
    cnt = vsprintf(buffer, fmt, argptr);
    va_end(argptr);

    if (out == 0)
        _inf = _inf + buffer;
    else
        _err = _err + buffer;

    return cnt;
}

//---------------------------------------------------------------------------

void TSegDObj::displayMemory(int out, byte *address, int length)
{
    byte *ao = hdr;

    int i = 0; // used to keep track of line lengths
    int j;

    for (i = 0; i < length && i < 16000; i += 16) {
        xprintf(out, "%08d | ", address - ao + _pos + i);

        for (j = 0; j < 16; j++) {
            if (i + j < length)
                xprintf(out, "%02X ", (unsigned char)(address[i + j]));
            else
                xprintf(out, "   ");
        }

        xprintf(out, " | ");

        for (j = 0; j < 16 && i + j < length; j++) {
            unsigned char c = (unsigned char)(address[i + j]);

            if (c < 33 || c > 126)
                c = 0x2e;

            xprintf(out, "%c", c);
        }

        xprintf(out, "\r\n");
    }

    if (i < length)
        xprintf(out, "...\r\n");
    xprintf(out, "\r\n");
}

//---------------------------------------------------------------------------

string TGenHdr1::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    char s[2048];

    if (_segd->Format == 200) {
        xprintf(0, pritem("File Number                   F  ", 0, hdr, 1, 4));
        xprintf(0, "SEG-B Header Block #1\r\r\n\r\r\n");
    } else {
        xprintf(0, "General Header Block #1\r\n\r\n");
    }
    displayMemory(0, hdr, len);

    if (_segd->Format == 200)
        return _inf;

    xprintf(0, pritem("File Number                   F  ", 0, hdr, 1, 4));
    xprintf(0, pritem("Format Code                   Y  ", 0, hdr, 5, 4, 0));
    xprintf(0, "(%s)\r\n", FormatName[_segd->Format]);
    xprintf(0, pritem("General Constants             K  ", 0, hdr, 9, 12));
    xprintf(0, pritem("Year                          YR ", 0, hdr, 21, 2));
    xprintf(0, pritem("Num add bocks in  gen header  GH ", 0, hdr, 23, 1));
    xprintf(0, pritem("Julian day                    DY ", 0, hdr, 24, 3));
    xprintf(0, pritem("Hour of day                   H  ", 0, hdr, 27, 2));
    xprintf(0, pritem("Minute of hour                MI ", 0, hdr, 29, 2));
    xprintf(0, pritem("Second of minute              SE ", 0, hdr, 31, 2));

    xprintf(0, pritem("Manufacturer's code           Mc ", 1, hdr, 33, 2, 0));
    xprintf(0, "(%s)\r\n", ManafactName[_segd->Mc]);

    xprintf(0, pritem("Manufacturer's serial nb      Ms ", 0, hdr, 35, 4));
    xprintf(0, pritem("Bytes per scan                B  ", 0, hdr, 39, 6));
    xprintf(0, pritem("Base scan interval            I  ", 1, hdr, 45, 2, 0));
    xprintf(0, "[%g ms]\r\n", _segd->Si);

    xprintf(0, pritem("Polarity                      P  ", 0, hdr, 47, 1, 0));
    xprintf(0, "(%s)\r\n", PolarityName[_segd->P]);

    xprintf(0, pritem("S/B exponent                  SB ", 1, hdr, 48, 3));

    xprintf(0, pritem("Record type                   Z  ", 0, hdr, 51, 1, 0));
    int z = hex2uint(hexs(hdr, 51, 1, s));
    xprintf(0, "(%s)\r\n", RecTypeName[z]);

    xprintf(0, pritem("Record length                 R  ", 0, hdr, 52, 3, 0));
    hexs(hdr, 52, 3, s);
    if (_segd->R < 0)
        xprintf(0, "\r\n");
    else
        xprintf(0, "[%g ms]\r\n", _segd->R_ms);

    xprintf(0, pritem("Scan types per record         STR", 0, hdr, 55, 2));
    xprintf(0, pritem("# of chan sets per scan type  CS ", 0, hdr, 57, 2));
    xprintf(0, pritem("# of sample skew 32 byte ext  SK ", 0, hdr, 59, 2));
    xprintf(0, pritem("Extended header length        EC ", 0, hdr, 61, 2));
    xprintf(0, pritem("External header length        EX ", 0, hdr, 63, 2));

    return _inf;
}

TGenHdr1::TGenHdr1(TSegD *segd)
    : TSegDObj(segd)
{
    int v;
    char s[2048];

    name = "General #1";

    byte *dat = _segd->read(32);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    hexs(dat, 1, 4, s);
    if (strcmp(s, "FFFF") == 0)
        _segd->F = -1;
    else
        _segd->F = atoi(s);

    memcpy(&(_segd->FFFF), dat, 2);

    _segd->Format = bcd_int_item(dat, 5, 4);

    _segd->Smpl = 0;

    if (_segd->Format == 8015)
        _segd->Smpl = 20;
    if (_segd->Format == 8022)
        _segd->Smpl = 8;
    if (_segd->Format == 8024)
        _segd->Smpl = 16;
    if (_segd->Format == 8036)
        _segd->Smpl = 24;
    if (_segd->Format == 8038)
        _segd->Smpl = 32;
    if (_segd->Format == 8042)
        _segd->Smpl = 2;
    if (_segd->Format == 8044)
        _segd->Smpl = 16;
    if (_segd->Format == 8048)
        _segd->Smpl = 32;
    if (_segd->Format == 8058)
        _segd->Smpl = 32;

    if (_segd->Format == 8080)
        _segd->Smpl = 64;

    if (_segd->Format == 9036)
        _segd->Smpl = 24;
    if (_segd->Format == 9038)
        _segd->Smpl = 32;
    if (_segd->Format == 9058)
        _segd->Smpl = 32;
    if (_segd->Format == 9080)
        _segd->Smpl = 64;

    if (_segd->Format == 15)
        segd->Smpl = 20;
    if (_segd->Format == 22)
        _segd->Smpl = 8;
    if (_segd->Format == 24)
        _segd->Smpl = 16;
    if (_segd->Format == 36)
        _segd->Smpl = 24;
    if (_segd->Format == 38)
        _segd->Smpl = 32;
    if (_segd->Format == 42)
        _segd->Smpl = 2;
    if (_segd->Format == 44)
        _segd->Smpl = 16;
    if (_segd->Format == 48)
        _segd->Smpl = 32;
    if (_segd->Format == 58)
        _segd->Smpl = 32;

    _segd->SmplB = _segd->Smpl / 8.;

    if (_segd->Format == 200) {
        _segd->Type = 2;
        return;
    }

    if (_segd->Smpl == 0) {
        hexs(dat, 5, 4, s);
        xprintf(1, "Unknown format %s\r\n", s);
        return;
    }

    _segd->Type = 3;

    bcd_str_item(dat, 9, 12, _segd->K);

    hexs(dat, 23, 1, s);
    if (strcmp(s, "F") == 0) {
        _segd->GH = -1;
    } else {
        _segd->GH = bcd_int_item(dat, 23, 1);
    }

    _segd->Mc = bcd_int_item(dat, 33, 2);
    _segd->Ms = bcd_int_item(dat, 35, 4);

    v = uint_item(dat, 45, 2);
    _segd->Si = v / 16.;

    _segd->P = bcd_int_item(dat, 47, 1);

    hexs(dat, 52, 3, s);
    if (strcmp(s, "FFF") == 0) {
        _segd->R = -1;
    } else {
        _segd->R = atoi(s);
        _segd->R_ms = _segd->R * 0.1024 * 1000;
    }

    hexs(dat, 55, 2, s);
    _segd->ST = atoi(s);

    hexs(dat, 57, 2, s);
    if (strcmp(s, "FF") == 0)
        _segd->CS = -1;
    else
        _segd->CS = atoi(s);

    hexs(dat, 59, 2, s);
    _segd->SK = atoi(s);

    hexs(dat, 61, 2, s);
    if (strcmp(s, "FF") == 0)
        _segd->EC = -1;
    else
        _segd->EC = atoi(s);

    hexs(dat, 63, 2, s);
    if (strcmp(s, "FF") == 0)
        _segd->EX = -1;
    else
        _segd->EX = atoi(s);

    set_len(32);
    memcpy(hdr, dat, 32);

    type = 0;
}

string TGenHdr2::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    int RevMaj = _segd->RevMaj;

    xprintf(0, "General Header Block #2\r\n\r\n");

    displayMemory(0, hdr, len);

    if (RevMaj != 3) {
        xprintf(0, pritem("Extended File number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Extended Ch Sets/Scan Types n/u", 1, hdr, 7, 4));
        xprintf(0, pritem("Extended Hdr Blocks         n/u", 1, hdr, 11, 4));

        if (_segd->EF) {
            xprintf(0, pritem("Undefined    (FairField rg16)", 0, hdr, 15, 2));
            xprintf(0, pritem("External header blocks (rg16)", 1, hdr, 17, 4));
        } else {
            xprintf(0, pritem("External header blocks       ", 1, hdr, 15, 4));
            xprintf(0, pritem("Undefined                    ", 0, hdr, 19, 2));
        }
        xprintf(0, pritem("SEG-D Revision Number          ", 1, hdr, 21, 2));
        xprintf(0, pritem(".                              ", 1, hdr, 23, 4));

        xprintf(0, pritem("Num Blocks of General Trailer  ", 0, hdr, 25, 4));
        xprintf(0, pritem("Extended Record Length ms      ", 1, hdr, 29, 6));
        xprintf(0, pritem("Not used                       ", 0, hdr, 35, 2));
        xprintf(0, pritem("General Header Block Number    ", 1, hdr, 37, 2));
        xprintf(0, pritem("Ext # of Gen Header Blocks     ", 1, hdr, 45, 2));
        xprintf(0, pritem("Undefined                      ", 0, hdr, 39, 13));
    } else {
        xprintf(0, pritem("Extended File number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Extended Ch Sets/Scan Types    ", 1, hdr, 7, 4));
        xprintf(0, pritem("Extended Hdr blocks            ", 1, hdr, 11, 6));
        xprintf(0, pritem("Extended Skew Blocks           ", 1, hdr, 17, 4));
        xprintf(0, pritem("SEG-D Revision Number          ", 1, hdr, 21, 2));
        xprintf(0, pritem(".                              ", 1, hdr, 23, 4));
        xprintf(0, pritem("# of Blocks of Gen Trailer     ", 1, hdr, 25, 4));
        xprintf(0, pritem("Extended Record Length         ", 1, hdr, 33, 8));
        xprintf(0, pritem("Record Set Number              ", 1, hdr, 41, 4));
        xprintf(0, pritem("Ext # Add Blocks in Gen Header ", 1, hdr, 45, 4));
        xprintf(0, pritem("Dominant Sampling Interval     ", 1, hdr, 49, 6));
        xprintf(0, pritem("External header blocks         ", 1, hdr, 55, 6));
        xprintf(0, pritem("Undefined                      ", 0, hdr, 61, 2));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : General Header Block #2","");
    }

    return _inf;
}

//---------------------------------------------------------------------------

TGenHdr2::TGenHdr2(TSegD *segd)
    : TSegDObj(segd)
{
    int CS;
    int EX;
    int R;
    int F;
    int EGH;

    int v;
    char s[2048];

    name = "General #2";

    byte *dat = _segd->read(32);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    v = hex2uint(hexs(dat, 21, 4, s));

    _segd->RevMaj = v >> 8;
    _segd->RevMin = v & 0xFF;

    int RevMaj = v >> 8;

    if (RevMaj != 3) {
        v = hex2uint(hexs(dat, 1, 6, s));
        F = v;
        v = hex2uint(hexs(dat, 7, 4, s));
        CS = v;
        v = hex2uint(hexs(dat, 11, 4, s));
        EC = v;
        v = hex2uint(hexs(dat, 29, 6, s));
        R = v;
        EGH = 0;
    } else {
        v = hex2uint(hexs(dat, 1, 6, s));
        F = v;
        v = hex2uint(hexs(dat, 7, 4, s));
        CS = v;
        v = hex2uint(hexs(dat, 11, 6, s));
        EC = v;
        v = hex2uint(hexs(dat, 33, 8, s));
        R = v;
        v = hex2uint(hexs(dat, 45, 4, s));
        EGH = v;
        bid = uint_item(dat, 63, 2);
    }

    if (_segd->F < 0)
        _segd->F = F;
    if (_segd->CS < 0)
        _segd->CS = CS;
    if (_segd->EC < 0)
        _segd->EC = EC;

    _segd->EGH = EGH;

    if (segd->Mc == 20 && segd->RevMaj == 0x01 && segd->RevMin == 0x06) {
        v = hex2uint(hexs(dat, 17, 4, s));
        EX = v;
        _segd->EF = EX;
    } else if (RevMaj == 3) {
        v = hex2uint(hexs(dat, 55, 6, s));
        EX = v;
    } else {
        v = hex2uint(hexs(dat, 15, 4, s));
        EX = v;
    }

    if (_segd->EX < 0) {
        _segd->EX = EX;
    }

    if (_segd->R_ms < 0)
        _segd->R_ms = R;

    set_len(32);
    memcpy(hdr, dat, 32);
    type = 0;
}

string TGenHdr3::get_inf()
{
    int v;

    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    int RevMaj = _segd->RevMaj;

    if (RevMaj == 3) {
        v = uint_item(hdr, 63, 2);
        xprintf(0, "General Header Block #3 (%s)\r\n\r\n", HeadBlkName[v]);
    } else {
        xprintf(0, "General Header Block #3\r\n\r\n");
    }

    displayMemory(0, hdr, len);

    if (RevMaj != 3) {
        xprintf(0, pritem("Expanded file number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Source Line Number             ", 1, hdr, 7, 6));
        xprintf(0, pritem(".                              ", 1, hdr, 13, 2));
        xprintf(0, pritem("Source Point Number            ", 1, hdr, 17, 6));
        xprintf(0, pritem(".                              ", 1, hdr, 23, 2));
        xprintf(0, pritem("Source Point Index             ", 1, hdr, 27, 2));

        xprintf(0, pritem("Phase Control                  ", 1, hdr, 29, 2, 0));
        v = uint_item(hdr, 29, 2);
        xprintf(0, "(%s)\r\n", PhaseCtrlName[v]);

        xprintf(0, pritem("Vibrator Type                  ", 1, hdr, 31, 2, 0));
        v = uint_item(hdr, 31, 2);
        xprintf(0, "(%s)\r\n", VibratorName[v]);

        xprintf(0, pritem("Phase angle                    ", 1, hdr, 33, 4));
        xprintf(0, pritem("General Header Block Number    ", 1, hdr, 37, 2));
        xprintf(0, pritem("Source Set Number              ", 1, hdr, 39, 2));
        xprintf(0, pritem("Not used                       ", 0, hdr, 42, 12));
    } else {
        xprintf(0, pritem("Time Zero                      ", 6, hdr, 1, 16));
        xprintf(0, pritem("Record size                    ", 11, hdr, 17, 16));
        xprintf(0, pritem("Data size                      ", 11, hdr, 33, 16));
        xprintf(0, pritem("Header size                    ", 1, hdr, 49, 8));
        xprintf(0, pritem("Extended Recording Mode        ", 1, hdr, 57, 2));
        xprintf(0, pritem("Relative Time Mode             ", 1, hdr, 59, 2));
        xprintf(0, pritem("Undefined                      ", 0, hdr, 61, 2));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Timestamp and size header","");
    }

    return _inf;
}

TGenHdr3::TGenHdr3(TSegD *segd)
    : TSegDObj(segd)
{
    name = "General #3";

    int RevMaj = _segd->RevMaj;

    byte *dat = _segd->read(32);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    if (RevMaj < 3) {
        _segd->SP = uint_item(dat, 17, 6);
    } else {
        bid = uint_item(dat, 63, 2);
    }

    set_len(32);
    memcpy(hdr, dat, 32);
    type = 0;
}

string TGenHdrB::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "SEG-B Header Block #1\r\r\n\r\r\n");

    displayMemory(0, hdr, len);

    xprintf(0, pritem("File Number                   F  ", 0, hdr, 1, 4));
    xprintf(0, pritem("Format Code                   Y  ", 0, hdr, 5, 4, 0));
    xprintf(0, "(%s)\r\n", "SEG-B 20 bit binary multiplexed");
    xprintf(0, pritem("General Constants             K  ", 0, hdr, 9, 12));

    int B = bcd_int_item(hdr, 21, 3);
    xprintf(0, pritem("# Bytes per Data Scan         B  ", 0, hdr, 21, 3));

    xprintf(0, pritem("# Sample interval (ms)        B  ", 0, hdr, 24, 1));
    xprintf(0, pritem("Manufacturer's code           Mc ", 0, hdr, 25, 2, 0));
    xprintf(0, "(%s)\r\n", ManafactName[_segd->Mc]);
    xprintf(0, pritem("Manufacturer's serial nb      Ms ", 0, hdr, 27, 6));
    xprintf(0, pritem("Record length (s)             R  ", 0, hdr, 33, 2));

    // int J = bcd_int_item(hdr, 35, 1);
    xprintf(0, pritem("Amplifier gain control mode   J  ", 0, hdr, 35, 1));

    int Z = bcd_int_item(hdr, 36, 1);
    xprintf(0, pritem("Type of record                Z  ", 0, hdr, 36, 1, 0));

    if (Z == 8)
        xprintf(0, "(%s)\r\n", "shot");
    else if (Z == 4)
        xprintf(0, "(%s)\r\n", "shot bridle");
    else if (Z == 2)
        xprintf(0, "(%s)\r\n", "test");
    else if (Z == 1)
        xprintf(0, "(%s)\r\n", "other");
    else
        xprintf(0, "(%s)\r\n", "unknown");

    xprintf(0, pritem("Low cut filter setting        Lc ", 0, hdr, 37, 2));
    xprintf(0, pritem("Low cut filter slope          Ls ", 0, hdr, 39, 1));
    xprintf(0, pritem("High cut filter setting       Hc ", 0, hdr, 41, 3));
    xprintf(0, pritem("High cut filter slope         Hs ", 0, hdr, 44, 1));
    xprintf(0, pritem("Special filter setting        S  ", 0, hdr, 45, 2));
    xprintf(0, pritem("Alias filter setting          A  ", 0, hdr, 47, 1));

    int NAUX = 5;
    int ntr = ((B - 4) / 2 - NAUX) / NAUX * 4;

    xprintf(0, "\r\n");

    int n;

    const char *chnam[8] = {
        "Undefined channel",   // 0
        "Seismic channel",     // 1
        "Time-break channel",  // 2
        "Uphole channel",      // 3
        "Water-break channel", // 4
        "Time counter",        // 5
        "Gain channel",        // 6
        "Other"                // 7;
    };

    int ntrfix = (_segd->hdr_len - 24) / 2 - 5;

    if (ntrfix < ntr)
        ntr = ntrfix;

    for (n = 0; n < ntr + 5; n++) {
        char s[1024];
        unsigned char *h = (unsigned char *)hdr;
        int nb = 24 + n * 2;
        unsigned char b1 = h[nb];
        //      unsigned char b2 = h[nb+1];

        int ty = b1 >> 5;

        //      int g1 = b1&0x1f;

        if (n < ntr)
            sprintf(s, "Seis channel #%2d Type / Gain     ", n + 1);
        else
            sprintf(s, "Aux  channel #%2d Type / Gain     ", n + 1 - ntr);
        xprintf(0, pritem(s, 0, hdr, nb * 2 + 1, 4, 0), ntr);
        if (ty >= 0 && ty <= 7)
            xprintf(0, "(%s)", chnam[ty]);

        xprintf(0, "\r\n");
    }
    return _inf;
}

TGenHdrB::TGenHdrB(TSegD *segd)
    : TSegDObj(segd)
{
    char s[2048];

    segd->seek(0, SEEK_SET);

    name = "SEG-B Header";

    byte *dat = _segd->read(32);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    hexs(dat, 1, 4, s);
    if (strcmp(s, "FFFF") == 0)
        _segd->F = -1;
    else
        _segd->F = atoi(s);

    memcpy(&(_segd->FFFF), dat, 2);

    _segd->Format = bcd_int_item(dat, 5, 4);

    _segd->Smpl = 0;

    if (_segd->Format == 200) {
        _segd->Type = 2;
        _segd->Smpl = 20;
    }

    if (_segd->Smpl == 0) {
        hexs(dat, 5, 4, s);
        xprintf(1, "Unknown format %s\r\n", s);
        return;
    }

    _segd->SmplB = _segd->Smpl / 8.;

    bcd_str_item(dat, 9, 12, _segd->K);

    _segd->GH = 0;

    _segd->Mc = bcd_int_item(dat, 25, 2);
    _segd->Ms = bcd_int_item(dat, 27, 4);

    _segd->Si = bcd_int_item(dat, 24, 1);

    _segd->R = bcd_int_item(dat, 33, 2);
    _segd->R_ms = _segd->R * 1000;

    _segd->CS = 2;

    int B = bcd_int_item(dat, 21, 3); // # Bytes per scan

    //  B=314;

    _segd->B = B;

    NAUX = 5;

    NTRS = ((B - 4) / 2 - NAUX) / NAUX * 4;

    int HDRL = NTRS * 2 + NAUX * 2 + 10 + 16;

    _segd->seek(0, SEEK_SET);
    dat = _segd->read(HDRL);

    set_len(HDRL);
    memcpy(hdr, dat, HDRL);

    // Check Real Header Len --------------

    int i;

    for (i = 24; i < HDRL; i += 2) {
        int b1 = hdr[i];
        int b2 = hdr[i + 1];
        if (b1 == -1 && b2 == -1)
            break;
        //     if(b1==0x03 && b2==0x03) break;
    }

    // ------------------------------------

    if (i < HDRL) {
        HDRL = i;
        _segd->seek(0, SEEK_SET);
        dat = _segd->read(HDRL);
    }

    _segd->hdr_len = HDRL;

    int DTL = _segd->getlen() - _segd->hdr_len;
    int NS = DTL / B;

    int NSo = _segd->R_ms / _segd->Si;

    if (NS > NSo)
        NS = NSo;

    //    NS=180;

    _segd->TrcSmpNum = NS;

    type = 0;
}

string TGenHdrN::get_inf()
{
    int type = 0;
    int v;

    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    if (_segd->RevMaj > 2) {
        type = uint_item(hdr, 63, 2);
        xprintf(0, "General Header Block #%d (%s)\r\n\r\n", ghn,
                HeadBlkName[type]);
    } else {
        xprintf(0, "General Header Block #%d\r\n\r\n", ghn);
    }

    displayMemory(0, hdr, len);

    if (type == 0x10) {
        xprintf(0, pritem("Abbreviated vessel / crew name ", 5, hdr, 1, 6));
        xprintf(0, pritem("Vessel  or  crew  name         ", 5, hdr, 7, 56));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Vessel/Crew Identification","");
    } else if (type == 0x11) {
        xprintf(0, pritem("Survey Area Name               ", 5, hdr, 1, 62));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Survey Area Name","");
    } else if (type == 0x12) {
        xprintf(0, pritem("(Client Identification         ", 5, hdr, 1, 62));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Client Identification","");
    } else if (type == 0x13) {
        xprintf(0, pritem("Abbreviated Job Identification ", 5, hdr, 1, 6));
        xprintf(0, pritem("Job  Identification            ", 5, hdr, 7, 56));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Job identification","");
    } else if (type == 0x14) {
        xprintf(0, pritem("Line Abbreviation              ", 5, hdr, 1, 14));
        xprintf(0, pritem("Line Identification            ", 5, hdr, 15, 48));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Line identification","");
    } else if (type == 0x15) {
        xprintf(0, pritem("Expanded File Number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Source Line Number             ", 1, hdr, 7, 6));
        xprintf(0, pritem("Source Line Number Fraction    ", 1, hdr, 13, 4));
        xprintf(0, pritem("Source Point Number            ", 1, hdr, 17, 6));
        xprintf(0, pritem("Source Point Number Fraction   ", 1, hdr, 23, 4));
        xprintf(0, pritem("Source Point Index             ", 1, hdr, 27, 2));

        xprintf(0, pritem("Phase Control                  ", 1, hdr, 29, 2, 0));
        v = uint_item(hdr, 29, 2);
        xprintf(0, "(%s)\r\n", PhaseCtrlName[v]);

        xprintf(0, pritem("Type Vibrator                  ", 1, hdr, 31, 2, 0));
        v = uint_item(hdr, 31, 2);
        xprintf(0, "(%s)\r\n", VibratorName[v]);

        xprintf(0, pritem("Phase Angle                    ", 2, hdr, 33, 4));
        xprintf(0, pritem("Source Id                      ", 1, hdr, 37, 2));
        xprintf(0, pritem("Source Set Number              ", 1, hdr, 39, 2));
        xprintf(0, pritem("Re-shoot Index                 ", 1, hdr, 41, 2));
        xprintf(0, pritem("Group Index                    ", 1, hdr, 43, 2));
        xprintf(0, pritem("Depth Index                    ", 1, hdr, 45, 2));
        xprintf(0, pritem("Offset cross-line (cm)         ", 2, hdr, 47, 4));
        xprintf(0, pritem("Offset in-line (cm)            ", 2, hdr, 51, 4));
        xprintf(0, pritem("Size of source                 ", 1, hdr, 55, 4));
        xprintf(0, pritem("Offset depth (cm)              ", 2, hdr, 59, 4));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Vibrator Source Identification","");
    } else if (type == 0x16) {
        xprintf(0, pritem("Expanded File Number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Source Line Number             ", 1, hdr, 7, 6));
        xprintf(0, pritem("Source Line Number Fraction    ", 1, hdr, 13, 4));
        xprintf(0, pritem("Source Point Number            ", 1, hdr, 17, 6));
        xprintf(0, pritem("Source Point Number Fraction   ", 1, hdr, 23, 4));
        xprintf(0, pritem("Source Point Index             ", 1, hdr, 27, 2));
        xprintf(0, pritem("Depth (cm)                     ", 1, hdr, 29, 4));
        xprintf(0, pritem("Charge length (dm)             ", 1, hdr, 33, 2));

        xprintf(0, pritem("Soil Type                      ", 1, hdr, 35, 2, 0));
        v = uint_item(hdr, 35, 2);
        xprintf(0, "(%s)\r\n", SoilTypeName[v]);

        xprintf(0, pritem("Source Id                      ", 1, hdr, 37, 2));
        xprintf(0, pritem("Source Set Number              ", 1, hdr, 39, 2));
        xprintf(0, pritem("Re-shoot Index                 ", 1, hdr, 41, 2));
        xprintf(0, pritem("Group Index                    ", 1, hdr, 43, 2));
        xprintf(0, pritem("Depth Index                    ", 1, hdr, 45, 2));
        xprintf(0, pritem("Offset cross-line (cm)         ", 2, hdr, 47, 4));
        xprintf(0, pritem("Offset in-line (cm)            ", 2, hdr, 51, 4));
        xprintf(0, pritem("Size of source (g)             ", 1, hdr, 55, 4));
        xprintf(0, pritem("Offset depth (cm)              ", 2, hdr, 59, 4));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Explosive Source Identification","");
    } else if (type == 0x17) {
        xprintf(0, pritem("Expanded File Number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Source Line Number             ", 1, hdr, 7, 6));
        xprintf(0, pritem("Source Line Number Fraction    ", 1, hdr, 13, 4));
        xprintf(0, pritem("Source Point Number            ", 1, hdr, 17, 6));
        xprintf(0, pritem("Source Point Number Fraction   ", 1, hdr, 23, 4));
        xprintf(0, pritem("Source Point Index             ", 1, hdr, 27, 2));
        xprintf(0, pritem("Depth (cm)                     ", 1, hdr, 29, 4));
        xprintf(0, pritem("Air Pressure (psi)             ", 1, hdr, 33, 4));
        xprintf(0, pritem("Source Id                      ", 1, hdr, 37, 2));
        xprintf(0, pritem("Source Set Number              ", 1, hdr, 39, 2));
        xprintf(0, pritem("Re-shoot Index                 ", 1, hdr, 41, 2));
        xprintf(0, pritem("Group Index                    ", 1, hdr, 43, 2));
        xprintf(0, pritem("Depth Index                    ", 1, hdr, 45, 2));
        xprintf(0, pritem("Offset cross-line (cm)         ", 2, hdr, 47, 4));
        xprintf(0, pritem("Offset in-line (cm)            ", 2, hdr, 51, 4));
        xprintf(0, pritem("Size of source (cu.in)         ", 1, hdr, 55, 4));
        xprintf(0, pritem("Offset depth (cm)              ", 2, hdr, 59, 4));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Airgun Source Identification","");
    } else if (type == 0x18) {
        xprintf(0, pritem("Expanded File Number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Source Line Number             ", 1, hdr, 7, 6));
        xprintf(0, pritem("Source Line Number Fraction    ", 1, hdr, 13, 4));
        xprintf(0, pritem("Source Point Number            ", 1, hdr, 17, 6));
        xprintf(0, pritem("Source Point Number Fraction   ", 1, hdr, 23, 4));
        xprintf(0, pritem("Source Point Index             ", 1, hdr, 27, 2));
        xprintf(0, pritem("Depth (cm)                     ", 1, hdr, 29, 4));
        xprintf(0, pritem("Air Pressure (psi)             ", 1, hdr, 33, 4));
        xprintf(0, pritem("Source Id                      ", 1, hdr, 37, 2));
        xprintf(0, pritem("Source Set Number              ", 1, hdr, 39, 2));
        xprintf(0, pritem("Re-shoot Index                 ", 1, hdr, 41, 2));
        xprintf(0, pritem("Group Index                    ", 1, hdr, 43, 2));
        xprintf(0, pritem("Depth Index                    ", 1, hdr, 45, 2));
        xprintf(0, pritem("Offset cross-line (cm)         ", 2, hdr, 47, 4));
        xprintf(0, pritem("Offset in-line (cm)            ", 2, hdr, 51, 4));
        xprintf(0, pritem("Size of source (cu.in)         ", 1, hdr, 55, 4));
        xprintf(0, pritem("Offset depth (cm)              ", 2, hdr, 59, 4));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Wartergun Source Identification","");
    } else if (type == 0x19) {
        xprintf(0, pritem("Expanded File Number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Source Line Number             ", 1, hdr, 7, 6));
        xprintf(0, pritem("Source Line Number Fraction    ", 1, hdr, 13, 4));
        xprintf(0, pritem("Source Point Number            ", 1, hdr, 17, 6));
        xprintf(0, pritem("Source Point Number Fraction   ", 1, hdr, 23, 4));
        xprintf(0, pritem("Source Point Index             ", 1, hdr, 27, 2));

        xprintf(0, pritem("EM Source Type                 ", 1, hdr, 29, 2, 0));
        v = uint_item(hdr, 29, 2);
        xprintf(0, "(%s)\r\n", EmSrcTypeName[v]);

        xprintf(0, pritem("Source   Moment                ", 1, hdr, 31, 6));
        xprintf(0, pritem("Source Id                      ", 1, hdr, 37, 2));
        xprintf(0, pritem("Source Set Number              ", 1, hdr, 39, 2));
        xprintf(0, pritem("Re-shoot Index                 ", 1, hdr, 41, 2));
        xprintf(0, pritem("Group Index                    ", 1, hdr, 43, 2));
        xprintf(0, pritem("Depth Index                    ", 1, hdr, 45, 2));
        xprintf(0, pritem("Offset cross-line (cm)         ", 2, hdr, 47, 4));
        xprintf(0, pritem("Offset in-line (cm)            ", 2, hdr, 51, 4));
        xprintf(0, pritem("Source Current or Voltage      ", 1, hdr, 55, 4));
        xprintf(0, pritem("Offset depth (cm)              ", 2, hdr, 59, 4));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Electromagnetic Source Identification","");
    } else if (type == 0x1f) {
        xprintf(0, pritem("Expanded File Number           ", 1, hdr, 1, 6));
        xprintf(0, pritem("Source Line Number             ", 1, hdr, 7, 6));
        xprintf(0, pritem("Source Line Number Fraction    ", 1, hdr, 13, 4));
        xprintf(0, pritem("Source Point Number            ", 1, hdr, 17, 6));
        xprintf(0, pritem("Source Point Number Fraction   ", 1, hdr, 23, 4));
        xprintf(0, pritem("Source Point Index             ", 1, hdr, 27, 2));
        xprintf(0, pritem("Undefined                      ", 0, hdr, 29, 8));
        xprintf(0, pritem("Source Id                      ", 1, hdr, 37, 2));
        xprintf(0, pritem("Source Set Number              ", 1, hdr, 39, 2));
        xprintf(0, pritem("Re-shoot Index                 ", 1, hdr, 41, 2));
        xprintf(0, pritem("Group Index                    ", 1, hdr, 43, 2));
        xprintf(0, pritem("Depth Index                    ", 1, hdr, 45, 2));
        xprintf(0, pritem("Offset cross-line (cm)         ", 2, hdr, 47, 4));
        xprintf(0, pritem("Offset in-line (cm)            ", 2, hdr, 51, 4));
        xprintf(0, pritem("Undefined                      ", 0, hdr, 55, 4));
        xprintf(0, pritem("Offset depth (cm)              ", 2, hdr, 59, 4));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Other Source Identification","");
    } else if (type == 0x20) {
        xprintf(0, pritem("Time                           ", 6, hdr, 1, 16));

        xprintf(0, pritem("Source Status                  ", 1, hdr, 17, 2, 0));
        v = uint_item(hdr, 17, 2);
        xprintf(0, "(%s)\r\n", SrcStatusName[v]);

        xprintf(0, pritem("Source Id                      ", 1, hdr, 19, 2));
        xprintf(0, pritem("Source Moving                  ", 1, hdr, 21, 2));
        xprintf(0, pritem("Error  Description             ", 5, hdr, 23, 40));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Additional Source Info","");
    } else if (type == 0x21) {
        xprintf(0, pritem("Source Id                      ", 1, hdr, 1, 2));

        xprintf(0, pritem("Channel 1, Scan Type Number    ", 0, hdr, 3, 2));
        xprintf(0, pritem("Channel 1, Channel Set Number  ", 1, hdr, 5, 4));
        xprintf(0, pritem("Channel 1, Trace Number        ", 1, hdr, 9, 6));

        xprintf(0, pritem("Channel 2, Scan Type Number    ", 0, hdr, 15, 2));
        xprintf(0, pritem("Channel 2, Channel Set Number  ", 1, hdr, 17, 4));
        xprintf(0, pritem("Channel 2, Trace Number        ", 1, hdr, 21, 6));

        xprintf(0, pritem("Channel 3, Scan Type Number    ", 0, hdr, 27, 2));
        xprintf(0, pritem("Channel 3, Channel Set Number  ", 1, hdr, 29, 4));
        xprintf(0, pritem("Channel 3, Trace Number        ", 1, hdr, 33, 6));

        xprintf(0, pritem("Channel 4, Scan Type Number    ", 0, hdr, 39, 2));
        xprintf(0, pritem("Channel 4, Channel Set Number  ", 1, hdr, 41, 4));
        xprintf(0, pritem("Channel 4, Trace Number        ", 1, hdr, 45, 6));

        xprintf(0, pritem("Channel 5, Scan Type Number    ", 0, hdr, 51, 2));
        xprintf(0, pritem("Channel 5, Channel Set Number  ", 1, hdr, 53, 4));
        xprintf(0, pritem("Channel 5, Trace Number        ", 1, hdr, 57, 6));

        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Source Auxiliary Channel Reference","");
    } else if (type == 0x50) {
        xprintf(0, pritem("Time of Position               ", 6, hdr, 1, 16));
        xprintf(0, pritem("Time of Measurement/Calculation", 6, hdr, 17, 16));
        xprintf(0, pritem("Vertical error quality         ", 3, hdr, 33, 8));
        xprintf(0, pritem("Horizontal error quality A     ", 3, hdr, 41, 8));
        xprintf(0, pritem("Horizontal error quality B     ", 3, hdr, 49, 8));
        xprintf(0, pritem("Horizontal error quality O     ", 1, hdr, 57, 4));

        xprintf(0, pritem("Position type                  ", 1, hdr, 61, 2, 0));
        v = uint_item(hdr, 61, 2);
        xprintf(0, "(%s)\r\n", PosTypeName[v]);

        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Position Block 1","");
    } else if (type == 0x51) {
        xprintf(0, pritem("Tuple 1 coordinate 1           ", 4, hdr, 1, 16));
        xprintf(0, pritem("Tuple 1 coordinate 2           ", 4, hdr, 17, 16));
        xprintf(0, pritem("Tuple 1 coordinate 3           ", 4, hdr, 33, 16));
        xprintf(0, pritem("Location Data Stanza ID 1      ", 1, hdr, 49, 4));
        xprintf(0, pritem("Position 1 Valid               ", 1, hdr, 53, 2));

        xprintf(0, pritem("Position 1 Quality             ", 1, hdr, 55, 2, 0));
        v = uint_item(hdr, 55, 2);
        xprintf(0, "(%s)\r\n", PosQualName[v]);

        xprintf(0, pritem("Undefined                      ", 0, hdr, 57, 6));

        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Position Block 2","");
    } else if (type == 0x52) {
        xprintf(0, pritem("Tuple 2 coordinate 1           ", 4, hdr, 1, 16));
        xprintf(0, pritem("Tuple 2 coordinate 2           ", 4, hdr, 17, 16));
        xprintf(0, pritem("Tuple 2 coordinate 3           ", 4, hdr, 33, 16));
        xprintf(0, pritem("Location Data Stanza ID 2      ", 1, hdr, 49, 4));
        xprintf(0, pritem("Position 2 Valid               ", 1, hdr, 53, 2));

        xprintf(0, pritem("Position 2 Quality             ", 1, hdr, 55, 2, 0));
        v = uint_item(hdr, 55, 2);
        xprintf(0, "(%s)\r\n", PosQualName[v]);

        xprintf(0, pritem("Undefined                      ", 0, hdr, 57, 6));

        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Position Block 3","");
    } else if (type == 0x55) {
        xprintf(0, "See CRID data at the end\r\n");
        //      xprintf(0,pritem("CRID                           ",15,hdr,
        //      1,62));
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Coordinate Ref System Identification","");
    } else if (type == 0x56) {
        xprintf(0, pritem("Offset Easting                 ", 3, hdr, 1, 8));
        xprintf(0, pritem("Offset Northing                ", 3, hdr, 9, 8));
        xprintf(0, pritem("Offset Vertical                ", 3, hdr, 17, 8));
        xprintf(0, pritem("Description                    ", 3, hdr, 25, 38));

        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : Relative Position Block","");
    } else {
        xprintf(0, pritem("Header block type              ", 1, hdr, 63, 2));
        //      xprintf(0,"%33s : User defined header block","");
    }

    return _inf;
}

TGenHdrN::TGenHdrN(TSegD *segd, int num)
    : TSegDObj(segd)
{
    int NH = 0;
    char s[1024];

    byte *dat = _segd->read(32);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    name = "General #?";

    int RevMaj = _segd->RevMaj;

    if (RevMaj < 3) {
        NH = uint_item(dat, 37, 2);

        ghn = NH;
        if (NH < 1)
            NH = num;
    } else {
        NH = num;
        ghn = num;
        bid = uint_item(dat, 63, 2);
    }

    sprintf(s, "General #%d", NH);
    name = s;

    set_len(32);
    memcpy(hdr, dat, 32);
    type = 0;
}
//---------------------------------------------------------------------------

TCs *TCsList::get_cs(int csn)
{
    TCs *cs;
    int n;

    for (n = 0; n < getCount(); n++) {
        cs = (TCs *)getObj(n);
        if (cs->csn == csn)
            return cs;
    }

    return NULL;
}

TCs *TCsList::get_csi(int idx)
{
    return (TCs *)getObj(idx);
}

TCs *TCsList::find(int stn, int csn)
{
    TCs *cs;
    int n;

    for (n = 0; n < getCount(); n++) {
        cs = (TCs *)getObj(n);
        if (cs->csn == csn && cs->stn == stn)
            return cs;
    }

    return NULL;
}

string TCsB::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "%s\r\n\r\n", name.c_str());

    int ntr = get_ntr();

    xprintf(0, "%d traces", ntr);

    return _inf;
}

TCsB::TCsB(TSegD *segd, int no, int ntr)
    : TCs(segd)
{
    if (no == 0) {
        typnm = baux;
    } else {
        typnm = bseis;
    }

    csn = no;
    stn = 0;

    nch = ntr;
    ito = 0;
    itn = _segd->TrcSmpNum * _segd->Si / 2;

    mto = 0;
    mtn = itn * 2;

    typn = no;
    type = 2;
}

string TCsD::get_inf()
{
    int v;

    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "%s\r\n\r\n", name.c_str());

    displayMemory(0, hdr, len);

    int RevMaj = _segd->RevMaj;

    if (RevMaj != 3) {
        xprintf(0, pritem("Scan Type #                    ", 0, hdr, 1, 2));
        xprintf(0, pritem("Channel Set Number             ", 0, hdr, 3, 2));

        xprintf(0, pritem("Channel Set Starting Time (2ms)", 1, hdr, 5, 4, 0));
        xprintf(0, "(%d ms)\r\n", ito * 2);

        xprintf(0, pritem("Channel Set End Time (2ms)     ", 1, hdr, 9, 4, 0));
        xprintf(0, "(%d ms)\r\n", itn * 2);

        xprintf(0, pritem("Descale Multiplier             ", 0, hdr, 13, 4, 0));
        xprintf(0, "(%g)\r\n", mp);

        xprintf(0, pritem("Num of channnels in this set   ", 0, hdr, 17, 4, 0));
        if (_segd->XF)
            xprintf(0, "(fixed to: %d)\r\n", _segd->XF);
        else
            xprintf(0, "\r\n");

        xprintf(0, pritem("Channel Type Identification    ", 1, hdr, 21, 1, 0));
        xprintf(0, "(%s)\r\n", typnm);

        xprintf(0, pritem("Undefined                      ", 0, hdr, 22, 1));

        xprintf(0, pritem("Number of samples exponent     ", 0, hdr, 23, 1, 0));
        xprintf(0, "(%d subscan(s))\r\n", nsscan);

        xprintf(0, pritem("Channel gain control method    ", 0, hdr, 24, 1, 0));
        v = uint_item(hdr, 24, 1);
        xprintf(0, "(%s)\r\n", ChanGCName[v]);

        xprintf(0, pritem("Alias filter freq at - 3dB (Hz)", 0, hdr, 25, 1));
        xprintf(0, pritem("Alias filter slope (dB/oct)    ", 0, hdr, 29, 4));
        xprintf(0, pritem("Low-cut filter frequency (Hz)  ", 0, hdr, 33, 4));
        xprintf(0, pritem("Low-cut filter slope (dB/oct)  ", 0, hdr, 37, 4));
        xprintf(0, pritem("First Notch Frequency          ", 0, hdr, 41, 4));
        xprintf(0, pritem("Second Notch Frequency         ", 0, hdr, 45, 4));
        xprintf(0, pritem("Third Notch Frequency          ", 0, hdr, 49, 4));
        xprintf(0, pritem("Extended channel set number    ", 1, hdr, 53, 4));
        xprintf(0, pritem("Extended header flag           ", 0, hdr, 57, 1));
        xprintf(0, pritem("Trace Header Extensions        ", 1, hdr, 58, 1));
        xprintf(0, pritem("Vertical Stack                 ", 1, hdr, 59, 2));
        xprintf(0, pritem("Streamer cable number          ", 0, hdr, 62, 2));

        xprintf(0, pritem("Array forming                  ", 0, hdr, 63, 1, 0));
        v = uint_item(hdr, 63, 1);
        xprintf(0, "(%s)\r\n", ArrayFormName[v]);
    } else {
        xprintf(0, pritem("Scan Type #                    ", 0, hdr, 1, 2));
        xprintf(0, pritem("Channel Set Number             ", 1, hdr, 3, 4));
        xprintf(0, pritem("Channel Type Identification    ", 1, hdr, 7, 2));
        xprintf(0, pritem("Channel Set Starting Time (uS) ", 2, hdr, 9, 8));
        xprintf(0, pritem("Channel Set End Time      (uS) ", 2, hdr, 17, 8));
        xprintf(0, pritem("Number of samples              ", 1, hdr, 25, 8));
        xprintf(0, pritem("Descale Multiplier             ", 3, hdr, 33, 8));
        xprintf(0, pritem("Num of channnels in this set   ", 1, hdr, 41, 6));
        xprintf(0, pritem("Sampling   Interval       (uS) ", 1, hdr, 47, 6));

        xprintf(0, pritem("Array forming                  ", 1, hdr, 53, 2, 0));
        v = uint_item(hdr, 53, 2);
        xprintf(0, "(%s)\r\n", ArrayFormName[v]);

        xprintf(0, pritem("Trace Header Extensions        ", 1, hdr, 55, 2));
        xprintf(0, pritem("Extended header flag           ", 1, hdr, 57, 1));

        xprintf(0, pritem("Channel gain control method    ", 1, hdr, 58, 1, 0));
        v = uint_item(hdr, 58, 1);
        xprintf(0, "(%s)\r\n", ChanGCName[v]);

        xprintf(0, pritem("Vertical Stack                 ", 1, hdr, 59, 2));
        xprintf(0, pritem("Streamer cable number          ", 1, hdr, 61, 2));
        xprintf(0, pritem("Header Block Type 1            ", 1, hdr, 63, 2));
        xprintf(0, pritem("Alias filter frequency    (Hz) ", 3, hdr, 65, 8));
        xprintf(0, pritem("Low-cut filter setting    (Hz) ", 3, hdr, 73, 8));
        xprintf(0, pritem("Alias filter slope    (dB/oct) ", 3, hdr, 81, 8));
        xprintf(0, pritem("Low-cut filter slope  (dB/oct) ", 3, hdr, 89, 8));
        xprintf(0, pritem("Notch filter frequency    (Hz) ", 3, hdr, 97, 8));
        xprintf(0, pritem("Second Notch frequency    (Hz) ", 3, hdr, 105, 8));
        xprintf(0, pritem("Third  Notch frequency    (Hz) ", 3, hdr, 113, 8));

        xprintf(0,
                pritem("Filter phase                   ", 1, hdr, 121, 2, 0));
        v = uint_item(hdr, 121, 2);
        xprintf(0, "(%s)\r\n", FiltPhaseName[v]);

        xprintf(0,
                pritem("Physical unit                  ", 1, hdr, 123, 2, 0));
        v = uint_item(hdr, 123, 2);
        xprintf(0, "(%s)\r\n", PhysUnitName[v]);

        xprintf(0, pritem("Undefined                      ", 0, hdr, 125, 2));
        xprintf(0, pritem("Header block type  2           ", 1, hdr, 127, 2));
        xprintf(0, pritem("Filter delay              (uS) ", 3, hdr, 129, 8));
        xprintf(0, pritem("Description                    ", 5, hdr, 137, 54));
        xprintf(0, pritem("Header block type  3           ", 1, hdr, 191, 2));
    }

    return _inf;
}

TCsD::TCsD(TSegD *segd)
    : TCs(segd)
{
    int RevMaj = _segd->RevMaj;

    mp = 0;

    if (RevMaj != 3) {
        byte *dat = _segd->read(32);

        if (!dat) {
            xprintf(1, "%s is too short\r\n", name.c_str());
            return;
        }

        char s[2048];

        hexs(dat, 3, 2, s);
        if (strcmp(s, "FF") == 0)
            csn = -1;
        else
            csn = atoi(s);

        hexs(dat, 1, 2, s);
        stn = atoi(s);
        hexs(dat, 17, 4, s);
        nch = atoi(s);

        ito = hex2uint(hexs(dat, 5, 4, s));
        itn = hex2uint(hexs(dat, 9, 4, s));

        mto = ito * 2;
        mtn = itn * 2;

        // if(to==tn) nch=0;

        hexs(dat, 23, 1, s);
        csse = atoi(s);

        if (csse < 1)
            nsscan = 1;
        else
            nsscan = 1 << csse;

        nsso = nsscan;

        if (csn <= 0) {
            hexs(dat, 53, 4, s);
            csn = hex2uint(s); // Fixfrom Igor Popov
        }

        sprintf(s, "Chan Set # %d:%d", stn, csn);
        name = s;

        hexs(dat, 21, 1, s);
        typn = atoi(s);
        // strcpy(typnm,typnum2nam(typn));
        typnm = ChanTypeName2[typn];
        tml = (itn - ito) * 2; // *(1<<csse);

        segd->R_ms = tml;

        unsigned char imp0 = dat[6];
        unsigned char imp1 = dat[7];

        int z = imp1 & 0x80;
        if (z)
            z = -1;
        else
            z = 1;

        int m = (imp1 & 0x7F) << 8 | imp0;

        mp = z * m / 1024.;

        set_len(32);
        memcpy(hdr, dat, 32);
        type = 2;
    } else {
        byte *dat = _segd->read(96);

        if (!dat) {
            xprintf(1, "%s is too short\r\n", name.c_str());
            return;
        }

        char s[2048];

        hexs(dat, 1, 2, s);
        stn = atoi(s);

        csn = hex2uint(hexs(dat, 3, 4, s));
        nch = hex2uint(hexs(dat, 41, 6, s));
        typn = hex2uint(hexs(dat, 7, 2, s));
        ns = hex2uint(hexs(dat, 25, 8, s));

        int the = hex2uint(hexs(dat, 55, 2, s));

        ln = ns * _segd->SmplB + 20 + 32 * the;

        //      strcpy(typnm,typnum2nam(typn>>4));
        //      typnm=TypNnam.namo(typn>>4);
        typnm = ChanTypeName3[typn];

        sprintf(s, "Chan Set # %d:%d", stn, csn);
        name = s;

        ito = hex2sint(hexs(dat, 9, 8, s));
        itn = hex2sint(hexs(dat, 17, 8, s));

        mto = ito / 1000.;
        mtn = itn / 1000.;

        si = hex2uint(hexs(dat, 47, 6, s)) / 1000.;

        if (ito == 0 && itn == 0) {
            mto = 0;
            mtn = si * ns;
        }

        tml = (mtn - mto) * si; // *(1<<csse);

        mp = hex2dbl(hexs(dat, 33, 8, s));

        set_len(96);
        memcpy(hdr, dat, 96);
        type = 2;
    }
}

TCs::TCs(TSegD *segd)
    : TSegDObj(segd)
{
    _trc = new TTrList();
    name = "Chan Set #?";
    nsscan = 1;
    nsso = 1;
    ns = -1;
    ln = 0;
    lj = 0;

    _segd->csl->Add(this);
}

TCs::~TCs()
{
    delete _trc;
}

//---------------------------------------------------------------------------

string TTr::get_inf()
{
    int v;

    if (_inf != "")
        return _inf;

    int n;

    int RevMaj = _segd->RevMaj;

    byte *hed = hdr;

    xprintf(0, "%s - (%d) - len = %d bytes\r\n", name.c_str(), atn, fln);

    if (junk)
        xprintf(0, "%d junk bytes before trace\r\n", junk);
    else
        xprintf(0, "\r\n");

    if (hdr == NULL) {
        xprintf(0, "No data are available");
        return _inf;
    }

    if (_err != "")
        xprintf(0, "Error: %s\r\n", _err.c_str());

    xprintf(0, "Trace Header\r\n\r\n");
    displayMemory(0, hed, 20);

    xprintf(0, pritem("File Number                    ", 0, hed, 1, 4));
    xprintf(0, pritem("Scan Type Number               ", 0, hed, 5, 2));
    xprintf(0, pritem("Channel Set Number             ", 0, hed, 7, 2));
    xprintf(0, pritem("Trace Number                   ", 0, hed, 9, 4));
    xprintf(0, pritem("First Timing Word              ", 0, hed, 13, 6));
    xprintf(0, pritem("Trace Header Extensions        ", 1, hed, 19, 2));
    xprintf(0, pritem("Sample Skew                    ", 1, hed, 21, 2));
    xprintf(0, pritem("Trace edit                     ", 1, hed, 23, 2));
    xprintf(0, pritem("Time break window              ", 1, hed, 25, 6));
    xprintf(0, pritem("Extended Channel Set Number    ", 1, hed, 31, 4));
    xprintf(0, pritem("Extended File Number           ", 1, hed, 35, 6));
    xprintf(0,
            "\r\n-------------------------------------------------------------"
            "------------------\r\n");

    hed += 20;

    if (RevMaj != 3) {
        for (n = 0; n < the; n++) {
            xprintf(0, "\r\nExtended Trace Header # %d\r\n\r\n", n + 1);
            if (n == 0) {
                displayMemory(0, hed, 32);
                xprintf(0,
                        pritem("Receiver Line Number         ", 2, hed, 1, 6));
                xprintf(0,
                        pritem("Receiver Point Number        ", 2, hed, 7, 6));
                xprintf(0,
                        pritem("Receiver Point Index         ", 2, hed, 13, 2));
                xprintf(0,
                        pritem("Ext # of Samples per Trace   ", 2, hed, 15, 6));
                xprintf(0, pritem("Ext Receiver Line Number     ", 12, hed, 21,
                                  10, -16));
                xprintf(0, pritem("Ext Receiver Point Number    ", 12, hed, 31,
                                  10, -16));

                xprintf(0, pritem("Sensor Type                  ", 2, hed, 41,
                                  2, 0));
                v = uint_item(hed, 41, 2);
                xprintf(0, "(%s)\r\n", SensorTypName[v]);
            } else {
                displayMemory(0, hed, 32);
            }
            hed += 32;
        }
    } else {
        for (n = 0; n < the; n++) {
            char s[1024];

            int type = hex2uint(hexs(hed, 63, 2, s));
            xprintf(0, "\r\nExtended Trace Header # %d (%s)\r\n\r\n", n + 1,
                    HeadBlkName[type]);

            displayMemory(0, hed, 32);

            if (type == 0x40) {
                xprintf(0,
                        pritem("Receiver Line Number         ", 2, hed, 1, 6));
                xprintf(0,
                        pritem("Receiver Point Number        ", 2, hed, 7, 6));
                xprintf(0,
                        pritem("Receiver Point Index         ", 2, hed, 13, 2));
                xprintf(0,
                        pritem("Re-shoot Index               ", 1, hed, 15, 2));
                xprintf(0,
                        pritem("Group Index                  ", 1, hed, 17, 2));
                xprintf(0,
                        pritem("Depth Index                  ", 1, hed, 19, 2));
                xprintf(0, pritem("Ext Receiver Line Number     ", 12, hed, 21,
                                  10, -16));
                xprintf(0, pritem("Ext Receiver Point Number    ", 12, hed, 31,
                                  10, -16));

                xprintf(0, pritem("Sensor Type                  ", 1, hed, 41,
                                  2, 0));
                v = uint_item(hed, 41, 2);
                xprintf(0, "(%s)\r\n", SensorTypName[v]);

                xprintf(0,
                        pritem("Ext Trace Number             ", 1, hed, 43, 6));
                xprintf(0,
                        pritem("# of Samples per Trace       ", 1, hed, 49, 8));
                xprintf(0,
                        pritem("Sensor Moving                ", 1, hed, 57, 2));
                xprintf(0,
                        pritem("Undefined                    ", 0, hed, 59, 2));
                xprintf(0,
                        pritem("Physical unit                ", 1, hed, 61, 2));
                xprintf(0,
                        pritem("Header block type            ", 1, hed, 63, 2));
                //              xprintf(0,"%33s : Trace  header extension
                //              1","");
            } else if (type == 0x41) {
                xprintf(0,
                        pritem("EquipmentTest Time           ", 6, hed, 1, 16));
                xprintf(0,
                        pritem("Sensor Sensitivity           ", 3, hed, 17, 8));

                xprintf(0, pritem("EquipmentTest Result         ", 1, hed, 25,
                                  2, 0));
                v = uint_item(hed, 25, 2);
                xprintf(0, "(%s)\r\n", EquipTestName[v]);

                xprintf(
                    0, pritem("Serial  Number               ", 5, hed, 27, 36));

                xprintf(0,
                        pritem("Header block type            ", 1, hed, 63, 2));
                //              xprintf(0,"%33s : Sensor Info Header
                //              Extension","");
            } else if (type == 0x42) {
                xprintf(0,
                        pritem("Time Zero                  ", 6, hed, 1, 16));
                xprintf(0,
                        pritem("Undefined                  ", 0, hed, 17, 46));
                xprintf(0,
                        pritem("Header block type          ", 1, hed, 63, 2));
                //              xprintf(0,"%33s : Timestamp Header","");
            } else if (type == 0x43) {
                xprintf(0,
                        pritem("Frequency 1                  ", 3, hed, 1, 8));
                xprintf(0,
                        pritem("Amplitude 1                  ", 3, hed, 9, 8));
                xprintf(0,
                        pritem("Phase 1                      ", 3, hed, 17, 8));
                xprintf(0,
                        pritem("Frequency 2                  ", 3, hed, 25, 8));
                xprintf(0,
                        pritem("Amplitude 2                  ", 3, hed, 33, 8));
                xprintf(0,
                        pritem("Phase 2                      ", 3, hed, 41, 8));
                xprintf(0,
                        pritem("Calibration                  ", 1, hed, 49, 2));
                xprintf(
                    0, pritem("Undefined                    ", 0, hed, 51, 12));
                xprintf(0,
                        pritem("Header block type            ", 1, hed, 63, 2));
                //              xprintf(0,"%33s : Sensor Calibration
                //              Header","");
            } else if (type == 0x44) {
                xprintf(0,
                        pritem("Time of deployment           ", 6, hed, 1, 16));
                xprintf(
                    0, pritem("Time of retrieval            ", 6, hed, 17, 16));
                xprintf(0,
                        pritem("Time offset at deployment    ", 2, hed, 33, 8));
                xprintf(0,
                        pritem("Time offset at retrieval     ", 2, hed, 41, 8));
                xprintf(0,
                        pritem("Time drift corrected         ", 1, hed, 49, 2));

                xprintf(0, pritem("Time drift correction method ", 1, hed, 51,
                                  2, 0));
                v = uint_item(hed, 51, 2);
                xprintf(0, "(%s)\r\n", TimeDriftName[v]);

                xprintf(
                    0, pritem("Undefined                    ", 0, hed, 53, 10));
                xprintf(0,
                        pritem("Header block type            ", 1, hed, 63, 2));
                //              xprintf(0,"%33s : Time  Drift Header","");
            } else if (type == 0x45) {
                xprintf(0,
                        pritem("Equipment Dimension X        ", 1, hed, 1, 6));
                xprintf(0,
                        pritem("Equipment Dimension Y        ", 1, hed, 7, 6));
                xprintf(0,
                        pritem("Equipment Dimension Z        ", 1, hed, 13, 6));
                xprintf(0,
                        pritem("Positive terminal            ", 1, hed, 19, 2));
                xprintf(0,
                        pritem("Equipment Offset X           ", 2, hed, 21, 6));
                xprintf(0,
                        pritem("Equipment Offset Y           ", 2, hed, 27, 6));
                xprintf(0,
                        pritem("Equipment Offset Z           ", 2, hed, 33, 6));
                xprintf(
                    0, pritem("Undefined                    ", 0, hed, 39, 24));
                xprintf(0, pritem("Header block type            ", 1, hed, 63,
                                  2, 0));
                //              xprintf(0,"%33s : Time  Drift
                //              Header","Electromagnetic Src/Recv Desc","");
            } else if (type == 0x50) {
                xprintf(0, pritem("Time of Position               ", 6, hed, 1,
                                  16));
                xprintf(0, pritem("Time of Measurement/Calculation", 6, hed, 17,
                                  16));
                xprintf(0, pritem("Vertical error quality         ", 3, hed, 33,
                                  8));
                xprintf(0, pritem("Horizontal error quality A     ", 3, hed, 41,
                                  8));
                xprintf(0, pritem("Horizontal error quality B     ", 3, hed, 49,
                                  8));
                xprintf(0, pritem("Horizontal error quality O     ", 1, hed, 57,
                                  4));
                xprintf(0, pritem("Position type                  ", 1, hed, 61,
                                  2));

                xprintf(0, pritem("Header block type              ", 1, hed, 63,
                                  2));
                //              xprintf(0,"%33s : Position Block 1","");
            } else if (type == 0x51) {
                xprintf(0, pritem("Tuple 1 coordinate 1           ", 4, hed, 1,
                                  16));
                xprintf(0, pritem("Tuple 1 coordinate 2           ", 4, hed, 17,
                                  16));
                xprintf(0, pritem("Tuple 1 coordinate 3           ", 4, hed, 33,
                                  16));
                xprintf(0, pritem("Location Data Stanza ID 1      ", 1, hed, 49,
                                  4));
                xprintf(0, pritem("Position 1 Valid               ", 1, hed, 53,
                                  2));
                xprintf(0, pritem("Position 1 Quality             ", 1, hed, 55,
                                  2));
                xprintf(0, pritem("Undefined                      ", 0, hed, 57,
                                  6));

                xprintf(0, pritem("Header block type              ", 1, hed, 63,
                                  2));
                //              xprintf(0,"%33s : Position Block 2","");
            } else if (type == 0x52) {
                xprintf(0, pritem("Tuple 2 coordinate 1           ", 4, hed, 1,
                                  16));
                xprintf(0, pritem("Tuple 2 coordinate 2           ", 4, hed, 17,
                                  16));
                xprintf(0, pritem("Tuple 2 coordinate 3           ", 4, hed, 33,
                                  16));
                xprintf(0, pritem("Location Data Stanza ID 2      ", 1, hed, 49,
                                  4));
                xprintf(0, pritem("Position 2 Valid               ", 1, hed, 53,
                                  2));
                xprintf(0, pritem("Position 2 Quality             ", 1, hed, 55,
                                  2));
                xprintf(0, pritem("Undefined                      ", 0, hed, 57,
                                  6));

                xprintf(0, pritem("Header block type              ", 1, hed, 63,
                                  2));
                //              xprintf(0,"%33s : Position Block 3","");
            } else if (type == 0x60) {
                displayMemory(0, hed, 32);

                xprintf(0,
                        pritem("Rotation angle around X     ", 3, hed, 1, 8));
                xprintf(0,
                        pritem("Rotation angle around Y     ", 3, hed, 9, 8));
                xprintf(0,
                        pritem("Rotation angle around Z     ", 3, hed, 17, 8));
                xprintf(0,
                        pritem("Reference Orientation       ", 3, hed, 25, 8));
                xprintf(0, pritem("Time stamp                  ", 6, hed, 33, 1,
                                  16));

                xprintf(0, pritem("Orientation Type            ", 1, hed, 49, 2,
                                  0));
                v = uint_item(hed, 49, 2);
                xprintf(0, "(%s)\r\n", OrientTypName[v]);

                xprintf(0,
                        pritem("Reference Orientation Valid ", 1, hed, 51, 2));
                xprintf(0,
                        pritem("Rotation Applied            ", 1, hed, 53, 2));
                xprintf(0,
                        pritem("Rotation to North Applied   ", 1, hed, 55, 2));
                xprintf(0,
                        pritem("Undefined                   ", 0, hed, 57, 6));
                xprintf(0,
                        pritem("Header block type           ", 1, hed, 63, 2));
                //              xprintf(0,"%33s : Orientation Header","");
            } else if (type == 0x61) {
                xprintf(0,
                        pritem("Time stamp                   ", 6, hed, 1, 16));
                xprintf(0,
                        pritem("Measurement value            ", 3, hed, 17, 8));
                xprintf(0,
                        pritem("Maximum Value                ", 3, hed, 25, 8));
                xprintf(0,
                        pritem("Minimum Value                ", 3, hed, 33, 8));
                xprintf(0,
                        pritem("Quantity Class               ", 1, hed, 41, 4));
                xprintf(0,
                        pritem("Unit of Measure              ", 1, hed, 45, 4));
                xprintf(0,
                        pritem("Measurement Description code ", 1, hed, 49, 4));
                xprintf(
                    0, pritem("Undefined                    ", 0, hed, 53, 10));
                xprintf(0,
                        pritem("Header block type            ", 1, hed, 63, 2));
                //              xprintf(0,"%33s : Measurement Block","");
            } else {
                xprintf(0,
                        pritem("Header block type            ", 1, hed, 63, 2));
            }
            xprintf(
                0,
                "\r\n---------------------------------------------------------"
                "----------------------\r\n");
            hed += 32;
        }
    }

    xprintf(0, "\r\n...\r\n");

    return _inf;
}

TTr::TTr(TSegD *segd, TTrInf ti)
    : TSegDObj(segd)
{
    char s[2048];
    int n;

    name = "Trace ??:??";
    smps = NULL;

    exns = 0;
    junk = 0;

    int RevMaj = _segd->RevMaj;

    segd->seek(ti.pos, SEEK_SET);

    byte *dat = _segd->read(20);
    byte *thdr = dat;

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    len = 0;
    len = len + 20;

    the = hex2uint(hexs(dat, 19, 2, s));
    skk = hex2uint(hexs(dat, 21, 2, s));

    hexs(dat, 5, 2, s);
    stn = atoi(s);

    hexs(dat, 7, 2, s);
    if (s[0] == 0 || strcmp(s, "FF"))
        csn = atoi(s);
    else
        csn = hex2uint(hexs(dat, 31, 4, s));

    int csidx = segd->find_csi(csn, stn);

    TCs *Cs = segd->get_csi(csidx);
    csi = csidx;

    ns = 0;

    hexs(dat, 9, 4, s);

    if (strcmp(s, "FFFF") == 0)
        tn = -1;
    else
        tn = atoi(s);

    //  tn = hex2uint(hexs(dat, 9, 4,s));

    //  Extended Trace Headers

    for (n = 0; n < the; n++) {
        dat = segd->read(32);
        if (!dat) {
            xprintf(1, "%s is too short\r\n", name.c_str());
            return;
        }

        len += 32;

        if (n == 0) {
            if (RevMaj != 3) {
                exns = uint_item(dat, 15, 6);
            } else {
                exns = uint_item(dat, 49, 8);
                if (tn < 0)
                    tn = hex2uint(hexs(dat, 43, 6, s));
            }
        }
    }

    sprintf(s, "Trace %02d:%02d:%02d", stn, csn, tn);
    name = s;

    _segd->THE = the;
    _segd->atn++;
    atn = segd->atn;

    byte *smpdat = NULL;

    set_len(len);
    memcpy(hdr, thdr, len);
    type = 3;

    if (Cs) {
        Cs->get_trc()->Add(this);
        ns = Cs->ns;
        fln = Cs->ln;

        int sampleslen = (int)((ns * _segd->Smpl) / 8.);
        if (sampleslen > 0) {
            smpdat = segd->read(sampleslen);
            // len = len + sampleslen;

            if (!smpdat) {
                xprintf(1, "%s is too short\r\n", name.c_str());
                ns = 0; //!!!
            }
        }
    } else {
        xprintf(1, "Unknown chan set: %d\r\n", csn);
    }

    if (ns > 0) {
        smps = new float[ns + 4];
        memset(smps, 0, ns * sizeof(float));
        cnv_smps(smpdat, smps);
    }
}

TTr::TTr(TSegD *segd, int nsmp, int chn, int ci)
    : TSegDObj()
{
    _segd = segd;

    csi = ci;

    TCs *cs = _segd->get_csi(csi);
    exns = 0;
    junk = 0;

    tn = chn;
    csn = cs->csn;
    stn = cs->stn;

    name = "Demux Trace";

    if (nsmp < 1)
        smps = NULL;
    else
        smps = new float[nsmp];

    ns = nsmp;
    type = 3;
}

TTr::~TTr()
{
    if (smps)
        delete[] smps;
}

void TTr::get_smps(float *osmp)
{
    if (!smps)
        return;

    float *fsmps = (float *)smps;

    memcpy(osmp, fsmps, ns * sizeof(float));
}

int TTr::cnv_smps(byte *ismp, float *osmp)
{
    TCs *cs = _segd->get_csi(csi);

    float fmp = pow(2, cs->mp);

    if (_segd->Format == 8058) {
        F8058_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 9058) {
        F9058_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 8080) {
        F8080_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 9080) {
        F9080_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 8048) {
        F8048_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 8036) {
        F8036_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 9036) {
        F9036_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 8038) {
        F8038_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 9038) {
        F9038_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    }

    else if (_segd->Format == 8015) {
        F8015_to_float(ismp, osmp, ns);
        mult_fa(fmp, osmp, ns);
    } else {
        return 0;
    }

    return ns;
}

//---------------------------------------------------------------------------

string TEcHdr::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "Extended Header Block len = %d bytes\r\n\r\n", len);

    displayMemory(0, hdr, len);

    char s[2048];

    if (_segd->Mc == 20 && _segd->RevMaj == 0x01 && _segd->RevMin == 0x06) {
        xprintf(0, "FairField Rev 0x0106 Extended Header\r\n\r\n");
        xprintf(0, "Number of record in file     %d\r\n", _segd->XF);
    }

    if (_segd->dev_type == 2 || _segd->Mc == 34) {
        xprintf(0, "Syntrak Extended Header\r\n\r\n");

        hexs(hdr, 3, 1, s);
        xprintf(0, "Tape Unit                    %s\r\n", s);

        hexs(hdr, 4, 1, s);
        xprintf(0, "Buffer                       %s\r\n", s);

        xprintf(0, "\r\n");

        xprintf(0, "Line Name                    %s\r\n",
                _segd->LineNm.c_str());
        xprintf(0, "Reel Number                  %d\r\n", _segd->ReelNo);
        xprintf(0, "Client                       %s\r\n",
                _segd->Client.c_str());
        xprintf(0, "Contractor                   %s\r\n",
                _segd->Contra.c_str());
        xprintf(0, "Extra 32 byte blocks         %d\r\n", _segd->XX);
    }

    if (_segd->Mc == 13 && len >= 1024 && is_month(hdr + 563)) // Seal
    {
        xprintf(0, "Seal Extended Header\r\n\r\n");
        xprintf(0, pritem("Acquisition length             ", 1, hdr, 1, 8));
        xprintf(0, pritem("Sample rate                    ", 1, hdr, 9, 8));
        xprintf(0, pritem("Total number of traces         ", 1, hdr, 17, 8));
        xprintf(0, pritem("Number of Auxes                ", 1, hdr, 25, 8));
        xprintf(0, pritem("Number of Seis traces          ", 1, hdr, 33, 8));
        xprintf(0, pritem("Number of dead Seis traces     ", 1, hdr, 41, 8));
        xprintf(0, pritem("Number of live Seis traces     ", 1, hdr, 49, 8));
        xprintf(0, pritem("Type of source                 ", 1, hdr, 57, 8));
        xprintf(0, pritem("Number of samples in trace     ", 1, hdr, 65, 8));
        xprintf(0, pritem("Shot number                    ", 1, hdr, 75, 8));
        xprintf(0, pritem("TB window                      ", 3, hdr, 80, 8));
        xprintf(0, pritem("Test record type               ", 1, hdr, 89, 8));
        xprintf(0, pritem("Swath first line               ", 1, hdr, 97, 8));
        xprintf(0, pritem("Swath first number             ", 1, hdr, 105, 8));
        xprintf(0, pritem("Spread number                  ", 1, hdr, 113, 8));
        xprintf(0, pritem("Spread type                    ", 1, hdr, 121, 8));
        xprintf(0, pritem("Timebreak                      ", 1, hdr, 129, 8));
        xprintf(0, pritem("Uphole time                    ", 1, hdr, 137, 8));
        xprintf(0, pritem("Blaster id                     ", 1, hdr, 145, 8));
        xprintf(0, pritem("Blaster status                 ", 1, hdr, 153, 8));
        xprintf(0, pritem("Refraction delay               ", 1, hdr, 163, 8));
        xprintf(0, pritem("Tb to T0 time                  ", 3, hdr, 169, 8));
        xprintf(0, pritem("Internal time break            ", 1, hdr, 177, 8));
        xprintf(0, pritem("Prestack within field units    ", 1, hdr, 185, 8));
        xprintf(0, pritem("Noise elimination type         ", 1, hdr, 193, 8));
        xprintf(0, pritem("Low trace percentage           ", 1, hdr, 201, 8));
        xprintf(0, pritem("Low trace value                ", 1, hdr, 209, 8));
        xprintf(0, pritem("Number of windows              ", 1, hdr, 217, 8));
        xprintf(0, pritem("Historic editing type          ", 1, hdr, 225, 8));
        xprintf(0, pritem("Noisy trace percentage         ", 1, hdr, 233, 8));
        xprintf(0, pritem("Historic range                 ", 1, hdr, 241, 8));
        xprintf(0, pritem("Threshold Hold/Var             ", 1, hdr, 257, 8));
        xprintf(0, pritem("Historic threshold Init value  ", 1, hdr, 265, 8));
        xprintf(0, pritem("Historic zeroing length        ", 1, hdr, 273, 8));
        xprintf(0, pritem("Type of process                ", 1, hdr, 281, 8));
        xprintf(0, pritem("Acquisition type table #1      ", 1, hdr, 289, 8));
        xprintf(0, pritem("Threshold type table   #1      ", 1, hdr, 545, 8));
        xprintf(0, pritem("Stacking fold                  ", 1, hdr, 801, 8));
        xprintf(0, pritem("Aux correlation description    ", 5, hdr, 809, 160));
        xprintf(0, pritem("Investigation length           ", 1, hdr, 869, 8));
        xprintf(0, pritem("Autocorrelation peak time      ", 1, hdr, 877, 8));
        xprintf(0, pritem("FFT exponent                   ", 1, hdr, 885, 8));
        xprintf(0, pritem("Source aux nb                  ", 1, hdr, 893, 8));
        xprintf(0, pritem("Pilot length                   ", 1, hdr, 1001, 8));
        xprintf(0, pritem("Sweep length                   ", 1, hdr, 1009, 8));
        xprintf(0, pritem("Acquisition number             ", 1, hdr, 1017, 8));
        if (len > 1073) {
            xprintf(0,
                    pritem("Max of max, Aux                ", 3, hdr, 1025, 8));
            xprintf(0,
                    pritem("Max of max, Seis               ", 3, hdr, 1033, 8));
            xprintf(0,
                    pritem("Dump stacking fold             ", 1, hdr, 1041, 8));
            xprintf(
                0, pritem("Tape label                     ", 5, hdr, 1049, 32));
            xprintf(0,
                    pritem("Tape number                    ", 1, hdr, 1081, 8));
            xprintf(
                0, pritem("Software version               ", 5, hdr, 1089, 32));
            xprintf(
                0, pritem("Date                           ", 5, hdr, 1121, 24));
            xprintf(
                0, pritem("Source easting                 ", 4, hdr, 1145, 16));
            xprintf(
                0, pritem("Source northing                ", 1, hdr, 1161, 16));
            xprintf(0,
                    pritem("Source elevation               ", 3, hdr, 1177, 8));
            xprintf(0,
                    pritem("Slip sweep mode used           ", 1, hdr, 1185, 8));
            xprintf(0,
                    pritem("Files per tape                 ", 1, hdr, 1193, 8));
            xprintf(0,
                    pritem("File count                     ", 1, hdr, 1201, 8));
            xprintf(0, pritem("Acquisition error description  ", 5, hdr, 1209,
                              160));
            xprintf(0,
                    pritem("Filter type                    ", 1, hdr, 1529, 8));
            xprintf(0,
                    pritem("Profile number                 ", 1, hdr, 1537, 8));
            xprintf(0,
                    pritem("Not used                       ", 1, hdr, 1545, 2));
            xprintf(0,
                    pritem("T0 mode                        ", 1, hdr, 1561, 8));
            xprintf(
                0, pritem("Source Line Name               ", 5, hdr, 1569, 32));
            xprintf(0,
                    pritem("Sequence Number                ", 1, hdr, 1601, 8));
            xprintf(0,
                    pritem("Nav. status                    ", 1, hdr, 1609, 8));
            xprintf(0,
                    pritem("Shots to go                    ", 1, hdr, 1617, 8));
            xprintf(0,
                    pritem("Shots before on line           ", 1, hdr, 1625, 8));
            xprintf(0,
                    pritem("Total num. of editions         ", 1, hdr, 1633, 8));
            xprintf(0,
                    pritem("Aux digital low-cut filter     ", 3, hdr, 1641, 8));
            xprintf(0,
                    pritem("Seis digital low-cut filter    ", 3, hdr, 1649, 8));
            xprintf(0,
                    pritem("-3dB seis low-cut filter       ", 3, hdr, 1657, 8));
            xprintf(0,
                    pritem("Num. of chars from nav         ", 1, hdr, 1665, 8));
            xprintf(0, 0, "Other Not used\r\n");
        }
    }

    char *ss;
    ss = strmem((char *)"$1", (char *)hdr, len);

    if (ss) {
        int p = ss - (char *)hdr;
        if (p + 36 < len && memcmp(ss + 33, "UTC", 3) == 0)
            DumpNavData(ss);
    }

    ss = strmem((char *)"*GCS90", (char *)hdr, len);
    if (ss)
        DumpGunData(ss);

    xprintf(0, "--------------------------------\r\n\r\n");

    return _inf;
}

TEcHdr::TEcHdr(TSegD *segd)
    : TSegDObj(segd)
{
    int n;
    char s[2048];
    byte *hed;
    int EC = _segd->EC;

    int l = _segd->EC * 32;

    name = "Extended Hdr";

    byte *dat = segd->read(32 * EC);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    if (segd->dev_type == 2 || segd->Mc == 34) {
        hed = dat;
        for (n = 0; n < EC; n++) {
            if (n == 0) {
                if (segd->Mc == 34) {
                    hexs(dat, 59, 4, s);
                    segd->XX = atoi(s);
                }
            } else if (n == 1) {
                strncpy1(s, hed, 8);
                trim(s);
                segd->LineNm = s;
            }
            if (n == 2) {
                strncpy1(s, hed + 16, 16);
                segd->ReelNo = atoi(s);
            } else if (n == 3) {
                strncpy1(s, hed, 16);
                trim(s);
                segd->Client = s;
                strncpy1(s, hed + 16, 16);
                trim(s);
                segd->Contra = s;
            }
            hed += 32;
        }
    } else if (segd->Mc == 20 && EC > 1 && segd->RevMaj == 0x01 &&
               segd->RevMin == 0x06) {
        char s[2048];
        int v = hex2uint(hexs(dat, 101, 4, s));

        segd->XF = v;
    }

    set_len(l);
    memcpy(hdr, dat, l);
    type = 1;
}

//---------------------------------------------------------------------------

void TSegDObj::DumpNavData(char *s)
{
    char v[1024];
    int l;
    int rev;

    xprintf(0, "Navigation (Spectra) Data - ACSII\r\n");
    strncpy1(v, s, 2);
    xprintf(0, "Header ID           : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 4);
    xprintf(0, "Header length       : %s\r\n", v);
    s += 4;

    strncpy1(v, s, 4);
    xprintf(0, "Program revision    : %s\r\n", v);
    s += 4;
    rev = atoi(v);

    strncpy1(v, s, 2);
    xprintf(0, "Line status         : %s\r\n", v);
    s += 2;

    if (s[6] == '.')
        l = 13;
    else
        l = 6;

    strncpy1(v, s, l);
    xprintf(0, "Shot time           : %s\r\n", v);
    s += l;

    strncpy1(v, s, 4);
    xprintf(0, "Year                : %s\r\n", v);
    s += 4;
    strncpy1(v, s, 2);
    xprintf(0, "Month               : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 2);
    xprintf(0, "Day                 : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 3);
    xprintf(0, "Time Ref            : %s\r\n", v);
    s += 3;

    strncpy1(v, s, 6);
    xprintf(0, "Shot number         : %s\r\n", v);
    s += 6;
    strncpy1(v, s, 16);
    xprintf(0, "LineName            : %s\r\n", v);
    s += 16;
    strncpy1(v, s, 11);
    xprintf(0, "Master latitude     : %s\r\n", v);
    s += 11;
    strncpy1(v, s, 11);
    xprintf(0, "Master longitude    : %s\r\n", v);
    s += 11;
    strncpy1(v, s, 6);
    xprintf(0, "Water depth         : %s\r\n", v);
    s += 6;
    strncpy1(v, s, 11);
    xprintf(0, "Source latitude     : %s\r\n", v);
    s += 11;
    strncpy1(v, s, 11);
    xprintf(0, "Source longitude    : %s\r\n", v);
    s += 11;
    strncpy1(v, s, 5);
    xprintf(0, "Master Gyro         : %s\r\n", v);
    s += 5;
    strncpy1(v, s, 5);
    xprintf(0, "Master GMG          : %s\r\n", v);
    s += 5;
    strncpy1(v, s, 4);
    xprintf(0, "Master Speed        : %s\r\n", v);
    s += 4;

    if (rev > 3) {
        strncpy1(v, s, 4);
        xprintf(0, "Sequence number     : %s\r\n", v);
        s += 4;
    }

    if (rev == 3 || rev == 6 || rev == 7) {
        strncpy1(v, s, 3);
        xprintf(0, "Vessel ID           : %s\r\n", v);
        s += 3;
    }

    if (!(s[0] == '*' || s[0] == '>')) {
        strncpy1(v, s, 11);
        xprintf(0, "Master easting      : %s\r\n", v);
        s += 11;
        strncpy1(v, s, 11);
        xprintf(0, "Master northing     : %s\r\n", v);
        s += 11;
        strncpy1(v, s, 7);
        xprintf(0, "Source delta east   : %s\r\n", v);
        s += 7;
        strncpy1(v, s, 7);
        xprintf(0, "Source delta north  : %s\r\n", v);
        s += 7;
        strncpy1(v, s, 5);
        xprintf(0, "Line bearing        : %s\r\n", v);
        s += 5;
        strncpy1(v, s, 3);
        xprintf(0, "Julian day          : %s\r\n", v);
    }

    xprintf(0, "\r\n-------------------------------------------\r\n\r\n");
}

void TSegDObj::DumpGunData(char *s)
{
    char v[1024];
    char x[5], y[5];

    xprintf(0, "Gun data\r\n");
    strncpy1(v, s, 6);
    xprintf(0, "Format Identifier   : %s\r\n", v);
    s += 6;
    strncpy1(v, s, 4);
    xprintf(0, "Header length       : %s\r\n", v);
    s += 4;
    // s=s+10;
    strncpy1(v, s, 8);
    xprintf(0, "Line number         : %s\r\n", v);
    s += 8;
    strncpy1(v, s, 10);
    xprintf(0, "Shot point number   : %s\r\n", v);
    s += 10;
    strncpy1(v, s, 2);
    xprintf(0, "Active string mask  : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 1);
    xprintf(0, "Trigger mode        : %s\r\n", v);
    s += 1;
    strncpy1(v, s, 8);
    xprintf(0, "Shot time YY/MM/DD  : %s\r\n", v);
    s += 8;
    strncpy1(v, s, 9);
    xprintf(0, "Shot time:HH/MM/SS  : %s\r\n", v);
    s += 9;
    strncpy1(v, s, 1);
    xprintf(0, "Sequence number     : %s\r\n", v);
    s += 1;
    strncpy1(x, s, 1);
    xprintf(0, "Number of strings   : %s\r\n", x);
    s += 1;
    strncpy1(y, s, 2);
    xprintf(0, "# of guns total     : %s\r\n", y);
    s += 2;
    strncpy1(v, s, 2);
    xprintf(0, "# of guns fired     : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 2);
    xprintf(0, "# of guns fire err  : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 2);
    xprintf(0, "# of guns autofired : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 2);
    xprintf(0, "# of guns mis-fired : %s\r\n", v);
    s += 2;
    strncpy1(v, s, 3);
    xprintf(0, "Total err spread    : %s\r\n", v);
    s += 3;
    strncpy1(v, s, 5);
    xprintf(0, "Volume fired cu.in  : %s\r\n", v);
    s += 5;
    strncpy1(v, s, 5);
    xprintf(0, "Aver. abs. error ms : %s\r\n", v);
    s += 5;
    strncpy1(v, s, 5);
    xprintf(0, "Stand deviat.err ms : %s\r\n", v);
    s += 5;
    strncpy1(v, s, 4);
    xprintf(0, "Spare               : %s\r\n", v);
    s += 4;
    strncpy1(v, s, 4);
    xprintf(0, "Mainfold pressure   : %s\r\n", v);
    s += 4;
    strncpy1(v, s, 4);
    xprintf(0, "Not used            : %s\r\n", v);
    s += 4;

    xprintf(0, "\r\n");

    int n;
    int ns = atoi(x);

    for (n = 0; n < ns; n++) {
        strncpy1(v, s, 4);
        xprintf(0, "String # %d pressure: %s\r\n", n + 1, v);
        s += 4;
    }

    int ng = atoi(y);

    xprintf(0, "\r\n");

    xprintf(0,
            "Gun #  Gun   Detect  Seq #  Auto   Static  Gun      Fire  Error  "
            "Gun\r\n");
    xprintf(0,
            "       mode  mode           fire   Offset  delay    time         "
            "Depth\r\n");

    char gn[10];
    char gm[10];
    char dm[10];
    char sn[10];
    char af[10];
    char bb[10];
    char so[10];
    char dl[10];
    char ft[10];
    char er[10];
    char dp[10];

    for (n = 0; n < ng; n++) {
        strncpy1(gn, s, 2);
        s += 2;
        strncpy1(gm, s, 1);
        s += 1;
        strncpy1(dm, s, 1);
        s += 1;
        strncpy1(sn, s, 1);
        s += 1;
        strncpy1(af, s, 1);
        s += 1;
        strncpy1(bb, s, 1);
        s += 1;
        strncpy1(so, s, 3);
        s += 3;
        strncpy1(dl, s, 3);
        s += 3;
        strncpy1(ft, s, 3);
        s += 3;
        strncpy1(er, s, 3);
        s += 3;
        strncpy1(dp, s, 3);
        s += 3;

        xprintf(
            0,
            "%s     %s     %s       %s      %s      %s     %s      %s    %s   "
            "%s\r\n",
            gn, gm, dm, sn, af, so, dl, ft, er, dp);
    }

    xprintf(0, "\r\n-------------------------------------------\r\n");
}

string TExHdr::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "External Header Block len = %d bytes \r\n\r\n", len);

    displayMemory(0, hdr, len);

    char *ss;
    ss = strmem((char *)"$1", (char *)hdr, len);

    if (ss) {
        int p = ss - (char *)hdr;
        if (p + 36 < len && memcmp(ss + 33, "UTC", 3) == 0)
            DumpNavData(ss);
    }

    ss = strmem((char *)"*GCS90", (char *)hdr, len);
    if (ss)
        DumpGunData(ss);

    xprintf(0, "\r\n-------------------------------------------\r\n\r\n");

    return _inf;
}

TExHdr::TExHdr(TSegD *segd)
    : TSegDObj(segd)
{
    char *s;
    char *navs;
    char *tmps;
    int EX = _segd->EX;

    unsigned int l = EX * 32;

    name = "External Hdr";

    byte *dat = segd->read(l);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    len = l;

    set_len(l);
    memcpy(hdr, dat, l);
    type = 1;

    tmps = new char[len + 1];
    navs = new char[len + 1];

    strncpy1(tmps, dat, len);
    tmps[len] = 0;

    s = strstr(tmps, "$1");
    if (!s)
        goto END;

    strncpy1(navs, s + 2, 4);
    if (strlen(navs) < 4)
        goto END;

    l = atoi(navs);
    if (l < 1)
        goto END;

    l = l + 2;
    if (l > strlen(s))
        l = strlen(s);

    strncpy1(navs, s, l);
    if (strlen(navs) != l)
        goto END;

    _segd->NavStr = navs;

END:

    delete[] tmps;
    delete[] navs;
}

string TSsHdr::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "HydroScience SS header\r\n\r\n");

    displayMemory(0, hdr, len);

    return _inf;
}

TSsHdr::TSsHdr(TSegD *segd)
    : TSegDObj(segd)
{
    name = "SS Hdr";

    byte *dat = segd->read(32);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    set_len(32);
    memcpy(hdr, dat, 32);
    type = 1;
}

string TXxHdr::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "Syntrak Extra Header Blocks %d\r\n\r\n", _segd->XX);

    displayMemory(0, hdr, len);

    return _inf;
}

TXxHdr::TXxHdr(TSegD *segd)
    : TSegDObj(segd)
{
    int l = _segd->XX * 32;

    name = "Extra Hdr";

    byte *dat = segd->read(l);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    set_len(l);
    memcpy(hdr, dat, l);
    type = 1;
}

string TJunk::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "%d bytes of unknown data\r\n\r\n", len);
    displayMemory(0, hdr, len);

    return _inf;
}

TJunk::TJunk(TSegD *segd, int l)
    : TSegDObj(segd)
{
    name = "Unknown data";
    len = l;

    byte *dat = segd->read(l);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    set_len(l);
    memcpy(hdr, dat, l);
}

string TTapeHdr::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    char seqn[5];
    char sdrv[6];
    char strc[7];
    char bnde[5];
    char mxbs[11];
    char apic[11];
    char date[12];
    char sern[13];
    char resr[7];
    char extl[13];
    char recn[25];
    char user[15];
    char mxsr[11];

    char *h = (char *)hdr;

    substr(h, 1, 4, seqn);
    substr(h, 5, 5, sdrv);
    substr(h, 10, 6, strc);
    substr(h, 16, 4, bnde);
    substr(h, 20, 10, mxbs);
    substr(h, 30, 10, apic);
    substr(h, 40, 11, date);
    substr(h, 51, 12, sern);
    substr(h, 63, 6, resr);
    substr(h, 69, 12, extl);
    substr(h, 81, 24, recn);
    substr(h, 105, 14, user);
    substr(h, 119, 10, mxsr);

    xprintf(0, "\n");
    xprintf(0, "SEG-D tape label------------------------------------------\n");
    xprintf(0, "Storage unit sequence number  : %s\n", seqn);
    xprintf(0, "SEG-D revision                : %s\n", sdrv);
    xprintf(0, "Storage unit structure        : %s\n", strc);
    xprintf(0, "Binding edition               : %s\n", bnde);
    xprintf(0, "Maximum block size            : %s\n", mxbs);
    xprintf(0, "API Producer org. code        : %s\n", apic);
    xprintf(0, "Creation date                 : %s\n", date);
    xprintf(0, "Serial number                 : %s\n", sern);
    xprintf(0, "Reserved                      : %s\n", resr);
    xprintf(0, "External label name           : %s\n", extl);
    xprintf(0, "Recording entity name         : %s\n", recn);
    xprintf(0, "User defined                  : %s\n", user);
    xprintf(0, "Max shot recs. per field rec. : %s\n", mxsr);
    xprintf(0,
            "----------------------------------------------------------\n\n");

    displayMemory(0, hdr, len);

    return _inf;
}

TTapeHdr::TTapeHdr(TSegD *segd)
    : TSegDObj(segd)
{
    byte *dat = segd->read(128);

    name = "SEG-D Tape Header";
    type = 21;

    set_len(128);
    memcpy(hdr, dat, 128);
}

string TSkew::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "Sample Skew Header %d*32 \r\n\r\n", _segd->SK);

    displayMemory(0, hdr, len);

    return _inf;
}

TSkew::TSkew(TSegD *segd)
    : TSegDObj(segd)
{
    //    long long pos;
    int l = _segd->SK * 32;

    name = "Skew Hdr";

    byte *dat = segd->read(l);

    if (!dat) {
        xprintf(1, "%s is too short\r\n", name.c_str());
        return;
    }

    set_len(l);
    memcpy(hdr, dat, l);
}

//---------------------------------------------------------------------------

string TMux::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    xprintf(0, "Multilplexed Data %d bytes\r\n\r\n", len);

    return _inf;
}

TMux::TMux(TSegD *segd)
    : TSegDObj(segd)
{

    traces = new TMyObjList;

    int nscnsmp = 0;
    int scnlen = 0;
    int n, nc;
    int ns;

    ns = 0;

    if (_segd->Si > 0)
        ns = _segd->TrcSmpNum;

    if (_segd->Format != 200) // SEG-D
    {

        for (n = 0; n < _segd->csl->getCount(); n++) {
            TCs *cs = _segd->get_csi(n);

            int nss = cs->nsscan;
            int nch = cs->nch;
            nscnsmp += nch * nss;

            cs->si = _segd->Si / (cs->nsscan);

            for (nc = 0; nc < nch; nc++) {
                TTr *tr = new TTr(segd, ns * nss, nc, n);
                traces->Add(tr);
                cs->get_trc()->Add(tr);
            }
        }

        scnlen = (int)(nscnsmp * _segd->Smpl / 8.) + 8;

        int l = ns * scnlen;

        name = "Multplexed data";

        hdr = segd->read(l);

        if (!hdr) {
            xprintf(1, "%s is too short\r\n", name.c_str());
            return;
        }

        byte *scan;
        float *fsmp;
        int is, iss;
        int nt;

        len = l;

        float *osmp = new float[nscnsmp];
        //    float  fmp  = 1;

        for (is = 0; is < ns; is++) {
            scan = hdr + is * scnlen + 8;

            if (_segd->Format == 15) {
                F0015_to_float(scan, osmp, nscnsmp);

                nt = 0;

                for (n = 0; n < _segd->csl->getCount(); n++) {
                    TCs *cs = _segd->get_csi(n);

                    float fmp = pow(2, cs->mp);

                    int nss = cs->nsscan;
                    int nch = cs->nch;

                    for (nc = 0; nc < nch; nc++) {
                        TTr *tr = cs->get_trc()->get_tr(nc);
                        int smpi = is * nss;
                        fsmp = (float *)tr->smps;
                        for (iss = 0; iss < nss; iss++)
                            fsmp[smpi + iss] = osmp[nt] * fmp;
                        nt++;
                    }
                }
            }
        }

        delete[] osmp;
    } else // SEG-B
    {
        for (n = 0; n < _segd->csl->getCount(); n++) {
            TCs *cs = _segd->get_csi(n);

            int nch = cs->nch;
            nscnsmp += nch;

            cs->si = _segd->Si;

            for (nc = 0; nc < nch; nc++) {
                TTr *tr = new TTr(segd, ns, nc, n);
                traces->Add(tr);
                cs->get_trc()->Add(tr);
            }
        }

        scnlen = _segd->B;

        int l = ns * scnlen;

        name = "Multplexed data";

        hdr = segd->read(l);

        if (!hdr) {
            xprintf(1, "%s is too short\r\n", name.c_str());
            return;
        }

        len = l;

        unsigned char *scan;
        float *fsmp;
        int is;
        int nt;

        float *osmp = new float[nscnsmp];

        memset(osmp, 0, nscnsmp * sizeof(float));

        for (is = 0; is < ns; is++) {
            scan = (unsigned char *)hdr + is * scnlen;

            // int nn;

            for (nt = 0; nt < 5; nt++) {
                short b1 = scan[4 + nt * 2];
                short b2 = scan[4 + nt * 2 + 1];

                if (nt == 0)
                    b1 = b1 >> 1;

                b2 = b2 >> 1;

                short bb = ((b2 | (b1 << 7)) << 1);
                bb = bb >> 1;

                osmp[nt] = bb;

                if (nt == 0) {
                    // printf("is = %6d  b1 = %2x  - b2 = %2x  bb = %4x - %d\n",
                    // is, b1,b2,bb,bb); fflush(stdout);
                }
            }

            B0015_to_float(scan + 14, osmp + 5, nscnsmp - 5);

            nt = 0;

            for (n = 0; n < _segd->csl->getCount(); n++) {
                TCs *cs = _segd->get_csi(n);

                int nch = cs->nch;

                for (nc = 0; nc < nch; nc++) {
                    TTr *tr = cs->get_trc()->get_tr(nc);
                    fsmp = (float *)tr->smps;
                    fsmp[is] = osmp[nt];
                    nt++;
                }
            }
        }

        delete[] osmp;
    }

    type = 4;
}

TMux::~TMux()
{
    delete traces;
}

//---------------------------------------------------------------------------

string TSegdInfo::get_inf()
{
    if (_err != "")
        return _err;
    if (_inf != "")
        return _inf;

    int n;

    xprintf(0, "Common Information\r\n\r\n");

    if (_segd->Type == 0) {
        // xprintf(0, "Unknown data: %s\r\n",_err);
    } else if (_segd->Type == 1) {
        // xprintf(0, "SEG-Y data: %s\r\n",_err);
    } else if (_segd->Type == 2) // SEG-B
    {
        xprintf(0, "File Format     : SEG-B\r\n");
        xprintf(0, "FFID            : %d\r\n", _segd->F);
        xprintf(0, "Data Format     : %04d - %s\r\n", _segd->Format,
                "SEG-B 20 bit binary multiplexed");
        xprintf(0, "Sample Interval : %g ms\r\n", _segd->Si);
        xprintf(0, "Total traces #       : %d\r\n", _segd->trc_cnt);

        xprintf(0, "\r\nChannel Sets (%d):\r\n\r\n", _segd->get_ncs());

        xprintf(
            0, "   #  type                     #chan #samples    to(ms) tn(ms) "
               "   si(ms)\r\n");

        for (n = 0; n < _segd->get_ncs(); n++) {
            TCs *cs = _segd->get_csi(n);
            double si = _segd->Si / (cs->nsscan);
            TTr *tr = cs->get_trc()->get_tr(0);

            xprintf(0, "%4d  ", cs->csn);
            xprintf(0, "%-25s ", cs->typnm);
            xprintf(0, "%-5d  ", cs->nch);
            if (tr == NULL)
                xprintf(0, "%-9s  ", "N/A");
            else
                xprintf(0, "%-9d  ", tr->ns);
            xprintf(0, "%-6g  ", cs->mto);
            xprintf(0, "%-6g  ", cs->mtn);
            xprintf(0, "  %-g", si);
            xprintf(0, "\r\n");
        }

        xprintf(0, "\r\n");
    } else {
        if (_segd->objs->getCount() == 2) {
            TSegDObj *obj = (TSegDObj *)(_segd->objs->getObj(1));
            if (obj->type == 21)

                xprintf(0, "SEG-D tape header\r\n");

            return _inf;
        }

        int minor = _segd->RevMin;
        int major = _segd->RevMaj;

        if (major == 2 || major == 3 || major == 1) {
            xprintf(0, "File Format     : SEG-D %d.%d\r\n", major, minor);
        } else {
            xprintf(0, "File Format     : SEG-D\r\n");
        }

        xprintf(0, "FFID            : %d\r\n", _segd->F);

        if (_segd->SP == -999999)
            xprintf(0, "SP              : %s\r\n", "N/A");
        else
            xprintf(0, "SP              : %d\r\n", _segd->SP);

        xprintf(0, "Data Format          : %04d - %s\r\n", _segd->Format,
                FormatName[_segd->Format]);
        xprintf(0, "Base Sample Interval : %g ms\r\n", _segd->Si);
        xprintf(0, "Total traces #       : %d\r\n", _segd->trc_cnt);

        xprintf(0, "\r\nChannel Sets (%d):\r\n\r\n", _segd->get_ncs());

        xprintf(0,
                "  st#  cs#  type                     #chan  #samples   to(ms) "
                "tn(ms)    si(ms)\r\n");

        for (n = 0; n < _segd->get_ncs(); n++) {
            TCs *cs = _segd->get_csi(n);
            TTr *tr = cs->get_trc()->get_tr(0);

            double si = 0;

            if (cs->si > 0)
                si = cs->si;
            else
                si = _segd->Si / (cs->nsscan);

            xprintf(0, "%4d  ", cs->stn);
            xprintf(0, "%4d  ", cs->csn);
            xprintf(0, "%-25s ", cs->typnm);
            xprintf(0, "%-5d  ", cs->nch);
            if (tr == NULL)
                xprintf(0, "%-9s  ", "N/A");
            else
                xprintf(0, "%-9d  ", tr->ns);
            xprintf(0, "%-5g  ", cs->mto);
            xprintf(0, "%-5g  ", cs->mtn);
            xprintf(0, "  %-g", si);
            xprintf(0, "\r\n");
        }
    }

    xprintf(0, "\r\n");

    if (_segd->hdr_len)
        xprintf(0, "File Header Len : %d bytes\r\n", _segd->hdr_len);
    xprintf(0, "File Length     : %d bytes\r\n", _segd->getlen());

    xprintf(0, "\r\n");
    xprintf(0, "%s\r\n", _segd->warn.c_str());

    return _inf;
}

TSegdInfo::TSegdInfo(TSegD *segd)
    : TSegDObj(segd)
{
    name = "Common Info";
}

//---------------------------------------------------------------------------

TGenHedAll::TGenHedAll(TSegD *segd)
    : TSegDObj(segd, 1)
{
    name = "General Headers";
}

string TGenHedAll::get_inf()
{
    int n;
    int nh = 0;
    char s[1024];
    string inf;
    string crid;

    for (n = 0; n < _segd->get_nobj(); n++) {
        TSegDObj *obj = _segd->get_obj(n);
        if (obj->type == 0)
            nh++;
    }

    sprintf(s, "General Headers (%d)\r\n\r\n", nh);

    _inf = s;

    for (n = 0; n < _segd->get_nobj(); n++) {
        TSegDObj *obj = _segd->get_obj(n);

        if (obj->type == 0) {
            inf = obj->get_inf();
            _inf = _inf + inf;
            _inf = _inf +
                   "----------------------------------------------------------"
                   "---------------------\r\n\r\n";
            if (obj->bid == 0x55) {
                byte *buf = obj->get_buf();
                crid = crid + str_item(buf, 1, 62);
            }
        }
    }

    if (crid != "") {
        _inf = _inf + "CRID - Coordinate Ref System Identification:\r\n\r\n";
        _inf = _inf + crid;
        _inf = _inf +
               "\r\n--------------------------------------------------------"
               "-----------------------\r\n\r\n";
    }

    return _inf;
}

//---------------------------------------------------------------------------

string TSegdWarn::get_inf()
{
    return _inf;
}

TSegdWarn::TSegdWarn(TSegD *segd, string warn)
    : TSegDObj(segd)
{
    name = "Warning";
    _wrn = warn;
    _inf = warn;
    segd->warn = segd->warn + warn + "\r\n";
}

TSegdEof::TSegdEof(TSegD *segd, string warn)
    : TSegDObj(segd)
{
    name = "End of File";
    _wrn = warn;
    _inf = warn;
}

string TSegdEof::get_inf()
{
    return _inf;
}

//---------------------------------------------------------------------------

byte *TSegD::read(int len)
{
    byte *dat;

    if (_pos + len > _len)
        return NULL; // len=_len-_pos;

    dat = _buf + _pos;

    if (len > 0)
        _pos += len;
    else {
        len = 0;
        dat = NULL;
    }

    return dat;
}

void TSegD::close()
{
    if (_len > 0) {
        delete[] _buf;
        _len = 0;
    }
}

int TSegD::open(char *)
{
    close();

    int len = 0;

    if (len > 0) {

        _buf = new byte[len];
        _len = len;
        _pos = 0;
    }

    return 0;
}

int TSegD::create(int len)
{
    close();

    if (len > 0) {
        _buf = new byte[len];
        _len = len;
        _pos = 0;
    }

    return 0;
}

int TSegD::seek(int pos, int from)
{
    if (from == SEEK_END)
        pos = _len - pos;

    if (from == SEEK_CUR)
        pos = _pos + pos;

    if (pos < 0)
        pos = 0;
    if (pos > _len - 1)
        pos = _len;

    _pos = pos;

    return pos;
}

int TSegD::tell()
{
    return _pos;
}

int TSegD::find_trcx(int ppos, short ffff, int fn)
{
    int pos;
    int plen = 20;

    for (pos = ppos; pos + plen < _len; pos++) {
        TTrInf ti = chck_trcx(pos, ffff, fn);

        if (ti.stat >= 0) {
            return pos;
        }
    }

    return -1;
}

TTrInf TSegD::chck_trcx(int ppos, short ffff, int fn)
{
    TTrInf ti;

    ti.stat = 0; // EOF
    ti.pos = ppos;

    if (ppos == _len) {
        return ti;
    }

    ti.stat = -2;

    if (ppos + 20 >= _len)
        return ti;

    byte *dat = _buf + ppos;

    char s[1024];

    int FN;
    int CS;
    int ST;
    int CH;
    int THE = 0;
    int EXNS = 0;

    int n;
    int v;

    ti.stat = -3;

    if (memcmp(dat, &ffff, 2) != 0)
        return ti;

    hexs(dat, 1, 4, s);
    if (strcmp(s, "FFFF")) {
        n = sscanf(s, "%d", &v);
        if (n == 1)
            FN = v;
        else
            return ti;
        //      FN=atoi(s);
    } else
        FN = hex2uint(hexs(dat, 35, 6, s));

    ti.stat = -4;
    if (FN != fn)
        return ti;

    hexs(dat, 5, 2, s);
    ST = atoi(s);

    hexs(dat, 7, 2, s);
    if (strcmp(s, "FF"))
        CS = atoi(s);
    else
        CS = hex2uint(hexs(dat, 31, 4, s));

    TCs *Cs = NULL;
    int csidx = find_csi(CS, ST);

    if (csidx >= 0) {
        Cs = get_csi(csidx);
    }

    ti.stat = -5;

    if (Cs == NULL)
        return ti;
    if (Cs->nch < 1)
        return ti;

    hexs(dat, 9, 4, s);
    CH = atoi(s);

    THE = hex2uint(hexs(dat, 19, 2, s));

    if (THE > 0) {
        ti.stat = -6;
        if (ppos + 20 + 32 * THE > _len)
            return ti;
        EXNS = uint_item(dat + 20, 15, 6);
    }

    ti.stat = 1;
    ti.chn = CH;
    ti.csn = CS;
    ti.stn = ST;
    ti.exns = EXNS;
    ti.the = THE;
    ti.pos = ppos;

    return ti;
}

static char prbuf[2048];

string str_item(byte *buf, int pos, int count)
{
    char s[2048];

    char *v = hex2ascii(hexs(buf, pos, count, s));

    return v;
}

int uint_item(byte *buf, int pos, int count)
{
    char s[2048];
    int v = hex2uint(hexs(buf, pos, count, s));
    return v;
}

int sint_item(byte *buf, int pos, int count)
{
    char s[2048];
    int v = hex2sint(hexs(buf, pos, count, s));
    return v;
}

float flt_item(byte *buf, int pos, int count)
{
    char s[2048];
    float vf = hex2flt(hexs(buf, pos, count, s));
    return vf;
}

char *bcd_str_item(byte *buf, int pos, int count, char *str)
{
    static char s[2048];

    hexs(buf, pos, count, s);

    if (str) {
        strcpy(str, s);
        return str;
    }

    return s;
}

/*
char* asc_item(byte *buf, int pos, int count, char* str)
{
    static char s[2048];

    char* v = hex2ascii(hexs(buf, pos, count,s));

    if(str)
    {
        strcpy(str,v);
        return str;
    }

    return v;
}
*/

const char *pritem(const char *desc, int type, byte *buf, int n, int count,
                   int eol)
{
    char s[2048];
    int v;
    double vd;

    char descr[1024];

    strcpy(descr, desc);

    trim(descr);

    if (type == 0) // bcd
    {
        hexs(buf, n, count, s);
        sprintf(prbuf, "%-33s : %-10s", descr, s);
    } else if (type == 1) // unsigned int
    {
        v = hex2uint(hexs(buf, n, count, s));
        sprintf(prbuf, "%-33s : %-10u [%s]", descr, v, s);
    } else if (type == 11) // unsigned int 64 bit
    {
        uint64_t v = hex2uint64(hexs(buf, n, count, s));

#if (!defined(_WIN64) && defined(_WIN32) && defined(__GNUC__))
        sprintf(prbuf, "%-33s : %-10I64d [%s]", descr, v, s);
#else
        sprintf(prbuf, "%-33s : %-10llu [%s]", descr, v, s);
#endif
    } else if (type == 2) // signed int
    {
        v = hex2sint(hexs(buf, n, count, s));
        sprintf(prbuf, "%-33s : %-10d [%s]", descr, v, s);
    } else if (type == 12) // signed int 64 bit
    {
        int64_t v = hex2sint64(hexs(buf, n, count, s));

#if (!defined(_WIN64) && defined(_WIN32) && defined(__GNUC__))
        sprintf(prbuf, "%-33s : %-10I64d [%s]", descr, v, s);
#else
        sprintf(prbuf, "%-33s : %-10lld [%s]", descr, v, s);
#endif
    } else if (type == 3) // float iEEE
    {
        vd = (double)hex2flt(hexs(buf, n, count, s));
        sprintf(prbuf, "%-33s : %-10.10g [%s]", descr, vd, s);
    } else if (type == 4) // double iEEE
    {
        vd = hex2dbl(hexs(buf, n, count, s));
        sprintf(prbuf, "%-33s : %-10.10g [%s]", descr, vd, s);
    } else if (type == 5) // asc
    {
        char *v = hex2ascii(hexs(buf, n, count, s));
        sprintf(prbuf, "%-33s : %-s", descr, v);
    } else if (type == 15) // asc data from new line
    {
        char *v = hex2ascii(hexs(buf, n, count, s));
        sprintf(prbuf, "%-33s : \r\n%-s\r\n--------------\r\n", descr, v);
    } else if (type == 6) // SEG-D time stamp
    {
        int64_t t = hex2sint64(hexs(buf, n, count, s));
        string v = SegdTimeToStr(t);

#if (!defined(_WIN64) && defined(_WIN32) && defined(__GNUC__))
        sprintf(prbuf, "%-33s : %-10I64d [%s] (%s)", descr, t, s, v.c_str());
#else
        sprintf(prbuf, "%-33s : %-10lld [%s] (%s)", descr, t, s, v.c_str());
#endif
    }

    if (eol)
        strcat(prbuf, "\r\n");
    else
        strcat(prbuf, " ");

    return prbuf;
}

//---------------------------------------------------------------------------

int int2hexd(int num, byte *hex, int nb)
{
    unsigned int n = (unsigned int)num;

    if (hex == NULL || nb < 1)
        return 0;

    memset(hex, 0, nb);

    int i = nb - 1;
    int j = 0;
    int k;

    for (k = 0;; k++) {
        if (i < 0)
            break;

        unsigned int x = n % 10;
        n = n / 10;

        if (j == 0) {
            hex[i] |= x;
            j = 1;
        } else {
            hex[i] |= x << 4;
            j = 0;
            i--;
        }

        if (n == 0)
            break;
    }

    return k;
}

/*
void segd8015_to_ieee(void* inp, float* out, int ns)
{
    int n, nq ,no;

    unsigned char*  z = (unsigned char* )inp;
    unsigned short* i = (unsigned short*)inp;
    unsigned int*   o = (unsigned int*  )out;

    int             cs;
    unsigned int    c;
    unsigned int    q;
    unsigned int    s;
    unsigned short  x;

    int ss;
    float qq,fc;
    int   cc;


    for(n=0;n<ns;)
    {
      cs=12;
      z = (unsigned char*)i;
      for(nq=1;nq<5;nq++)
      {
        x = i[0 ];
        swap2(&x);
        c  = ((x>>cs)&0xF); cc=c;
        c=c+127;
        c = c<<23;
        x = i[nq];
        swap2(&x);
        q =   x&0x7FFF; qq=q;
        qq = qq*pow(2,-15);
        q = q<<9;
        s =   x&0x8000;
        if(s) ss = -1;
        else  ss =  1;
        s = s<<16;
        q =   s | c | q;
//      swap2(&q);
        o[n]=q;
        fc=pow(2,cc);
        out[n]=ss * (qq) * fc ;
        out[n]*=fmp;
        n++; cs-=4;
      }
      i+=10;
    }
}
*/

static int nbyte;

static short GET_S(void *inp)
{
    int n1, n2;
    int n;
    short s;
    unsigned char *inps = (unsigned char *)inp;

    n1 = inps[nbyte++];
    n2 = inps[nbyte++];
    n = (n1 << 8);
    n = n | n2;
    s = (short)((n > 32767) ? n - 65536 : n);

    return (s);
}

static int GET_3(void *inp, int swap)
{
    int num = 0;

    memcpy((char *)(&num), (char *)(inp) + nbyte, 3);
    nbyte += 3;

    if (swap)
        swap4(&num);

    num = num >> 8;

    return (num);
}

static int GET_4(void *inp, int swap)
{
    int num;

    memcpy(&num, (char *)(inp) + nbyte, 4);
    nbyte += 4;

    if (swap)
        swap4(&num);

    return (num);
}

static int64_t GET_8(void *inp, int swap)
{
    int64_t num;

    memcpy(&num, (char *)(inp) + nbyte, 8);
    nbyte += 8;

    if (swap)
        swap8(&num);

    return (num);
}

static char GET_C(void *inp)
{
    int n;
    char c;

    unsigned char *inps = (unsigned char *)inp;

    n = inps[nbyte++];
    c = (char)((n > 127) ? (n - 256) : n);

    return (c);
}

static unsigned int GET_UC(void *inp)
{
    int n;
    unsigned char uc;

    unsigned char *inps = (unsigned char *)inp;

    n = inps[nbyte++];
    uc = (unsigned char)n;

    return (uc);
}

// From CWP - Thanks to Colorado Mining School
/* F8015_to_float - convert 20 bit binary demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      EOPG: Marc Schaming, Jean-Daniel Tissot
 *      SEP:  Stew Levin - fixed low-order bit error in conversion
 *            of negative values on 2's complement machines.
 *            Use ldexp() function instead of much slower value*pow(2,expo)
 *
 * Parameters:
 *    from   - input vector
 *    to     - output vector
 *    len    - number of packets of 4 floats in vectors
 *
 * Format 8015 is a 10 byte per 4 words (2 1/2 bytes per word)
 * representation.  According to the SEG specifications, the
 * bit layout of the 10 bytes is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1    C3    C2    C1    C0    C3    C2    C1    C0    Exponents for
 * Byte 2    C3    C2    C1    C0    C3    C2    C1    C0    channels 1 thru 4
 *
 * Byte 3     S    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7   Channel 1
 * Byte 4    Q-8   Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15
 * Byte 5     S    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7   Channel 2
 * Byte 6    Q-8   Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15
 * Byte 7     S    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7   Channel 3
 * Byte 8    Q-8   Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15
 * Byte 9     S    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7   Channel 4
 * Byte 10   Q-8   Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15
 *
 * S=sign bit. - (One = negative number)
 * C=binary exponents. - This is a 4 bit positive binary exponent of 2
 *               CCCC
 *   written as 2     where CCCC can assume values of 0-15.  The four
 *   exponents are in channel number order for the four channels starting
 *   with channel one in bits 0-3 of Byte 1.
 * Q1-15-fraction. - This is a 15 bit one's complement binary fraction.
 *   The radix point is to the left of the most significant bit (Q-1)
 *                                  -1
 *   with the MSB being defined as 2 .  The sign and fraction can assume
 *                  -15       -15
 *   values from 1-2   to -1+2  .  Negative zero is invalid and must be
 *   converted to positive zero.
 *                                        CCCC    MP                   MP
 * Input signal = S.QQQQ,QQQQ,QQQQ,QQQ x 2    x 2    millivolts where 2
 *   is the value required to descale the data word to the recording
 *   system input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 * Note that in utilizing this data recording method, the number of data
 *   channels per channel set must be exactly divisible by 4 in order to
 *   preserve the data grouping of this method.
 */

void F8015_to_float(void *inp, float *out, int len)
{
    int i;
    short ex1_4;
    int expo;
    short fraction;

    nbyte = 0;

    for (i = 0; i < len; i += 4) {
        ex1_4 = GET_S(inp);
        expo = ((ex1_4 >> 12) & 15) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -(~fraction);
        *(out++) = ldexp((double)fraction, expo);

        expo = ((ex1_4 >> 8) & 15) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -(~fraction);
        *(out++) = ldexp((double)fraction, expo);

        expo = ((ex1_4 >> 4) & 15) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -(~fraction);
        *(out++) = ldexp((double)fraction, expo);

        expo = (ex1_4 & 15) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -(~fraction);
        *(out++) = ldexp((double)fraction, expo);
    }
}

void B0015_to_float(void *inp, float *out, int len)

{
    int i;
    short ex1_4;
    int expo;
    short fraction;

    nbyte = 0;

    for (i = 0; i < len; i += 4) {
        ex1_4 = GET_S(inp);
        expo = ((ex1_4 >> 12) & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, -expo);

        expo = ((ex1_4 >> 8) & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, -expo);

        expo = ((ex1_4 >> 4) & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, -expo);

        expo = (ex1_4 & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, -expo);
    }
}

void F0015_to_float(void *inp, float *out, int len)
{
    int i;
    short ex1_4;
    int expo;
    short fraction;

    nbyte = 0;

    for (i = 0; i < len; i += 4) {
        ex1_4 = GET_S(inp);
        expo = ((ex1_4 >> 12) & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, expo);

        expo = ((ex1_4 >> 8) & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, expo);

        expo = ((ex1_4 >> 4) & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, expo);

        expo = (ex1_4 & 0x0F) - 15;
        fraction = GET_S(inp);
        if (fraction < 0)
            fraction = -((~fraction) & (~1));
        *(out++) = ldexp((double)fraction, expo);
    }
}

/* F8022_to_float - convert 8 bit quaternary demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    to     - output vector
 *    len    - number of packets of 4 floats in vectors
 *
 * Format 8022 is a 1 byte per word representation.
 * According to the SEG specifications, the bit
 * layout of the byte is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1     S    C2    C1    C0    Q-1   Q-2   Q-3   Q-4
 *
 * S=sign bit. - (One = negative number)
 * C=quaternary exponent. - This is a 3 bit positive binary exponent of 4
 *               CCC
 *   written as 4    where CCC can assume values of 0-7.
 * Q1-4-fraction. - This is a 4 bit one's complement binary fraction.
 *   The radix point is to the left of the most significant bit (Q-1)
 *                                  -1
 *   with the MSB being defined as 2 .  The fraction can have values
 *           -4        -4
 *   inp 1-2   to -1+2  .  Negative zero is invalid and must be
 *   converted to positive zero.
 *                          CCC   MP                   MP
 * Input signal = S.QQQQ x 4   x 2   millivolts where 2    is the
 *   value required to descale the data word to the recording system
 *   input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 */

void F8022_to_float(void *inp, float *out, int len)
{
    int i;
    int ex1_4;
    int expo;
    short fraction;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        ex1_4 = GET_C(inp);
        expo = ((ex1_4 >> 3) & 14) - 4;
        fraction = ex1_4 & 15;
        if (ex1_4 & 128)
            fraction = -(15 ^ fraction);
        *(out++) = ldexp((double)fraction, expo);
    }
}

/* F8024_to_float - convert 16 bit quaternary demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    out     - output vector
 *    len    - number of packets of 4 floats in vectors
 *
 *
 * Format 8024 is a 2 byte per word representation.
 * According out the SEG specifications, the bit
 * layout of the bytes is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1     S    C2    C1    C0    Q-1   Q-2   Q-3   Q-4
 * Byte 2    Q-5   Q-6   Q-7   Q-8   Q-9   Q-10  Q-11  Q-12
 *
 * S=sign bit. - (One = negative number)
 * C=quaternary exponent. - This is a 3 bit positive binary exponent of 4
 *               CCC
 *   written as 4    where CCC can assume values of 0-7.
 * Q1-12-fraction. - This is a 12 bit one's complement binary fraction.
 *   The radix point is out the left of the most significant bit (Q-1)
 *                                  -1
 *   with the MSB being defined as 2 .  The fraction can have values
 *           -12        -12
 *   inp 1-2    out -1+2   .  Negative zero is invalid and must be
 *   converted out positive zero.
 *                                    CCC   MP                   MP
 * Input signal = S.QQQQ,QQQQ,QQQQ x 4   x 2   millivolts where 2
 *   is the value required out descale the data word out the recording
 *   system input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 */

void F8024_to_float(void *inp, float *out, int len)
{
    int i;
    int ex1_4;
    int expo;
    short fraction;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        ex1_4 = GET_S(inp);
        expo = ((ex1_4 >> 11) & 14) - 12;
        fraction = ex1_4 & 4095;
        if (ex1_4 & 32768)
            fraction = -(4095 ^ fraction);
        *(out++) = ldexp((double)fraction, expo);
    }
}

/* F8036_to_float - convert 24 bit quaternary demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    out     - output vector
 *    len    - number of packets of 4 floats in vectors
 *
 *
 * Format 8036 is a 3 byte per word representation.
 * According out the SEG specifications, the bit
 * layout of the bytes is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7   Q-8
 * Byte 2    Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15  Q-16
 * Byte 3    Q-17  Q-18  Q-19  Q-20  Q-21  Q-22  Q-23  Q-24
 *
 * Q1-24-integer. - This is a 24 bit two's complement binary integer.
 *                         MP                   MP
 * Input signal = Q...Q x 2   millivolts where 2
 *   is the value required out descale the data word out the recording
 *   system input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 */
/*
void F8036_to_float (void* inp, float* out, int len)
{
     int i;
     long int ival;

    nbyte=0;

    for (i = 0; i < len; i ++)
    {
        ival = GET_UC(inp);
        ival <<= 8;
        ival |= GET_UC(inp);
        ival <<= 8;
        ival |= GET_UC(inp);
        if(ival > 8388607) ival -= 16777216;
        *(out++) = (float)ival;
    }
}
*/
void F8036_to_float(void *inp, float *out, int len)
{
    int i;
    int v;
    int *p = (int *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_3(inp, 1);
        *(out++) = *p;
    }
}

void F9036_to_float(void *inp, float *out, int len)
{
    int i;
    int v;
    int *p = (int *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_3(inp, 0);
        *(out++) = *p;
    }
}

/* F8038_to_float - convert 32 bit quaternary demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    out     - output vector
 *    len    - number of packets of 4 floats in vectors
 *
 *
 * Format 8038 is a 4 byte per word representation.
 * According out the SEG specifications, the bit
 * layout of the bytes is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7   Q-8
 * Byte 2    Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15  Q-16
 * Byte 3    Q-17  Q-18  Q-19  Q-20  Q-21  Q-22  Q-23  Q-24
 * Byte 4    Q-25  Q-26  Q-27  Q-28  Q-29  Q-30  Q-31  Q-32
 *
 * Q1-32-fraction. - This is a 32 bit two's complement binary integer.
 *                         MP                   MP
 * Input signal = Q...Q x 2   millivolts where 2
 *   is the value required out descale the data word out the recording
 *   system input level.  MP is defined in Byte 8 of each of the corre-
 *
 * Note this conversion routine assumes the target architecture is
 * already 2's complement.
 */

/*
void F8038_to_float (void* inp, float* out, int len)
{
    int i;
    long int ex1_4;
    long int ex2_4;
    long int value;

    nbyte=0;

    for (i = 0; i < len; i ++)
    {
        ex1_4 = GET_S(inp);
        ex2_4 = GET_S(inp);
//      value = (ex1_4<<16) | (ex2_4&65535);
        value = (ex1_4<<16) | (ex2_4&0xFFFF);
        *(out++) = (float)value;
    }
}
*/

void F8038_to_float(void *inp, float *out, int len)
{
    int i;
    int v;
    int *p = (int *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_4(inp, 1);
        *(out++) = *p;
    }
}

void F9038_to_float(void *inp, float *out, int len)
{
    int i;
    int v;
    int *p = (int *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_4(inp, 1);
        *(out++) = *p;
    }
}

/* F8042_to_float - convert 8 bit hexadecimal demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    out     - output vector
 *    len    - number of floats in vector
 *
 * Format 8042 is a 1 byte per word representation.
 * According out the SEG specifications, the bit
 * layout of the byte is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1     S    C1    C0    Q-1   Q-2   Q-3   Q-4   Q-5
 *
 * S=sign bit. - (One = negative number)
 * C=hexadecimal exponent. - This is a 2 bit positive binary exponent of 16
 *                CC
 *   written as 16    where CC can assume values of 0-3.
 * Q1-5-fraction. - This is a 5 bit positive binary fraction.
 *   The radix point is out the left of the most significant bit (Q-1)
 *                                  -1
 *   with the MSB being defined as 2 .  The fraction can have values
 *           -5        -5
 *   inp 1-2   out -1+2  .
 *                             CC    MP                   MP
 * Input signal = S.QQQQ,Q x 16   x 2   millivolts where 2    is the
 *   value required out descale the data word out the recording system
 *   input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 */

void F8042_to_float(void *inp, float *out, int len)
{
    int i;
    int ex1_4;
    int expo;
    short fraction;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        ex1_4 = GET_C(inp);
        expo = ((ex1_4 >> 3) & 12) - 5;
        fraction = ex1_4 & 31;
        if (ex1_4 & 128)
            fraction = -fraction;
        *(out++) = ldexp((double)fraction, expo);
    }
}

/* F8044_to_float - convert 16 bit hexadecimal demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    out     - output vector
 *    len    - number of floats in vector
 *
 *
 * Format 8044 is a 2 byte per word representation.
 * According out the SEG specifications, the bit
 * layout of the bytes is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1     S    C1    C0    Q-1   Q-2   Q-3   Q-4   Q-5
 * Byte 2    Q-6   Q-7   Q-8   Q-9   Q-10  Q-11  Q-12  Q-13
 *
 * S=sign bit. - (One = negative number)
 * C=hexadecimal exponent. - This is a 2 bit positive binary exponent of 16
 *                CC
 *   written as 16    where CC can assume values of 0-3.
 * Q1-13-fraction. - This is a 13 bit positive binary fraction.
 *   The radix point is out the left of the most significant bit (Q-1)
 *                                  -1
 *   with the MSB being defined as 2 .  The fraction can have values
 *           -13        -13
 *   inp 1-2    out -1+2   .
 *                                       CC    MP                   MP
 * Input signal = S.QQQQ,QQQQ,QQQQ,Q x 16   x 2   millivolts where 2
 *   is the value required out descale the data word out the recording system
 *   input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 */

void F8044_to_float(void *inp, float *out, int len)
{
    int i;
    int ex1_4;
    int expo;
    short fraction;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        ex1_4 = GET_S(inp);
        expo = ((ex1_4 >> 11) & 12) - 13;
        fraction = ex1_4 & 8191;
        if (ex1_4 & 32768)
            fraction = -fraction;
        *(out++) = ldexp((double)fraction, expo);
    }
}

/* F8048_to_float - convert 32 bit hexadecimal demultiplexed data into floating
 * numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    out     - output vector
 *    len    - number of floats in vector
 *
 */
/*
 *
 * Format 8048 is a 4 byte per word representation.
 * According out the SEG specifications, the bit
 * layout of the bytes is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1     S    C6    C5    C4    C3    C2    C1    C0
 * Byte 2    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7   Q-8
 * Byte 3    Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15  Q-16
 * Byte 4    Q-17  Q-18  Q-19  Q-20  Q-21  Q-22  Q-23  0
 *
 * S=sign bit. - (One = negative number)
 * C=hexadecimal exponent. - This is a binary exponent of 16
 *                (CCCCCCC-64)
 *   written as 16             where CC can assume values of 0-127.
 * Q1-23-fraction. - This is a 23 bit positive binary fraction.
 *   The radix point is out the left of the most significant bit (Q-1)
 *                                  -1
 *   with the MSB being defined as 2 .  The sign and fraction can have
 *                 -23        -23
 *   values inp 1-2    out -1+2   .
 *                                   C-64    MP                   MP
 * Input signal = S.QQQQ,...,QQQ x 16     x 2   millivolts where 2
 *   is the value required out descale the data word out the recording system
 *   input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 *   The data recording method has more than sufficient range out
 *   handle the dynamic range of a typical seismic system.  Thus, MP
 *   may not be needed out account for any scaling and may be recorded
 *   as zero.
 */

void F8048_to_float(void *inp, float *out, int len)
{
    int i;
    int ex1_4;
    int expo;
    long int fraction;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        ex1_4 = GET_S(inp);
        expo = ((ex1_4 >> 6) & 508) - (24 + 256);
        fraction = ex1_4 & 255;
        fraction <<= 16;
        fraction |= (GET_S(inp) & 65535);
        if (ex1_4 & 32768)
            fraction = -fraction;
        *(out++) = ldexp((double)fraction, expo);
    }
}

/* F8058_to_float - convert 32 bit IEEE float demultiplexed data into floating
numbers
 *
 * Credits:
 *      SEP:  Stew Levin
 *
 * Parameters:
 *    inp   - input sfio unit
 *    out     - output vector
 *    len    - number of floats in vector
 *
 *
 * Format 8058 is a 4 byte per word representation.
 * According out the SEG specifications, the bit
 * layout of the bytes is:
 *
 *
 *  Bit       0     1     2     3     4     5     6     7
 *-----------------------------------------------------------
 * Byte 1     S    C7    C6    C5    C4    C3    C2    C1
 * Byte 2    C0    Q-1   Q-2   Q-3   Q-4   Q-5   Q-6   Q-7
 * Byte 3    Q-8   Q-9   Q-10  Q-11  Q-12  Q-13  Q-14  Q-15
 * Byte 4    Q-16  Q-17  Q-18  Q-19  Q-20  Q-21  Q-22  Q-23
 *
 * S=sign bit. - (One = negative number)
 * C=exponent. - This is a excess-127 binary exponent of 2
 *               (CCCCCCCC-127)
 *   written as 2               where CC can assume values of 0-255.
 * Q1-23-fraction. - This is a 23 bit positive binary fraction.
 *   The radix point is out the left of the most significant bit (Q-1)
 *                                  -1
 *   with the MSB being defined as 2 .  With the exceptions noted below:
 *
 *                    S                    C-127    MP                   MP
 * Input signal = (-1) x 1.QQQQ,...,QQQ x 2     x 2   millivolts where 2
 *   is the value required out descale the data word out the recording system
 *   input level.  MP is defined in Byte 8 of each of the corre-
 *   sponding channel set descriptors in the scan type header.
 *   The data recording method has more than sufficient range out
 *   handle the dynamic range of a typical seismic system.  Thus, MP
 *   may not be needed out account for any scaling and may be recorded
 *   as zero.
 *
 * Exceptions:
 *
 * If C=0 then
 *                    S                    -126     MP
 * Input signal = (-1) x 0.QQQQ,...,QQQ x 2     x 2   millivolts
 *
 * If C=255 and Q=0, then
 *                    S
 * Input signal = (-1) x infinity  (overflow)
 *
 * If C=255 and Q!=0, then
 *
 * Input signal = NaN  (Not-a-Number)

void F8058_to_float (void* inp, float* out, int len)
{
    int i;
    int ex1_4, ex2_4;
    int expo;
    long int fraction;


    for (i = 0; i < len; i ++)
    {
        ex1_4 = GET_S(inp);
        ex2_4 = GET_S(inp);
        expo = ((ex1_4 >> 7) & 255);
        fraction = ex1_4 & 127;
        fraction <<= 16;
        fraction |= (ex2_4&65535);
        if(expo) fraction |= 8388608;
        else fraction <<= 1;
        if (ex1_4 & 32768) fraction = -fraction;
        *(out++) = ldexp((double) fraction, expo-(23+127));
    }
}
*/

void F8058_to_float(void *inp, float *out, int len)
{
    int i;
    int v;
    float *fp = (float *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_4(inp, 1);
        *(out++) = *fp;
    }
}

void F9058_to_float(void *inp, float *out, int len)
{
    int i;
    int v;
    float *fp = (float *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_4(inp, 0);
        *(out++) = *fp;
    }
}

void F8080_to_float(void *inp, float *out, int len)
{
    int i;
    int64_t v;
    double *fp = (double *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_8(inp, 1);
        *(out++) = *fp;
    }
}

void F9080_to_float(void *inp, float *out, int len)
{
    int i;
    int64_t v;
    double *fp = (double *)&v;

    nbyte = 0;

    for (i = 0; i < len; i++) {
        v = GET_8(inp, 0);
        *(out++) = *fp;
    }
}

void mult_fa(float m, float *inp, int len)
{
    int n;

    for (n = 0; n < len; n++) {
        *inp = m * (*inp);
        inp++;
    }
}
