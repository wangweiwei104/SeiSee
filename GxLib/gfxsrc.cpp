

#include "gfxsrc.h"

GfxSrc::GfxSrc(QObject *parent)
    : QObject(parent)
{
}

GfxSrc::~GfxSrc()
{
    int n;

    for (n = 0; n < m_links.count(); n++)
        m_links[n]->setSrc(NULL);

    // qDebug() << "~GfxSrc";
}
// ----------------------------------------------------------------------

void GfxSrc::RegisterLink(GfxObj *v)
{
    m_links.append(v);
}

void GfxSrc::UnRegisterLink(GfxObj *v)
{
    m_links.removeAll(v);
}

// ----------------------------------------------------------------------
void GfxSrc::Clear(void)
{
}
