#ifndef MxStringTable_H
#define MxStringTable_H

#include <stdint.h>

#include <QAbstractTableModel>
#include <QColor>

#include <QHeaderView>
#include <QPainter>
#include <QResizeEvent>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVector>

class MxStringTable;

class XHeaderView : public QHeaderView
{
private:
    virtual void paintSection(QPainter *painter, const QRect &rect,
                              int logicalIndex) const;

public:
    XHeaderView(QWidget *parent = 0);
    ~XHeaderView()
    {
    }
};

class MxStringTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT
private:
    MxStringTable *_parent;

public:
    explicit MxStringTableDelegate(MxStringTable *parent = 0)
        : QStyledItemDelegate((QObject *)parent)
    {
        _parent = parent;
    }

    // чтобы управлять отрисовкой нужно переопределить этот метод
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
};

class MxStringTableCell
{
public:
    QString text;
    QString imgnm;
    QIcon icon;
    int check;
    int64_t tag;
    QColor color; // background color
    QColor textc; // text color
    bool edit;    // editable
    QString data;

    explicit MxStringTableCell()
    {
        check = 0;
        tag = 0;
        edit = false;
    }
};

class MxStringTableRow
{
public:
    bool sel;
    QColor color;
    QColor textc; // text color
    QString label;
    QColor labbg;

    QVector<MxStringTableCell> cells;

    explicit MxStringTableRow()
    {
        sel = false;
    }
};

class MxStringTableCol
{
public:
    bool chkbx; // show check
    QString label;
    QColor labbg;
    QString imgnm;
    int width;
    bool edit;

    explicit MxStringTableCol()
    {
        chkbx = false;
        width = 0;
        edit = false;
    }
};

class MxStringTable;
class MxStringTableModel;

class MxStringTableModel : public QAbstractTableModel
{
    Q_OBJECT

    friend class MxStringTable;

protected:
    MxStringTable *_tab;

public:
    explicit MxStringTableModel(MxStringTable *parent);

    virtual int rowCount(const QModelIndex &) const;
    virtual int columnCount(const QModelIndex &) const;

    virtual QVariant data(const QModelIndex &, int) const;

    virtual bool setData(const QModelIndex &index, const QVariant &value,
                         int role);
    virtual bool setHeaderData(int section, Qt::Orientation orientation,
                               const QVariant &value, int role);
    virtual Qt::ItemFlags flags(const QModelIndex & /*index*/) const;

    virtual QVariant headerData(int section, Qt::Orientation orientation,
                                int role) const;

signals:

public slots:
};

class MxStringTable : public QTableView
{
    Q_OBJECT

    friend class MxStringTableModel;

protected:
    //  QVector<MxStringTableRow> _rows;
    //  QVector<MxStringTableCol> _cols;

    int _nrows;
    int _ncols;

    QColor _color; // background color
    QColor _textc; // text color

    QColor _selcolor;
    bool _hidefoc;

    bool _changed;

    virtual void resizeEvent(QResizeEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);

    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);

    virtual void editorDestroyed(QObject *editor);

    virtual void selectionChanged(const QItemSelection &selected,
                                  const QItemSelection &deselected);

    virtual QSize sizeHint() const;

    void setColumnWidth(int c, int w)
    {
        QTableView::setColumnWidth(c, w);
    }

    void paintEvent(QPaintEvent *event);
    /*
    {
        QTableView::paintEvent(event); // draw original content

           QPainter p(this->viewport());
           p.drawRect(0, 0, 20, 20);
    }
    */

public:
    MxStringTableModel *_model;

    explicit MxStringTable(QWidget *parent = 0);

    void setIndexWidget(int r, int c, QWidget *w);

    void delRow(int row, bool reset = true);

    QColor SelColor();
    void setSelColor(QColor v);

    bool HideFoc();
    void setHideFoc(bool v);

    int RowCount();
    void setRowCount(int v, bool reset = true);

    int ColCount();
    void setColCount(int v, bool reset = true);

    QColor RowColor(int r);
    void setRowColor(int r, QColor v);

    QColor RowTextc(int r);
    void setRowTextc(int r, QColor v);

    QString RowLabel(int r);
    void setRowLabel(int r, QString v);

    bool ColChkbx(int c);
    void setColChkbx(int c, bool);

    void setColCheck(int c, int v);

    void setColEdit(int c, bool v);
    bool ColEdit(int c);

    bool CellEdit(int r, int c);
    void setCellEdit(int r, int c, bool v);

    int ColWidth(int c);
    void setColWidth(int c, int);

    QString ColLabel(int c);
    void setColLabel(int c, QString);
    void setColLabBg(int c, QColor v = QColor());

    QColor Color()
    {
        return _color;
    }
    void setColor(QColor);

    QColor Textc()
    {
        return _textc;
    }
    void setTextc(QColor);

    QString Cell(int r, int c);
    void setCell(int r, int c, QString v);

    QString ImgName(int r, int c);
    void setImgName(int r, int c, QString v);

    QIcon ImgIcon(int r, int c);
    void setImgIcon(int r, int c, QIcon);

    int Check(int r, int c);
    void setCheck(int r, int c, int v);

    int64_t Tag(int r, int c);
    void setTag(int r, int c, int64_t v);

    QString Data(int r, int c);
    void setData(int r, int c, QString v);

    QColor CellColor(int r, int c);
    void setCellColor(int r, int c, QColor v);

    void setCurCell(int r, int c = 0);

    int curRow();
    int curCol();

    void setTextChanged(bool v);

signals:

    void hHeaderEvent(int col);

    void rowEvent(int row, int mode);

    void cellDataRequest(int row, int col, QString &data);

    void textChanged();

public slots:

    void hHeaderPressed(int logicalIndex)
    {
        emit hHeaderEvent(logicalIndex);
    }
};

#endif // MxStringTable_H
