#include <QApplication>

#include <QHeaderView>
#include <QPushButton>

#include "mxstringtable.h"

void MxStringTable::paintEvent(QPaintEvent *event)
{

    QPainter p(this->viewport());

    QRect r = event->rect();

    r.setRight(r.right() - 1);
    r.setBottom(r.bottom() - 1);

    int xo = r.left();
    int yo = r.top();

    p.drawLine(-xo, -yo, -xo + 100, -yo + 100);

    //    painter.begin(this);

    //    p.drawRect(r);
    /*

        int x;
        int y;


        QPainter p(this->viewport());

        p.drawLine(0,0,100,100);

        QRect boundingRect;


        int nc, nr;

        int firstVisualRow = qMax(verticalHeader()->visualIndexAt(0),0);
        int lastVisualRow =
       verticalHeader()->visualIndexAt(verticalHeader()->viewport()->height());

        int rh = rowHeight(0);

        // qDebug() << "rowHeight: " << rh;

        for(nr=0; nr<RowCount() && nr<100 ;nr++)
        {
            for(nc=0;nc<ColCount();nc++)
            {
                //y= rowViewportPosition(nr);
                x= columnViewportPosition(nc);
                y= rh*nr+1;
                p.drawRect(x, y, 6, 6);
                p.drawText(x,y,QString::number(nr));
            }
        }
    */

    /*
        Q_D(MxStringTable);
        // setup temp variables for the painting
        QStyleOptionViewItem option = d->viewOptionsV1();
        const QPoint offset = d->scrollDelayOffset;
        const bool showGrid = d->showGrid;
        const int gridSize = showGrid ? 1 : 0;
        const int gridHint = style()->styleHint(QStyle::SH_Table_GridLineColor,
    &option, this); const QColor gridColor = static_cast<QRgb>(gridHint); const
    QPen gridPen = QPen(gridColor, 0, d->gridStyle); const QHeaderView
    *verticalHeader = d->verticalHeader; const QHeaderView *horizontalHeader =
    d->horizontalHeader; const bool alternate = d->alternatingColors; const bool
    rightToLeft = isRightToLeft(); QPainter painter(d->viewport);
        // if there's nothing to do, clear the area and return
        if (horizontalHeader->count() == 0 || verticalHeader->count() == 0 ||
    !d->itemDelegate) return; const int x = horizontalHeader->length() -
    horizontalHeader->offset() - (rightToLeft ? 0 : 1); const int y =
    verticalHeader->length() - verticalHeader->offset() - 1;
        //firstVisualRow is the visual index of the first visible row.
    lastVisualRow is the visual index of the last visible Row.
        //same goes for ...VisualColumn
        int firstVisualRow = qMax(verticalHeader->visualIndexAt(0),0);
        int lastVisualRow =
    verticalHeader->visualIndexAt(verticalHeader->viewport()->height()); if
    (lastVisualRow == -1) lastVisualRow = d->model->rowCount(d->root) - 1; int
    firstVisualColumn = horizontalHeader->visualIndexAt(0); int lastVisualColumn
    = horizontalHeader->visualIndexAt(horizontalHeader->viewport()->width()); if
    (rightToLeft) qSwap(firstVisualColumn, lastVisualColumn); if
    (firstVisualColumn == -1) firstVisualColumn = 0; if (lastVisualColumn == -1)
            lastVisualColumn = horizontalHeader->count() - 1;
        QBitArray drawn((lastVisualRow - firstVisualRow + 1) * (lastVisualColumn
    - firstVisualColumn + 1)); const QRegion region =
    event->region().translated(offset); if (d->hasSpans()) {
            d->drawAndClipSpans(region, &painter, option, &drawn,
                                 firstVisualRow, lastVisualRow,
    firstVisualColumn, lastVisualColumn);
        }
        for (QRect dirtyArea : region) {
            dirtyArea.setBottom(qMin(dirtyArea.bottom(), int(y)));
            if (rightToLeft) {
                dirtyArea.setLeft(qMax(dirtyArea.left(), d->viewport->width() -
    int(x))); } else { dirtyArea.setRight(qMin(dirtyArea.right(), int(x)));
            }
            // dirtyArea may be invalid when the horizontal header is not
    stretched if (!dirtyArea.isValid()) continue;
            // get the horizontal start and end visual sections
            int left = horizontalHeader->visualIndexAt(dirtyArea.left());
            int right = horizontalHeader->visualIndexAt(dirtyArea.right());
            if (rightToLeft)
                qSwap(left, right);
            if (left == -1) left = 0;
            if (right == -1) right = horizontalHeader->count() - 1;
            // get the vertical start and end visual sections and if alternate
    color int bottom = verticalHeader->visualIndexAt(dirtyArea.bottom()); if
    (bottom == -1) bottom = verticalHeader->count() - 1; int top = 0; bool
    alternateBase = false; if (alternate && verticalHeader->sectionsHidden()) {
                const int verticalOffset = verticalHeader->offset();
                int row = verticalHeader->logicalIndex(top);
                for (int y = 0;
                     ((y += verticalHeader->sectionSize(top)) <= verticalOffset)
    && (top < bottom);
                     ++top) {
                    row = verticalHeader->logicalIndex(top);
                    if (alternate && !verticalHeader->isSectionHidden(row))
                        alternateBase = !alternateBase;
                }
            } else {
                top = verticalHeader->visualIndexAt(dirtyArea.top());
                alternateBase = (top & 1) && alternate;
            }
            if (top == -1 || top > bottom)
                continue;
            // Paint each row item
            for (int visualRowIndex = top; visualRowIndex <= bottom;
    ++visualRowIndex) { int row = verticalHeader->logicalIndex(visualRowIndex);
    if (verticalHeader->isSectionHidden(row)) continue; int rowY =
    rowViewportPosition(row); rowY += offset.y(); int rowh = rowHeight(row) -
    gridSize;
                // Paint each column item
                for (int visualColumnIndex = left; visualColumnIndex <= right;
    ++visualColumnIndex) { int currentBit = (visualRowIndex - firstVisualRow) *
    (lastVisualColumn - firstVisualColumn + 1)
                            + visualColumnIndex - firstVisualColumn;
                    if (currentBit < 0 || currentBit >= drawn.size() ||
    drawn.testBit(currentBit)) continue; drawn.setBit(currentBit); int col =
    horizontalHeader->logicalIndex(visualColumnIndex); if
    (horizontalHeader->isSectionHidden(col)) continue; int colp =
    columnViewportPosition(col); colp += offset.x(); int colw = columnWidth(col)
    - gridSize; const QModelIndex index = d->model->index(row, col, d->root); if
    (index.isValid()) { option.rect = QRect(colp + (showGrid && rightToLeft ? 1
    : 0), rowY, colw, rowh); if (alternate) { if (alternateBase) option.features
    |= QStyleOptionViewItem::Alternate; else option.features &=
    ~QStyleOptionViewItem::Alternate;
                        }
                        d->drawCell(&painter, option, index);
                    }
                }
                alternateBase = !alternateBase && alternate;
            }
            if (showGrid) {
                // Find the bottom right (the last rows/columns might be hidden)
                while
    (verticalHeader->isSectionHidden(verticalHeader->logicalIndex(bottom)))
    --bottom; QPen old = painter.pen(); painter.setPen(gridPen);
                // Paint each row
                for (int visualIndex = top; visualIndex <= bottom;
    ++visualIndex) { int row = verticalHeader->logicalIndex(visualIndex); if
    (verticalHeader->isSectionHidden(row)) continue; int rowY =
    rowViewportPosition(row); rowY += offset.y(); int rowh = rowHeight(row) -
    gridSize; painter.drawLine(dirtyArea.left(), rowY + rowh, dirtyArea.right(),
    rowY + rowh);
                }
                // Paint each column
                for (int h = left; h <= right; ++h) {
                    int col = horizontalHeader->logicalIndex(h);
                    if (horizontalHeader->isSectionHidden(col))
                        continue;
                    int colp = columnViewportPosition(col);
                    colp += offset.x();
                    if (!rightToLeft)
                        colp +=  columnWidth(col) - gridSize;
                    painter.drawLine(colp, dirtyArea.top(), colp,
    dirtyArea.bottom());
                }
                //draw the top & left grid lines if the headers are not visible.
                //We do update this line when subsequent scroll happen (see
    scrollContentsBy) if (horizontalHeader->isHidden() && top == 0) { const int
    row = verticalHeader->logicalIndex(top); if
    (!verticalHeader->isSectionHidden(row)) { const int rowY =
    rowViewportPosition(row) + offset.y(); if (rowY == dirtyArea.top())
                            painter.drawLine(dirtyArea.left(), rowY,
    dirtyArea.right(), rowY);
                    }
                }
                if (verticalHeader->isHidden() && left == 0) {
                    const int col = horizontalHeader->logicalIndex(left);
                    if (!horizontalHeader->isSectionHidden(col)) {
                        int colX = columnViewportPosition(col) + offset.x();
                        if (!isLeftToRight())
                            colX += columnWidth(left) - 1;
                        if (isLeftToRight() && colX == dirtyArea.left())
                            painter.drawLine(colX, dirtyArea.top(), colX,
    dirtyArea.bottom()); if (!isLeftToRight() && colX == dirtyArea.right())
                            painter.drawLine(colX, dirtyArea.top(), colX,
    dirtyArea.bottom());
                    }
                }
                painter.setPen(old);
            }
        }
    #ifndef QT_NO_DRAGANDDROP
        // Paint the dropIndicator
        d->paintDropIndicator(&painter);
    #endif
    */
}
void MxStringTableDelegate::paint(QPainter *painter,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    // такой себе костыль, чтобы побороть баг в старых версиях Qt
    QStyleOptionViewItemV4 itemOption = option;

    initStyleOption(&itemOption, index);

    if (_parent->SelColor().isValid() &&
        (option.state & QStyle::State_Selected)) {
        itemOption.state &= ~QStyle::State_Selected;
    }

    if (_parent->HideFoc() && (option.state & QStyle::State_HasFocus)) {
        itemOption.state &= ~QStyle::State_HasFocus;
    }

    QStyledItemDelegate::paint(painter, itemOption, index);

    if (_parent->SelColor().isValid() &&
        (option.state & QStyle::State_Selected)) {
        QBrush brush =
            QBrush(_parent->SelColor()); // viewOption.backgroundBrush;
        painter->fillRect(itemOption.rect, brush);
    }

    painter->drawRect(option.rect);
}

