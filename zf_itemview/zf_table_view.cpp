#include "zf_table_view.h"
#include "texthyphenationformatter.h"
#include "zf_utils.h"
#include "zf_header_view.h"
#include "zf_item_delegate.h"
#include "zf_itemview_header_item.h"
#include "zf_itemview_header_model.h"

#include <private/qtableview_p.h>
#include <private/qabstractslider_p.h>

#include "private/zf_item_view_p.h"
#include "private/zf_table_view_p.h"

#include <QApplication>
#include <QDebug>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QLabel>
#include <QMenu>
#include <QPaintEvent>
#include <QStylePainter>
#include <QScrollBar>
#include <QBuffer>

#define UPDATE_FROZEN_PROPERTIES_EVENT (QEvent::User + 10000)

namespace zf
{
TableViewScrollBar::TableViewScrollBar(Qt::Orientation orientation, QWidget* parent)
    : QScrollBar(orientation, parent)
{
}

void TableViewScrollBar::hack_sliderChange()
{
    sliderChange(QAbstractSlider::SliderStepsChange);
}

QAbstractSliderPrivate* TableViewScrollBar::privatePtr() const
{
    return reinterpret_cast<QAbstractSliderPrivate*>(d_ptr.data());
}

CheckBoxPanel::CheckBoxPanel(TableView* parent)
    : QWidget(parent)
    , _view(parent)
{
    setMouseTracking(true);
}

void CheckBoxPanel::paintEvent(QPaintEvent* e)
{
    QWidget::paintEvent(e);

    if (_view->model() == nullptr)
        return;

    QStylePainter painter(this);

    QStyleOptionButton check_option;
    auto rects = checkboxRects();

    for (auto& rect : qAsConst(rects)) {
        if (rect.first.isNull())
            continue;

        int row = _view->rowAt(rect.first.center().y() - offset());
        Q_ASSERT(row >= 0);
        row = Utils::getTopSourceIndex(_view->model()->index(row, 0)).row();

        // чекбокс
        bool checked = _view->isRowChecked(row);
        painter.save();
        check_option.rect = rect.second;
        check_option.state = QStyle::State_Enabled;
        check_option.state |= checked ? QStyle::State_On : QStyle::State_Off;

        QPoint mouse_pos = mapFromGlobal(QCursor::pos());
        check_option.state.setFlag(QStyle::State_MouseOver, check_option.rect.contains(mouse_pos));

        painter.drawControl(QStyle::ControlElement::CE_CheckBox, check_option);
        painter.restore();

        // линии
        painter.save();
        QStyleOptionViewItem grid_option;
        grid_option.initFrom(_view);

        const int gridHint = style()->styleHint(QStyle::SH_Table_GridLineColor, &grid_option, this);
        const QColor gridColor = static_cast<QRgb>(gridHint);
        const QPen gridPen = QPen(gridColor, 0, _view->gridStyle());
        painter.setPen(gridPen);

        if (_view->showGrid())
            painter.drawLine(1, rect.first.bottom(), width() - 1, rect.first.bottom());

        painter.setPen(Utils::uiLineColor(true));
        painter.drawLine(width() - 1, rect.first.top() - 1, width() - 1, rect.first.bottom());
        painter.restore();
    }

    // заголовок
    if (_view->horizontalHeader()->isVisible()) {
        painter.save();
        QRect header_rect
            = QRect(1, _view->horizontalHeader()->geometry().top(), width() - 1, _view->horizontalHeader()->geometry().bottom());
        painter.fillRect(header_rect, palette().brush(QPalette::Button));

        painter.setPen(Utils::uiLineColor(true));
        painter.drawLine(header_rect.right(), header_rect.top(), header_rect.right(), header_rect.bottom());
        painter.drawLine(header_rect.left(), header_rect.bottom(), header_rect.right(), header_rect.bottom());

        painter.restore();

        // чекбокс в заголовке
        painter.save();
        bool checked = _view->isAllRowsChecked();
        check_option.rect = headerCheckboxRect().second;

        check_option.state = QStyle::State_Enabled;
        check_option.state |= checked ? QStyle::State_On : QStyle::State_Off;
        QPoint mouse_pos = mapFromGlobal(QCursor::pos());
        check_option.state.setFlag(QStyle::State_MouseOver, check_option.rect.contains(mouse_pos));

        painter.drawControl(QStyle::ControlElement::CE_CheckBox, check_option);
        painter.restore();
    }
}

void CheckBoxPanel::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() == Qt::LeftButton && !_ignore_group_check) {
        int row = cursorRow(e->pos());
        if (row >= 0)
            _view->checkRow(row, _is_group_checked);
    }

    update();
}

