#include "zf_utils.h"
#include "zf_itemview_header_item.h"
#include "zf_header_view.h"

#include <QApplication>
#include <QPalette>
#include <QAbstractProxyModel>
#include <QMainWindow>
#include <QPainter>
#include <QTime>
#include <QDateTime>
#include <QIODevice>

namespace zf
{
//! Количество знаков после запятой для дробных чисел
const int Utils::DOUBLE_DECIMALS = 4;

QColor Utils::uiLineColor(bool bold)
{
    return qApp->palette().color(bold ? QPalette::Mid : QPalette::Midlight);
}

QColor Utils::uiWindowColor()
{
    return qApp->palette().color(QPalette::Normal, QPalette::Window);
}

QColor Utils::uiButtonColor()
{
    return qApp->palette().color(QPalette::Normal, QPalette::Button);
}

QColor Utils::uiDarkColor()
{
    return qApp->palette().color(QPalette::Normal, QPalette::Dark);
}

QColor Utils::uiAlternateTableBackgroundColor()
{
    return QColor(252, 248, 227);
}

QColor Utils::uiInfoTableBackgroundColor()
{
    return QColor(255, 214, 0);
}

QColor Utils::uiInfoTableTextColor()
{
    return QColor(162, 0, 37);
}

QPen Utils::pen(const QColor& color, int width)
{
    QPen pen = QPen(color, width);
    pen.setCosmetic(true);
    return pen;
}

QModelIndex Utils::getTopSourceIndex(const QModelIndex& index)
{
    if (!index.isValid())
        return index;

    const QAbstractProxyModel* proxy;
    QModelIndex source_index = index;
    do {
        proxy = qobject_cast<const QAbstractProxyModel*>(source_index.model());
        if (proxy != nullptr)
            source_index = proxy->mapToSource(source_index);

    } while (proxy != nullptr);

    Q_ASSERT(source_index.isValid());
    return source_index;
}

QModelIndex Utils::getNextItemModelIndex(const QModelIndex& index, bool forward)
{
    if (!index.isValid())
        return QModelIndex();

    const QAbstractItemModel* model = index.model();
    // Индекс в той же строке, но в первой колонке. У него могут быть дочерние узлы
    QModelIndex idx = (index.column() == 0) ? index : model->index(index.row(), 0, index.parent());

    if (forward) { // Поиск "вперед
        if (model->rowCount(idx) > 0) {
            // Если есть дочерние, то выбираем первый дочерний
            return model->index(0, 0, idx);
        } else {
            /* Ищем узел, который не является последним на своем уровне.
             * При этом поднимаемся вверх по иерархии */
            while (idx.isValid()) {
                if (idx.row() < model->rowCount(idx.parent()) - 1) {
                    return model->index(idx.row() + 1, 0, idx.parent());
                }
                idx = idx.parent();
            }
            return QModelIndex();
        }
    } else { // Поиск "назад"
        if (idx.row() > 0) {
            // Если есть предыдущий на этом же уровне, то ищем для него самый нижний и последний в иерархии
            idx = model->index(idx.row() - 1, 0, idx.parent());
            while (model->rowCount(idx) > 0) {
                idx = model->index(model->rowCount(idx) - 1, 0, idx);
            }
            return idx;
        } else {
            // Узел первый на своем уровне - возвращаем родителя
            return idx.parent();
        }
    }
}

QMainWindow* Utils::getMainWindow()
{
    auto tw = QApplication::topLevelWidgets();
    for (QWidget* widget : qAsConst(tw)) {
        if (widget->inherits("QMainWindow")) {
            return dynamic_cast<QMainWindow*>(widget);
        }
    }
    return nullptr;
}

QWidget* Utils::getTopWindow()
{
    QWidget* w = QApplication::activeWindow();
    if (w != nullptr)
        return w;

    w = QApplication::activeModalWidget();
    if (w != nullptr)
        return w;

    return getMainWindow();
}

bool Utils::hasParent(const QObject* obj, const QObject* parent)
{
    Q_ASSERT(obj != nullptr);
    Q_ASSERT(parent != nullptr);

    QObject* obj_parent = obj->parent();
    while (obj_parent != nullptr) {
        if (obj_parent == parent)
            return true;

        obj_parent = obj_parent->parent();
    }
    return false;
}

static const int _header_data_structure_version = 1;
Error Utils::saveHeader(QIODevice* device, HeaderItem* root_item, int frozen_group_count, int data_stream_version)
{
    Q_ASSERT(device && root_item && frozen_group_count >= 0);
    Q_ASSERT(root_item->isRoot());
    if (device->isOpen())
        device->close();

    if (!device->open(QIODevice::WriteOnly | QIODevice::Truncate))
        return Error("open error");

    QDataStream st(device);
    st.setVersion(data_stream_version);
    st << _header_data_structure_version;
    st << frozen_group_count;

    auto all = root_item->allChildren();
    st << all.count();

    for (auto h : qAsConst(all)) {
        st << h->parent()->id();
        st << h->id();
        st << h->visualPos();
        st << (h->isHidden() && !h->isPermanentHidded());
        st << h->sectionSize();
    }

    if (st.status() != QDataStream::Ok) {
        device->close();
        return Error("write error");
    }

    device->close();

    return Error();
}

Error Utils::loadHeader(QIODevice* device, HeaderItem* root_item, int& frozen_group_count, int data_stream_version)
{
    frozen_group_count = 0;

    Q_ASSERT(device && root_item);
    Q_ASSERT(root_item->isRoot());

    if (device->isOpen())
        device->close();

    if (!device->open(QIODevice::ReadOnly))
        return Error("open error");

    QDataStream st(device);
    st.setVersion(data_stream_version);

    Error error;

    int version;
    st >> version;
    if (version != _header_data_structure_version)
        error = Error("version error");

    int f_count = 0;
    if (error.isOk()) {
        st >> f_count;
        if (st.status() != QDataStream::Ok)
            error = Error("read error");
    }

    int data_count = 0;
    if (error.isOk()) {
        st >> data_count;
        if (st.status() != QDataStream::Ok)
            error = Error("read error");
    }

    struct Data
    {
        int parent_id;
        int id;
        int pos;
        bool hidden;
        int size;
    };

    QList<Data> data;
    if (error.isOk()) {
        for (int i = 0; i < data_count; i++) {
            int parent_id;
            int id;
            int pos;
            bool hidden;
            int size;

            st >> parent_id;
            st >> id;
            st >> pos;
            st >> hidden;
            st >> size;

            data << Data {parent_id, id, pos, hidden, size};
        }
        if (st.status() != QDataStream::Ok)
            error = Error("read error");
    }

    device->close();

    if (error.isOk()) {
        frozen_group_count = f_count;

        std::sort(data.begin(), data.end(), [](const Data& d1, const Data& d2) -> bool {
            if (d1.parent_id == d2.parent_id)
                return d1.pos < d2.pos;
            else
                return d1.parent_id < d2.parent_id;
        });

        root_item->beginUpdate();
        for (const Data& d : qAsConst(data)) {
            if (d.id < 0)
                continue;

            HeaderItem* item = root_item->item(d.id, false);
            if (item == nullptr)
                continue;

            HeaderItem* parent_item = d.parent_id < 0 ? root_item : root_item->item(d.parent_id, false);
            if (item == nullptr || item->parent() != parent_item || d.pos >= item->parent()->count())
                continue;

            HeaderItem* move_to_item = parent_item->childVisual(d.pos);
            item->move(move_to_item, true);

            if (d.size > 0 && !d.hidden)
                item->setSectionSize(d.size);
            else
                item->setSectionSize(item->defaultSectionSize());

            if (!item->isPermanentHidded())
                item->setHidden(d.hidden);
        }

        // проверяем не получилось ли так, что скрыто все (например после изменения структуры заголовка программистом)
        if (root_item->childrenVisual(Qt::AscendingOrder, true).isEmpty()) {
            // отображаем все
            root_item->setHidden(false);
        }

        root_item->endUpdate();
    }

    return error;
}

void Utils::getAllIndexes(const QAbstractItemModel* m, QModelIndexList& indexes)
{
    Q_ASSERT(m != nullptr);
    indexes.clear();
    getAllIndexesHelper(m, QModelIndex(), indexes);
}

void Utils::getAllIndexesHelper(const QAbstractItemModel* m, const QModelIndex& parent, QModelIndexList& indexes)
{
    for (int i = 0; i < m->rowCount(parent); i++) {
        QModelIndex index = m->index(i, 0, parent);
        indexes << index;
        getAllIndexesHelper(m, index, indexes);
    }
}

QAbstractItemModel* Utils::getTopSourceModel(const QAbstractItemModel* model)
{
    Q_ASSERT(model != nullptr);
    const QAbstractItemModel* source = model;
    while (true) {
        auto proxy = qobject_cast<const QAbstractProxyModel*>(source);
        if (proxy != nullptr)
            source = proxy->sourceModel();
        else
            return const_cast<QAbstractItemModel*>(source);
    }
    Q_ASSERT(false);
    return nullptr;
}

QModelIndex Utils::alignIndexToModel(const QModelIndex& index, const QAbstractItemModel* model)
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(model != nullptr);
    if (index.model() == model)
        return index;

