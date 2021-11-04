#include "zf_header_view.h"
#include <texthyphenationformatter.h>
#include "zf_utils.h"
#include "zf_itemview_header_item.h"
#include "zf_itemview_header_model.h"
#include "zf_table_view_base.h"
#include "zf_tree_view.h"

#include <QApplication>
#include <QDebug>
#include <QDrag>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QStylePainter>
#include <QTimer>
#include <limits>
#include <QScrollBar>
#include <QTextDocument>

#define UPDATE_JOINED_PROPERTIES_EVENT (QEvent::User + 10000)

namespace zf
{
const QString HeaderView::MimeType = "application/x-ZFHeaderView";

HeaderView::HeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
    _reload_data_from_root_item_timer = new QTimer(this);
    _reload_data_from_root_item_timer->setSingleShot(true);
    _reload_data_from_root_item_timer->setInterval(0);
    connect(_reload_data_from_root_item_timer, &QTimer::timeout, this, [&]() { reloadDataFromRootItemHelper(); });

    if (orientation == Qt::Horizontal) {
        setContextMenuPolicy(Qt::CustomContextMenu);
        setSectionsMovable(true);
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QHeaderView::InternalMove);

        setMinimumSectionSize(qMax(22, qApp->fontMetrics().averageCharWidth() * 2));
        setSortIndicatorShown(true);
        setSortIndicator(-1, Qt::AscendingOrder);
    } else {
        setVisible(false);

        QTextDocument doc;
        doc.setPlainText("X");

        int default_row_height = qMax(int(doc.size().height()) + 1, qApp->fontMetrics().xHeight() + 4);
        default_row_height = qMax(22, default_row_height);

        setMinimumSectionSize(default_row_height);
        setDefaultSectionSize(default_row_height);
    }

    connect(this, &QHeaderView::sectionResized, this, &HeaderView::sl_sectionResized);
    connect(this, &QHeaderView::customContextMenuRequested, this, [&](const QPoint& pos) {
        if (_allow_config)
            emit sg_configMenuRequested(pos);
    });

    setJoinedModel(new ItemViewHeaderModel(orientation, this));
}

HeaderView::~HeaderView()
{
}

void HeaderView::setModel(QAbstractItemModel* m)
{
    _block_change_header_items_counter++;
    _model->setSourceModel(m);
    _block_change_header_items_counter--;
    QHeaderView::setModel(m);
}

QModelIndex HeaderView::indexAt(const QPoint& pos) const
{
    const int rows = orientation() == Qt::Horizontal ? rootItem()->levelSpan() : model()->rowCount();
    const int cols = model()->columnCount();
    int logicalIdx = logicalIndexAt(pos);
    int delta = 0;

    if (orientation() == Qt::Horizontal) {
        for (int row = 0; row < rows; ++row) {
            HeaderItem* item = model()->item(row, logicalIdx, true);
            if (item == nullptr)
                return QHeaderView::indexAt(pos);

            QModelIndex cellIndex = model()->index(row, logicalIdx);
            if (row == rows - 1)
                return cellIndex;

            delta += item->levelSize();
            if (pos.y() <= delta)
                return cellIndex;
        }
    } else {
        for (int col = 0; col < cols; ++col) {
            HeaderItem* item = model()->item(logicalIdx, col, true);
            if (item == nullptr)
                return QHeaderView::indexAt(pos);

            QModelIndex cellIndex = model()->index(logicalIdx, col);

            delta += item->levelSize();
            if (pos.x() <= delta)
                return cellIndex;
        }
    }

    return QModelIndex();
}

void HeaderView::paintEvent(QPaintEvent* e)
{
    _painted_rect.clear();
    QHeaderView::paintEvent(e);
    _painted_rect.clear();

    QStylePainter painter(viewport());

    if (orientation() == Qt::Vertical) {
        // отрисовка вертикальной линии
        painter.save();
        painter.setPen(Utils::pen(Utils::uiLineColor(true)));
        painter.drawLine(width() - 1, 0, width() - 1, height());
        painter.restore();
    }

    if (joinedHeader() != nullptr) {
        /* Синхронизация свойств со связанным заголовком. Т.к. методы установки большинства свойств
         * не виртуальные, то нормальным путем невозможно перехватить их изменение */
        if (!_need_update_joined_properties
            && (joinedHeader()->sectionsMovable() != sectionsMovable()
                || joinedHeader()->defaultSectionSize() != defaultSectionSize())) {
            QApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(UPDATE_JOINED_PROPERTIES_EVENT)));
            _need_update_joined_properties = true;
        }
    }
}

void HeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    paintSectionHelper(painter, rect, logicalIndex, false);
}

QSize HeaderView::sectionSizeFromContents(int logicalIndex) const
{
    QSize size = QHeaderView::sectionSizeFromContents(logicalIndex);
    if (_model == nullptr)
        return size;

    if (logicalIndex >= _model->rootItem()->sectionSpan()) {
        if (_model->rootItem()->hasChildren()) {
            if (orientation() == Qt::Horizontal)
                size.setHeight(_model->rootItem()->levelSizeToBottom());
            else
                size.setWidth(_model->rootItem()->levelSizeToBottom());
        }

        return size;
    }

    HeaderItem* item = rootItem()->findByPos(0, logicalIndex, true);
    if (item == nullptr)
        return size;

    return item->itemGroupSize();
}

std::shared_ptr<HeaderView::CellInfo> HeaderView::cellInfo(const QModelIndex& cell_index) const
{
    return cellInfo(this, model()->item(cell_index, true));
}

std::shared_ptr<HeaderView::CellInfo> HeaderView::cellInfo(HeaderItem* item) const
{
    if (item == nullptr)
        return std::make_shared<CellInfo>();
    return cellInfo(this, item);
}