void CheckBoxPanel::mousePressEvent(QMouseEvent* e)
{
    if (_view->model() == nullptr)
        return;

    if (headerCheckboxRect().second.contains(e->pos())) {
        _view->checkAllRows(!_view->isAllRowsChecked());
        _ignore_group_check = true;
        return;
    }

    int row = cursorRow(e->pos());
    if (row >= 0) {
        bool checked = !_view->isRowChecked(row);
        _view->checkRow(row, checked);
        _is_group_checked = checked;
        _ignore_group_check = false;

    } else {
        _ignore_group_check = true;
    }
}

void CheckBoxPanel::mouseReleaseEvent(QMouseEvent* e)
{
    Q_UNUSED(e)
    _ignore_group_check = false;
}

int CheckBoxPanel::width()
{
    return checkboxSize().width() + 10;
}

int CheckBoxPanel::offset() const
{
    return _view->viewport()->geometry().top() + 1;
}

QList<QPair<QRect, QRect>> CheckBoxPanel::checkboxRects() const
{
    int first_col = _view->horizontalHeader()->logicalIndexAt(0);
    int offset = this->offset();
    QSize check_size = checkboxSize();
    QList<QPair<QRect, QRect>> res;

    int first_visual_row = _view->rowAt(0);
    bool found = false;

    for (int visual_row = first_visual_row; true; visual_row++) {
        QRect cell_rect = _view->visualRect(_view->model()->index(visual_row, first_col));

        if (cell_rect.isNull())
            break;

        QPair<QRect, QRect> rects;

        cell_rect.adjust(0, offset, 0, offset);
        rects.first = cell_rect;

        cell_rect.setLeft((width() - check_size.width()) / 2);
        cell_rect.setRight(cell_rect.left() + check_size.width());

        int cell_height = cell_rect.height();
        cell_rect.setTop(cell_rect.top() + (cell_height - check_size.height()) / 2);
        cell_rect.setBottom(cell_rect.top() + check_size.height());
        rects.second = cell_rect;

        if (_view->viewport()->geometry().top() > rects.first.bottom() || _view->viewport()->geometry().bottom() < rects.first.top()) {
            // возвращаем только видимые части
            if (found)
                break;
            continue;
        }

        res << rects;
        found = true;
    }

    return res;
}

QPair<QRect, QRect> CheckBoxPanel::headerCheckboxRect() const
{
    if (!_view->horizontalHeader()->isVisible())
        return {};

    QPair<QRect, QRect> res;

    QStyleOptionHeader opt_header_section;

    QRect rect = QRect(0, _view->horizontalHeader()->geometry().top(), width(), _view->horizontalHeader()->geometry().bottom());
    res.first = rect;

    QSize check_size = checkboxSize();
    rect.setLeft((width() - check_size.width()) / 2);
    rect.setRight(rect.left() + check_size.width());
    rect.setTop((width() - check_size.width()) / 2);

    int cell_height = rect.height();
    rect.setTop(rect.top() + (cell_height - check_size.height()) / 2);
    rect.setBottom(rect.top() + check_size.height());
    res.second = rect;

    return res;
}

QSize CheckBoxPanel::checkboxSize()
{
    QStyleOptionButton check_option;
    return (qApp->style()->sizeFromContents(QStyle::CT_CheckBox, &check_option, QSize()).expandedTo(QApplication::globalStrut()));
}

int CheckBoxPanel::cursorRow(const QPoint& c) const
{    
    auto rects = checkboxRects();
    int shift = 0;
    for (auto& rect : qAsConst(rects)) {
        if (rect.second.contains(c)) {
            return Utils::getTopSourceIndex(_view->indexAt({0, c.y() - offset()})).row();
        }

        shift++;
    }
    return -1;
}

QWidget* TableView::cornerWidget() const
{
    return _corner_widget;
}