    // индекс имеет модель в предках по прокси?
    const QAbstractItemModel* m = index.model();
    QModelIndex idx = index;
    while (true) {
        if (m == model) {
            Q_ASSERT(idx.model() == model);
            return idx;
        }

        const QAbstractProxyModel* proxy = qobject_cast<const QAbstractProxyModel*>(m);
        if (proxy == nullptr)
            break;

        m = proxy->sourceModel();
        idx = proxy->mapToSource(idx);
    }

    // модель имеет индекс в предках по прокси?
    m = model;
    QList<const QAbstractProxyModel*> proxy_chain;
    while (true) {
        if (m == index.model()) {
            // поднимаемся назад по цепочке прокси
            QModelIndex idx = index;
            for (int i = 0; i < proxy_chain.count(); i++) {
                idx = proxy_chain.at(i)->mapFromSource(idx);
                if (!idx.isValid())
                    return QModelIndex(); // отфильтровано
            }

            Q_ASSERT(idx.model() == model);
            return idx;
        }
        const QAbstractProxyModel* proxy = qobject_cast<const QAbstractProxyModel*>(m);
        if (proxy == nullptr)
            return QModelIndex();

        m = proxy->sourceModel();
        proxy_chain.prepend(proxy);
    }

    return QModelIndex();
}

