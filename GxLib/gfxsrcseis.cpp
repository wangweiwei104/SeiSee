#include "gfxsrcseis.h"



GfxSrcSeis::GfxSrcSeis(QObject *parent)
    : GfxSrc(parent)
{
    m_Nt = 0;
    m_Si = 0.004;
    m_To = 0;
    m_Ti = 25;

    m_Tp = NULL;
    m_Ts = NULL;

    m_Ns = 0;
}

GfxSrcSeis::~GfxSrcSeis()
{
    Clear();
}

void GfxSrcSeis::Invalidate(void)
{
    int n;
    int nn = m_links.count();

    for (n = 0; n < nn; n++)
        m_links[n]->Preset();
}

void GfxSrcSeis::Clear(void)
{
    if (m_Tp)
        delete[] m_Tp;
    m_Tp = NULL;
    m_Nt = 0;
    m_To = 0;
}