void TableView::setCornerWidget(QWidget* widget)
{
    if (_corner_widget != nullptr)
        delete _corner_widget;

    _corner_widget = widget;

    if (_corner_widget != nullptr) {
        _corner_widget->setParent(this);
        _corner_widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    updateCornerWidget();
}

void TableView::setCornerText(
    const QString& text, Qt::Alignment alignment, int margin_left, int margin_right, int margin_top, int margin_bottom)
{
    _corner_text = text;
    _corner_alignment = alignment;
    _corner_margin_left = margin_left;
    _corner_margin_right = margin_right;
    _corner_margin_top = margin_top;
    _corner_margin_bottom = margin_bottom;

    updateCornerWidget();
}

void TableView::setCornerTextOptions(bool bold, const QColor& text_color)
{
    _corner_bold = bold;
    _corner_text_color = text_color;

    updateCornerWidget();
}

int TableView::frozenGroupCount() const
{
    return _frozen_group_count;
}

void TableView::setFrozenGroupCount(int count)
{
    Q_ASSERT(!rootHeaderItem(Qt::Horizontal)->isUpdating());

    if (count < 0 || count >= horizontalRootHeaderItem()->childrenVisual(Qt::AscendingOrder, true).count())
        count = 0;

    if (_frozen_group_count == count)
        return;

    _frozen_group_count = count;
    updateFrozenCount();
}

void TableView::setUseHtml(bool b)
{
    TableViewBase::setUseHtml(b);

    if (_frozen_table_view != nullptr)
        _frozen_table_view->setUseHtml(b);
}

void TableView::requestResizeRowsToContents()
{
    TableViewBase::requestResizeRowsToContents();

    if (_frozen_table_view != nullptr)
        _frozen_table_view->requestResizeRowsToContents();
}

Error TableView::serialize(QIODevice* device) const
{
    return Utils::saveHeader(device, horizontalRootHeaderItem(), frozenGroupCount());
}

Error TableView::serialize(QByteArray& ba) const
{
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    return serialize(&buffer);
}

Error TableView::deserialize(QIODevice* device)
{
    int frozen_count;
    Error error = Utils::loadHeader(device, horizontalRootHeaderItem(), frozen_count);
    if (error.isError())
        return error;
    setFrozenGroupCount(frozen_count);

    // размер последней секции при условии, что stretchLastSection() == true
    if (header(Qt::Horizontal)->stretchLastSection()) {
        int last_visible = horizontalRootHeaderItem()->lastVisibleSection();
        if (last_visible >= 0 && horizontalRootHeaderItem()->bottomCount() > last_visible)
            horizontalRootHeaderItem()->bottomItem(last_visible)->setSectionSize(horizontalRootHeaderItem()->defaultSectionSize());
    }

    return Error();
}

Error TableView::deserialize(const QByteArray& ba)
{
    if (ba.isNull())
        return Error();

    QBuffer buffer;
    buffer.setData(ba);
    buffer.open(QIODevice::ReadOnly);
    return deserialize(&buffer);
}

HeaderItem* TableView::horizontalRootHeaderItem() const
{
    return rootHeaderItem(Qt::Horizontal);
}

HeaderItem* TableView::verticalRootHeaderItem() const
{
    return rootHeaderItem(Qt::Vertical);
}

HeaderItem* TableView::horizontalHeaderItem(int id) const
{
    return rootHeaderItem(Qt::Horizontal)->item(id);
}

HeaderItem* TableView::verticalHeaderItem(int id) const
{
    return rootHeaderItem(Qt::Vertical)->item(id);
}

QModelIndex TableView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    QModelIndex current = currentIndex();
    if (current.isValid()) {
        int v_column = horizontalHeader()->visualIndex(current.column());
        if (v_column < 0)
            return QModelIndex();
        int v_first = horizontalHeader()->visualIndex(horizontalRootHeaderItem()->firstVisibleSection());
        int v_last = horizontalHeader()->visualIndex(horizontalRootHeaderItem()->lastVisibleSection());
        if (cursorAction == MoveLeft && v_column <= v_first)
            return current;
        if (cursorAction == MoveRight && v_column >= v_last)
            return current;
    }

    current = TableViewBase::moveCursor(cursorAction, modifiers);

    int forzen_sections = frozenSectionCount(false);
    if (forzen_sections > 0) {
        int pos = frozenRightPos();

        if (cursorAction == MoveLeft) {
            if (horizontalHeader()->visualIndex(current.column()) > forzen_sections - 1) {
                if (visualRect(current).topLeft().x() < pos) {
                    int newValue = horizontalScrollBar()->value() + visualRect(current).topLeft().x() - pos;
                    horizontalScrollBar()->setValue(newValue);
                }
            }
        }
    }
    return current;
}

void TableView::scrollTo(const QModelIndex& index, QAbstractItemView::ScrollHint hint)
{
    if (!index.isValid() || horizontalHeader()->visualIndex(index.column()) > frozenSectionCount(false) - 1) {
        TableViewBase::scrollTo(index, hint);
    } else {
        TableViewBase::scrollTo(model()->index(index.row(), indexAt(rect().topLeft()).column()), hint);
    }

    if (index.isValid() && _frozen_table_view) {
        _frozen_table_view->scrollTo(
            model()->index(index.row(), _frozen_table_view->indexAt(_frozen_table_view->rect().topLeft()).column()));
    }
}

void TableView::updateGeometries()
{
    TableViewBase::updateGeometries();

    _check_panel->setGeometry(0, 0, leftPanelWidth(), geometry().height());
}