std::shared_ptr<HeaderView::CellInfo> HeaderView::cellInfo(const HeaderView* header, HeaderItem* item)
{
    auto info = std::make_shared<CellInfo>();

    if (item == nullptr)
        return info;

    Q_ASSERT(!item->isRoot());

    if (header->orientation() == Qt::Horizontal)
        info->cell_index = header->model()->index(item->rowFrom(), item->columnFrom());
    else
        info->cell_index = header->model()->index(item->columnFrom(), item->rowFrom());

    info->header_item = item;    
    info->span_begin_index = header->model()->index(item->rowFrom(), item->columnFrom());
    info->header_view = header;

    info->col_span_from = item->columnFrom();
    info->col_span = item->columnSpan();
    info->col_span_to = item->columnTo();
    info->row_span_from = item->rowFrom();
    info->row_span = item->rowSpan();
    info->row_span_to = item->rowTo();

    if (header->orientation() == Qt::Horizontal) {
        info->visual_col_from = header->visualIndex(info->col_span_from);
        info->visual_col_to = header->visualIndex(info->col_span_to);
        info->visual_row_from = info->row_span_from;
        info->visual_row_to = info->row_span_to;

        bool hidden = header->isSectionHidden(info->col_span_from);
        info->visual_col_from_hidden = hidden ? -1 : info->visual_col_from;
        info->visual_col_to_hidden = info->visual_col_from_hidden;
        info->col_span_hidden = hidden ? -1 : 0;
    } else {
        info->visual_row_from = header->visualIndex(info->row_span_from);
        info->visual_row_to = header->visualIndex(info->row_span_to);
        info->visual_col_from = info->col_span_from;
        info->visual_col_to = info->col_span_to;

        bool hidden = header->isSectionHidden(info->row_span_from);
        info->visual_row_from_hidden = hidden ? -1 : info->visual_row_from;
        info->visual_row_to_hidden = info->visual_row_from_hidden;
        info->row_span_hidden = hidden ? -1 : 0;
    }

    if (info->col_span_from != info->col_span_to) {
        info->visual_col_from = INT_MAX;
        info->visual_col_to = -1;
        info->visual_col_from_hidden = INT_MAX;
        info->visual_col_to_hidden = -1;
        for (int i = info->col_span_from; i <= info->col_span_to; i++) {
            if (header->orientation() == Qt::Horizontal && !header->isSectionHidden(i)) {
                info->visual_col_from_hidden = qMin(info->visual_col_from_hidden, header->visualIndex(i));
                info->visual_col_to_hidden = qMax(info->visual_col_to_hidden, header->visualIndex(i));
            }
            info->visual_col_from = qMin(info->visual_col_from, header->visualIndex(i));
            info->visual_col_to = qMax(info->visual_col_to, header->visualIndex(i));
        }
        if (info->visual_col_from == INT_MAX) {
            info->visual_col_from = -1;
            info->visual_col_to = -1;
        }
        if (info->visual_col_from_hidden == INT_MAX) {
            info->visual_col_from_hidden = -1;
            info->visual_col_to_hidden = -1;
            info->col_span_hidden = -1;

        } else {
            info->col_span_hidden = info->visual_col_to_hidden - info->visual_col_from_hidden;
        }
    }
    if (info->row_span_from != info->row_span_to) {
        info->visual_row_from = INT_MAX;
        info->visual_row_to = -1;
        info->visual_row_from_hidden = INT_MAX;
        info->visual_row_to_hidden = -1;
        for (int i = info->row_span_from; i <= info->row_span_to; i++) {
            if (header->orientation() == Qt::Vertical && !header->isSectionHidden(i)) {
                info->visual_row_from_hidden = qMin(info->visual_row_from_hidden, header->visualIndex(i));
                info->visual_row_to_hidden = qMax(info->visual_row_to_hidden, header->visualIndex(i));
            }
            info->visual_row_from = qMin(info->visual_row_from, header->visualIndex(i));
            info->visual_row_to = qMax(info->visual_row_to, header->visualIndex(i));
        }
        if (info->visual_row_from == INT_MAX) {
            info->visual_row_from = -1;
            info->visual_row_to = -1;
        }
        if (info->visual_row_from_hidden == INT_MAX) {
            info->visual_row_from_hidden = -1;
            info->visual_row_to_hidden = -1;
            info->row_span_hidden = -1;

        } else {
            info->row_span_hidden = info->visual_row_to_hidden - info->visual_row_from_hidden;
        }
    }

    info->span_start_index = header->model()->index(info->row_span_from, info->col_span_from);
    info->span_finish_index = header->model()->index(info->row_span_to, info->col_span_to);

    info->cell_rect = item->sectionRect();
    int shift = header->sectionViewportPosition(header->logicalIndex(info->visual_col_from_hidden));
    if (header->orientation() == Qt::Horizontal)
        info->cell_rect.moveLeft(shift);
    else
        info->cell_rect.moveTop(shift);

    info->group_rect = item->groupRect();
    if (header->orientation() == Qt::Horizontal)
        info->group_rect.moveLeft(shift);
    else
        info->group_rect.moveTop(shift);

    if (!item->parent()->isRoot())
        info->group_index = header->model()->index(item->parent()->rowFrom(), item->parent()->columnFrom());

    return info;
}

