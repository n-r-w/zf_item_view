#include "zf_table_view_base.h"

#include "zf_item_delegate.h"
#include "zf_itemview_header_item.h"
#include "zf_utils.h"

#include <QApplication>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <private/qtableview_p.h>
#include <private/qabstractslider_p.h>

#include "private/zf_item_view_p.h"
#include "private/zf_table_view_p.h"

namespace zf
{
TableViewBase::TableViewBase(QWidget* parent)
    : QTableView(parent)
{
    init();
}

TableViewBase::~TableViewBase()
{
}

void TableViewBase::setModel(QAbstractItemModel* model)
{
    if (model == this->model())
        return;

    if (this->model() != nullptr) {
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved, this, &TableViewBase::sl_rowsRemoved);
        disconnect(this->model(), &QAbstractItemModel::modelReset, this, &TableViewBase::sl_modelReset);
    }

    QTableView::setModel(model);

    if (model != nullptr) {        
        connect(model, &QAbstractItemModel::rowsRemoved, this, &TableViewBase::sl_rowsRemoved);
        connect(model, &QAbstractItemModel::modelReset, this, &TableViewBase::sl_modelReset);

        requestResizeRowsToContents();
    }
}

HeaderView* TableViewBase::header(Qt::Orientation orientation) const
{
    HeaderView* header;
    if (orientation == Qt::Horizontal) {
        header = qobject_cast<HeaderView*>(QTableView::horizontalHeader());
    } else {
        header = qobject_cast<HeaderView*>(QTableView::verticalHeader());
    }
    return header;
}

int TableViewBase::frozenSectionCount(bool visible_only) const
{
    if (frozenGroupCount() == 0)
        return 0;

    auto items = horizontalHeader()->rootItem()->childrenVisual(Qt::AscendingOrder);
    int count_top = 0;
    int count = 0;
    for (auto h : items) {
        if ((visible_only && h->isHidden()) || count_top >= frozenGroupCount())
            continue;
        count_top++;

        auto bottom = h->allBottomVisual(Qt::AscendingOrder);
        for (auto h_b : bottom) {
            if (visible_only && h_b->isHidden())
                continue;
            count++;
        }
    }

    return count;
}

void TableViewBase::setSortingEnabled(bool enable)
{
    horizontalHeader()->setAllowSorting(enable);
    QTableView::setSortingEnabled(enable);
}

bool TableViewBase::isConfigMenuEnabled() const
{
    return horizontalHeader()->isConfigMenuEnabled();
}

void TableViewBase::setConfigMenuEnabled(bool b)
{
    horizontalHeader()->setConfigMenuEnabled(b);
}

void TableViewBase::setUseHtml(bool b)
{
    if (auto d = qobject_cast<ItemDelegate*>(itemDelegate())) {
        d->setUseHtml(b);
        update();
    } else {
        Q_ASSERT(false);
    }
}

bool TableViewBase::isUseHtml() const
{
    if (auto d = qobject_cast<ItemDelegate*>(itemDelegate()))
        return d->isUseHtml();

    Q_ASSERT(false);
    return false;
}