void TableView::setModel(QAbstractItemModel* model)
{
    if (model == this->model())
        return;

    if (this->model() != nullptr) {
        disconnect(this->model(), &QAbstractItemModel::layoutChanged, this, &TableView::sl_layoutChanged);
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved, this, &TableView::sl_rowsRemoved);
        disconnect(this->model(), &QAbstractItemModel::rowsInserted, this, &TableView::sl_rowsInserted);
    } else {
        connect(model, &QAbstractItemModel::layoutChanged, this, &TableView::sl_layoutChanged);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &TableView::sl_rowsRemoved);
        connect(model, &QAbstractItemModel::rowsInserted, this, &TableView::sl_rowsInserted);
    }

    _checked.clear();
    _all_checked = false;

    TableViewBase::setModel(model);
}

void TableView::showCheckRowPanel(bool show)
{
    if (isShowCheckRowPanel() == show)
        return;

    _check_panel->setHidden(!show);
    updateGeometries();

    emit sg_checkPanelVisibleChanged();
}

bool TableView::isShowCheckRowPanel() const
{
    return !_check_panel->isHidden();
}

bool TableView::hasCheckedRows() const
{
    return isAllRowsChecked() || !_checked.isEmpty();
}

bool TableView::isRowChecked(int row) const
{
    if (_all_checked)
        return true;

    return _checked.contains(row);
}

void TableView::checkRow(int row, bool checked)
{
    if (isRowChecked(row) == checked)
        return;

    if (checked) {
        if (_all_checked)
            return;

        _checked << row;
    } else {
        if (_all_checked) {
            Q_ASSERT(_checked.isEmpty());
            for (int i = 0; i < model()->rowCount(); i++) {
                if (row == i)
                    continue;
                _checked << i;
            }
            _all_checked = false;

        } else {
            _checked.remove(row);
        }
    }

    _check_panel->update();
    emit sg_checkedRowsChanged();
}

QSet<int> TableView::checkedRows() const
{
    if (isAllRowsChecked() && _checked.isEmpty()) {
        QSet<int> res;
        QModelIndexList all_indexes;
        Utils::getAllIndexes(Utils::getTopSourceModel(model()), all_indexes);
        for (int i = 0; i < all_indexes.count(); i++) {
            res << all_indexes.at(i).row();
        }

        return res;
    }

    return _checked;
}

bool TableView::isAllRowsChecked() const
{
    return _all_checked;
}

void TableView::checkAllRows(bool checked)
{
    if (_all_checked && checked)
        return;

    _checked.clear();
    _all_checked = checked;

    _check_panel->update();
    emit sg_checkedRowsChanged();
}

QMap<int, bool> TableView::cellCheckColumns(int level) const
{
    Q_UNUSED(level)
    return _cell_сheck_сolumns;
}

void TableView::setCellCheckColumn(int logical_index, bool visible, bool enabled, int level)
{
    Q_UNUSED(level)

    if (visible) {
        if (_cell_сheck_сolumns.contains(logical_index))
            return;
        _cell_сheck_сolumns[logical_index] = enabled;
    } else {
        if (!_cell_сheck_сolumns.contains(logical_index))
            return;
        _cell_сheck_сolumns.remove(logical_index);
    }

    viewport()->update();
}

bool TableView::isCellChecked(const QModelIndex& index) const
{
    Q_ASSERT(index.isValid());
    QModelIndex source_index = sourceIndex(index);

    for (int i = _cell_checked.count() - 1; i >= 0; i--) {
        if (!_cell_checked.at(i).isValid()) {
            _cell_checked.removeAt(i);
            continue;
        }

        if (_cell_checked.at(i) == source_index)
            return true;
    }

    return false;
}

void TableView::setCellChecked(const QModelIndex& index, bool checked)
{
    Q_ASSERT(index.isValid());
    QModelIndex source_index = sourceIndex(index);
    QModelIndex view_index = Utils::alignIndexToModel(index, model());

    for (int i = _cell_checked.count() - 1; i >= 0; i--) {
        if (!_cell_checked.at(i).isValid()) {
            _cell_checked.removeAt(i);
            continue;
        }

        if (_cell_checked.at(i) != source_index)
            continue;

        if (checked)
            return;

        _cell_checked.removeAt(i);
        emit sg_checkedCellChanged(source_index, false);
        if (view_index.isValid())
            update(view_index);
        return;
    }

    if (checked) {
        _cell_checked << source_index;
        emit sg_checkedCellChanged(source_index, true);
        if (view_index.isValid())
            update(view_index);
    }
}

QModelIndexList TableView::checkedCells() const
{
    QModelIndexList res;
    for (int i = _cell_checked.count() - 1; i >= 0; i--) {
        if (!_cell_checked.at(i).isValid()) {
            _cell_checked.removeAt(i);
            continue;
        }

        res << _cell_checked.at(i);
    }

    return res;
}

void TableView::clearCheckedCells()
{
    for (auto& c : qAsConst(_cell_checked)) {
        setCellChecked(c, false);
    }
}

bool TableView::isAutoShrink() const
{
    return _auto_shrink;
}

