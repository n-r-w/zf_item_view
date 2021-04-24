#include "zf_itemview_header_model.h"
#include "zf_utils.h"
#include "zf_header_view.h"
#include "zf_itemview_header_item.h"

#include <QDebug>

namespace zf
{
ItemViewHeaderModel::ItemViewHeaderModel(Qt::Orientation orientation, QObject* parent)
    : QIdentityProxyModel(parent)
    , _orientation(orientation)
    , _root_item(HeaderItem::createRoot(orientation, this))
{
}

ItemViewHeaderModel::~ItemViewHeaderModel()
{    
    delete _root_item;
}

QModelIndex ItemViewHeaderModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    HeaderItem* item = this->item(row, column, false);
    if (item == nullptr) {
        item = HeaderItem::createEmpty(_orientation, const_cast<ItemViewHeaderModel*>(this),
            _orientation == Qt::Horizontal ? row : column, _root_item->orientation() == Qt::Horizontal ? column : row);
        _empty_items[QPair<int, int>(row, column)] = item;
    }
    return createIndex(row, column, item);
}

int ItemViewHeaderModel::rowCount(const QModelIndex& parent) const
{
    return QIdentityProxyModel::rowCount(parent);
}

int ItemViewHeaderModel::columnCount(const QModelIndex& parent) const
{
    return QIdentityProxyModel::columnCount(parent);
}

Qt::ItemFlags ItemViewHeaderModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return QIdentityProxyModel::flags(index);
}

QVariant ItemViewHeaderModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || index.row() < 0 || index.column() >= columnCount()
        || index.column() < 0) {
        if (role == Qt::SizeHintRole)
            return QSize(0, 0);
        else
            return QVariant();
    }

    HeaderItem* item = static_cast<HeaderItem*>(index.internalPointer());
    if (item == nullptr || item->isEmpty()) {
        if (role == Qt::SizeHintRole)
            return QSize(0, 0);
        else
            return QVariant();
    }

    Q_ASSERT(!item->isRoot());

    switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return item->labelMultiline();
        case Qt::DecorationRole:
            return item->icon();
        case Qt::FontRole:
            return item->font();
        case Qt::BackgroundRole:
            return item->background();
        case Qt::ForegroundRole:
            return item->foreground();
        case Qt::SizeHintRole: {
            if (_orientation == Qt::Horizontal)
                return QSize(item->sectionSize(), item->levelSize());
            else
                return QSize(item->levelSize(), item->sectionSize());
        }
        case HeaderItemRole:
            return reinterpret_cast<quintptr>(item);

        default:
            return QVariant();
    }
}

bool ItemViewHeaderModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    return QIdentityProxyModel::setData(index, value, role);
}

HeaderItem* ItemViewHeaderModel::item(const QModelIndex& index, bool use_span) const
{
    if (!index.isValid())
        return nullptr;

    Q_ASSERT(index.model() == this);
    return item(index.row(), index.column(), use_span);
}

HeaderItem* ItemViewHeaderModel::item(int row, int column, bool use_span) const
{
    HeaderItem* item = _root_item->findByPos(_root_item->orientation() == Qt::Horizontal ? row : column,
        _root_item->orientation() == Qt::Horizontal ? column : row, use_span);
    if (item != nullptr || use_span)
        return item;

    return _empty_items.value(QPair<int, int>(row, column), nullptr);
}

QModelIndex ItemViewHeaderModel::index(HeaderItem* item) const
{
    Q_ASSERT(item != nullptr);
    return index(item->rowFrom(), item->columnFrom());
}

HeaderItem* ItemViewHeaderModel::rootItem() const
{
    return _root_item;
}

void ItemViewHeaderModel::beginUpdateHeader()
{
    _update_counter++;
    if (_update_counter > 1)
        return;

    beginResetModel();
}

void ItemViewHeaderModel::endUpdateHeader()
{
    _update_counter--;
    Q_ASSERT(_update_counter >= 0);
    if (_update_counter > 0)
        return;

    endResetModel();
}

void ItemViewHeaderModel::itemDataChanged(HeaderItem* item, int role)
{
    if (_update_counter > 0)
        return;

    Q_ASSERT(item != nullptr);
    QModelIndex index = this->index(item->rowFrom(), item->rowTo());

    emit dataChanged(index, index, {role});
    emit sg_itemDataChanged(item, role);
}

} // namespace zf