int TableViewBase::horizontalHeaderHeight() const
{
    int height = qMax(horizontalHeader()->minimumHeight(), horizontalHeader()->sizeHint().height());
    return qMin(height, horizontalHeader()->maximumHeight());
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
QStyleOptionViewItem TableViewBase::viewOptions() const
{
    QStyleOptionViewItem opt;
    initViewItemOption(&opt);
    return opt;
}
#endif

void TableViewBase::updateGeometries()
{
    // Метод setViewportMargins предназначен для управления отступами. Мало того, в документации указано что это может быть полезно при
    // создании нестандартных боковиков у таблиц, что нам и надо. Однако криворукие дебилы из Qt плюют на свою же документацию и
    // принудительно устанавливают setViewportMargins внутри QTableView::updateGeometries. Если попытаться задать его после вызова
    // QTableView::updateGeometries, то начнется рекурсивный вызов. Поэтому мы вынуждены полностью клонировать данный метод и задавать
    // setViewportMargins самостоятельно.

    if (_geometry_recursion_block)
        return;
    _geometry_recursion_block = true;

    const int left_panel_width = leftPanelWidth();

    int width = 0;
    if (!verticalHeader()->isHidden()) {
        width = qMax(verticalHeader()->minimumWidth(), verticalHeader()->sizeHint().width());
        width = qMin(width, verticalHeader()->maximumWidth());
    }
    int height = 0;
    if (!horizontalHeader()->isHidden()) {
        height = horizontalHeaderHeight();
    }
    bool reverse = isRightToLeft();
    if (reverse)
        setViewportMargins(qMax(0, left_panel_width - 1), height, width, 0);
    else
        setViewportMargins(qMax(0, left_panel_width - 1) + width, height, 0, 0);

    // update headers

    QRect vg = viewport()->geometry();

    int verticalLeft = reverse ? vg.right() + 1 : (vg.left() - width);
    verticalHeader()->setGeometry(verticalLeft, vg.top(), width, vg.height());
    if (verticalHeader()->isHidden())
        QMetaObject::invokeMethod(verticalHeader(), "updateGeometries");

    int horizontalTop = vg.top() - height;
    horizontalHeader()->setGeometry(vg.left(), horizontalTop, vg.width(), height);
    if (horizontalHeader()->isHidden())
        QMetaObject::invokeMethod(horizontalHeader(), "updateGeometries");

#if QT_CONFIG(abstractbutton)
    // update cornerWidget
    QTableViewPrivate* private_ptr = reinterpret_cast<QTableViewPrivate*>(d_ptr.data());

    if (horizontalHeader()->isHidden() || verticalHeader()->isHidden()) {
        private_ptr->cornerWidget->setHidden(true);
    } else {
        private_ptr->cornerWidget->setHidden(false);
        private_ptr->cornerWidget->setGeometry(verticalLeft, horizontalTop, width, height);
    }

#endif

    // update scroll bars

    // ### move this block into the if
    QSize vsize = viewport()->size();
    QSize max = maximumViewportSize();
    const int horizontalLength = horizontalHeader()->length();
    const int verticalLength = verticalHeader()->length();
    if (max.width() >= horizontalLength && max.height() >= verticalLength)
        vsize = max;

    // horizontal scroll bar
    const int columnCount = horizontalHeader()->count();
    const int viewportWidth = vsize.width();
    int columnsInViewport = 0;
    for (int width = 0, column = columnCount - 1; column >= 0; --column) {
        int logical = horizontalHeader()->logicalIndex(column);
        if (!horizontalHeader()->isSectionHidden(logical)) {
            width += horizontalHeader()->sectionSize(logical);
            if (width > viewportWidth)
                break;
            ++columnsInViewport;
        }
    }
    columnsInViewport = qMax(columnsInViewport, 1); // there must be always at least 1 column

    if (horizontalScrollMode() == QAbstractItemView::ScrollPerItem) {
        const int visibleColumns = columnCount - horizontalHeader()->hiddenSectionCount();
        horizontalScrollBar()->setRange(0, visibleColumns - columnsInViewport);
        horizontalScrollBar()->setPageStep(columnsInViewport);
        if (columnsInViewport >= visibleColumns)
            horizontalHeader()->setOffset(0);
        horizontalScrollBar()->setSingleStep(1);
    } else { // ScrollPerPixel
        horizontalScrollBar()->setPageStep(vsize.width());
        horizontalScrollBar()->setRange(0, horizontalLength - vsize.width());

        //***
        //            horizontalScrollBar()->d_func()->itemviewChangeSingleStep(qMax(vsize.width() / (columnsInViewport + 1), 2));
        // У нас нет доступа к приватному классу ибо программисты Qt дают его только своим friend классам, поэтому получаем его через
        // анус. Кроме того методы приватного класса не экпортируются, поэтому копируем их код
        auto h_scroll = qobject_cast<TableViewScrollBar*>(horizontalScrollBar());
        Q_ASSERT(h_scroll != nullptr);

        int step = qMax(vsize.width() / (columnsInViewport + 1), 2);
        h_scroll->privatePtr()->singleStepFromItemView = step;
        if (h_scroll->privatePtr()->viewMayChangeSingleStep && h_scroll->privatePtr()->singleStep != step) {
            h_scroll->privatePtr()->singleStep = qAbs(step);
            h_scroll->privatePtr()->pageStep = qAbs(h_scroll->privatePtr()->pageStep);
            h_scroll->hack_sliderChange();
        }
        // *****
    }

    // vertical scroll bar
    const int rowCount = verticalHeader()->count();
    const int viewportHeight = vsize.height();
    int rowsInViewport = 0;
    for (int height = 0, row = rowCount - 1; row >= 0; --row) {
        int logical = verticalHeader()->logicalIndex(row);
        if (!verticalHeader()->isSectionHidden(logical)) {
            height += verticalHeader()->sectionSize(logical);
            if (height > viewportHeight)
                break;
            ++rowsInViewport;
        }
    }
    rowsInViewport = qMax(rowsInViewport, 1); // there must be always at least 1 row

    if (verticalScrollMode() == QAbstractItemView::ScrollPerItem) {
        const int visibleRows = rowCount - verticalHeader()->hiddenSectionCount();
        verticalScrollBar()->setRange(0, visibleRows - rowsInViewport);
        verticalScrollBar()->setPageStep(rowsInViewport);
        if (rowsInViewport >= visibleRows)
            verticalHeader()->setOffset(0);
        verticalScrollBar()->setSingleStep(1);
    } else { // ScrollPerPixel
        verticalScrollBar()->setPageStep(vsize.height());
        verticalScrollBar()->setRange(0, verticalLength - vsize.height());

        // *****
        // verticalScrollBar()->d_func()->itemviewChangeSingleStep(qMax(vsize.height() / (rowsInViewport + 1), 2));
        // У нас нет доступа к приватному классу ибо программисты Qt дают его только своим friend классам, поэтому получаем его через
        // анус. Кроме того методы приватного класса не экпортируются, поэтому копируем их код
        auto v_scroll = qobject_cast<TableViewScrollBar*>(verticalScrollBar());
        Q_ASSERT(v_scroll != nullptr);

        int step = qMax(vsize.height() / (rowsInViewport + 1), 2);
        v_scroll->privatePtr()->singleStepFromItemView = step;
        if (v_scroll->privatePtr()->viewMayChangeSingleStep && v_scroll->privatePtr()->singleStep != step) {
            v_scroll->privatePtr()->singleStep = qAbs(step);
            v_scroll->privatePtr()->pageStep = qAbs(v_scroll->privatePtr()->pageStep);
            v_scroll->hack_sliderChange();
        }
        // *****
    }

    // *****
    // verticalHeader()->d_func()->setScrollOffset(verticalScrollBar(), verticalScrollMode());
    // У нас нет доступа к приватному классу, поэтому копируем метод verticalHeader()->d_func()->setScrollOffset. На наше счастье в нем
    // использованы только public методы, поэтому можно обойтись без приватного класса
    if (verticalScrollMode() == QAbstractItemView::ScrollPerItem) {
        if (verticalScrollBar()->maximum() > 0 && verticalScrollBar()->value() == verticalScrollBar()->maximum())
            verticalHeader()->setOffsetToLastSection();
        else
            verticalHeader()->setOffsetToSectionPosition(verticalScrollBar()->value());
    } else {
        verticalHeader()->setOffset(verticalScrollBar()->value());
    }
    // *****

    _geometry_recursion_block = false;
    QAbstractItemView::updateGeometries();
}

bool TableViewBase::isReloading() const
{
    return _reloading > 0;
}

QSize TableViewBase::sizeHint() const
{
    QSize size = QTableView::sizeHint();
    if (!isAutoShrink())
        return size;

    int height = viewport()->geometry().top() + contentsMargins().top() + contentsMargins().bottom();
    if (horizontalScrollBar()->isVisible())
        height += horizontalScrollBar()->sizeHint().height();

    int row_count = model()->rowCount();
    int target_row_count = qMax(shrinkMinimumRowCount(), qMin(row_count, shrinkMaximumRowCount()));
    if (row_count < target_row_count)
        height += (target_row_count - row_count) * verticalHeader()->defaultSectionSize();
    else
        row_count = target_row_count;

    if (row_count > 0)
        height += rowViewportPosition(row_count - 1) + visualRect(model()->index(row_count - 1, 0)).height() + (showGrid() ? 1 : 0)
                  - rowViewportPosition(0);

    return {size.width(), height};
}

void TableViewBase::requestResizeRowsToContents()
{
    _resize_timer->start();
}

HeaderView* TableViewBase::horizontalHeader() const
{
    return header(Qt::Horizontal);
}

HeaderView* TableViewBase::verticalHeader() const
{
    return header(Qt::Vertical);
}

void TableViewBase::onColumnsDragging(int from_begin, int from_end, int to, int to_hidden, bool left, bool allow)
{
    Q_UNUSED(from_begin)
    Q_UNUSED(from_end)
    Q_UNUSED(to)
    Q_UNUSED(to_hidden)
    Q_UNUSED(left)
    Q_UNUSED(allow)

    viewport()->update();
}

void TableViewBase::onColumnsDragFinished()
{
    viewport()->update();
}

void TableViewBase::onColumnResized(int column, int oldWidth, int newWidth)
{
    Q_UNUSED(column)
    Q_UNUSED(oldWidth)
    Q_UNUSED(newWidth)
    requestResizeRowsToContents();
}

void TableViewBase::delegateGetCheckInfo(QAbstractItemView* item_view, const QModelIndex& index, bool& show, bool& checked) const
{
    Q_UNUSED(item_view)
    show = cellCheckColumns().contains(index.column());
    if (!show) {
        checked = false;
        return;
    }
    checked = isCellChecked(index);
}

void TableViewBase::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.save();
    QTableView::paintEvent(event);
    painter.restore();

    HeaderView* header = this->horizontalHeader();

    int first_visible_visual = header->visualIndex(header->rootItem()->firstVisibleSection());
    if (first_visible_visual >= 0) {
        QStyleOptionViewItem option = viewOptions();
        QColor grid_color = static_cast<QRgb>(style()->styleHint(QStyle::SH_Table_GridLineColor, &option, this));

        int last_visible_visual = header->visualIndex(header->rootItem()->lastVisibleSection());

        // отрисовка вертикальных линий по всей таблице
        painter.save();

        int row_count = model()->rowCount();
        int bottom_row = rowAt(height());
        QPair<int, int> bottom_interval; // интервал между последней видимой строкой и нижней границей виджета
        if (bottom_row < 0) {
            // внизу таблицы есть свободное пространство, по которому надо дорисовать вертикальные линии
            if (row_count == 0) {
                bottom_interval = QPair<int, int>(0, viewport()->rect().bottom());

            } else {
                int last_item_pos
                    = visualRect(model()->index(row_count - 1, header->logicalIndex(first_visible_visual))).top();
                if (last_item_pos >= 0)
                    bottom_interval = QPair<int, int>(last_item_pos, viewport()->rect().bottom());

                bottom_row = row_count - 1;
            }
        }

        for (int vusual_col = first_visible_visual; vusual_col <= last_visible_visual; vusual_col++) {
            int logical_col = header->logicalIndex(vusual_col);

            if (header->isSectionHidden(logical_col))
                continue;

            int left = header->sectionViewportPosition(logical_col);

            QList<QPair<int, int>> intervals;
            if (bottom_interval.first >= 0)
                intervals << bottom_interval;

            // исключение интервалов для span
            int top_row = rowAt(0);
            if (top_row >= 0) {
                // интервалы по строкам с данными
                for (int row = top_row; row <= bottom_row; row++) {
                    int span = columnSpan(row, logical_col);
                    if (span > 1)
                        continue;

                    QRect item_rect = visualRect(model()->index(row, logical_col));
                    int bottom_pos;
                    if (row < row_count - 1)
                        bottom_pos = visualRect(model()->index(row + 1, logical_col)).top();
                    else
                        bottom_pos = item_rect.bottom();

                    intervals << QPair<int, int>(item_rect.top(), bottom_pos);
                }
            }

            for (auto i = intervals.constBegin(); i != intervals.constEnd(); ++i) {
                if (vusual_col > first_visible_visual) {
                    painter.setPen(Utils::pen(grid_color));
                    painter.drawLine(left - 1, i->first, left - 1, i->second);
                }

                if (vusual_col < last_visible_visual
                    || (!header->stretchLastSection() && vusual_col <= last_visible_visual)) {
                    int size = header->sectionSize(logical_col);
                    painter.setPen(Utils::pen(grid_color));
                    painter.drawLine(left + size - 1, i->first, left + size - 1, i->second);
                }
            }
        }
        painter.restore();
    }

    // отображение куда будет вставлен перетаскиваемый заголовок
    Utils::paintHeaderDragHandle(this, header);
}