void TableView::setAutoShring(bool b)
{
    if (b == _auto_shrink)
        return;

    _auto_shrink = b;

    updateGeometry();
    if (_frozen_table_view != nullptr)
        _frozen_table_view->updateGeometry();
}

int TableView::shrinkMinimumRowCount() const
{
    return _shrink_minimum_row_count;
}

void TableView::setShrinkMinimumRowCount(int n)
{
    if (n == _shrink_minimum_row_count)
        return;

    _shrink_minimum_row_count = n;

    updateGeometry();
    if (_frozen_table_view != nullptr)
        _frozen_table_view->updateGeometry();
}

int TableView::shrinkMaximumRowCount() const
{
    return _shrink_maximum_row_count;
}

void TableView::setShrinkMaximumRowCount(int n)
{
    if (n == _shrink_maximum_row_count)
        return;

    _shrink_maximum_row_count = n;

    updateGeometry();
    if (_frozen_table_view != nullptr)
        _frozen_table_view->updateGeometry();
}

bool TableView::isAutoResizeRowsHeight() const
{
    return _auto_resize_rows;
}

void TableView::setAutoResizeRowsHeight(bool b)
{
    if (b == _auto_resize_rows)
        return;

    _auto_resize_rows = b;
    requestResizeRowsToContents();
}

TableViewBase* TableView::frozenTableView() const
{
    return _frozen_table_view;
}

void TableView::onColumnResized(int column, int oldWidth, int newWidth)
{
    if (_request_resize)
        return;
    _request_resize = true;
    TableViewBase::onColumnResized(column, oldWidth, newWidth);
    updateFrozenTableGeometry();
    if (_frozen_table_view != nullptr)
        _frozen_table_view->onColumnResized(column, oldWidth, newWidth);
    _request_resize = false;
}

void TableView::resizeEvent(QResizeEvent* event)
{
    TableViewBase::resizeEvent(event);

    if (frozenSectionCount(false) > 0 && _frozen_table_view)
        updateFrozenTableGeometry();
}

bool TableView::event(QEvent* event)
{
    bool res = TableViewBase::event(event);
    if (_frozen_table_view && (event->type() == QEvent::WindowUnblocked || event->type() == QEvent::LayoutRequest))
        updateFrozenTableGeometry();

    return res;
}

void TableView::doItemsLayout()
{
    TableViewBase::doItemsLayout();
    updateFrozenTableGeometry();
}

bool TableView::edit(const QModelIndex& index, QAbstractItemView::EditTrigger trigger, QEvent* event)
{
    if (_frozen_table_view == nullptr)
        return TableViewBase::edit(index, trigger, event);

    if (editTriggers() != _frozen_table_view->editTriggers())
        _frozen_table_view->setEditTriggers(editTriggers());

    if (horizontalHeader()->visualIndex(index.column()) >= frozenSectionCount(false)) {
        // Не фиксированная колонка
        return TableViewBase::edit(index, trigger, event);
    }

    _frozen_table_view->edit(index, trigger, event);
    return true;
}

void TableView::customEvent(QEvent* event)
{
    TableViewBase::customEvent(event);

    if (event->type() == UPDATE_FROZEN_PROPERTIES_EVENT) {
        if (_frozen_table_view != nullptr) {
            _frozen_table_view->setEditTriggers(editTriggers());
            _frozen_table_view->setAlternatingRowColors(alternatingRowColors());
            _frozen_table_view->setSelectionMode(selectionMode());
            _frozen_table_view->setSelectionBehavior(selectionBehavior());
            _frozen_table_view->setSortingEnabled(isSortingEnabled());
            _frozen_table_view->setConfigMenuEnabled(isConfigMenuEnabled());
            _frozen_table_view->setToolTip(toolTip());
            _frozen_table_view->verticalHeader()->setDefaultSectionSize(verticalHeader()->defaultSectionSize());
            _frozen_table_view->setWordWrap(wordWrap());
        }
        _need_update_frozen_properties = false;

        if (_frozen_table_view != nullptr)
            updateFrozenTableGeometry();
    }
}

bool TableView::eventFilter(QObject* watched, QEvent* event)
{
    bool res = TableViewBase::eventFilter(watched, event);

    if (event->type() == QEvent::Resize && _frozen_table_view != nullptr) {
        if (watched == horizontalHeader() || watched == _frozen_table_view->horizontalHeader())
            updateFrozenTableGeometry();
    }

    return res;
}

void TableView::scrollContentsBy(int dx, int dy)
{
    TableViewBase::scrollContentsBy(dx, dy);
    _check_panel->update();
}

int TableView::leftPanelWidth() const
{
    return TableViewBase::leftPanelWidth() + (isShowCheckRowPanel() ? CheckBoxPanel::width() : 0);
}

