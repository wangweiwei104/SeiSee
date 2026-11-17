#ifndef GFXSRCFILE_H
#define GFXSRCFILE_H



#include "gfxsrcseis.h"
#include "hdrdef.h"
#include "seisfile.h"

class GfxSrcFile : public GfxSrcSeis
{
    Q_OBJECT

protected:
    SeisFile *m_Sfile;

public:
    explicit GfxSrcFile(QObject *parent = 0);

    virtual ~GfxSrcFile()
    {
        if (m_Sfile)
            m_Sfile->UnRegisterLink(this);
    }

    virtual void setSfile(SeisFile *v)
    {
        if (m_Sfile)
            m_Sfile->UnRegisterLink(this);

        m_Sfile = v;

        if (m_Sfile) {
            m_Sfile->RegisterLink(this);
            m_Nt = m_Sfile->Nt();
            m_Ns = m_Sfile->Ns();
            m_Si = m_Sfile->Si();
            m_Ti = m_Sfile->Ti();
        } else {
            m_Nt = 0;
            m_Ns = 0;
            m_Si = 0.04;
            m_Ti = 1;
        }

        Invalidate();
    }

    SeisFile *Sfile()
    {
        return m_Sfile;
    }

    virtual double Tp(int tidx)
    {
        if (!m_Sfile)
            return 0;
        return m_Sfile->Tp(tidx);
    }

    virtual double Th(int tidx, int nitm)
    {
        if (!m_Sfile)
            return 0;

        return m_Sfile->Th(tidx, nitm);
    }

    virtual double Th(int tidx, QString hname)
    {
        if (!m_Sfile)
            return 0;

        return m_Sfile->Th(tidx, hname);
    }

    virtual float *Ts(int tidx)
    {
        if (!m_Sfile)
            return NULL;
        return m_Sfile->Ts(tidx);
    }

    virtual Ttr Tt(long long tidx)
    {
        Ttr tr;
        if (!m_Sfile)
            return tr;
        return m_Sfile->Tt(tidx);
    }

    virtual char *GetTraceHeader(int /*ntr*/)
    {
        if (!m_Sfile)
            return NULL;
        return NULL;
    }

    virtual int Nt()
    {
        if (!m_Sfile)
            return 0;
        return m_Sfile->Nt();
    }

    virtual int Ns()
    {
        if (!m_Sfile)
            return 0;
        return m_Sfile->Ns();
    }

    virtual double Tx(double pos)
    {
        if (!m_Sfile)
            return 0;
        return m_Sfile->Tx(pos);
        ////
    }

signals:

public slots:
};

#endif // GFXSRCFILE_H
