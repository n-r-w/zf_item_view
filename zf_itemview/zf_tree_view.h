#pragma once

#include "zf_itemview.h"
#include "zf_itemview_header_item.h"
#include "zf_item_delegate.h"
#include "zf_i_cell_column_check.h"
#include "zf_error.h"

#include <QTreeView>

class QTreeViewPrivate;

namespace zf
{
class HeaderView;
class TreeCheckBoxPanel;

//! Древовидная таблица с иерархическим заголовком
class ZF_ITEMVIEW_DLL_API TreeView : public QTreeView, public I_ItemDelegateCheckInfo, public I_CellColumnCheck
{
    Q_OBJECT
public:
    explicit TreeView(QWidget* parent = nullptr);

    //! Корневой узел иерархического заголовка
    HeaderItem* rootHeaderItem() const;

    //! Узел иерархического заголовка по его идентификатору
    HeaderItem* headerItem(int id) const;

    //! Иерархический заголовок
    HeaderView* horizontalHeader() const;
    HeaderView* header() const;

    //! Разрешать сортировку
    void setSortingEnabled(bool enable);

    //! Разрешать диалог конфигурации
    bool isConfigMenuEnabled() const;
    void setConfigMenuEnabled(bool b);

    void setModel(QAbstractItemModel* model) override;

    //! Находится в процессе перезагрузки данных из rootItem
    bool isReloading() const;

    //! Использовать html форматирование
    void setUseHtml(bool b);
    bool isUseHtml() const;

    //! Сохранить состояние заголовков
    Error serialize(QIODevice* device) const;
    Error serialize(QByteArray& ba) const;
    //! Восстановить состояние заголовков
    Error deserialize(QIODevice* device);
    Error deserialize(const QByteArray& ba);

public: // выделение строк (панель слева от таблицы)
    //! Показать панель с чекбоксами
    void showCheckRowPanel(bool show);
    //! Отображается ли панель с чекбоксами
    bool isShowCheckRowPanel() const;
    //! Есть ли строки, выделенные чекбоксами
    bool hasCheckedRows() const;
    //! Проверка строки на выделение чекбоксом.
    bool isRowChecked(
        //! Если TreeView подключена к наследнику QAbstractProxyModel, то это индекс source
        const QModelIndex& index) const;
    //! Задать выделение строки чекбоксом
    void checkRow(
        //! Если TreeView подключена к наследнику QAbstractProxyModel, то это индекс source
        const QModelIndex& index, bool checked);
    //! Выделенные индексы. Если TreeView подключена к наследнику QAbstractProxyModel, то это индекс source
    QSet<QModelIndex> checkedRows() const;
    //! Все строки выделены чекбоксами
    bool isAllRowsChecked() const;
    //! Выделить все строки чекбоксами
    void checkAllRows(bool checked);

public: // выделение ячеек (чекбоксы внутри ячеек)
    //! Колонки, в ячейках которых, находятся чекбоксы (ключ - логические индексы колонок, значение - можно менять)
    QMap<int, bool> cellCheckColumns(
        //! Уровень вложенности
        int level) const override;
    //! Колонки, в ячейках которых, находятся чекбоксы
    void setCellCheckColumn(int logical_index,
                            //! Колонка видима
                            bool visible,
                            //! Пользователь может менять состояние чекбоксов
                            bool enabled,
                            //! Ячейки выделяются только для данного уровня вложенности. Если -1, то для всех
                            int level = -1) override;
    //! Состояние чекбокса ячейки
    bool isCellChecked(const QModelIndex& index) const override;
    //! Задать состояние чекбокса ячейки. Если колонка не находится в cellCheckColumns, то она туда добавляется
    void setCellChecked(const QModelIndex& index, bool checked) override;
    //! Все выделенные чеками ячейки
    QModelIndexList checkedCells() const override;
    //! Очистить все выделение чекбоксами
    void clearCheckedCells() override;

public:
    void updateGeometries() override;

    //! I_ItemDelegateCheckInfo
    void delegateGetCheckInfo(QAbstractItemView* item_view, const QModelIndex& index, bool& show, bool& checked) const override;

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    //! Выводим в паблик
    using QTreeView::viewOptions;
#else
    //! Для совместимости с Qt5
    QStyleOptionViewItem viewOptions() const;
#endif

protected:
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    bool viewportEvent(QEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void scrollContentsBy(int dx, int dy) override;

signals:
    //! Изменилось выделение чекбоксами
    void sg_checkedRowsChanged();
    //! Изменилась видимость панели выделения чекбоксами
    void sg_checkPanelVisibleChanged();
    //! Изменилось выделение ячеек чекбоксами
    void sg_checkedCellChanged(
        //! Если TableView подключена к наследнику QAbstractProxyModel, то это индекс source
        const QModelIndex& index, bool checked) override;

private slots:
    //! Выделить указанную колонку
    void selectColumn(int column);    
    //! Вызывается перед началом перезагрузки данных из rootItem
    void sl_beforeLoadDataFromRootHeader();
    //! Вызывается после окончания перезагрузки данных из rootItem
    void sl_afterLoadDataFromRootHeader();
    //! Колонки горизонтального заголовка перемещаются через Drag&Drop
    void sl_columnsDragging(
        //! Визуальный индекс колонки начала перемещаемой группы
        int from_begin,
        //! Визуальный индекс колонки окончания перемещаемой группы
        int from_end,
        //! Визуальный индекс колонки куда произошло перемещение
        int to,
        //! Визуальный индекс колонки куда произошло перемещение с учетом скрытых
        int to_hidden,
        //! Если истина, то вставка слева от to, иначе справа
        bool left,
        //! Перемещение колонки разрешено
        bool allow);

    //! Перемещение колонок завершено
    void sl_columnsDragFinished();    
    //! Смена ширины колонки
    void sl_columnResized(int column, int oldWidth, int newWidth);

    void sl_expanded(const QModelIndex& index);
    void sl_collapsed(const QModelIndex& index);

private:
    void init();
    void selectColumnHelper(QItemSelection& selection, int column, const QModelIndex& parent);
    //! Глубина вложенности индекса
    static int indexLevel(const QModelIndex& index);
    std::shared_ptr<QMap<int, bool>> cellCheckColumnsHelper(int level) const;
    QModelIndex sourceIndex(const QModelIndex& index) const;

    QModelIndex _saved_index;
    int _reloading = 0;

    //! Таймер изменения ширины колонки
    QTimer* _column_resize_timer;

    //! Панель с чекбоксами
    TreeCheckBoxPanel* _check_panel;
    //! Выделенные строки. Если TreeView подключена к наследнику QAbstractProxyModel, то это индекс source
    QSet<QPersistentModelIndex> _checked;
    //! Все строки выделены
    int _all_checked = false;

    bool _geometry_recursion_block = false;
    // Отслеживание обновления ячеек при переходе с одной на другую
    QPersistentModelIndex _hover_index;

    //! Колонки, в ячейках которых, находятся чекбоксы (логические индексы колонок)
    //! Ключ - уровень вложенности (-1 для всех уровней)
    //! Значение мап: ключ - логические индексы колонок, значение - можно менять
    QMap<int, std::shared_ptr<QMap<int, bool>>> _cell_check_columns;
    //! Состояние чекбокса ячейки. Если TableView подключена к наследнику QAbstractProxyModel, то это номер индекс source
    mutable QList<QPersistentModelIndex> _cell_checked;

public:
    //! Указатель на приватные данные Qt для особых извращений
    QTreeViewPrivate* privatePtr() const;
};

} // namespace zf