std::shared_ptr<HeaderView::DragInfo> HeaderView::dragInfo(const QModelIndex& source_index, bool source_only, const QModelIndex& target_index,
                                                           const QPoint& target_pos) const
{
    auto info = std::make_shared<DragInfo>();
    info->source_info = cellInfo(source_index);
    info->source_index = info->source_info->span_start_index;

    if (target_index.isValid()) {
        Q_ASSERT(!target_pos.isNull());
        info->target_pos = target_pos;
        info->target_info = cellInfo(target_index);
        info->target_index = info->target_info->span_start_index;

        if (info->source_index.isValid() && info->target_index.isValid()) {
            info->target_info
                = cellInfo(model()->index(info->source_info->row_span_from, info->target_info->cell_index.column()));
            info->target_index = info->target_info->span_start_index;

            QRect visual_rect = info->target_info->group_rect;

            info->drop_left = target_pos.x() < (visual_rect.right() - visual_rect.width() / 2);

            if (!info->drop_left) {
                // ищем следующую видимую ячейку справа
                QModelIndex right_target_idx = model()->index(info->target_info->span_start_index.row(),
                    logicalIndex(visualIndex(info->target_info->col_span_to) + 1));
                if (right_target_idx.isValid() && isSectionHidden(right_target_idx.column())) {
                    while (true) {
                        right_target_idx = model()->index(
                            right_target_idx.row(), logicalIndex(visualIndex(right_target_idx.column()) + 1));
                        if (!right_target_idx.isValid())
                            break;

                        if (!isSectionHidden(right_target_idx.column())) {
                            info->target_info = cellInfo(right_target_idx);
                            info->target_index = info->target_info->span_start_index;
                            break;
                        }
                    }
                }
            }

            if (info->target_info->visual_col_to < info->source_info->visual_col_from
                || info->target_info->visual_col_from > info->source_info->visual_col_to) {
                // надо определить совпадение группы
                QModelIndex source_idx = info->source_info->group_index;
                QModelIndex target_idx = info->target_info->group_index;

                // надо проверить совпадение целевой группы на уровне (строке) source
                while (target_idx.row() >= source_idx.row()) {
                    if (target_idx.row() == source_idx.row()) {
                        info->allow = (target_idx == source_idx);
                        break;
                    }
                    target_idx = cellInfo(model()->index(target_idx.row() - 1, target_idx.column()))->group_index;
                }
            }

            if (info->target_info->visual_col_to < info->source_info->visual_col_from) {
                info->visual_drop_to
                    = info->drop_left ? info->target_info->visual_col_from : info->target_info->visual_col_to;
                info->visual_drop_to_hidden = info->drop_left ? info->target_info->visual_col_from_hidden
                                                              : info->target_info->visual_col_to_hidden;
            } else {
                info->visual_drop_to
                    = info->drop_left ? info->target_info->visual_col_from : info->target_info->visual_col_to;
                info->visual_drop_to_hidden = info->drop_left ? info->target_info->visual_col_from_hidden
                                                              : info->target_info->visual_col_to_hidden;
            }
        }

        if (info->allow && !isFirstSectionMovable() && orientation() == Qt::Horizontal
            && info->target_info->col_span_from == 0 && info->drop_left) {
            // запрещено перетаскивать первую секцию
            info->allow = false;
        }

    } else {
        if (source_only)
            info->allow = source_index.isValid();
        else
            info->allow = false;
    }

    return info;
}

void HeaderView::paintSectionHelper(QPainter* painter, const QRect& rect, int logicalIndex, bool transparent) const
{
    Q_UNUSED(rect)

    int first_visible = rootItem()->firstVisibleSection();

    int depth = (orientation() == Qt::Horizontal) ? rootItem()->levelSpan() : model()->columnCount();

    for (int level = 0; level < depth; ++level) {
        QStyleOptionHeader opt;
        initStyleOption(&opt);

        auto info = cellInfo(rootItem()->findByPos(level, logicalIndex, true));

        QString text;
        QRect cell_rect;
        QColor text_color;
        QColor background_color;
        QFont font;
        QIcon icon;
        bool is_bottom = true;
        int section_from = -1;
        int visual_col_from_hidden = -1;        
        int visual_row_from_hidden = -1;        

        if (info->header_item == nullptr) {
            text = QString::number(visualIndex(logicalIndex) + 1);
            cell_rect = rect;
            section_from = logicalIndex;
            if (orientation() == Qt::Horizontal) {
                visual_col_from_hidden = visualIndex(logicalIndex);
            } else {
                visual_row_from_hidden = visualIndex(logicalIndex);
            }

        } else {
            is_bottom = info->header_item->isBottom();
            text = info->header_item->labelMultiline();
            cell_rect = info->cell_rect;
            section_from = info->header_item->sectionFrom();
            text_color = info->header_item->foreground();
            background_color = info->header_item->background();
            font = info->header_item->font();
            icon = info->header_item->icon();
            if (orientation() == Qt::Horizontal) {
                visual_col_from_hidden = info->visual_col_from_hidden;
            } else {
                visual_row_from_hidden = info->visual_row_from_hidden;
            }
        }

        if (cell_rect.width() > 0 && cell_rect.height() > 0) {
            opt.textAlignment = Qt::AlignCenter;
            opt.iconAlignment = Qt::AlignVCenter;
            opt.section = logicalIndex;
            opt.text = text;
            opt.rect = cell_rect;

            if (_painted_rect.contains(opt.rect))
                continue;
            _painted_rect << cell_rect;

            if (background_color.isValid()) {
                opt.palette.setBrush(QPalette::Button, QBrush(background_color));
                opt.palette.setBrush(QPalette::Window, QBrush(background_color));
            }
            if (text_color.isValid())
                opt.palette.setBrush(QPalette::ButtonText, QBrush(text_color));

            painter->save();
            painter->setFont(font);
            // рамка и фон
            painter->save();
            if (transparent)
                painter->setOpacity(0.8);

            painter->setBrush(opt.palette.brush(QPalette::Button));
            painter->fillRect(cell_rect, opt.palette.brush(QPalette::Button));
            painter->restore();

            painter->save();
            if (transparent)
                painter->setOpacity(1);

            painter->setPen(Utils::pen(opt.palette.color(QPalette::Mid)));

            if (orientation() == Qt::Horizontal) {
                if (this->logicalIndex(visual_col_from_hidden) != first_visible) {
                    painter->drawLine(cell_rect.left() - 1, cell_rect.top(), cell_rect.left() - 1, cell_rect.bottom());
                }

                if (cell_rect.right() < viewport()->geometry().right()) {
                    painter->drawLine(cell_rect.right(), cell_rect.top(), cell_rect.right(), cell_rect.bottom());
                }
                painter->drawLine(cell_rect.left(), cell_rect.bottom(), cell_rect.right(), cell_rect.bottom());

            } else {
                if (this->logicalIndex(visual_row_from_hidden) != first_visible) {
                    painter->drawLine(cell_rect.left() - 1, cell_rect.top(), cell_rect.left() - 1, cell_rect.bottom());
                }

                painter->drawLine(cell_rect.right(), cell_rect.top(), cell_rect.right(), cell_rect.bottom());

                if (cell_rect.bottom() < viewport()->geometry().bottom()) {
                    painter->drawLine(cell_rect.left(), cell_rect.bottom(), cell_rect.right(), cell_rect.bottom());
                }
            }
            painter->restore();

            // текст и иконка
            painter->save();
            opt.icon = icon;
            if (!opt.icon.isNull()) {
                opt.iconAlignment = opt.text.isEmpty() ? Qt::AlignCenter : Qt::AlignVCenter | Qt::AlignLeft;
                opt.rect = cell_rect.adjusted(HeaderItem::ICON_LEFT_SHIFT, 0, 0, 0);
            }
            style()->drawControl(QStyle::CE_HeaderLabel, &opt, painter, this);
            painter->restore();

            // сортировка
            if (orientation() == Qt::Horizontal && isSortIndicatorShown() && sortIndicatorSection() == section_from
                && is_bottom) {
                opt.rect = cell_rect;
                auto sort_pe = sortIndicatorOrder() == Qt::AscendingOrder ? QStyle::PE_IndicatorArrowUp
                                                                          : QStyle::PE_IndicatorArrowDown;
                const int sort_size = 8;
                opt.rect.moveTo(opt.rect.left() + (opt.rect.width() - sort_size) / 2,
                                opt.rect.top() - sort_size / 4 + 1);
                opt.rect.setWidth(sort_size);
                opt.rect.setHeight(sort_size);
                painter->save();
                style()->drawPrimitive(sort_pe, &opt, painter, this);
                painter->restore();
            }

            painter->restore();
        }
    }
}