void TableViewBase::mousePressEvent(QMouseEvent* e)
{
    QModelIndex idx = indexAt(e->pos());

    if (idx.isValid()) {
        // было ли нажатие на чекбокс внутри ячейки
        ItemDelegate* delegate = qobject_cast<ItemDelegate*>(itemDelegate());
        if (delegate != nullptr) {
            if (cellCheckColumns().value(idx.column())) {
                QRect check_rect = delegate->checkBoxRect(idx, false);
                if (check_rect.contains(e->pos())) {
                    setCellChecked(idx, !isCellChecked(idx));
                }
            }
        }
    }

    QTableView::mousePressEvent(e);
}

void TableViewBase::mouseDoubleClickEvent(QMouseEvent* e)
{
    QModelIndex idx = indexAt(e->pos());

    // игнорируем нажатия в область чекбокса
    if (idx.isValid()) {
        ItemDelegate* delegate = qobject_cast<ItemDelegate*>(itemDelegate());
        if (delegate != nullptr) {
            if (delegate->checkBoxRect(idx, true).contains(e->pos())) {
                e->ignore();
                return;
            }
        }
    }

    QTableView::mouseDoubleClickEvent(e);
}

void TableViewBase::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QTableView::currentChanged(current, previous);

    // Обновление ячеек с общим span - иначе будут глюки с отрисовкой фона текущей строки
    if (previous.isValid() && rowSpan(previous.row(), previous.column()) > 1)
        update();
}