void TableView::sl_horizontalGeometriesChanged()
{
    updateCornerWidget();
    updateFrozenTableGeometry();
    _check_panel->update();
}

void TableView::updateCornerWidget()
{
    if (_corner_widget == nullptr || model() == nullptr)
        return;

    if (_corner_label == nullptr) {
        _corner_label = new QLabel(this);
        _corner_label->setMargin(0);
        _corner_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    _corner_widget->stackUnder(_corner_label);

    if (_corner_text.isEmpty()) {
        _corner_label->setVisible(false);
    } else {
        _corner_label->setText(_corner_text);

        QFont font = _corner_label->font();
        font.setBold(_corner_bold);
        _corner_label->setFont(font);

        if (_corner_text_color.isValid()) {
            QPalette p = _corner_label->palette();
            p.setColor(QPalette::WindowText, _corner_text_color);
            _corner_label->setPalette(p);
        }

        _corner_label->setHidden(false);
        _corner_label->setAlignment(_corner_alignment);
        _corner_label->setGeometry(_corner_margin_left, _corner_margin_top,
            verticalHeader()->width() - _corner_margin_left - _corner_margin_right,
            horizontalHeader()->height() - _corner_margin_top - _corner_margin_bottom);

        _corner_label->setText(Hyphenation::GlobalTextHyphenationFormatter::stringToMultiline(
            _corner_label->fontMetrics(), _corner_text, _corner_label->rect().width()));
    }

    if (verticalHeader() == nullptr || horizontalHeader() == nullptr || !verticalHeader()->isVisible()) {
        _corner_widget->setHidden(true);
    } else {
        _corner_widget->setHidden(false);
        _corner_widget->setGeometry(0, 0, verticalHeader()->width() + 1, horizontalHeader()->height() + 1);
    }
}

void TableView::sl_verticalSectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)

    updateFrozenTableGeometry();
}

void TableView::sl_verticalGeometriesChanged()
{
    updateCornerWidget();
    updateFrozenTableGeometry();
}

void TableView::sl_rootItemHiddenChanged(const QList<HeaderItem*>& bottom_items, bool is_hide)
{
    if (is_hide) {
        QSet<HeaderItem*> top_hide;
        for (auto h : bottom_items) {
            Q_ASSERT(h->isBottom());
            if (h->topParent()->visualPos(true) < frozenGroupCount())
                top_hide << h->topParent();
        }
        setFrozenGroupCount(frozenGroupCount() - top_hide.count());
    }
}

void TableView::sl_frozen_currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous)

    if (!_frozen_table_view || _block_select_current || currentIndex() == current)
        return;

    _block_select_current = true;
    setCurrentIndex(current);
    _block_select_current = false;
}

void TableView::sl_frozen_horizontalSectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)

    updateFrozenTableGeometry();
    if (_frozen_table_view != nullptr)
        _frozen_table_view->requestResizeRowsToContents();
}

void TableView::sl_frozenClicked(const QModelIndex& index)
{
    emit clicked(index);
}

void TableView::sl_frozenDoubleClicked(const QModelIndex& index)
{
    emit doubleClicked(index);
}

void TableView::sl_frozenCloseEditor(QWidget* w)
{
    if (w == this)
        return; // по загадочным причинам приходит событие от самого себя

    if (!hasFocus())
        setFocus();
}

void TableView::sl_layoutChanged()
{
    _check_panel->update();
}

void TableView::sl_rowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)

    if (!_checked.isEmpty()) {
        for (int row = first; row <= last; row++) {
            _checked.remove(row);
        }

        // коррекция
        QSet<int> update_checked;
        for (auto row : qAsConst(_checked)) {
            if (row > last) {
                update_checked << row - 1 - (last - first);
            } else {
                update_checked << row;
            }
        }
        _checked = update_checked;
    }

    _check_panel->update();
}

void TableView::sl_rowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)

    if (!_checked.isEmpty()) {
        // коррекция
        QSet<int> update_checked;
        for (auto row : qAsConst(_checked)) {
            if (row > last) {
                update_checked << row + 1 + (last - first);
            } else {
                update_checked << row;
            }
        }
        _checked = update_checked;
    }

    _check_panel->update();
}

void TableView::init()
{
    _auto_resize_rows = true;

    _check_panel = new CheckBoxPanel(this);
    _check_panel->setHidden(true);

    connect(horizontalHeader(), &HeaderView::geometriesChanged, this, &TableView::sl_horizontalGeometriesChanged);
    connect(horizontalRootHeaderItem(), &HeaderItem::sg_visualMoved, this, &TableView::sl_rootItemColumnsMoved);
    connect(horizontalRootHeaderItem(), &HeaderItem::sg_hiddenChanged, this, &TableView::sl_rootItemHiddenChanged);

    connect(verticalHeader(), &HeaderView::geometriesChanged, this, &TableView::sl_verticalGeometriesChanged);
    connect(verticalHeader(), &HeaderView::sectionResized, this, &TableView::sl_verticalSectionResized);

    // иначе не будет обновляться отрисовка фильтра и т.п. глюки
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [&]() { horizontalHeader()->viewport()->update(); });

    setCornerButtonEnabled(false);
    setCornerWidget(new TableViewCorner(this));

    setItemDelegate(new ItemDelegate(this, nullptr, this, this));

    requestResizeRowsToContents();
}