QAbstractItemView* HeaderView::itemView() const
{
    return qobject_cast<QAbstractItemView*>(parentWidget());
}

HeaderItem* HeaderView::rootItem() const
{
    return _model->rootItem();
}

ItemViewHeaderModel* HeaderView::model() const
{
    return _model;
}

void HeaderView::setJoinedHeader(HeaderView* header_view)
{
    if (header_view == this || _joined_header == header_view)
        return;

    if (_joined_header != nullptr) {
        disconnect(_joined_header, &HeaderView::sg_columnsDragging, this, &HeaderView::sl_joinedColumnsDragging);
        disconnect(_joined_header, &HeaderView::sg_columnsDragStarted, this, &HeaderView::sl_joinedColumnsDragStarted);
        disconnect(
            _joined_header, &HeaderView::sg_columnsDragFinished, this, &HeaderView::sl_joinedColumnsDragFinished);
    }

    if (header_view != nullptr) {
        connect(header_view, &HeaderView::sg_columnsDragging, this, &HeaderView::sl_joinedColumnsDragging);
        connect(header_view, &HeaderView::sg_columnsDragStarted, this, &HeaderView::sl_joinedColumnsDragStarted);
        connect(header_view, &HeaderView::sg_columnsDragFinished, this, &HeaderView::sl_joinedColumnsDragFinished);
    }

    _joined_header = header_view;
    setJoinedModel(header_view == nullptr ? nullptr : header_view->model());

    if (header_view != nullptr)
        header_view->setJoinedHeader(this);
}

HeaderView* HeaderView::joinedHeader() const
{
    return _joined_header;
}

void HeaderView::setLimit(int group_count)
{
    if (group_count == _limit)
        return;

    Q_ASSERT(orientation() == Qt::Horizontal);
    Q_ASSERT(group_count >= 0);

    _limit = group_count;
    reloadDataFromRootItemHelper();
}

int HeaderView::limit() const
{
    return _limit;
}

int HeaderView::limitSectionCount() const
{
    if (_limit <= 0)
        return 0;

    int count = 0;
    auto bottom = rootItem()->allBottomVisual(Qt::AscendingOrder);
    QSet<HeaderItem*> top;

    for (auto h : qAsConst(bottom)) {
        if (h->isHidden())
            continue;

        top << h->topParent();
        if (top.count() >= _limit)
            break;

        count++;
    }
    return count;
}

bool HeaderView::isAllowSorting() const
{
    return _allow_sorting;
}

void HeaderView::setAllowSorting(bool b)
{
    _allow_sorting = b;
}

bool HeaderView::isConfigMenuEnabled() const
{
    return _allow_config;
}

void HeaderView::setConfigMenuEnabled(bool b)
{
    _allow_config = b;
}

QSize HeaderView::sizeHint() const
{
    return QHeaderView::sizeHint();
}

QSize HeaderView::minimumSizeHint() const
{
    return QHeaderView::minimumSizeHint();
}