bool TableViewBase::event(QEvent* event)
{
    bool res = QTableView::event(event);

    if (event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent* e = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (e->propertyName() == QStringLiteral("sortingEnabled") && header(Qt::Horizontal) != nullptr) {
            header(Qt::Horizontal)->setAllowSorting(isSortingEnabled());
        }
    }

    return res;
}

bool TableViewBase::viewportEvent(QEvent* event)
{
    switch (event->type()) {
        case QEvent::ToolTip: {
            if (!toolTip().isEmpty())
                // вызываем метод пропуская QTableView, т.к. он подавляет тултипы самого виджета в пользу делегата
                return QAbstractScrollArea::viewportEvent(event);
            break;
        }
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove: {
            // обновления ячеек при переходе с одной на другую
            QHoverEvent* e = static_cast<QHoverEvent*>(event);
            QModelIndex index = indexAt(e->pos());

            if (index != _hover_index) {
                if (_hover_index.isValid()) {
                    update(_hover_index);
                }

                _hover_index = index;
            }

            if (index.isValid())
                update(index);

            break;
        }
        default:
            break;
    }

    return QTableView::viewportEvent(event);
}

bool TableViewBase::eventFilter(QObject* object, QEvent* event)
{
    return QTableView::eventFilter(object, event);
}

