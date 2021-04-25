#include "zf_itemview_header_item.h"
#include "texthyphenationformatter.h"
#include "zf_utils.h"
#include "zf_itemview_header_model.h"

#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <limits>

namespace zf
{
//! Сдвиг иконки относительно левой части ячейки заголовка
const int HeaderItem::ICON_LEFT_SHIFT = 3;
const QChar HeaderItem::AVERAGE_CHAR = 'X';

HeaderItem::~HeaderItem()
{
    emit sg_beforeDelete();

    for (auto child : qAsConst(_children)) {
        disconnect(child, &HeaderItem::sg_structureChanged, this, &HeaderItem::sl_childStructureChanged);
        disconnect(child, &HeaderItem::sg_beforeDelete, this, &HeaderItem::sl_childDeleted);
    }

    qDeleteAll(_children);

    if (_font != nullptr)
        delete _font;

    if (!isRoot())
        emit sg_structureChanged();
}

bool HeaderItem::isRoot() const
{
    return _type == Type::Root;
}

bool HeaderItem::isBottom() const
{
    return !isRoot() && _children.isEmpty();
}

bool HeaderItem::isTop() const
{
    return parent() == root();
}

void HeaderItem::beginUpdate()
{
    if (!isRoot()) {
        root()->beginUpdate();
        return;
    }

    _update_counter++;
}

bool HeaderItem::isUpdating() const
{
    if (!isRoot())
        return root()->isUpdating();

    return _update_counter > 0;
}

void HeaderItem::clear()
{
    Q_ASSERT(isRoot());

    beginUpdate();

    while (!_children.isEmpty()) {
        remove(_children.last());
    }

    endUpdate();

    if (_update_counter > 0) {
        _level_span = 1;
        _section_span = 0;
    }
}

void HeaderItem::endUpdate()
{
    if (!isRoot()) {
        root()->endUpdate();
        return;
    }

    _update_counter--;
    Q_ASSERT(_update_counter >= 0);

    if (_update_counter == 0)
        recalc();
}

HeaderItem* HeaderItem::root() const
{
    return _root;
}

HeaderItem* HeaderItem::item(int id, bool halt_if_not_found) const
{
    if (!isRoot())
        return root()->item(id, halt_if_not_found);

    if (id < 0)
        return const_cast<HeaderItem*>(this);

    updateCache();

    HeaderItem* res = _all_children_by_id.value(id, nullptr);
    if (halt_if_not_found)
        Q_ASSERT(res != nullptr);

    return res;
}

bool HeaderItem::containsItem(int id) const
{
    return item(id, false) != nullptr;
}

QString HeaderItem::label(bool original_text) const
{
    if (original_text)
        return _label;

    if (!_icon.isNull() && _hide_label)
        return QString();

    return _label.isEmpty() && _icon.isNull()
        ? QString::number(1 + (orientation() == Qt::Horizontal ? _section_from : _level_from))
        : _label;
}

HeaderItem* HeaderItem::setLabel(const QString& s)
{
    QString prepared = s.trimmed().simplified();
    prepared.replace("\n", " ");

    if (_label == prepared)
        return this;

    _label = prepared;

    calculateSectionsSize();

    return this;
}

QFont HeaderItem::font() const
{
    QFont f = _font == nullptr ? QApplication::font() : *_font;

    f.setBold(_bold);
    f.setUnderline(_underline);
    f.setItalic(_italic);

    return f;
}

HeaderItem* HeaderItem::setFont(const QFont& f)
{
    if (_font != nullptr) {
        delete _font;
        _font = nullptr;
    }

    _font = new QFont(f);

    calculateSectionsSize();
    dataChanged(Qt::FontRole);

    return this;
}

HeaderItem* HeaderItem::setBold(bool b)
{
    if (_bold == b)
        return this;

    _bold = b;

    calculateSectionsSize();
    dataChanged(Qt::FontRole);

    return this;
}

bool HeaderItem::isBold() const
{
    return _bold;
}

HeaderItem* HeaderItem::setItalic(bool b)
{
    if (_italic == b)
        return this;

    _italic = b;

    calculateSectionsSize();
    dataChanged(Qt::FontRole);

    return this;
}

bool HeaderItem::isItalic() const
{
    return _italic;
}

HeaderItem* HeaderItem::setUnderline(bool b)
{
    if (_underline == b)
        return this;

    _underline = b;

    calculateSectionsSize();
    dataChanged(Qt::FontRole);

    return this;
}

bool HeaderItem::isUnderline() const
{
    return _underline;
}

QColor HeaderItem::foreground() const
{
    return _foreground;
}

HeaderItem* HeaderItem::setForeground(const QColor& color)
{
    if (_foreground == color)
        return this;

    _foreground = color;
    dataChanged(Qt::ForegroundRole);

    return this;
}

QColor HeaderItem::background() const
{
    return _background;
}

HeaderItem* HeaderItem::setBackground(const QColor& color)
{
    if (_background == color)
        return this;

    _background = color;
    dataChanged(Qt::BackgroundRole);

    return this;
}

HeaderItem* HeaderItem::setIcon(const QIcon& icon, bool hide_label)
{
    _icon = icon;
    _hide_label = hide_label;

    calculateSectionsSize();
    dataChanged(Qt::DecorationRole);

    return this;
}

bool HeaderItem::isSorting() const
{
    return _sorting;
}

Qt::SortOrder HeaderItem::sortOrder() const
{
    return _sort_order;
}

int HeaderItem::visualToLogical(int pos) const
{
    Q_ASSERT(pos >= 0);

    auto visual_items = allBottomVisual();
    Q_ASSERT(pos < visual_items.count());
    auto all_items = allBottom();
    int i = all_items.indexOf(visual_items.at(pos));
    Q_ASSERT(i >= 0);
    return i;
}

int HeaderItem::logicalToVisual(int pos) const
{
    Q_ASSERT(pos >= 0);

    auto all_items = allBottom();
    Q_ASSERT(pos < all_items.count());
    auto visual_items = allBottomVisual();
    return visual_items.indexOf(all_items.at(pos));
}

void HeaderItem::sort(Qt::SortOrder order)
{
    sortHelper(this, order);
}

void HeaderItem::sort(int section, Qt::SortOrder order)
{
    if (!isRoot()) {
        root()->sort(section, order);
        return;
    }

    if (section < 0) {
        sortOff();
        return;
    }

    HeaderItem* item = bottomItem(section);
    if (item == nullptr)
        return;

    item->sort(order);
}

void HeaderItem::sortOff()
{
    sortHelper(nullptr);
}

QIcon HeaderItem::icon() const
{    
    return _icon;
}

HeaderItem* HeaderItem::setDefaultSectionSizeCharCount(int c)
{
    return setDefaultSectionSize(fontMetrics().horizontalAdvance(AVERAGE_CHAR) * c + margin() * 2);
}

HeaderItem* HeaderItem::setDefaultSectionSize(int sectionSize)
{
    if (!isRoot())
        return root()->setDefaultSectionSize(sectionSize);

    _default_section_size = sectionSize;

    return this;
}

int HeaderItem::defaultSectionSize() const
{
    if (!isRoot())
        return root()->defaultSectionSize();

    return _default_section_size;
}

HeaderItem* HeaderItem::setSectionSizeCharCount(int c)
{
    Q_ASSERT(orientation() == Qt::Horizontal);

    return setSectionSize(fontMetrics().horizontalAdvance(AVERAGE_CHAR) * c + margin() * 2);
}

HeaderItem* HeaderItem::setSectionSize(int width)
{    
    setSizeSplitHelper(width, true);
    return this;
}

int HeaderItem::sectionSize() const
{    
    return _section_size;
}

int HeaderItem::verticalCellWidth() const
{
    Q_ASSERT(orientation() == Qt::Vertical);
    return levelSize();
}

HeaderItem* HeaderItem::setVerticalCellWidth(int size)
{
    Q_ASSERT(orientation() == Qt::Vertical);

    setLevelSize(size);
    return this;
}

HeaderItem* HeaderItem::setVerticalCellCharCount(int c)
{
    return setVerticalCellWidth(fontMetrics().horizontalAdvance(AVERAGE_CHAR) * c + margin() * 2);
}

HeaderItem* HeaderItem::setResizeMode(QHeaderView::ResizeMode mode)
{
    if (_resize_mode == mode)
        return this;

    _resize_mode = mode;

    if (isBottom() && !isUpdating()) {
        emit root()->sg_resizeModeChanged(sectionFrom(), mode);

    } else {
        for (HeaderItem* h : qAsConst(_children)) {
            h->setResizeMode(mode);
        }
    }

    return this;
}

QHeaderView::ResizeMode HeaderItem::resizeMode() const
{
    return _resize_mode;
}

HeaderItem* HeaderItem::setHidden(bool b)
{
    QList<HeaderItem*> changed = setHiddenHelper(b);
    if (!changed.isEmpty() && !isUpdating()) {
        clearCache();
        calculateSectionsSize();
        emit root()->sg_hiddenChanged(changed, b);
    }

    return this;
}

QList<HeaderItem*> HeaderItem::setHiddenHelper(bool b)
{
    if (_is_hidden == b && isBottom())
        return {};

    _is_hidden = b;

    if (isBottom()) {
        return {this};

    } else {
        QList<HeaderItem*> res;
        for (HeaderItem* h : qAsConst(_children)) {
            res << h->setHiddenHelper(b);
        }
        return res;
    }
}

QList<HeaderItem*> HeaderItem::setPermanentHiddenHelper(bool b)
{
    if (_is_permananet_hidden == b && isBottom())
        return {};

    bool old_hidden = isHidden();
    _is_permananet_hidden = b;

    if (isBottom()) {
        if (old_hidden != isHidden())
            return {this};
        else
            return {};

    } else {
        QList<HeaderItem*> res;
        for (HeaderItem* h : qAsConst(_children)) {
            res << h->setPermanentHiddenHelper(b);
        }
        return res;
    }
}

bool HeaderItem::isHidden() const
{
    return _is_hidden || _is_permananet_hidden;
}

static const int _header_item_structure_version = 1;
QByteArray HeaderItem::toByteArray(int data_stream_version) const
{
    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    ds.setVersion(data_stream_version);

    ds << _header_item_structure_version;
    ds << isRoot();

    toByteArrayHelper(ds);

    return data;
}

void HeaderItem::toByteArrayHelper(QDataStream& ds) const
{
    if (!isRoot()) {
        ds << _id;
        ds << _label;

        ds << (_font != nullptr);
        if (_font != nullptr)
            ds << *_font;

        ds << _bold;
        ds << _italic;
        ds << _underline;
        ds << _foreground;
        ds << _background;
        ds << _icon;
        ds << static_cast<int>(_resize_mode);
        ds << _is_hidden;
        ds << _is_permananet_hidden;
    }

    ds << _children.count();
    for (auto c : qAsConst(_children)) {
        c->toByteArrayHelper(ds);
    }
}

void HeaderItem::fromByteArrayHelper(QDataStream& ds)
{
    int c_count;
    ds >> c_count;
    for (int i = 0; i < c_count; i++) {
        int id;
        QString label;

        ds >> id;
        ds >> label;

        auto item = append(id, label);

        bool has_font;
        ds >> has_font;
        if (has_font) {
            QFont font;
            ds >> font;
            item->setFont(font);
        }

        bool bool_v;
        ds >> bool_v;
        item->setBold(bool_v);

        ds >> bool_v;
        item->setItalic(bool_v);

        ds >> bool_v;
        item->setUnderline(bool_v);

        QColor color;
        ds >> color;
        item->setForeground(color);

        ds >> color;
        item->setBackground(color);

        QIcon icon;
        ds >> icon;
        item->setIcon(icon);

        int resize_mode;
        ds >> resize_mode;
        item->setResizeMode(static_cast<QHeaderView::ResizeMode>(resize_mode));

        ds >> bool_v;
        item->setHidden(bool_v);

        ds >> bool_v;
        item->setPermanentHidden(bool_v);

        item->fromByteArrayHelper(ds);
    }
    Q_ASSERT(ds.status() == QDataStream::Ok);
}

void HeaderItem::fromByteArray(const QByteArray& data, int data_stream_version)
{
    beginUpdate();

    clear();

    QDataStream ds(data);
    ds.setVersion(data_stream_version);

    int version;
    ds >> version;
    Q_ASSERT(version == _header_item_structure_version);

    bool is_root;
    ds >> is_root;
    Q_ASSERT(is_root == isRoot());

    fromByteArrayHelper(ds);

    endUpdate();
}

bool HeaderItem::move(int id, bool before)
{
    return move(root()->item(id), before);
}

bool HeaderItem::move(HeaderItem* item, bool before)
{
    Q_ASSERT(item != nullptr && !isRoot());

    if (item == this)
        return true;

    if (item->parent() != parent())
        return false;

    int from_pos = _parent->_children_visual_order.indexOf(this);
    int to_pos = _parent->_children_visual_order.indexOf(item);
    int to_pos_real = to_pos;

    if (from_pos < to_pos && before)
        to_pos_real--;
    else if (from_pos > to_pos && !before)
        to_pos_real++;

    _parent->_children_visual_order.move(from_pos, to_pos_real);

    clearCache();

    if (!isUpdating())
        emit root()->sg_visualMoved(this, from_pos, to_pos, before);

    return true;
}

HeaderItem* HeaderItem::setPermanentHidden(bool b)
{
    Q_ASSERT(!isRoot());

    QList<HeaderItem*> changed = setPermanentHiddenHelper(b);
    if (!changed.isEmpty() && !isUpdating()) {
        clearCache();
        emit root()->sg_hiddenChanged(changed, b);
    }

    return this;
}

bool HeaderItem::isPermanentHidded() const
{
    return _is_permananet_hidden;
}

HeaderItem* HeaderItem::parent() const
{
    return _parent;
}

HeaderItem* HeaderItem::topParent() const
{
    Q_ASSERT(!isRoot());

    const HeaderItem* top = this;
    while (top != nullptr) {
        if (top->parent() == root())
            return const_cast<HeaderItem*>(top);
        top = top->parent();
    }
    Q_ASSERT(false); // тут быть не должны
    return nullptr;
}

HeaderItem* HeaderItem::previousVisual(bool use_cache) const
{
    Q_ASSERT(!isRoot());
    int pos =  use_cache? visualPos() : visualPosHelper(false);
    Q_ASSERT(pos >= 0);

    HeaderItem* found_item = nullptr;
    if (pos == 0) {
        if (!isTop()) {
            // надо найти крайнего справа на этом уровне у левого узла верхнего уровня
            HeaderItem* left_item = parent();
            // сначала идем вверх и находим первый узел, у которого есть сосед слева
            while (left_item != nullptr && !left_item->isRoot()) {
                if ((pos = (use_cache? left_item->visualPos() : left_item->visualPosHelper(false))) > 0) {
                    // затем спускаемся по его правой стороне до уровня данного узла или до нижнего
                    left_item = left_item->parent()->childVisual(pos - 1);
                    while (true) {
                        if (left_item->isBottom() || left_item->levelFrom() <= levelFrom()) {
                            found_item = left_item;
                            break;
                        }
                        left_item = left_item->childrenVisual(Qt::AscendingOrder).constLast();
                    }
                    break;
                }

                left_item = left_item->parent();
            }
        }
    } else {
        found_item = parent()->childVisual(pos - 1);
    }

    return found_item;
}

HeaderItem* HeaderItem::secondVisual(bool use_cache) const
{
    Q_ASSERT(!isRoot());
    int pos = visualPos();
    Q_ASSERT(pos >= 0);

    HeaderItem* found_item = nullptr;
    if (pos == count() - 1) {
        if (!isTop()) {
            // надо найти крайнего слева на этом уровне у правого узла верхнего уровня
            HeaderItem* right_item = parent();
            // сначала идем вверх и находим первый узел, у которого есть сосед слева
            while (right_item != nullptr && !right_item->isRoot()) {
                if ((pos = (use_cache ? right_item->visualPos() : right_item->visualPosHelper(false))) < right_item->parent()->count() - 1) {
                    // затем спускаемся по его левой стороне до уровня данного узла или до нижнего
                    right_item = right_item->parent()->childVisual(pos + 1);
                    while (true) {
                        if (right_item->isBottom() || right_item->levelFrom() <= levelFrom()) {
                            found_item = right_item;
                            break;
                        }
                        right_item = right_item->childrenVisual(Qt::AscendingOrder).constFirst();
                    }
                    break;
                }

                right_item = right_item->parent();
            }
        }
    } else {
        found_item = parent()->childVisual(pos + 1);
    }

    return found_item;
}

QList<HeaderItem*> HeaderItem::children() const
{
    return _children;
}

HeaderItem* HeaderItem::child(int pos) const
{
    Q_ASSERT(pos >= 0 && pos < count());
    return _children.at(pos);
}

HeaderItem* HeaderItem::childVisual(int visual_pos, Qt::SortOrder order) const
{
    Q_ASSERT(visual_pos >= 0 && visual_pos < count());
    return childrenVisual(order).at(visual_pos);
}

int HeaderItem::count() const
{
    return _children.count();
}

bool HeaderItem::hasChildren() const
{
    return count() > 0;
}

int HeaderItem::pos() const
{
    Q_ASSERT(!isRoot());
    return parent()->childPos(const_cast<HeaderItem*>(this));
}

int HeaderItem::bottomPos() const
{
    if (!isBottom())
        return -1;
    return root()->allBottom().indexOf(const_cast<HeaderItem*>(this));
}

int HeaderItem::bottomVisualPos(Qt::SortOrder order, bool visible_only) const
{
    if (!isBottom())
        return -1;
    return allBottomVisual(order, visible_only).indexOf(const_cast<HeaderItem*>(this));
}

QList<HeaderItem*> HeaderItem::childrenVisual(Qt::SortOrder order, bool visible_only) const
{
    QList<HeaderItem*> items = _children_visual_order;
    if (order == Qt::DescendingOrder)
        std::reverse(items.begin(), items.end());

    if (!visible_only)
        return items;

    QList<HeaderItem*> res;
    for (auto h : qAsConst(items)) {
        if (!h->isHidden())
            res << h;
    }
    return res;
}

int HeaderItem::visualPos(bool visible_only) const
{
    Q_ASSERT(!isRoot());
    updateCache();
    return visible_only? root()->_all_visual_pos_visible_only.value(_id,-1) : root()->_all_visual_pos.value(_id,-1);
}

QList<HeaderItem*> HeaderItem::allChildren(int depth) const
{
    if (depth == 0)
        return QList<HeaderItem*>();

    QList<HeaderItem*> res = _children;
    for (HeaderItem* h : _children) {
        res << h->allChildren(depth < 0 ? -1 : depth - 1);
    }
    return res;
}

QList<HeaderItem*> HeaderItem::allParent() const
{
    QList<HeaderItem*> res;

    HeaderItem* parent = _parent;
    while (parent && !parent->isRoot()) {
        res << parent;
        parent = parent->parent();
    }

    return res;
}

QList<HeaderItem*> HeaderItem::allBottom() const
{
    if (isBottom())
        return {const_cast<HeaderItem*>(this)};

    QList<HeaderItem*> res;
    for (HeaderItem* h : _children) {
        res += h->allBottom();
    }

    return res;
}

QList<HeaderItem*> HeaderItem::allBottomVisual(Qt::SortOrder order, bool visible_only) const
{
    if (isBottom())
        return {const_cast<HeaderItem*>(this)};

    return allBottomVisualHelper(order, visible_only);
}

HeaderItem* HeaderItem::bottomItem(int section) const
{
    if (isRoot()) {
        Q_ASSERT(section >= 0 && section < root()->sectionSpan());
        return root()->allBottom().at(section);
    } else {
        return root()->bottomItem(section);
    }
}

int HeaderItem::bottomCount(bool visible_only) const
{
    if (visible_only)
        return allBottomVisual(Qt::AscendingOrder, true).count();

    int res = allBottom().count();

#ifdef QT_DEBUG
    if (isRoot() && _update_counter == 0)
        Q_ASSERT(qMax(0, root()->sectionSpan()) == res);
#endif

    return res;
}

int HeaderItem::bottomPermanentVisibleCount() const
{
    return bottomPermanentVisible().count();
}

QList<HeaderItem*> HeaderItem::bottomPermanentVisible() const
{
    QList<HeaderItem*> res;
    auto bottom = allBottom();
    for (auto h : bottom) {
        if (!h->isPermanentHidded())
            res << h;
    }

    return res;
}

int HeaderItem::levelSizeToBottom() const
{
    int size = isRoot() ? 0 : levelSize();
    int max = 0;
    for (auto h : _children) {
        max = qMax(max, h->levelSizeToBottom());
    }
    return size + max;
}

int HeaderItem::levelSizeToTop() const
{
    int size = levelSize();
    if (!isTop())
        size += parent()->levelSizeToTop();
    return size;
}

bool HeaderItem::contains(HeaderItem* child) const
{
    return childPos(child) >= 0;
}

bool HeaderItem::contains(int child_id) const
{
    return childPos(child_id) >= 0;
}

int HeaderItem::childPos(HeaderItem* child) const
{
    Q_ASSERT(child != nullptr);
    return _children.indexOf(child);
}

int HeaderItem::childPos(int child_id) const
{
    for (int i = 0; i < count(); ++i) {
        if (_children.at(i)->id() == child_id)
            return i;
    }

    return -1;
}

void HeaderItem::recalc()
{
    if (!isRoot()) {
        root()->recalc();
        return;
    }

    if (_update_counter > 0)
        return;

    if (_model != nullptr)
        _model->beginUpdateHeader();

    clearCache();

    calculateSpan(0, 0);
    calculateSectionsSizeHelper();

    clearCache();

    if (_model != nullptr)
        _model->endUpdateHeader();
}

int HeaderItem::levelFrom() const
{
    return _level_from;
}

int HeaderItem::sectionFrom(bool logical, bool visible_only) const
{
    if (logical) {
        Q_ASSERT(!visible_only);
        return _section_from;
    }

    int section = INT_MAX;
    for (int i = _section_from; i <= _section_span_to; i++) {
        if (visible_only && isSectionHidden(i))
            continue;

        section = qMin(section, visualSection(i));
    }

    return section == INT_MAX ? -1 : section;
}

int HeaderItem::levelSpan() const
{
    return _level_span;
}

int HeaderItem::sectionSpan(bool visible_only) const
{
    if (!visible_only)
        return _section_span;

    int section = 0;
    for (int i = _section_from; i <= _section_span_to; i++) {
        if (isSectionHidden(i))
            continue;
        section++;
    }

    return section;
}

int HeaderItem::levelTo() const
{
    return _level_span_to;
}

int HeaderItem::sectionTo(bool logical, bool visible_only) const
{
    if (logical) {
        Q_ASSERT(!visible_only);
        return _section_span_to;
    }

    int section = -1;
    for (int i = _section_from; i <= _section_span_to; i++) {
        if (visible_only && isSectionHidden(i))
            continue;

        section = qMax(section, visualSection(i));
    }

    return section;
}

int HeaderItem::rowFrom() const
{
    return orientation() == Qt::Horizontal ? levelFrom() : sectionFrom();
}

int HeaderItem::columnFrom() const
{
    return orientation() == Qt::Horizontal ? sectionFrom() : levelFrom();
}

int HeaderItem::rowSpan() const
{
    return orientation() == Qt::Horizontal ? levelSpan() : sectionSpan();
}

int HeaderItem::columnSpan() const
{
    return orientation() == Qt::Horizontal ? sectionSpan() : levelSpan();
}

int HeaderItem::rowTo() const
{
    return orientation() == Qt::Horizontal ? levelTo() : sectionTo();
}

int HeaderItem::columnTo() const
{
    return orientation() == Qt::Horizontal ? sectionTo() : levelTo();
}

QSize HeaderItem::itemSize() const
{    
    updateCache();
    return root()->_all_sizes.value(_id);
}

QSize HeaderItem::itemGroupSize() const
{
    updateCache();
    return root()->_all_group_sizes.value(_id);
}

QRect HeaderItem::sectionRect() const
{
    updateCache();
    return root()->_all_sections_rect.value(_id);
}

QRect HeaderItem::groupRect() const
{
    updateCache();
    return root()->_all_group_rect.value(_id);
}

int HeaderItem::margin() const
{
    return qApp->style()->pixelMetric(QStyle::PM_HeaderMargin);
}

bool HeaderItem::isEmpty() const
{
    return _type == Type::Empty;
}

void HeaderItem::showHiddenSections()
{
    if (!isRoot())
        return root()->showHiddenSections();

    auto items = allChildren();
    for (auto h : items) {
        h->show();
    }
}

bool HeaderItem::canHideSection(HeaderItem* item) const
{
    Q_ASSERT(item != nullptr);
    if (!isRoot())
        return root()->canHideSection(item);

    auto all_bottom = allBottom();
    auto item_bottom = item->allBottom();
    for (auto h : all_bottom) {
        if (item_bottom.contains(h))
            continue;

        if (!h->isHidden())
            return true;
    }

    return false;
}

bool HeaderItem::hasHiddenSections() const
{
    if (!isRoot())
        return root()->hasHiddenSections();

    auto items = allBottom();
    for (auto h : items) {
        if (!h->isPermanentHidded() && h->isHidden())
            return true;
    }

    return false;
}

Qt::Orientation HeaderItem::orientation() const
{
    return _orientation;
}

QFontMetrics HeaderItem::fontMetrics() const
{
    return QFontMetrics(font());
}

HeaderItem* HeaderItem::append(int id, const QString& label)
{
    return insert(count(), id, label);
}

HeaderItem* HeaderItem::append(const QString& label)
{
    return append(root()->allChildren().count() + 1, label);
}

HeaderItem* HeaderItem::insert(int pos, int id, const QString& label)
{
    Q_ASSERT(pos >= 0 && pos <= count() && id >= 0);
    auto check_item = item(id, false);
    Q_ASSERT(check_item == nullptr);

    clearCache();

    HeaderItem* child = new HeaderItem(this, id, label);
    _children.insert(pos, child);
    _children_visual_order.insert(pos, child);

    connect(child, &HeaderItem::sg_structureChanged, this, &HeaderItem::sl_childStructureChanged);
    connect(child, &HeaderItem::sg_beforeDelete, this, &HeaderItem::sl_childDeleted);

    if (isRoot())
        recalc();

    emit sg_structureChanged();

    return child;
}

void HeaderItem::remove(HeaderItem* child)
{
    Q_ASSERT(child != nullptr);
    int pos = childPos(child);
    Q_ASSERT(pos >= 0);
    remove(pos);
}

void HeaderItem::remove(int pos)
{
    Q_ASSERT(pos >= 0 && pos < count());

    clearCache();

    auto c = _children.at(pos);
    _children.removeAt(pos);
    _children_visual_order.removeOne(c);
    delete c;

    emit sg_structureChanged();

    if (isRoot())
        recalc();
}

bool HeaderItem::isOrderChanged() const
{
    if (_children != _children_visual_order)
        return true;

    for (auto c : _children) {
        if (c->isOrderChanged())
            return true;
    }
    return false;
}

void HeaderItem::resetOrder()
{
    for (int i = _children.count() - 1; i >= 0; i--) {
        _children.at(i)->move(_children_visual_order.at(0), true);
        _children.at(i)->resetOrder();
    }
}

void HeaderItem::resetHidden()
{
    setHidden(false);
}

void HeaderItem::sl_childDeleted()
{
    clearCache();

    auto c = qobject_cast<HeaderItem*>(sender());
    Q_ASSERT(c != nullptr);

    if (childPos(c) >= 0)
        remove(c);

    emit sg_structureChanged();
}

void HeaderItem::sl_childStructureChanged()
{
    clearCache();

    emit sg_structureChanged();

    if (isRoot())
        recalc();
}

HeaderItem* HeaderItem::createRoot(Qt::Orientation orientation, ItemViewHeaderModel* model)
{
    return new HeaderItem(Type::Root, orientation, model);
}

HeaderItem* HeaderItem::createEmpty(Qt::Orientation orientation, ItemViewHeaderModel* model, int level, int section)
{
    HeaderItem* item = new HeaderItem(Type::Empty, orientation, model);
    item->_level_from = level;
    item->_level_span = 1;
    item->_level_span_to = level;
    item->_section_from = section;        
    item->_section_span = 1;
    item->_section_span_to = section;
    return item;
}

HeaderItem::HeaderItem(Type type, Qt::Orientation orientation, ItemViewHeaderModel* model)
    : QObject(type == Type::Empty ? model : nullptr)
    , _type(type)
    , _model(model)    
    , _orientation(orientation)
{
    Q_ASSERT(_model != nullptr);

    if (isRoot())
        _root = this;
    else
        _root = _model->rootItem();

    if (orientation == Qt::Horizontal) {
        setDefaultSectionSizeCharCount(10);
    } else {
        setVerticalCellCharCount(10);
        setDefaultSectionSize(qApp->style()->pixelMetric(QStyle::PM_HeaderDefaultSectionSizeVertical));
    }

    if (isRoot()) {
        _recalc_timer = new QTimer(this);
        _recalc_timer->setInterval(0);
        _recalc_timer->setSingleShot(true);
        connect(_recalc_timer, &QTimer::timeout, this, [&]() { calculateSectionsSizeHelper(); });
    } else {
        Q_ASSERT(isEmpty());
    }

    _is_initialized = true;
}

HeaderItem::HeaderItem(HeaderItem* parent, int id, const QString& label)
    : QObject()
    , _type(Type::Item)
    , _parent(parent)
    , _id(id)
{
    Q_ASSERT(_parent != nullptr);
    _orientation = _parent->_orientation;
    _model = _parent->_model;
    _root = _parent->_root;
    _section_size = root()->_default_section_size;
    _level_size = _parent->_level_size;
    setLabel(label);

    _is_initialized = true;
}

void HeaderItem::dataChanged(int role)
{
    if (!_is_initialized || _model == nullptr || isRoot() || isUpdating())
        return;

    _model->itemDataChanged(this, role);
}

QList<HeaderItem*> HeaderItem::allBottomVisualHelper(Qt::SortOrder order, bool visible_only) const
{
    QList<HeaderItem*> res;
    QList<HeaderItem*> children = childrenVisual(order, visible_only);
    for (auto h : children) {        
        if (h->isBottom())
            res += h;
        else
            res += h->allBottomVisualHelper(order, visible_only);
    }

    return res;
}

void HeaderItem::setPermanentHiddenSection(int logicalIndex, bool hidden)
{
    bottomItem(logicalIndex)->setPermanentHidden(hidden);
}

bool HeaderItem::isSectionPermanentHidden(int logicalIndex) const
{
    return bottomItem(logicalIndex)->isPermanentHidded();
}

void HeaderItem::sortHelper(HeaderItem* item, Qt::SortOrder order)
{
    if (!isRoot()) {
        root()->sortHelper(item, order);
        return;
    }

    Q_ASSERT(item == nullptr || item->isBottom());
    Q_ASSERT(orientation() == Qt::Horizontal);

    HeaderItem* current = currentSortingItem();
    if (current != nullptr) {
        if (item == nullptr) {
            // отключаем сортировку
            current->_sorting = false;
            current->_sort_order = Qt::AscendingOrder;
        } else {
            if (current == item) {
                if (current->_sort_order == order)
                    // ничего не изменилось
                    return;

                current->_sorting = true;
                current->_sort_order = order;

            } else {
                // отключаем у предыдущего
                current->_sorting = false;
                current->_sort_order = Qt::AscendingOrder;
                // включаем у нового
                item->_sorting = true;
                item->_sort_order = order;
            }
        }

    } else {
        if (item == nullptr)
            // ничего не изменилось
            return;
        // включаем у нового
        item->_sorting = true;
        item->_sort_order = order;
    }

    emit sg_sortChanged(
        current == nullptr ? -1 : current->sectionFrom(), item == nullptr ? -1 : item->sectionFrom(), order);
}

HeaderItem* HeaderItem::currentSortingItem() const
{
    if (!isRoot())
        return root()->currentSortingItem();

    auto bottom = allBottom();
    for (auto h : bottom) {
        if (h->isSorting())
            return h;
    }
    return nullptr;
}

bool HeaderItem::isSectionHidden(int section) const
{
    return bottomItem(section)->isHidden();
}

int HeaderItem::visualSection(int logical_section) const
{
    Q_ASSERT(logical_section >= 0 && logical_section < root()->sectionSpan());

    if (!isRoot())
        return root()->visualSection(logical_section);

    return allBottomVisual(Qt::AscendingOrder).indexOf(allBottom().at(logical_section));
}

int HeaderItem::logicalSection(int visual_section) const
{
    Q_ASSERT(visual_section >= 0 && visual_section < root()->sectionSpan());

    if (!isRoot())
        return root()->logicalSection(visual_section);

    return allBottom().indexOf(allBottomVisual(Qt::AscendingOrder).at(visual_section));
}

int HeaderItem::realVisualSection(int visual_section) const
{
    if (!isRoot())
        return root()->realVisualSection(visual_section);

    if (visual_section < 0)
        return -1;

    int res = 0;
    for (int i = 0; i <= visual_section; i++) {
        if (isSectionHidden(logicalSection(i)))
            continue;
        res++;
    }
    return res - 1;
}

int HeaderItem::firstVisibleSection() const
{
    if (!isRoot())
        return root()->firstVisibleSection();

    for (int i = 0; i < root()->sectionSpan(); i++) {
        int logical = logicalSection(i);
        if (!isSectionHidden(logical))
            return logical;
    }

    return -1;
}

int HeaderItem::lastVisibleSection() const
{
    if (!isRoot())
        return root()->lastVisibleSection();

    for (int i = root()->sectionSpan() - 1; i >= 0; i--) {
        int logical = logicalSection(i);
        if (!isSectionHidden(logical))
            return logical;
    }

    return -1;
}

void HeaderItem::setSectionsSizes(const QMap<int, int>& sizes)
{
    if (sizes.isEmpty())
        return;

    if (!isRoot()) {
        root()->setSectionsSizes(sizes);
        return;
    }

    QList<HeaderItem*> items = allBottom();

    bool changed = false;
    auto s_keys = sizes.keys();
    QSet<int> not_used(s_keys.begin(), s_keys.end());
    for (auto h : items) {
        not_used.remove(h->sectionFrom());

        if (h->isHidden())
            continue;

        int size = sizes.value(h->sectionFrom(), -1);
        if (size < 0 || h->_section_size == size)
            continue;

        h->_section_size = size;

        changed = true;
    }

    if (changed)
        clearCache();

    if (orientation() == Qt::Vertical) {
        for (int section : not_used) {
            int size = sizes.value(section, -1);
            if (size <= 0 || section < sectionSpan())
                continue;

            emit sg_outOfRangeResized(section, size);
        }
    }

    if (changed)
        calculateSectionsSize();
}

QPoint HeaderItem::sectionCorner(bool use_cache) const
{
    if (isHidden())
        return QPoint(0, 0);

    if (isTop()) {
        int pos = 0;
        auto top_items = root()->childrenVisual(Qt::AscendingOrder);
        for (auto h : top_items) {
            if ((!use_cache && h->visualPosHelper(false) >= visualPosHelper(false)) || (use_cache && h->visualPos() >= visualPos()))
                break;
            pos +=  h->sectionSize();
        }

        if (orientation() == Qt::Horizontal)
            return QPoint(pos, 0);
        else
            return QPoint(0, pos);
    }

    QPoint point(0, 0);
    HeaderItem* parent = this->parent();
    HeaderItem* prev = previousVisual(use_cache);
    if (orientation() == Qt::Horizontal) {
        if (prev != nullptr)
            point.setX((use_cache? prev->sectionRect().right() : prev->sectionRectHelper().right()) + 1);
        if (!isTop())
            point.setY((use_cache? parent->sectionRect().bottom() : parent->sectionRectHelper().bottom()) + 1);
    } else {
        if (!isTop())
            point.setX((use_cache? parent->sectionRect().right() : parent->sectionRectHelper().right()) + 1);
        if (prev != nullptr)
            point.setY((use_cache? prev->sectionRect().bottom() : prev->sectionRectHelper().bottom()) + 1);
    }

    return point;
}

QString HeaderItem::labelMultiline() const
{
    if (!_icon.isNull() && _hide_label)
        return QString();

    return _label_multi_line.isEmpty() && _icon.isNull()
        ? QString::number(1 + (orientation() == Qt::Horizontal ? _section_from : _level_from))
        : _label_multi_line;
}

bool HeaderItem::calculateLabelMultiline()
{
    Q_ASSERT(!isRoot() && !isEmpty());

    int icon_shift = 0;
    if (!_icon.isNull())
        icon_shift = qApp->style()->pixelMetric(QStyle::PM_SmallIconSize) + ICON_LEFT_SHIFT;

    int width = _orientation == Qt::Horizontal ? _section_size : _level_size;

    QString multi_line;
    if (width <= 0)
        multi_line = _label;
    else
        multi_line = Hyphenation::GlobalTextHyphenationFormatter::stringToMultiline(
            fontMetrics(), _label, width - margin() * 2 - icon_shift);

    if (_label_multi_line == multi_line)
        return false;

    _label_multi_line = multi_line;
    return true;
}

int HeaderItem::levelSize() const
{
    return _level_size;
}

int HeaderItem::groupLevelSize() const
{
    return _group_level_size;
}

HeaderItem* HeaderItem::setLevelSize(int size)
{
    Q_ASSERT(_orientation == Qt::Vertical);

    if (_level_size == size)
        return this;

    _level_size = size;
    dataChanged(Qt::SizeHintRole);

    return this;
}

int HeaderItem::calculateSpan(int level, int section)
{
    int level_count = calcLevelCount();
    int section_count = calcSectionCount();

    if (isRoot()) {
        _level_span = qMax(1, level_count - 1);
        _section_span = section_count;

    } else {
        _level_from = level;
        _level_span = 1;
        _section_from = section;

        _section_span = section_count;
        _level_span_to = _level_span > 0 ? _level_from + _level_span - 1 : _level_from;
        _section_span_to = _section_span > 0 ? _section_from + _section_span - 1 : _section_from;
    }

    for (HeaderItem* h : qAsConst(_children)) {
        if (isRoot())
            section += h->calculateSpan(level, section);
        else
            section += h->calculateSpan(level + 1, section);
    }

    if (isRoot()) {
        for (HeaderItem* h : qAsConst(_children)) {
            h->calculateLevelSpan(level_count - 1);
        }
    }

    return section_count;
}

void HeaderItem::calculateSectionsSize()
{
    if (!_is_initialized || isUpdating())
        return;

    if (!isRoot()) {
        root()->calculateSectionsSize();
        return;
    }
    if (!_recalc_timer->isActive())
        _recalc_timer->start();
}

void HeaderItem::calculateSectionsSizeHelper()
{
    if (isRoot())
        _model->beginUpdateHeader();

    clearCache();

    // сначала расчитываем все дочерние узлы
    for (HeaderItem* h : qAsConst(_children)) {
        h->calculateSectionsSizeHelper();
    }

    int max_group_level_size = 0;

    if (!isBottom()) {
        int new_section_size = 0;
        for (auto h : qAsConst(_children)) {
            if (h->isHidden())
                continue;
            max_group_level_size = qMax(max_group_level_size, h->groupLevelSize());
            new_section_size += h->sectionSize();
        }

        _section_size = new_section_size;
    }

    if (isRoot()) {
        _level_size = max_group_level_size;
        _group_level_size = max_group_level_size;

        _model->endUpdateHeader();

    } else {
        calculateLabelMultiline();
        if (orientation() == Qt::Horizontal) {
            if (labelMultiline().isEmpty())
                _level_size = qApp->style()->pixelMetric(QStyle::PM_SmallIconSize);
            else
                _level_size = fontMetrics().size(0, labelMultiline()).height() + margin() * 2;
        }

        _group_level_size = _level_size + max_group_level_size;
    }
}

int HeaderItem::visualPosHelper(bool visible_only) const
{
    Q_ASSERT(!isRoot());

    auto items = parent()->childrenVisual(Qt::AscendingOrder);

    if (!visible_only) {
        int pos = items.indexOf(const_cast<HeaderItem*>(this));
        Q_ASSERT(pos >= 0);
        return pos;
    }

    int pos = 0;
    for (auto h : items) {
        if (h->isHidden())
            continue;

        if (h == this)
            return pos;

        pos++;
    }

    return -1;
}

QSize HeaderItem::itemSizeHelper() const
{
    if (isHidden())
        return QSize(0, 0);

    int diff = 0;
    if (isBottom()) {
        // надо выровнять высоту у всех элементов нижнего уровня
        auto all_parent = allParent();
        int bottom_size = _level_size;
        for (auto h : all_parent) {
            bottom_size += h->levelSize();
        }

        diff = root()->levelSize() - bottom_size;
    }

    QSize res = _orientation == Qt::Horizontal ? QSize(_section_size, _level_size + diff)
                                               : QSize(_level_size + diff, _section_size);
    return res;
}

QSize HeaderItem::itemGroupSizeHelper() const
{
    Q_ASSERT(!isRoot());
    QSize size(0, 0);

    if (isHidden())
        return size;

    if (isBottom()) {
        size = itemSizeHelper();

    } else {
        for (auto h : _children) {
            if (orientation() == Qt::Horizontal)
                size.setHeight(qMax(size.height(), h->itemGroupSizeHelper().height()));
            else
                size.setWidth(qMax(size.width(), h->itemGroupSizeHelper().width()));
        }

        QSize section_size = itemSizeHelper();
        if (orientation() == Qt::Horizontal) {
            size.setWidth(section_size.width());
            size.setHeight(size.height() + section_size.height());
        } else {
            size.setHeight(section_size.height());
            size.setWidth(size.width() + section_size.width());
        }
    }

    return size;
}

QRect HeaderItem::sectionRectHelper() const
{
    QRect rect;
    rect.setSize(itemSizeHelper());
    if (rect.size() == QSize(0, 0))
        return QRect();

    QPoint corner = sectionCorner(false);
    rect.moveTo(corner);

    return rect;
}

QRect HeaderItem::groupRectHelper() const
{
    QRect rect;
    rect.setSize(itemGroupSizeHelper());
    if (rect.size() == QSize(0, 0))
        return QRect();

    QPoint corner = sectionCorner(false);
    rect.moveTo(corner);

    return rect;
}

HeaderItem *HeaderItem::findByPosHelper(int level, int section, bool use_span) const
{
    if (!isRoot()) {
        if (use_span) {
            if (level >= _level_from && (level <= _level_span_to || (isBottom() && level >= _level_from))
                && section >= _section_from && section <= _section_span_to)
                return const_cast<HeaderItem*>(this);
        } else {
            if (_level_from == level && _section_from == section)
                return const_cast<HeaderItem*>(this);
        }
    }

    for (HeaderItem* h : _children) {
        HeaderItem* res = h->findByPosHelper(level, section, use_span);
        if (res != nullptr)
            return res;
    }

    return nullptr;
}

void HeaderItem::clearCache() const
{
    if (!isRoot()) {
        root()->clearCache();
        return;
    }

    if (!_cached)
        return;

    _cached = false;

    _all_children_by_id.clear();
    _all_visual_pos.clear();
    _all_visual_pos_visible_only.clear();
    _all_sections_rect.clear();
    _all_group_rect.clear();
    _all_sizes.clear();
    _all_group_sizes.clear();
    _all_find_by_pos.clear();
    _all_find_by_pos_span.clear();
}

void HeaderItem::updateCache() const
{
    if (!isRoot()) {
        root()->updateCache();
        return;
    }

    if (_cached)
        return;

    Q_ASSERT(!_is_cache_updating);
    _is_cache_updating = true;

    if (!_children.isEmpty()) {
        auto all = allChildren();
        for (auto h : all) {
            _all_children_by_id[h->_id] = h;
            _all_visual_pos[h->_id] = h->visualPosHelper(false);
            _all_visual_pos_visible_only[h->_id] = h->visualPosHelper(true);
            _all_sections_rect[h->_id] = h->sectionRectHelper();
            _all_group_rect[h->_id] = h->groupRectHelper();
            _all_sizes[h->_id] = h->itemSizeHelper();
            _all_group_sizes[h->_id] = h->itemGroupSizeHelper();
        }
    }

    _is_cache_updating = false;
    _cached = true;
}

void HeaderItem::calculateLevelSpan(int level_count)
{
    if (isBottom()) {
        if (_level_from + _level_span < level_count) {
            _level_span = level_count - _level_from;
        }

    } else {
        for (HeaderItem* h : qAsConst(_children)) {
            h->calculateLevelSpan(level_count);
        }
    }
}

int HeaderItem::calcLevelCount() const
{
    int count = 1;
    int max_child = 0;
    for (HeaderItem* h : _children) {
        max_child = qMax(max_child, h->calcLevelCount());
    }
    return count + max_child;
}

int HeaderItem::calcSectionCount() const
{
    if (isBottom())
        return 1;

    int count = 0;
    for (HeaderItem* h : _children) {
        count += h->calcSectionCount();
    }
    return count;
}

static bool _findByPos_processing = false;
HeaderItem* HeaderItem::findByPos(int level, int section, bool use_span) const
{
    Q_ASSERT(!_findByPos_processing);

    _findByPos_processing = true;

    auto key = QPair<int,int>(level,section);
    HeaderItem* res = use_span? _all_find_by_pos_span.value(key,nullptr) : _all_find_by_pos.value(key,nullptr);

    if (res == nullptr) {
        res = findByPosHelper(level,section,use_span);
        if (use_span)
            _all_find_by_pos_span[key] = res;
        else
            _all_find_by_pos[key] = res;
    }

    _findByPos_processing = false;
    return res;
}

HeaderItem* HeaderItem::findByPos(const QModelIndex& index, bool use_span) const
{
    if (!index.isValid())
        return nullptr;

    return orientation() == Qt::Horizontal ? findByPos(index.row(), index.column(), use_span)
                                           : findByPos(index.column(), index.row(), use_span);
}

void HeaderItem::setSizeSplitHelper(int size, bool split_size)
{
    if (isBottom() || isEmpty()) {
        if (_section_size == size)
            return;

        _section_size = size;
        dataChanged(Qt::SizeHintRole);
        calculateSectionsSize();

    } else {
        bool changed = _section_size != size;

        _section_size = size;

        int child_size = split_size ? size / count() : size;
        if (child_size == 0)
            child_size = 1;

        for (HeaderItem* h : qAsConst(_children)) {
            h->setSizeSplitHelper(child_size, true);
        }

        if (changed)
            dataChanged(Qt::SizeHintRole);
    }
}

} // namespace zf