void HeaderView::setJoinedModel(ItemViewHeaderModel* m)
{
    if (m == _model)
        return;

    if (_model != nullptr) {
        disconnect(_model, &ItemViewHeaderModel::columnsInserted, this, &HeaderView::sl_columnsInserted);
        disconnect(_model, &ItemViewHeaderModel::rowsInserted, this, &HeaderView::sl_rowsInserted);
        disconnect(_model, &ItemViewHeaderModel::sg_itemDataChanged, this, &HeaderView::sl_itemDataChanged);
        disconnect(_model, &ItemViewHeaderModel::modelReset, this, &HeaderView::sl_modelReset);
        disconnect(_model->rootItem(), &HeaderItem::sg_visualMoved, this, &HeaderView::sl_rootItemVisualMoved);
        disconnect(
            _model->rootItem(), &HeaderItem::sg_outOfRangeResized, this, &HeaderView::sl_rootItemOutOfRangeResized);
        disconnect(_model->rootItem(), &HeaderItem::sg_sortChanged, this, &HeaderView::sl_rootItemSortChanged);
        disconnect(_model->rootItem(), &HeaderItem::sg_resizeModeChanged, this, &HeaderView::sl_itemResizeModeChanged);
        disconnect(_model->rootItem(), &HeaderItem::sg_hiddenChanged, this, &HeaderView::sl_rootItemHiddenChanged);

        delete _model;
        _model = nullptr;
    }

    if (m != nullptr) {
        _model = m;
        _model->setSourceModel(QHeaderView::model());
        connect(_model, &ItemViewHeaderModel::columnsInserted, this, &HeaderView::sl_columnsInserted);
        connect(_model, &ItemViewHeaderModel::rowsInserted, this, &HeaderView::sl_rowsInserted);
        connect(_model, &ItemViewHeaderModel::sg_itemDataChanged, this, &HeaderView::sl_itemDataChanged);
        connect(_model, &ItemViewHeaderModel::modelReset, this, &HeaderView::sl_modelReset);
        connect(_model->rootItem(), &HeaderItem::sg_visualMoved, this, &HeaderView::sl_rootItemVisualMoved);
        connect(_model->rootItem(), &HeaderItem::sg_outOfRangeResized, this, &HeaderView::sl_rootItemOutOfRangeResized);
        connect(_model->rootItem(), &HeaderItem::sg_sortChanged, this, &HeaderView::sl_rootItemSortChanged);
        connect(_model->rootItem(), &HeaderItem::sg_resizeModeChanged, this, &HeaderView::sl_itemResizeModeChanged);
        connect(_model->rootItem(), &HeaderItem::sg_hiddenChanged, this, &HeaderView::sl_rootItemHiddenChanged);
    }
}


void HeaderView::mousePressEvent(QMouseEvent* event)
{
    QModelIndex index = indexAt(event->pos());

    int handle_left = sectionHandleAt(event->pos());
    bool allow_resize = allowResize(event->pos());

    if (allow_resize)
        QHeaderView::mousePressEvent(event);

    // не реагируем на нажатия между заголовками
    if (handle_left >= 0 && allow_resize)
        return;

    if (!index.isValid())
        return;

    auto info = cellInfo(index);

    if (event->button() == Qt::LeftButton && orientation() == Qt::Horizontal
        && QApplication::keyboardModifiers() == Qt::NoModifier) {
        // тащим заголовок
        if (sectionsMovable() && info->header_item->isMovable() && info->col_span_hidden >= 0
            && info->col_span_hidden <= count() - hiddenSectionCount()) {
            if (isFirstSectionMovable() || info->col_span_from > 0) {
                // начало перетаскивания заголовка
                _drag_start_pos = event->pos();
            }
        }
        // сортировка
        _sort_mouse_press_pos = event->pos();
    }
}

void HeaderView::mouseMoveEvent(QMouseEvent* event)
{
    QHeaderView::mouseMoveEvent(event);

    // убираем курсор изменения размера
    if (sectionHandleAt(event->pos()) >= 0 && !allowResize(event->pos()))
        unsetCursor();

    if (!_sort_mouse_press_pos.isNull()
        && (_sort_mouse_press_pos - event->pos()).manhattanLength() > QApplication::startDragDistance())
        _sort_mouse_press_pos = QPoint(); // вместо сортировки начали перетасивание

    if ((event->buttons() & Qt::LeftButton) && !_drag_start_pos.isNull()
        && (event->pos() - _drag_start_pos).manhattanLength() >= QApplication::startDragDistance()) {
        QModelIndex index = indexAt(_drag_start_pos);
        auto info = dragInfo(indexAt(_drag_start_pos), true, {}, {});
        if (!info->source_index.isValid()) {
            QDrag::cancel();
            return;
        }

        // формируем картинку для отображения при переносе
        QDrag* drag = new QDrag(this);
        drag->setMimeData(encodeMimeData(_drag_start_pos, index));
        _drag_start_pos = QPoint();

        QPixmap pixmap(info->source_info->group_rect.width(), info->source_info->group_rect.height());
        pixmap.fill(Qt::transparent);
        QPainter painter;
        QRect rect;
        painter.begin(&pixmap);        
        painter.translate(-info->source_info->group_rect.left(), -info->source_info->group_rect.top());
        _painted_rect.clear();
        for (int col = info->source_info->col_span_from; col <= info->source_info->col_span_to; col++) {
            painter.save();
            paintSectionHelper(&painter, rect, col, true);
            painter.restore();
        }
        painter.setPen(Utils::pen(Qt::darkGray));
        painter.setOpacity(0.5);
        painter.drawRect(info->source_info->group_rect.adjusted(0, 0, -1, -1));
        painter.end();

        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));

        emit sg_columnsDragStarted();

        drag->exec(Qt::MoveAction);

        _drag_from_begin = -1;
        _drag_from_end = -1;
        _drag_to = -1;
        _drag_to_hidden = -1;
        _drag_left = false;
        _drag_allowed = false;
        emit sg_columnsDragFinished();
    }
}