void TableViewBase::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
    QTableView::dataChanged(topLeft, bottomRight, roles);

    if (isAutoResizeRowsHeight())
        requestResizeRowsToContents();
}

void TableViewBase::sl_columnsDragging(int from_begin, int from_end, int to, int to_hidden, bool left, bool allow)
{
    onColumnsDragging(from_begin, from_end, to, to_hidden, left, allow);
}

void TableViewBase::sl_columnsDragFinished()
{
    onColumnsDragFinished();
}

void TableViewBase::sl_beforeLoadDataFromRootHeader()
{
    _saved_index = currentIndex();
    _reloading++;
}

void TableViewBase::sl_afterLoadDataFromRootHeader()
{
    if (_saved_index.isValid())
        setCurrentIndex(_saved_index);
    _reloading--;
}

void TableViewBase::sl_columnResized(int column, int oldWidth, int newWidth)
{
    onColumnResized(column, oldWidth, newWidth);
}

void TableViewBase::rowsInserted(const QModelIndex& parent, int first, int last)
{
    QTableView::rowsInserted(parent, first, last);

    if (isAutoShrink())
        updateGeometry();

    if (isAutoResizeRowsHeight())
        requestResizeRowsToContents();
}

int TableViewBase::leftPanelWidth() const
{
    return 0;
}

