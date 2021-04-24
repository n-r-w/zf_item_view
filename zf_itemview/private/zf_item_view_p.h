#pragma once

#include "zf_table_view_base.h"

namespace zf
{
//! Угловой виджет
class TableViewCorner : public QWidget
{
    Q_OBJECT
public:
    TableViewCorner(TableViewBase* table_view);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    TableViewBase* _table_view;
};

//! Фиксированный боковик
class FrozenTableView : public TableViewBase
{
    Q_OBJECT
public:
    FrozenTableView(
        //! Основная таблица
        TableViewBase* base);
    ~FrozenTableView() override;

    //! Корневой узел иерархического заголовка
    HeaderItem* rootHeaderItem(Qt::Orientation orientation) const override;

    //! Количество зафиксированных групп (узлов верхнего уровня)
    int frozenGroupCount() const override;
    //! Установить количество зафиксированных групп (узлов верхнего уровня)
    void setFrozenGroupCount(int count) override;
    //! У основной таблицы больше колонок, значит надо наследовать от нее высоту строки
    int sizeHintForRow(int row) const override;

    //! Автоматически растягивать высоту под количество строк
    bool isAutoShrink() const final;
    //! Минимальное количество строк при автоподгоне высоты
    int shrinkMinimumRowCount() const final;
    //! Максимальное количество строк при автоподгоне высоты
    int shrinkMaximumRowCount() const final;
    //! Автоматически подгонять высоту строк под содержимое
    bool isAutoResizeRowsHeight() const final;

public: // выделение ячеек (чекбоксы внутри ячеек)
    //! Колонки, в ячейках которых, находятся чекбоксы (ключ - логические индексы колонок, значение - можно менять)
    QMap<int, bool> cellCheckColumns(
        //! Уровень вложенности (для таблиц игнорируется)
        int level = -1) const override;
    //! Колонки, в ячейках которых, находятся чекбоксы
    void setCellCheckColumn(int logical_index,
                            //! Колонка видима
                            bool visible,
                            //! Пользователь может менять состояние чекбоксов
                            bool enabled,
                            //! Уровень вложенности (для таблиц игнорируется)
                            int level = -1) override;
    //! Состояние чекбокса ячейки
    bool isCellChecked(const QModelIndex& index) const override;
    //! Задать состояние чекбокса ячейки. Если колонка не находится в cellCheckColumns, то она туда добавляется
    void setCellChecked(const QModelIndex& index, bool checked) override;
    //! Все выделенные чеками ячейки
    QModelIndexList checkedCells() const override;
    //! Очистить все выделение чекбоксами
    void clearCheckedCells() override;
    //! Изменилось выделение ячеек чекбоксами
    Q_SIGNAL void sg_checkedCellChanged(
        //! Если TableView подключена к наследнику QAbstractProxyModel, то это индекс source
        const QModelIndex& index, bool checked) override;

    //! Основная таблица
    TableViewBase* base() const;

    //! Высота горизонтального заголовка
    int horizontalHeaderHeight() const override;

protected:
    //! Смена ширины колонки
    void onColumnResized(int column, int oldWidth, int newWidth) override;

    //! Колонки горизонтального заголовка перемещаются через Drag&Drop
    void onColumnsDragging(
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
        bool allow) override;
    //! Перемещение колонок завершено
    void onColumnsDragFinished() override;

    void paintEvent(QPaintEvent* event) override;

private:
    void init();

    TableViewBase* _base;
    friend class TableView;
    bool _request_resize = false;
};

} // namespace zf
