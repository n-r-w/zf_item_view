#pragma once

#include "zf_itemview.h"
#include "zf_header_view.h"
#include "zf_item_delegate.h"
#include "zf_i_cell_column_check.h"

#include <QTableView>

namespace zf
{

//! Таблица с иерархическим заголовком, базовая для основной и фиксированной таблицы
class ZF_ITEMVIEW_DLL_API TableViewBase : public QTableView, public I_ItemDelegateCheckInfo, public I_CellColumnCheck
{
    Q_OBJECT
public:
    explicit TableViewBase(QWidget* parent = nullptr);        
    ~TableViewBase() override;

    void setModel(QAbstractItemModel* model) override;

    //! Корневой узел иерархического заголовка
    virtual HeaderItem* rootHeaderItem(Qt::Orientation orientation) const = 0;

    //! Горизонтальный иерархический заголовок
    HeaderView* horizontalHeader() const;
    //! Вертикальный иерархический заголовок
    HeaderView* verticalHeader() const;
    //!  Иерархический заголовок
    HeaderView* header(Qt::Orientation orientation) const;

    //! Количество зафиксированных групп (узлов верхнего уровня)
    virtual int frozenGroupCount() const = 0;
    //! Установить количество зафиксированных групп (узлов верхнего уровня)
    virtual void setFrozenGroupCount(int count) = 0;
    //! Количество зафиксированных секций
    int frozenSectionCount(bool visible_only) const;

    //! Разрешать сортировку
    void setSortingEnabled(bool enable);

    //! Разрешать диалог конфигурации
    bool isConfigMenuEnabled() const;
    void setConfigMenuEnabled(bool b);

    //! Автоматически растягивать высоту под количество строк
    virtual bool isAutoShrink() const = 0;
    //! Минимальное количество строк при автоподгоне высоты
    virtual int shrinkMinimumRowCount() const = 0;
    //! Максимальное количество строк при автоподгоне высоты
    virtual int shrinkMaximumRowCount() const = 0;
    //! Автоматически подгонять высоту строк под содержимое
    virtual bool isAutoResizeRowsHeight() const = 0;

    //! Использовать html форматирование
    virtual void setUseHtml(bool b);
    bool isUseHtml() const;

    void updateGeometries() override;

public:
    //! Находится в процессе перезагрузки данных из rootItem
    bool isReloading() const;

    QSize sizeHint() const override;
    //! Запросить подгонку строк по высоте
    virtual void requestResizeRowsToContents();
    //! Смена ширины колонки
    virtual void onColumnResized(int column, int oldWidth, int newWidth);
    //! I_ItemDelegateCheckInfo
    void delegateGetCheckInfo(QAbstractItemView* item_view, const QModelIndex& index, bool& show, bool& checked) const override;

    using QTableView::sizeHintForRow;

signals:
    //! Вызывается до инициации подгонки высоты строк
    void sg_beforeResizeRowsToContent();
    //! Вызывается после инициации подгонки высоты строк
    void sg_afterResizeRowsToContent();

protected:
    //! Колонки горизонтального заголовка перемещаются через Drag&Drop
    virtual void onColumnsDragging(
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
    virtual void onColumnsDragFinished();

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    bool event(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles = QVector<int>()) override;
    void rowsInserted(const QModelIndex& parent, int first, int last) override;

    //! Ширина бокового сдвига
    virtual int leftPanelWidth() const;
    //! Высота горизонтального заголовка
    virtual int horizontalHeaderHeight() const;

private slots:

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

    //! Вызывается перед началом перезагрузки данных из rootItem
    void sl_beforeLoadDataFromRootHeader();
    //! Вызывается после окончания перезагрузки данных из rootItem
    void sl_afterLoadDataFromRootHeader();

    //! Смена ширины колонки
    void sl_columnResized(int column, int oldWidth, int newWidth);

    void sl_rowsRemoved(const QModelIndex& parent, int first, int last);
    void sl_modelReset();

    void sl_resizeToContents();

private:
    void init();

    //! Надо ли
    static bool isNeedRowsAutoHeight(int row_count, int column_count);

    QModelIndex _saved_index;
    int _reloading = 0;

    //! Таймер изменения ширины колонки и автовысоты строк
    QTimer* _resize_timer;

    // Отслеживание обновления ячеек при переходе с одной на другую
    QPersistentModelIndex _hover_index;

    bool _geometry_recursion_block = false;

    //! Надо ли было подгонять высоту таблиц при последнем анализе
    bool _last_need_row_auto_height = false;

    friend class FrozenTableView;
};

} // namespace zf