void TableView::sl_rootItemColumnsMoved(HeaderItem* moved, int visual_pos_from, int visual_pos_to, bool before)
{
    if (frozenGroupCount() == 0 || !moved->isTop())
        return;

    int count = frozenGroupCount();

    if (visual_pos_from >= count && visual_pos_to < count)
        count++;
    else if (visual_pos_from < count && ((visual_pos_to > count && !before) || (visual_pos_to >= count && before)))
        count--;

    setFrozenGroupCount(count);
}

void TableView::updateFrozenTableGeometry()
{
    if (_frozen_table_view == nullptr || _block_update_geometry_count > 0 || frozenGroupCount() == 0)
        return;

    QRect rect;
    int horizShift = horizontalHeader()->geometry().left();
    int vertShift = horizontalHeader()->geometry().top();

    rect = QRect(horizShift, vertShift, frozenRightPos(), viewport()->height() + horizontalHeader()->height());
    if (rect.width() <= 0) {
        setFrozenGroupCount(0);
        return;
    }

    if (_frozen_table_view->geometry() != rect)
        _frozen_table_view->setGeometry(rect);

    _frozen_table_line->setGeometry(rect.right(), 0, _frozen_table_line->width(), rect.height() + 1);

    int top = viewport()->geometry().top() - vertShift;
    if (_frozen_table_view->viewport()->geometry().top() != top) {
        rect = _frozen_table_view->viewport()->geometry();
        rect.setTop(top);
        _frozen_table_view->viewport()->setGeometry(rect);
    }

    if (_frozen_table_view->horizontalHeader()->minimumHeight() != horizontalHeader()->height())
        _frozen_table_view->horizontalHeader()->setMinimumHeight(horizontalHeader()->height());
}

int TableView::frozenRightPos() const
{
    if (_frozen_table_view == nullptr
        || _frozen_table_view->horizontalHeader()->count()
            == _frozen_table_view->horizontalHeader()->hiddenSectionCount())
        return 0;

    int w = 0;
    int count = frozenSectionCount(false);
    for (int col = 0; col < count; ++col) {
        int width = _frozen_table_view->columnWidth(_frozen_table_view->horizontalHeader()->logicalIndex(col));
        if (width <= 0 || _frozen_table_view->isColumnHidden(_frozen_table_view->horizontalHeader()->logicalIndex(col))) {
            count++;
            continue;
        }
        w += width;
    }
    return w;
}

void TableView::updateFrozenCurrentCellPosition()
{
    if (_frozen_table_view == nullptr)
        return;

    QModelIndex current = currentIndex();
    if (!current.isValid())
        return;

    int frozen_sections = frozenSectionCount(false);

    if (!_block_select_current && _frozen_table_view->currentIndex() != current) {
        _block_select_current = true;

        if (horizontalHeader()->visualIndex(current.column()) >= frozen_sections)
            _frozen_table_view->setCurrentIndex(QModelIndex());
        else
            _frozen_table_view->setCurrentIndex(current);

        _block_select_current = false;
    }

    int pos = frozenRightPos();
    QRect currentRect = visualRect(current);

    if (frozen_sections > 0 && horizontalHeader()->visualIndex(current.column()) >= frozen_sections
        && currentRect.left() < pos) {
        int newValue = horizontalScrollBar()->value() + currentRect.left() - pos;
        horizontalScrollBar()->setValue(newValue);
    }

    _frozen_table_view->viewport()->update();
}