void TableViewBase::sl_rowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    if (isAutoShrink())
        updateGeometry();

    requestResizeRowsToContents();
}

void TableViewBase::sl_modelReset()
{
    requestResizeRowsToContents();
}

void TableViewBase::sl_resizeToContents()
{
    updateGeometries();

    bool last_need_auto = _last_need_row_auto_height;
    bool need_auto = false;
    if (model() != nullptr)
        need_auto = isNeedRowsAutoHeight(model()->rowCount(), model()->columnCount()) && isAutoResizeRowsHeight();
    _last_need_row_auto_height = need_auto;

    if (model() != nullptr && model()->rowCount() > 0 && (isAutoResizeRowsHeight() || last_need_auto != need_auto)) {
        emit sg_beforeResizeRowsToContent();

        if (need_auto) {
            resizeRowsToContents();
        } else {
            verticalHeader()->resizeSections(QHeaderView::Fixed);
            for (int i = 0; i < verticalHeader()->count(); i++) {
                verticalHeader()->resizeSection(i, verticalHeader()->defaultSectionSize());
            }
        }
        updateGeometry();

        emit sg_afterResizeRowsToContent();
    }
}

void TableViewBase::init()
{
    _geometry_recursion_block = true; // чтобы исключить загадочные глюки

    setWordWrap(true);
    // без этого не получается отследить положение мыши над viewport
    viewport()->setAttribute(Qt::WA_Hover);

    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);

    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectRows);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setHorizontalScrollMode(ScrollPerPixel);
    setVerticalScrollMode(ScrollPerPixel);
    setTextElideMode(Qt::ElideNone);
    setEditTriggers(DoubleClicked | EditKeyPressed | AnyKeyPressed);

    setHorizontalHeader(new HeaderView(Qt::Horizontal, this));
    if (isSortingEnabled())
        horizontalHeader()->setAllowSorting(true);

    connect(horizontalHeader(), &HeaderView::sg_columnsDragging, this, &TableViewBase::sl_columnsDragging);
    connect(horizontalHeader(), &HeaderView::sg_columnsDragFinished, this, &TableViewBase::sl_columnsDragFinished);    
    connect(horizontalHeader(), &HeaderView::sg_beforeLoadDataFromRootHeader, this,
        &TableViewBase::sl_beforeLoadDataFromRootHeader);
    connect(horizontalHeader(), &HeaderView::sg_afterLoadDataFromRootHeader, this,
        &TableViewBase::sl_afterLoadDataFromRootHeader);

    setVerticalHeader(new HeaderView(Qt::Vertical, this));

    // задаем свои скролбары для доступа к их протектед методам
    setVerticalScrollBar(new TableViewScrollBar(Qt::Orientation::Vertical, this));
    setHorizontalScrollBar(new TableViewScrollBar(Qt::Orientation::Horizontal, this));

    // иначе не будет обновляться отрисовка фильтра и т.п. глюки
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [&]() { horizontalHeader()->viewport()->update(); });

    // т.к. заголовок меняет высоту при смене ширины колонки, то надо обновлять геометрию таблицы
    connect(horizontalHeader(), &QHeaderView::sectionResized, this, &TableViewBase::sl_columnResized);
    _resize_timer = new QTimer(this);
    _resize_timer->setSingleShot(true);
    _resize_timer->setInterval(1);
    connect(_resize_timer, &QTimer::timeout, this, &TableViewBase::sl_resizeToContents);

    _geometry_recursion_block = false;
}

bool TableViewBase::isNeedRowsAutoHeight(int row_count, int column_count)
{
    return quint64(row_count) * quint64(column_count) < 5000;
}

} // namespace zf