XHeaderView::XHeaderView(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
#if QT_VERSION >= 0x050000
    setSectionsClickable(true);
#else
    setClickable(true);
#endif
}

void XHeaderView::paintSection(QPainter *painter, const QRect &rect,
                               int logicalIndex) const
{
    QVariant bg =
        model()->headerData(logicalIndex, Qt::Horizontal, Qt::BackgroundRole);

    painter->save();

    QHeaderView::paintSection(painter, rect, logicalIndex);
    painter->restore();

    QColor bgc = qvariant_cast<QColor>(bg);

    if (bgc.isValid()) {
        painter->fillRect(rect, QBrush(bgc));
    }
}

MxStringTableModel::MxStringTableModel(MxStringTable *parent)
    : QAbstractTableModel((QObject *)parent)
{
    _tab = parent;
}

QVariant MxStringTableModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    QColor color;

    if (!index.isValid()) {
        return result;
    }

    int r = index.row();
    int c = index.column();

    // цвет фона ячейки
    if (role == Qt::BackgroundRole) {
        /*
                color = _tab->CellColor(r,c);
                if(color.isValid()) return color;

                color = _tab->_rows[r].color;
                if(color.isValid()) return color;
        */
    }

    // Если необходимо отобразить картинку - ловим роль Qt::DecorationRole
    if (role == Qt::DecorationRole) {
        //        QString imgname = _tab->ImgName(r,c);
        //        if(imgname!="") return _tab->ImgIcon(r,c); //QIcon(imgname);
    }

    // Если хотим отобразить CheckBox, используем роль Qt::CheckStateRole
    // если текущий вызов не относится к роли отображения, завершаем

    if (role == Qt::CheckStateRole) {

        // if(_tab->_cols[c].chkbx) return _tab->Check(r,c);
    }

    if (role == Qt::DisplayRole) {
        result = _tab->Cell(r, c);
        return result;
    }

    if (role == Qt::EditRole) {
        result = _tab->Cell(r, c);
        return result;
    }

    return result;
    // return QAbstractItemModel::data(index,role);
}