void HeaderView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!_sort_mouse_press_pos.isNull()) {
        _drag_start_pos = QPoint();

        if (isAllowSorting()) {
            QModelIndex release_index = indexAt(event->pos());
            QModelIndex press_index = indexAt(_sort_mouse_press_pos);
            _sort_mouse_press_pos = QPoint();

            if (!release_index.isValid() || !press_index.isValid())
                return;

            HeaderItem* release_item = rootItem()->findByPos(release_index, true);
            HeaderItem* press_item = rootItem()->findByPos(press_index, true);

            if (release_item == press_item) {
                if (release_item == nullptr) {
                    if (release_index == press_index)
                        flipSortIndicator(press_index.column());
                } else {
                    if (release_item->isBottom())
                        flipSortIndicator(release_item->columnFrom());
                }
            }
        }
    } else {
        QHeaderView::mouseReleaseEvent(event);
    }
}

void HeaderView::flipSortIndicator(int section)
{
    Qt::SortOrder sortOrder;
    if (sortIndicatorSection() == section) {
        sortOrder = (sortIndicatorOrder() == Qt::DescendingOrder) ? Qt::AscendingOrder : Qt::DescendingOrder;
    } else {
        const QVariant value = model()->index(0, section).data(Qt::InitialSortOrderRole);
        if (value.canConvert(QVariant::Int))
            sortOrder = static_cast<Qt::SortOrder>(value.toInt());
        else {
            if (sortIndicatorSection() < 0 && section == rootItem()->firstVisibleSection())
                sortOrder = Qt::DescendingOrder;
            else
                sortOrder = Qt::AscendingOrder;
        }
    }
    setSortIndicator(section, sortOrder);
    rootItem()->sort(section, sortOrder);
}

void HeaderView::reloadDataFromRootItem()
{
    _reload_data_from_root_item_timer->start();
}

void HeaderView::reloadDataFromRootItemHelper()
{
    if (_block_change_header_items_counter > 0)
        return;

    if (_reload_data_from_root_item_timer->isActive())
        _reload_data_from_root_item_timer->stop();

    emit sg_beforeLoadDataFromRootHeader();

    _block_change_header_items_counter++;

    // необходимо сбросить внутренний кэш QHeaderView
    initializeSections(); //    reset();

    // перечитать и применить параметры заголовка
    auto all_bottom = rootItem()->allBottom();
    for (auto h : qAsConst(all_bottom)) {
        if (logicalIndex(h->sectionFrom()) < 0)
            continue;

        bool is_out_of_limit = _limit > 0 && h->topParent()->visualPos(true) >= _limit;

        if (!is_out_of_limit) {
            if (sectionResizeMode(h->sectionFrom()) != h->resizeMode())
                setSectionResizeMode(h->sectionFrom(), h->resizeMode());

            if ((h->resizeMode() == Interactive || h->resizeMode() == Fixed)
                && sectionSize(h->sectionFrom()) != h->sectionSize()) {
                resizeSection(h->sectionFrom(), h->sectionSize());
            }
        }

        if (is_out_of_limit) {
            setSectionHidden(h->sectionFrom(), true);

        } else if (isSectionHidden(h->sectionFrom()) != h->isHidden()) {
            setSectionHidden(h->sectionFrom(), h->isHidden());
        }
    }

    updateVisualOrder();

    if (orientation() == Qt::Horizontal) {
        // после применения размеров к заголовку, они могут измениться (например задано stretchLastSection), поэтому
        // надо заново записать их в HeaderItem
        QMap<int, int> sizes;
        for (int i = 0; i < count(); ++i) {
            if (_limit > 0) {
                HeaderItem* top_item = rootItem()->findByPos(0, i, true);
                if (top_item != nullptr && top_item->visualPos(true) >= _limit)
                    continue;
            }

            auto new_sizes = getSectionsSizes(i, -1);
            for (auto i = new_sizes.constBegin(); i != new_sizes.constEnd(); ++i) {
                sizes.insert(i.key(), i.value());
            }

            if (i < rootItem()->sectionSpan()) {
                if (parentWidget() != nullptr)
                    // это извращение нужно, что заставить Qt пересчитать размер заголовка
                    QMetaObject::invokeMethod(parentWidget(), "columnResized", Qt::DirectConnection, Q_ARG(int, i),
                        Q_ARG(int, sectionSize(i)), Q_ARG(int, sectionSize(i)));

            } else {
                // скрываем лишние
                hideSection(i);
            }
        }

        rootItem()->setSectionsSizes(sizes);
    }

    _block_change_header_items_counter--;

    emit sg_afterLoadDataFromRootHeader();

    viewport()->update();
}

void HeaderView::updateVisualOrder()
{
    auto items = rootItem()->allBottomVisual(Qt::DescendingOrder);

    for (auto h : qAsConst(items)) {
        int new_visual = h->sectionFrom(false);
        int current_visual = visualIndex(h->sectionFrom());

        if (new_visual != current_visual)
            moveSection(current_visual, new_visual);
    }
}

QMap<int, int> HeaderView::getSectionsSizes(int logicalIndex, int new_size) const
{
    QMap<int, int> sizes;

    QList<HeaderItem*> items;

    if (logicalIndex >= 0) {
        HeaderItem* item = rootItem()->findByPos(0, logicalIndex, true);
        if (item == nullptr) {
            if (orientation() == Qt::Vertical) {
                item = _model->item(logicalIndex, 0, false);
                if (item == nullptr)
                    return QMap<int, int>();

            } else {
                return QMap<int, int>();
            }
        }
        items << item;
    } else {
        items = rootItem()->children();
    }

    for (auto item : qAsConst(items)) {
        for (int i = item->sectionFrom(); i <= item->sectionTo(); i++) {
            if (isSectionHidden(i))
                continue;

            if (i == logicalIndex && new_size > 0) {
                sizes[i] = new_size;
            } else {
                if (sectionSize(i) > 0)
                    sizes[i] = sectionSize(i);
            }
        }
    }

    return sizes;
}

void HeaderView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(MimeType)) {
        if (event->source() == this || event->source() == joinedHeader()) {
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    } else {
        QHeaderView::dragEnterEvent(event);
    }
}