void TableView::updateFrozenCount()
{
    if (_frozen_group_count == 0) {
        _frozen_table_view->hide();
        _frozen_table_line->hide();

    } else {
        if (_frozen_table_view == nullptr) {
            _frozen_table_line = new QFrame(this);
            _frozen_table_line->setObjectName("frozen_table_line");

            _frozen_table_line->setFrameShape(QFrame::VLine);
            _frozen_table_line->setFrameShadow(QFrame::Plain);
            _frozen_table_line->setGeometry(0, 0, 1, 1);
            _frozen_table_line->setAttribute(Qt::WA_TransparentForMouseEvents);

            _frozen_table_view = new FrozenTableView(this);

            if (qobject_cast<ItemDelegate*>(itemDelegate()) != nullptr)
                _frozen_table_view->setUseHtml(isUseHtml());

            _frozen_table_view->setObjectName("frozen_table_view");
            _frozen_table_view->setModel(model());
            _frozen_table_view->horizontalHeader()->setObjectName("frozen_table_view_header");

            _frozen_table_view->stackUnder(_frozen_table_line);
            viewport()->stackUnder(_frozen_table_view);

            horizontalHeader()->installEventFilter(this);
            _frozen_table_view->horizontalHeader()->installEventFilter(this);

            _frozen_table_view->setToolTip(toolTip());
            _frozen_table_view->setFont(font());
            _frozen_table_view->setSelectionMode(selectionMode());
            _frozen_table_view->setSortingEnabled(isSortingEnabled());

            _frozen_table_view->setFocusPolicy(Qt::NoFocus);
            _frozen_table_view->verticalHeader()->hide();
            _frozen_table_view->horizontalHeader()->setStretchLastSection(false);
            _frozen_table_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            _frozen_table_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            connect(verticalScrollBar(), &QScrollBar::valueChanged, _frozen_table_view->verticalScrollBar(), &QScrollBar::setValue);

            connect(_frozen_table_view->horizontalHeader(), &HeaderView::sectionResized, this,
                &TableView::sl_frozen_horizontalSectionResized);

            connect(_frozen_table_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
                &TableView::sl_frozen_currentChanged);
            connect(_frozen_table_view, &TableViewBase::clicked, this, &TableView::sl_frozenClicked);
            connect(_frozen_table_view, &TableViewBase::doubleClicked, this, &TableView::sl_frozenDoubleClicked);
            connect(_frozen_table_view->verticalScrollBar(), &QScrollBar::valueChanged, verticalScrollBar(),
                &QScrollBar::setValue);

            // Надо обеспечить сохранение фокуса на остновной таблице после окончания
            // редактирования фиксированной ячейки
            connect(_frozen_table_view->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
                &TableView::sl_frozenCloseEditor, Qt::QueuedConnection);

            _frozen_table_view->horizontalHeader()->setJoinedHeader(horizontalHeader());
            _frozen_table_view->verticalHeader()->setJoinedHeader(verticalHeader());

            _frozen_table_view->show();
            _frozen_table_line->show();

            _frozen_table_view->requestResizeRowsToContents();
        }

        if (_frozen_table_view->isHidden()) {
            _frozen_table_view->show();
            _frozen_table_line->show();
        }
    }

    _frozen_table_view->horizontalHeader()->setLimit(_frozen_group_count);
    updateFrozenTableGeometry();
}

void TableView::blockUpdateFrozenGeometry()
{
    _block_update_geometry_count++;
}

void TableView::unblockUpdateFrozenGeometry()
{
    _block_update_geometry_count--;
    Q_ASSERT(_block_update_geometry_count >= 0);

    if (_block_update_geometry_count == 0)
        updateFrozenTableGeometry();
}

QModelIndex TableView::sourceIndex(const QModelIndex& index) const
{
    Q_ASSERT(index.isValid());
    QModelIndex idx = Utils::getTopSourceIndex(index);
    if (model() != nullptr)
        Q_ASSERT(idx.model() == Utils::getTopSourceModel(model()));
    return idx;
}

TableView::TableView(QWidget* parent)
    : TableViewBase(parent)
{
    init();
}

TableView::~TableView()
{ 
}

HeaderItem* TableView::rootHeaderItem(Qt::Orientation orientation) const
{
    return orientation == Qt::Horizontal ? horizontalHeader()->rootItem() : verticalHeader()->rootItem();
}

void TableView::paintEvent(QPaintEvent* event)
{
    TableViewBase::paintEvent(event);

    if (_frozen_table_view) {
        /* Синхронизация свойств таблицы с таблицей фиксированных колонок. Т.к. методы установки большинства свойств
         * не виртуальные, то нормальным путем невозможно перехватить их изменение */
        if (!_need_update_frozen_properties
            && (_frozen_table_view->isSortingEnabled() != isSortingEnabled() || _frozen_table_view->editTriggers() != editTriggers()
                || _frozen_table_view->alternatingRowColors() != alternatingRowColors()
                || _frozen_table_view->selectionBehavior() != selectionBehavior()
                || _frozen_table_view->isConfigMenuEnabled() != isConfigMenuEnabled()
                || _frozen_table_view->selectionMode() != selectionMode() || _frozen_table_view->toolTip() != toolTip()
                || _frozen_table_view->verticalHeader()->defaultSectionSize() != verticalHeader()->defaultSectionSize()
                || _frozen_table_view->wordWrap() != wordWrap())) {
            QApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(UPDATE_FROZEN_PROPERTIES_EVENT)));
            _need_update_frozen_properties = true;
        }
    }
}

void TableView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    TableViewBase::currentChanged(current, previous);

    updateFrozenCurrentCellPosition();
}

} // namespace zf