bool MxStringTableModel::setData(const QModelIndex &index,
                                 const QVariant &value, int role)
{
    //    qDebug() << "setData" << index << value.toString() << role;

    if (!index.isValid())
        return false;

    int r = index.row();
    int c = index.column();

    // нас интересует только роль, сообщающая об изменении
    if (role == Qt::EditRole) {
        /*
        if(_tab->_rows[r].cells[c].text != value.toString())
        {
           _tab->_rows[r].cells[c].text = value.toString();
           _tab->setTextChanged(true);
        }
        */
        emit(dataChanged(index, index));
        // данные приняты
        return true;
    }

    if (role == Qt::BackgroundRole) {
        /*
        QColor color = value.value<QColor>();
        if(_tab->_rows[r].cells[c].color != color)
        {
           _tab->_rows[r].cells[c].color = color;
        }
        */
        emit(dataChanged(index, index));
        // данные приняты
        return true;
    }

    if (role == Qt::DisplayRole) {
        /*
        _tab->_rows[r].cells[c].text = value.toString();
        */
        emit(dataChanged(index, index));
        // данные приняты
        return true;
    }

    if (role == Qt::CheckStateRole) {
        /*
        _tab->_rows[r].cells[c].check = value.toInt();
        */
        emit(dataChanged(index, index));
        // данные приняты
        return true;
    }

    return false;
}