void HeaderView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(MimeType)) {
        if (event->source() == this || event->source() == joinedHeader()) {
            QPoint source_pos;
            QModelIndex source_index;
            decodeMimeData(event->mimeData(), event->source(), source_pos, source_index);
            if (!source_index.isValid()) {
                event->ignore();
                return;
            }

            auto info = dragInfo(source_index, false, indexAt(event->pos()), event->pos());

            if (_drag_from_begin != info->source_info->visual_col_from
                || _drag_from_end != info->source_info->visual_col_to || _drag_to != info->visual_drop_to
                || _drag_to_hidden != info->visual_drop_to_hidden || _drag_left != info->drop_left
                || _drag_allowed != info->allow) {
                _drag_from_begin = info->source_info->visual_col_from;
                _drag_from_end = info->source_info->visual_col_to;
                _drag_to = info->visual_drop_to;
                _drag_to_hidden = info->visual_drop_to_hidden;
                _drag_left = info->drop_left;
                _drag_allowed = info->allow;

                emit sg_columnsDragging(info->source_info->visual_col_from, info->source_info->visual_col_to,
                                        info->visual_drop_to, info->visual_drop_to_hidden, info->drop_left,
                                        info->allow);
            }

            if (!info->allow) {
                event->ignore();
                return;
            }

            event->acceptProposedAction();

        } else {
            event->ignore();
        }
    } else {
        QHeaderView::dragMoveEvent(event);
    }
}

void HeaderView::dropEvent(QDropEvent* event)
{
    auto mimedata = event->mimeData();
    if (mimedata->hasFormat(MimeType)) {
        if (event->source() == this || event->source() == joinedHeader()) {
            QPoint source_pos;
            QModelIndex source_index;
            decodeMimeData(event->mimeData(), event->source(), source_pos, source_index);
            if (!source_index.isValid()) {
                event->ignore();
                return;
            }

            auto drop_index = indexAt(event->pos());
            auto info = dragInfo(source_index, false, drop_index, event->pos());
            if (!info->allow) {
                event->ignore();
                return;
            }

            event->acceptProposedAction();

            info->source_info->header_item->move(info->target_info->header_item->id(), info->drop_left);

        } else {
            event->ignore();
        }
    } else {
        QHeaderView::dropEvent(event);
    }
}

void HeaderView::dragLeaveEvent(QDragLeaveEvent* event)
{
    QHeaderView::dragLeaveEvent(event);
    if (isColumnsDragging()) {
        _drag_allowed = false;
        emit sg_columnsDragging(_drag_from_begin, _drag_from_end, _drag_to, _drag_to_hidden, _drag_left, _drag_allowed);
    }
}

void HeaderView::resizeEvent(QResizeEvent* event)
{
    QHeaderView::resizeEvent(event);
}

void HeaderView::customEvent(QEvent* event)
{
    QHeaderView::customEvent(event);

    if (event->type() == UPDATE_JOINED_PROPERTIES_EVENT) {
        copyJoinedProperties();
        _need_update_joined_properties = false;
    }
}

bool HeaderView::event(QEvent* event)
{    
    bool res = QHeaderView::event(event);

    if (event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent* e = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (e->propertyName() == QStringLiteral("showSortIndicator")) {
            setAllowSorting(isSortIndicatorShown());
        }
    }

    return res;
}

void HeaderView::sl_sectionResized(int logical_index, int old_size, int new_size)
{
    Q_UNUSED(old_size)

    if (_block_change_header_items_counter > 0)
        return;

    if (_limit > 0) {
        HeaderItem* top_item = rootItem()->findByPos(0, logical_index, true);
        if (top_item != nullptr && top_item->visualPos(true) >= _limit)
            return;
    }

    _block_change_header_items_counter++;
    rootItem()->setSectionsSizes(getSectionsSizes(logical_index, new_size));
    _block_change_header_items_counter--;

    viewport()->update();
}

void HeaderView::sl_columnsInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)
}

void HeaderView::sl_rowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)
}

void HeaderView::sl_itemDataChanged(HeaderItem* item, int role)
{
    switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            viewport()->update(item->groupRect());
            break;

        case Qt::SizeHintRole:
            if (item->isBottom())
                resizeSection(item->sectionFrom(), item->sectionSize());
            break;

        default:
            break;
    }
}

void HeaderView::sl_modelReset()
{
    reloadDataFromRootItem();
}

void HeaderView::sl_itemResizeModeChanged(int section, ResizeMode mode)
{
    if (sectionResizeMode(section) != mode && visualIndex(section) >= 0) {
        setSectionResizeMode(section, mode);
        if (mode == Stretch || mode == ResizeToContents)
            resizeSection(section, minimumSectionSize());
    }
}

void HeaderView::sl_rootItemHiddenChanged(const QList<HeaderItem*>& bottom_items, bool is_hide)
{
    Q_UNUSED(bottom_items)
    Q_UNUSED(is_hide)

    reloadDataFromRootItemHelper();
}

void HeaderView::sl_rootItemVisualMoved()
{
    reloadDataFromRootItemHelper();
}

void HeaderView::sl_rootItemOutOfRangeResized(int section, int size)
{
    if (sectionSize(section) != size)
        resizeSection(section, size);
}

void HeaderView::sl_rootItemSortChanged(int old_section, int new_section, Qt::SortOrder order)
{
    Q_UNUSED(old_section)

    if (new_section == -1) {
        if (sortIndicatorSection() >= 0)
            setSortIndicator(-1, Qt::AscendingOrder);
    } else {
        if (sortIndicatorSection() != new_section || sortIndicatorOrder() != order)
            setSortIndicator(new_section, order);
    }
}

void HeaderView::sl_joinedColumnsDragStarted()
{
    if (_block_emit_joined_flag)
        return;

    takeDragDataFromJoined();

    _block_emit_joined_flag = true;
    emit sg_columnsDragStarted();
    _block_emit_joined_flag = false;
}

