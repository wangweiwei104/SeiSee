//---------------------------------------------------------------------------

#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util2.h"

#include <QColor>

#include <QDir>
#include <QFile>
#include <QRgb>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(                                                               \
    disable : 4244 4267 4305) // implicit conversion, possible loss of data
#else
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

//---------------------------------------------------------------------------

int is_segd_f(QString fname, int &FFID, long long pos)
{
    QByteArray qfn = fname.toLocal8Bit();
    return is_segd_f(qfn.data(), FFID, pos);
}

off64_t getfilesize64(QString inpfn)
{
    QByteArray qfn = inpfn.toLocal8Bit();

    return getfilesize64(qfn.data());
}

int is_segy_f(QString fn)
{
    QByteArray qfn = fn.toLocal8Bit();
    char *fname = qfn.data();

    return is_segy_f(fname);
}

//---------------------------------------------------------------------------

int is_segy_f(QString fn, int &s, int &cgg, int &junk)
{
    QByteArray qfn = fn.toLocal8Bit();
    char *fname = qfn.data();

    return is_segy_f(fname, s, cgg, junk);
}

int is_cst_f(QString fname)
{
    QByteArray qfn = fname.toLocal8Bit();

    return is_cst_f(qfn.data());
}

//---------------------------------------------------------------------------

bool FileExists(QString Name)
{
    QFile f(Name);

    return f.exists();
}

bool DirExists(QString Name)
{
    QDir d(Name);

    return d.exists();
}

bool RemoveDir(const QString &dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists(dirName)) {
        Q_FOREACH (QFileInfo info,
                   dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System |
                                         QDir::Hidden | QDir::AllDirs |
                                         QDir::Files,
                                     QDir::DirsFirst)) {
            if (info.isDir()) {
                result = RemoveDir(info.absoluteFilePath());
            } else {
                result = QFile::remove(info.absoluteFilePath());
            }

            if (!result) {
                return result;
            }
        }
        result = dir.rmdir(dirName);
    }

    return result;
}

bool IsDirEmpty(QString path)
{
    QDir dir(path);

    int count =
        dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).count();

    return (count == 0);
}

//---------------------------------------------------------------------------

int open_q(QString fn, int flags, int perm)
{
    QByteArray qfn = fn.toLocal8Bit();
    const char *fname = qfn.data();

    return open((const char *)fname, flags, perm);
}

FILE *fopen_q(QString fn, QString ff)
{
    QByteArray qfn = fn.toLocal8Bit();
    QByteArray qff = ff.toLocal8Bit();

    return fopen(qfn.data(), qff.data());
}

int unlink_q(QString fn)
{
    QByteArray qfn = fn.toLocal8Bit();
    char *fname = qfn.data();

    return unlink(fname);
}

//---------------------------------------------------------------------------
QList<QRgb> MkVPalette(QRgb *colors, int ncolors, double vmin, double vmax,
                       double vinc)
{
    QList<QRgb> pal;
    int i, j;

    int nvels = (vmax - vmin) / vinc;

    if (nvels < 1) {
        return pal;
    } else if (nvels == 1) {
        pal.append(colors[0]);
        return pal;
    }

    double *vels = new double[nvels];
    QRgb *pals = new QRgb[nvels];

    double jj;

    for (i = 0; i < nvels; i++) {
        double rc, gc, bc;

        jj = double(i) * (ncolors - 1) / (nvels - 1.);
        j = int(jj);

        double r1 = qRed(colors[j]);
        double r2 = qRed(colors[j + 1]);

        double g1 = qGreen(colors[j]);
        double g2 = qGreen(colors[j + 1]);

        double b1 = qBlue(colors[j]);
        double b2 = qBlue(colors[j + 1]);

        double n1 = j;
        if (n1 > ncolors - 1)
            n1 = ncolors - 1;
        double n2 = n1 + 1;
        if (n2 > ncolors - 1)
            n2 = n1;
        double n = jj;

        if (n2 == n1)
            rc = r1;
        else
            rc = r1 + (r2 - r1) / (n2 - n1) * (n - n1);

        if (n2 == n1)
            gc = g1;
        else
            gc = g1 + (g2 - g1) / (n2 - n1) * (n - n1);

        if (n2 == n1)
            bc = b1;
        else
            bc = b1 + (b2 - b1) / (n2 - n1) * (n - n1);

        pals[i] = QColor(rc, gc, bc).rgb();
        pal.append(pals[i]);
    }

    delete[] vels;
    delete[] pals;

    return pal;
}
