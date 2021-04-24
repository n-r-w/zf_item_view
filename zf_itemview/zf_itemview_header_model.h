#pragma once

#include "zf_itemview.h"
#include <QIdentityProxyModel>

namespace zf
{
class HeaderItem;

//! Хранение информации об иерархическом заголовке
class ItemViewHeaderModel : public QIdentityProxyModel
{
    Q_OBJECT
public:
    enum HeaderRole
    {
        HeaderItemRole = Qt::UserRole + 1,
    };

    ItemViewHeaderModel(Qt::Orientation orientation, QObject* parent = nullptr);
    ~ItemViewHeaderModel() override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    HeaderItem* item(const QModelIndex& index, bool use_span) const;
    HeaderItem* item(int row, int column, bool use_span) const;
    QModelIndex index(HeaderItem* item) const;

    HeaderItem* rootItem() const;

    void beginUpdateHeader();
    void endUpdateHeader();
    void itemDataChanged(HeaderItem* item, int role);

signals:
    void sg_itemDataChanged(zf::HeaderItem* item, int role);

private:
    Qt::Orientation _orientation;
    HeaderItem* _root_item = nullptr;
    mutable QHash<QPair<int, int>, HeaderItem*> _empty_items;
    int _update_counter = 0;
};

} // namespace zf
