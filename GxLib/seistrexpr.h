#ifndef SeisTrExpr_H
#define SeisTrExpr_H

#include <QString>

#include <iostream>
#include <string>

#include "expreval.h"

typedef unsigned char byte;

using namespace ExprEval;
using std::string;

class TrcHdrNode : public FunctionNode
{
public:
    TrcHdrNode(Expression *expr)
        : FunctionNode(expr)
    {
        SetArgumentCount(2, 2, 0, 0);
    }

    double DoEvaluate();
};

class TrcHdrFactory : public FunctionFactory
{
public:
    std::string GetName() const
    {
        return "H";
    }

    FunctionNode *DoCreate(Expression *expr)
    {
        return new TrcHdrNode(expr);
    }
};

class SeisTrExpr : public Expression
{
protected:
    byte *Hdr; // Address of Header buffer
    int Len;   // Header Len;
    int Swp;   //
    double L;  // Number traces in file
    double N;  // Current trace index

    ValueList val_list;
    FunctionList fun_list;

    QString ExprStr;

    bool error;

    void Parse();

public:
    double V; // Prev value for undo

    SeisTrExpr(QString e = "");
    SeisTrExpr(const SeisTrExpr &src);

    SeisTrExpr &operator=(const SeisTrExpr &src);

    bool Error();

    double Evaluate();

    void setHdr(byte *buf, int len, int swp, long long n, long long l);

    QString ExprString();
    void setExprString(QString str);
};

#endif // SeisTrExpr_H