bool MxStringTableModel::setHeaderData(int section, Qt::Orientation orientation,
                                       const QVariant &value, int role)
{
    if (role == Qt::BackgroundRole) {
        QColor color = value.value<QColor>();

        if (orientation == Qt::Horizontal) {
            //_tab->_cols[section].labbg=color;
        } else {
            //_tab->_rows[section].labbg=color;
        }

        emit headerDataChanged(orientation, section, section);
        // данные приняты
        return true;
    }

    if (role == Qt::DisplayRole) {
        QString label = value.value<QString>();

        if (orientation == Qt::Horizontal) {
            //_tab->_cols[section].label=label;
        } else {
            //_tab->_rows[section].label=label;
        }

        emit headerDataChanged(orientation, section, section);
        return true;
    }

    return false;
}

Qt::ItemFlags MxStringTableModel::flags(const QModelIndex &index) const
{
    int r = index.row();
    int c = index.column();

    if (_tab->CellEdit(r, c))
        return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;

    if (_tab->ColEdit(c))
        return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;

    return QAbstractTableModel::flags(index);
}

QVariant MxStringTableModel::headerData(int section,
                                        Qt::Orientation orientation,
                                        int role = Qt::DisplayRole) const
{
    if (role == Qt::DisplayRole) {
        // if     (orientation == Qt::Horizontal) return
        // _tab->_cols[section].label; else if(orientation == Qt::Vertical  )
        // return _tab->_rows[section].label;
    } else if (role == Qt::BackgroundColorRole) {
        // if     (orientation == Qt::Horizontal) return
        // _tab->_cols[section].labbg; else if(orientation == Qt::Vertical  )
        // return _tab->_rows[section].labbg;
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

int MxStringTableModel::rowCount(const QModelIndex &) const
{
    int v = _tab->RowCount();
    return v;
}

int MxStringTableModel::columnCount(const QModelIndex &) const
{
    int v = _tab->ColCount();
    return v;
}

MxStringTable::MxStringTable(QWidget *parent)
    : QTableView(parent)
{
    setStyleSheet("QHeaderView::section { background-color:lightgrey }");

    _nrows = 0;
    _ncols = 0;

    _changed = false;

    _hidefoc = false;

    _model = new MxStringTableModel(this);
    setModel(_model);

    setHorizontalHeader(new XHeaderView(this));

    verticalHeader()->setDefaultSectionSize(18);

    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    horizontalHeader()->setHighlightSections(false);

#if QT_VERSION >= 0x050000
    horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
#else
    horizontalHeader()->setResizeMode(QHeaderView::Fixed);
#endif

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setItemDelegate(new MxStringTableDelegate(this));

    connect(horizontalHeader(), SIGNAL(sectionPressed(int)), this,
            SLOT(hHeaderPressed(int)));
}

void MxStringTable::setIndexWidget(int r, int c, QWidget *w)
{
    QTableView::setIndexWidget(_model->index(r, c), w);
}

void MxStringTable::setRowCount(int v, bool reset)
{
    int n;

    _model->beginResetModel();

    _nrows = v;

    //_rows.resize(v);

    // for(n=0;n<_rows.count();n++) _rows[n].cells.resize(ColCount());

    if (reset)
        _model->endResetModel();

    update();
}

void MxStringTable::delRow(int row, bool reset)
{
    /*
        int i;
        int n = RowCount();

        if(row<0 || row>=n) return;

        _model->beginResetModel();

        QVector<MxStringTableRow> rows = _rows;

        _rows.resize(n-1);

        for(i=n=0;n<rows.count();n++)
        {
            if(n==row) continue;
            _rows[i].cells = rows[n].cells; i++;
        }

        if(reset) _model->endResetModel();
    */
}

void MxStringTable::setColCount(int v, bool reset)
{
    int n;

    _model->beginResetModel();

    _ncols = v;

    //    for(n=0;n<_rows.count();n++) _rows[n].cells.resize(v);
    //    _cols.resize(v); update();

    if (reset)
        _model->endResetModel();

    update();
}

QSize MxStringTable::sizeHint() const
{
    return QSize(200, 100);
}

QColor MxStringTable::SelColor()
{
    return _selcolor;
}

void MxStringTable::setSelColor(QColor v)
{
    _selcolor = v;
    update();
}

bool MxStringTable::HideFoc()
{
    return _hidefoc;
}

void MxStringTable::setHideFoc(bool v)
{
    _hidefoc = v;
    update();
}

int MxStringTable::RowCount()
{
    return _nrows;
    //    return _rows.count();
}

int MxStringTable::ColCount()
{
    return _ncols;
    //    return _cols.count();
}

QString MxStringTable::Cell(int r, int c)
{
    if (r < 0 || r >= RowCount())
        return "";
    if (c < 0 || c >= ColCount())
        return "";

    int rc = receivers(SIGNAL(cellDataRequest(int, int, QString &)));
    //  if(rc<1) return _rows[r].cells[c].text;
    if (rc < 1)
        return QString::number(r);

    QString v;

    emit cellDataRequest(r, c, v);

    return v;
}

void MxStringTable::setCell(int r, int c, QString v)
{
    if (r < 0 || r >= RowCount())
        return;
    if (c < 0 || c >= ColCount())
        return;

    _model->setData(_model->index(r, c), v, Qt::EditRole);
}

QString MxStringTable::ImgName(int r, int c)
{
    /*
        if(r<0 || r>=RowCount()) return "";
        if(c<0 || c>=ColCount()) return "";

        return _rows[r].cells[c].imgnm;
    */
    return "";
}

QIcon MxStringTable::ImgIcon(int r, int c)
{
    /*
        if(r<0 || r>=RowCount()) return QIcon();
        if(c<0 || c>=ColCount()) return QIcon();

        return _rows[r].cells[c].icon;
    */
    return QIcon();
}

void MxStringTable::setImgName(int r, int c, QString v)
{
    /*
        if(r<0 || r>=RowCount()) return;
        if(c<0 || c>=ColCount()) return;

        _rows[r].cells[c].imgnm = v;
        _rows[r].cells[c].icon = QIcon(v);
    */
}

void MxStringTable::setImgIcon(int r, int c, QIcon v)
{
    /*
        if(r<0 || r>=RowCount()) return;
        if(c<0 || c>=ColCount()) return;

        _rows[r].cells[c].imgnm = "__ICON__";
        _rows[r].cells[c].icon  = v;
    */
}

bool MxStringTable::CellEdit(int r, int c)
{
    /*
        if(r<0 || r>=RowCount()) return false;
        if(c<0 || c>=ColCount()) return false;

        return _rows[r].cells[c].edit;
    */
    return false;
}

void MxStringTable::setCellEdit(int r, int c, bool v)
{
    /*
        if(r<0 || r>=RowCount()) return;
        if(c<0 || c>=ColCount()) return;

        _rows[r].cells[c].edit = v;
    */
}

QColor MxStringTable::CellColor(int r, int c)
{
    /*
        if(r<0 || r>=RowCount()) return QColor();
        if(c<0 || c>=ColCount()) return QColor();

        return _rows[r].cells[c].color;
    */
    return Qt::white;
}

void MxStringTable::setCellColor(int r, int c, QColor v)
{
    /*
        if(r<0 || r>=RowCount()) return;
        if(c<0 || c>=ColCount()) return;

        _model->setData(_model->index(r,c),v,Qt::BackgroundRole);
    */
}

void MxStringTable::setColCheck(int c, int v)
{
    /*
        if(c<0 || c>=ColCount()) return;

        int r;

        for(r=0;r<RowCount();r++)
        {
            _model->setData(_model->index(r,c),v,Qt::CheckStateRole);
        }
    */
}

void MxStringTable::setCheck(int r, int c, int v)
{
    /*
        if(r<0 || r>=RowCount()) return;
        if(c<0 || c>=ColCount()) return;

        //_rows[r].cells[c].check = v; //_model->updateRow(r);
        _model->setData(_model->index(r,c),v,Qt::CheckStateRole);
    */
}

int MxStringTable::Check(int r, int c)
{
    /*
        if(r<0 || r>=RowCount()) return 4;
        if(c<0 || c>=ColCount()) return 4;

        return _rows[r].cells[c].check;
    */
    return 0;
}

void MxStringTable::setTag(int r, int c, int64_t v)
{
    /*
        if(r<0 || r>=RowCount()) return;
        if(c<0 || c>=ColCount()) return;

        _rows[r].cells[c].tag = v;
    */
}

int64_t MxStringTable::Tag(int r, int c)
{
    /*
        if(r<0 || r>=RowCount()) return -1;
        if(c<0 || c>=ColCount()) return -1;

        return _rows[r].cells[c].tag;
    */
    return 0;
}

void MxStringTable::setColor(QColor v)
{
    _color = v;
    update();
}

void MxStringTable::setTextc(QColor v)
{
    _textc = v;
    update();
}

void MxStringTable::setRowColor(int r, QColor v)
{
    /*
        if(r<0 || r>=RowCount()) return;

        int n;

        for(n=0;n<ColCount();n++) setCellColor(r,n,v);
    */
}

void MxStringTable::setRowTextc(int r, QColor v)
{
    /*
        if(r<0 || r>=RowCount()) return;

        _rows[r].textc=v;
    */
}

void MxStringTable::setRowLabel(int r, QString v)
{
    /*
        if(r<0 || r>=RowCount()) return;

        _model->setHeaderData(r,Qt::Vertical,v,Qt::DisplayRole);
    */
}

void MxStringTable::setColLabBg(int c, QColor v)
{
    /*
        // c < 0 - set all
        int n;

        if(c>=ColCount()) return;

        if(c<0)
        {
            for(n=0;n<ColCount();n++)
            {
                _model->setHeaderData(n,Qt::Horizontal,v,Qt::BackgroundRole);
            }
        }
        else
        {
           v.setAlpha(128);
            _model->setHeaderData(c,Qt::Horizontal,v,Qt::BackgroundRole);
        }
    */
}

void MxStringTable::setColChkbx(int c, bool v)
{
    /*
        if(c<0 || c>=ColCount()) return;

        _cols[c].chkbx=v;  update();
    */
}

bool MxStringTable::ColChkbx(int c)
{
    /*
        if(c<0 || c>=ColCount()) return false;

        return _cols[c].chkbx;
    */
    return false;
}

void MxStringTable::setColWidth(int c, int v)
{
    /*
        if(c<0 || c>=ColCount()) return;

        _cols[c].width=v;  update();
    */
}

int MxStringTable::ColWidth(int c)
{
    return 0;
    /*
        if(c<0 || c>=ColCount()) return false;

        return _cols[c].width;
    */
}

void MxStringTable::setColLabel(int c, QString v)
{
    /*
        if(c<0 || c>=ColCount()) return;

        _model->setHeaderData(c,Qt::Horizontal,v,Qt::DisplayRole);
    */
}

QString MxStringTable::ColLabel(int c)
{
    return "";
    /*
        if(c<0 || c>=ColCount()) return "";

        return _cols[c].label;
    */
}

void MxStringTable::resizeEvent(QResizeEvent *event)
{
    int w = event->size().width();

    if (ColCount() > 0) {
        int ws = w / ColCount();
        int ns;
        int ww = 0;
        int nn = 0;

        for (ns = 0; ns < ColCount(); ns++) {
            ws = ColWidth(ns);
            if (ws > 0)
                ww += ws;
            else
                nn++;
        }

        if (nn > 0) {
            ww = (w - ww) / nn;
            if (ww < 10)
                ww = 10;
        }

        for (ns = 0; ns < ColCount(); ns++) {
            ws = ColWidth(ns);
            if (ws < 1)
                ws = ww;
            horizontalHeader()->resizeSection(ns, ws);
        }
    }
    QTableView::resizeEvent(event);
}

void MxStringTable::keyPressEvent(QKeyEvent *event)
{
    QTableView::keyPressEvent(event);

    QItemSelectionModel *sel = selectionModel();

    if (!sel->hasSelection())
        return;

    int key = event->key();

    if (key == Qt::Key_Enter || key == Qt::Key_Return) {
        int r = currentIndex().row();
        int c = currentIndex().column();

        if (ColEdit(c)) {

            if (this->state() != QAbstractItemView::EditingState) {
                edit(currentIndex());
            }
        } else {
            emit rowEvent(r, 2);
        }
    } else if (key == Qt::Key_Home) {
        //      QApplication::beep();
        QModelIndex index = _model->index(0, 0, QModelIndex());

        sel->clearSelection();
        setCurrentIndex(index);
    } else if (key == Qt::Key_End) {
        QModelIndex index = _model->index(RowCount() - 1, 0, QModelIndex());

        sel->clearSelection();
        setCurrentIndex(index);
    }
}

void MxStringTable::mousePressEvent(QMouseEvent *event)
{
    QTableView::mousePressEvent(event);

    QItemSelectionModel *sel = selectionModel();

    if (!sel->hasSelection())
        return;

    QModelIndex idx = currentIndex();

    int c = idx.column();
    int r = idx.row();

    int ck = Check(r, c);

    if (ck)
        ck = 0;
    else
        ck = 2;

    if (ColChkbx(c)) {
        setCheck(r, c, ck);
        emit rowEvent(r, 4);
    }
}

void MxStringTable::mouseDoubleClickEvent(QMouseEvent *event)
{
    QTableView::mouseDoubleClickEvent(event);

    QItemSelectionModel *sel = selectionModel();

    if (!sel->hasSelection())
        return;

    QModelIndexList list = sel->selectedRows();

    if (list.count() < 1)
        return;

    int row = list.at(0).row();

    emit rowEvent(row, 2);
}

void MxStringTable::selectionChanged(const QItemSelection &selected,
                                     const QItemSelection &deselected)
{
    QAbstractItemView::selectionChanged(selected, deselected);

    QItemSelectionModel *sel = selectionModel();

    if (!sel->hasSelection())
        return;

    QModelIndexList list = sel->selectedRows();

    if (list.count() < 1)
        return;

    int row = list.at(0).row();

    emit rowEvent(row, 1);
}

void MxStringTable::setCurCell(int r, int c)
{
    QModelIndex index = _model->index(r, c, QModelIndex());
    setCurrentIndex(index);
}

int MxStringTable::curRow()
{
    QModelIndex index = currentIndex();
    return index.row();
}

int MxStringTable::curCol()
{
    QModelIndex index = currentIndex();
    return index.column();
}

void MxStringTable::editorDestroyed(QObject * /*editor*/)
{
}

void MxStringTable::setColEdit(int c, bool v)
{
    /*
        if(c<0 || c>=ColCount()) return;
        _cols[c].edit = v;
    */
}

bool MxStringTable::ColEdit(int c)
{
    return "";
    /*
        if(c<0 || c>=ColCount()) return false;
        return _cols[c].edit;
    */
}

void MxStringTable::setData(int r, int c, QString v)
{
    /*
        if(r<0 || r>=RowCount()) return;
        if(c<0 || c>=ColCount()) return;

        _rows[r].cells[c].data = v;
    */
}

QString MxStringTable::Data(int r, int c)
{
    return "";
    /*
        if(r<0 || r>=RowCount()) return QString();
        if(c<0 || c>=ColCount()) return QString();

        return _rows[r].cells[c].data;
    */
}

void MxStringTable::setTextChanged(bool v)
{
    _changed = v;

    if (v)
        emit textChanged();
}
