#include "seistrexpr.h"
#include "util2.h"

SeisTrExpr::SeisTrExpr(QString e)
{
    Hdr = NULL;
    Len = 240;
    L = 0;
    N = 0;

    error = true;

    val_list.AddDefaultValues();
    val_list.AddAddress("_Hdr_", (double *)&Hdr);
    val_list.AddAddress("_Len_", (double *)&Len);
    val_list.AddAddress("_Swp_", (double *)&Swp);
    val_list.AddAddress("L", (double *)&L);
    val_list.AddAddress("N", (double *)&N);

    fun_list.AddDefaultFunctions();
    fun_list.Add(new TrcHdrFactory());

    SetValueList(&val_list);
    SetFunctionList(&fun_list);

    ExprStr = e;
    Parse();
}

SeisTrExpr::SeisTrExpr(const SeisTrExpr &src)
{
    Hdr = src.Hdr;
    Len = src.Len;
    Swp = src.Swp;          // ← 原来漏了
    L   = src.L;
    N   = src.N;
    V   = src.V;

    val_list.AddDefaultValues();
    val_list.AddAddress("_Hdr_", (double *)&Hdr);
    val_list.AddAddress("_Len_", (double *)&Len);
    val_list.AddAddress("_Swp_", (double *)&Swp);
    val_list.AddAddress("L", (double *)&L);
    val_list.AddAddress("N", (double *)&N);

    fun_list.AddDefaultFunctions();
    fun_list.Add(new TrcHdrFactory());

    SetValueList(&val_list);
    SetFunctionList(&fun_list);

    ExprStr = src.ExprStr;
    Parse();
}

SeisTrExpr &SeisTrExpr::operator=(const SeisTrExpr &src)
{
    if (this == &src)
        return *this;

    Hdr = src.Hdr;
    Len = src.Len;
    Swp = src.Swp;
    L   = src.L;
    N   = src.N;
    V   = src.V;

    ExprStr = src.ExprStr;
    Parse();

    return *this;
}

bool SeisTrExpr::Error()
{
    if (ExprStr == "")
        return true;
    else
        return error;
}

void SeisTrExpr::Parse()
{
    if (ExprStr == "") {
        error = true;
        return;
    }

    QByteArray es = ExprStr.toLocal8Bit();

    try {
        Expression::Parse(es.data());
        error = false;
    } catch (Exception &e) {
        error = true;
    }
}

void SeisTrExpr::setExprString(QString str)
{
    ExprStr = str;
    Parse();
}

QString SeisTrExpr::ExprString()
{
    return ExprStr;
}

double SeisTrExpr::Evaluate()
{
    if (error)
        return 0;

    double v = 0;

    try {
        v = Expression::Evaluate();
        // error=false;
    } catch (Exception &e) {
        error = true;
        v = 0;
    }

    return v;
}

void SeisTrExpr::setHdr(byte *buf, int len, int swp, long long n, long long l)
{
    Hdr = buf;
    Len = len;
    Swp = swp;
    L = l;
    N = n;
}

double TrcHdrNode::DoEvaluate()
{
    byte **a = (byte **)(m_expr->GetValueList()->GetAddress("_Hdr_"));

    byte *b = *a;
    int *i = *((int **)a);

    //    qDebug() << "i[0]="  << i[0];

    if (b == NULL)
        throw(NullPointerException(GetName()));

    int len = *((int *)(m_expr->GetValueList()->GetAddress("_Len_")));
    int swp = *((int *)(m_expr->GetValueList()->GetAddress("_Swp_")));

    //    qDebug() << "len=" << len;
    //    qDebug() << "swp=" << swp;

    int pos = m_nodes[0]->Evaluate();
    int frm = m_nodes[1]->Evaluate();

    //    qDebug() << "frm="   << frm;

    int l = frm % 10;

    //    qDebug() << "l="   << l;
    //    qDebug() << "pos=" << pos;
    //    qDebug() << "len=" << len;

    if (pos < 0 || pos + l > len)
        throw(MathException(GetName()));

    double v = iFmt(frm, b, pos, swp);

    //    qDebug() << "v=" << v;

    return v;
}