void Utils::paintHeaderDragHandle(QAbstractScrollArea* area, HeaderView* header)
{
    if (header->isColumnsDragging() && header->dragAllowed() && header->dragToHidden() >= 0) {
        int drag_col = header->logicalIndex(header->dragToHidden());
        if (!header->isSectionHidden(drag_col)) {
            QPainter painter(area->viewport());
            painter.save();

            QPixmap pixmap = qApp->style()->standardPixmap(QStyle::SP_ArrowDown);

            int drag_x = header->sectionViewportPosition(drag_col);
            const int shift = 3;
            if (header->dragLeft())
                drag_x += shift;
            else
                drag_x += header->sectionSize(drag_col) - pixmap.width() - shift;

            QRect pixmap_rect(drag_x, 2, pixmap.width(), pixmap.height());

            painter.drawPixmap(pixmap_rect, pixmap, QRect(0, 0, pixmap_rect.width(), pixmap_rect.height()));

            painter.restore();
        }
    }
}

QString Utils::variantToString(const QVariant& value, QLocale::Language language, int max_list_count)
{
    QLocale locale(language == QLocale::AnyLanguage ? QLocale::system() : language);
    return variantToStringHelper(value, &locale, max_list_count);
}

QString Utils::variantToStringHelper(const QVariant& value, const QLocale* locale, int max_list_count)
{
    // в идеале надо писать конверторы через QMetaType::registerConverter, а не расширять этот метод

    if (!value.isValid() || value.isNull())
        return QString();

    switch (value.type()) {
        case QVariant::StringList:
            return containerToString(value.toStringList(), max_list_count);
        case QVariant::List:
            return containerToString(value.toList(), max_list_count);

        case QVariant::Time:
            return locale->toString(value.toTime(), QLocale::ShortFormat);

        case QVariant::Date:
            return locale->toString(value.toDate(), QLocale::ShortFormat);

        case QVariant::DateTime:
            return locale->toString(value.toDateTime(), QLocale::ShortFormat);

        case QVariant::ULongLong:
        case QVariant::UInt:
            return locale->toString(value.toULongLong());

        case QVariant::LongLong:
        case QVariant::Int:
            return locale->toString(value.toLongLong());

        case QVariant::Double:
            return locale->toString(value.toDouble(), 'f', DOUBLE_DECIMALS);

        case QVariant::String:
        case QVariant::Char:
            return value.toString();

        default: {
            auto list = value.value<QList<int>>();
            if (!list.isEmpty())
                return containerToString(list, max_list_count);

            return value.toString();
        }
    }
}

} // namespace zf