void HeaderView::sl_joinedColumnsDragFinished()
{
    if (_block_emit_joined_flag)
        return;

    takeDragDataFromJoined();
    _block_emit_joined_flag = true;
    emit sg_columnsDragFinished();
    _block_emit_joined_flag = false;
}

void HeaderView::sl_joinedColumnsDragging(int from_begin, int from_end, int to, int to_hidden, bool left, bool allow)
{
    if (_block_emit_joined_flag)
        return;

    takeDragDataFromJoined();
    _block_emit_joined_flag = true;
    emit sg_columnsDragging(from_begin, from_end, to, to_hidden, left, allow);
    _block_emit_joined_flag = false;
}

void HeaderView::copyJoinedProperties()
{
    if (joinedHeader() == nullptr)
        return;

    setSectionsMovable(joinedHeader()->sectionsMovable());

    QList<int> sizes;
    for (int i = 0; i < count(); i++) {
        sizes << sectionSize(i);
    }
    // установка размера по умолчанию сбрасывает текущие размеры, зачем - загадка
    setDefaultSectionSize(joinedHeader()->defaultSectionSize());

    for (int i = 0; i < count(); i++) {
        if (sectionResizeMode(i) == QHeaderView::ResizeMode::Interactive)
            resizeSection(i, sizes.at(i));
    }
}

void HeaderView::takeDragDataFromJoined()
{
    if (joinedHeader() == nullptr)
        return;

    _drag_from_begin = joinedHeader()->_drag_from_begin;
    _drag_from_end = joinedHeader()->_drag_from_end;
    _drag_to = joinedHeader()->_drag_to;
    _drag_to_hidden = joinedHeader()->_drag_to_hidden;
    _drag_left = joinedHeader()->_drag_left;
    _drag_allowed = joinedHeader()->_drag_allowed;
}

QMimeData* HeaderView::encodeMimeData(const QPoint& pos, const QModelIndex& index) const
{
    QMimeData* data = new QMimeData;

    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream.setVersion(QDataStream::Qt_5_6);
    dataStream << pos;
    dataStream << index.row();
    dataStream << index.column();

    data->setData(MimeType, itemData);
    return data;
}

void HeaderView::decodeMimeData(
    const QMimeData* data, const QObject* source_object, QPoint& source_pos, QModelIndex& source_index) const
{
    source_pos = QPoint();
    source_index = QModelIndex();

    if (!data->hasFormat(MimeType))
        return;

    QByteArray itemData = data->data(MimeType);
    QDataStream dataStream(&itemData, QIODevice::ReadOnly);
    dataStream.setVersion(QDataStream::Qt_5_6);

    int row;
    int column;
    dataStream >> source_pos;
    dataStream >> row;
    dataStream >> column;

    source_index = (source_object == this || joinedHeader() == nullptr) ? model()->index(row, column)
                                                                        : joinedHeader()->model()->index(row, column);
}

// взято из исходников QHeaderView
int HeaderView::sectionHandleAt(const QPoint& point) const
{
    int position = (orientation() == Qt::Horizontal) ? point.x() : point.y();
    int visual = visualIndexAt(position);
    if (visual == -1)
        return -1;
    int log = logicalIndex(visual);
    int pos = sectionViewportPosition(log);
    int grip = style()->pixelMetric(QStyle::PM_HeaderGripMargin);

    bool atLeft = position < pos + grip;
    bool atRight = (position > pos + sectionSize(log) - grip);
    if (orientation() == Qt::Horizontal && isRightToLeft())
        qSwap(atLeft, atRight);

    if (atLeft) {
        // grip at the beginning of the section
        while (visual > -1) {
            int logical = logicalIndex(--visual);
            if (!isSectionHidden(logical))
                return logical;
        }
    } else if (atRight) {
        // grip at the end of the section
        return log;
    }
    return -1;
}

bool HeaderView::allowResize(const QPoint& point) const
{
    bool allow_resize = false;
    auto handle = sectionHandleAt(point);
    if (handle != -1) {
        QModelIndex index = indexAt(point);
        if (!index.isValid())
            return true;

        // мышь с левой стороны
        auto info_left = cellInfo(orientation() == Qt::Horizontal ? model()->index(index.row(), handle)
                                                                  : model()->index(handle, index.column()));
        if (!info_left->cell_index.isValid())
            return true;

        // мышь с правой стороны
        int right_handle = logicalIndex(visualIndex(handle) + 1);
        while (right_handle >= 0
            && ((isSectionHidden(right_handle))
                && (joinedHeader() == nullptr || (joinedHeader()->isSectionHidden(right_handle))))
            && right_handle < count()) {
            right_handle = logicalIndex(visualIndex(right_handle) + 1);
        }
        if (right_handle < 0 || right_handle >= count()) {
            allow_resize = true;

        } else {
            auto info_right = cellInfo(orientation() == Qt::Horizontal ? model()->index(index.row(), right_handle)
                                                                       : model()->index(right_handle, index.column()));
            if (!info_right->cell_index.isValid())
                return true;

            if (orientation() == Qt::Horizontal)
                allow_resize = (logicalIndex(info_left->visual_col_to_hidden) == handle
                                || logicalIndex(info_right->visual_col_from_hidden) == right_handle);
            else
                allow_resize = (logicalIndex(info_left->visual_row_to_hidden) == handle
                                || logicalIndex(info_right->visual_row_from_hidden) == right_handle);
        }
    }

    return allow_resize;
}

void HeaderView::sl_autoSearchStatusChanged(bool)
{
    viewport()->update();
}

void HeaderView::sl_autoSearchTextChanged(const QString&)
{
    viewport()->update();
}

#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
bool HeaderView::isFirstSectionMovable() const
{
    return parent() != nullptr && qobject_cast<QTreeView*>(parent()) == nullptr;
}
#endif

} // namespace zf
