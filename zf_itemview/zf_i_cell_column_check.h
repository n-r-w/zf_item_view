#pragma once

#include <QMap>
#include <QModelIndex>

//! Интерфейс для унификации доступа к управлению чекбоксами ячеек TableView/TreeView
class I_CellColumnCheck
{
public:
    //! Колонки, в ячейках которых, находятся чекбоксы (ключ - логические индексы колонок, значение - можно менять)
    virtual QMap<int, bool> cellCheckColumns(
        //! Уровень вложенности (только для TreeView)
        int level = -1) const = 0;
    //! Колонки, в ячейках которых, находятся чекбоксы
    virtual void setCellCheckColumn(int logical_index,
                                    //! Колонка видима
                                    bool visible,
                                    //! Пользователь может менять состояние чекбоксов
                                    bool enabled,
                                    //! Ячейки выделяются только для данного уровня вложенности. Если -1, то для всех (только для TreeView)
                                    int level = -1)
        = 0;
    //! Состояние чекбокса ячейки
    virtual bool isCellChecked(const QModelIndex& index) const = 0;
    //! Задать состояние чекбокса ячейки
    virtual void setCellChecked(const QModelIndex& index, bool checked) = 0;
    //! Все выделенные чеками ячейки
    //! Если TableView подключена к наследнику QAbstractProxyModel, то это индексы source
    virtual QModelIndexList checkedCells() const = 0;
    //! Очистить все выделение чекбоксами
    virtual void clearCheckedCells() = 0;

    //! Сигнал - изменилось выделение ячеек чекбоксами
    virtual void sg_checkedCellChanged(
        //! Если TableView подключена к наследнику QAbstractProxyModel, то это индекс source
        const QModelIndex& index, bool checked)
        = 0;
};
