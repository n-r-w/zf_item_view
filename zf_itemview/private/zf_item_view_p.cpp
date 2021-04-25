#include "zf_utils.h"
#include "zf_item_view_p.h"
#include "zf_item_delegate.h"

#include <QPaintEvent>
#include <QPainter>

namespace zf
{
TableViewCorner::TableViewCorner(TableViewBase* table_view)
    : QWidget(table_view)
    , _table_view(table_view)
{
}

void TableViewCorner::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    if (_table_view->verticalHeader() != nullptr && _table_view->verticalHeader()->isVisible()) {
        QPainter painter(this);

        painter.setBrush(palette().brush(QPalette::Button));
        painter.fillRect(rect().adjusted(1, 1, 0, 0), palette().brush(QPalette::Button));

        painter.setPen(Utils::pen(palette().color(QPalette::Mid)));
        painter.drawLine(rect().right(), rect().top(), rect().right(), rect().bottom());
        painter.drawLine(rect().left(), rect().bottom(), rect().right(), rect().bottom());
    }
}

FrozenTableView::FrozenTableView(TableViewBase* base)
    : TableViewBase(base)
    , _base(base)
{
    init();
}

FrozenTableView::~FrozenTableView()
{
}

HeaderItem* FrozenTableView::rootHeaderItem(Qt::Orientation orientation) const
{
    return _base->rootHeaderItem(orientation);
}

int FrozenTableView::frozenGroupCount() const
{
    return _base->frozenGroupCount();
}

void FrozenTableView::setFrozenGroupCount(int count)
{
    _base->setFrozenGroupCount(count);
}

int FrozenTableView::sizeHintForRow(int row) const
{
    return _base->sizeHintForRow(row);
}

bool FrozenTableView::isAutoShrink() const
{
    return _base->isAutoShrink();
}

int FrozenTableView::shrinkMinimumRowCount() const
{
    return _base->shrinkMinimumRowCount();
}

int FrozenTableView::shrinkMaximumRowCount() const
{
    return _base->shrinkMaximumRowCount();
}

bool FrozenTableView::isAutoResizeRowsHeight() const
{
    return _base->isAutoResizeRowsHeight();
}

void FrozenTableView::setCellCheckColumn(int logical_index, bool visible, bool enabled, int level)
{
    _base->setCellCheckColumn(logical_index, visible, enabled, level);
}

QMap<int, bool> FrozenTableView::cellCheckColumns(int level) const
{
    return _base->cellCheckColumns(level);
}

bool FrozenTableView::isCellChecked(const QModelIndex& index) const
{
    return _base->isCellChecked(index);
}

void FrozenTableView::setCellChecked(const QModelIndex& index, bool checked)
{
    _base->setCellChecked(index, checked);
}

QModelIndexList FrozenTableView::checkedCells() const
{
    return _base->checkedCells();
}

void FrozenTableView::clearCheckedCells()
{
    _base->clearCheckedCells();
}

TableViewBase* FrozenTableView::base() const
{
    return _base;
}

int FrozenTableView::horizontalHeaderHeight() const
{
    return _base->horizontalHeaderHeight();
}

void FrozenTableView::onColumnResized(int column, int oldWidth, int newWidth)
{
    if (_request_resize)
        return;
    _request_resize = true;
    TableViewBase::onColumnResized(column, oldWidth, newWidth);
    _base->onColumnResized(column, oldWidth, newWidth);
    _request_resize = false;
}

void FrozenTableView::onColumnsDragging(int from_begin, int from_end, int to, int to_hidden, bool left, bool allow)
{
    TableViewBase::onColumnsDragging(from_begin, from_end, to, to_hidden, left, allow);
}

void FrozenTableView::onColumnsDragFinished()
{
    TableViewBase::onColumnsDragFinished();
}

void FrozenTableView::paintEvent(QPaintEvent* event)
{
    TableViewBase::paintEvent(event);
}

void FrozenTableView::init()
{
    Q_ASSERT(_base != nullptr);
    setFrameShape(QFrame::NoFrame);

    setItemDelegate(new ItemDelegate(this, _base, _base, this));

    // т.к. сигнал изначально определен в интерфейсе и у него нет Q_OBJECT, синтаксис Qt5 не подходит
    connect(_base, SIGNAL(sg_checkedCellChanged(QModelIndex, bool)), this, SIGNAL(sg_checkedCellChanged(QModelIndex, bool)));

    connect(verticalHeader(), &QHeaderView::sectionResized, this, [&]() { requestResizeRowsToContents(); });

    requestResizeRowsToContents();
}

} // namespace zf
