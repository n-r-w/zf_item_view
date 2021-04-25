#pragma once

#include "zf_itemview.h"
#include "zf_table_view_base.h"
#include "zf_error.h"
#include "zf_itemview_header_item.h"

#include <QLabel>

namespace zf
{
class HeaderView;
class FrozenTableView;
class CheckBoxPanel;

//! Таблица с иерархическим заголовком
class ZF_ITEMVIEW_DLL_API TableView : public TableViewBase
{
    Q_OBJECT
public:
    explicit TableView(QWidget* parent = nullptr);
    ~TableView() override;

    //! Корневой узел иерархического заголовка
    HeaderItem* rootHeaderItem(Qt::Orientation orientation) const override;

    //! Корневой узел горизонтального иерархического заголовка по его идентификатору
    HeaderItem* horizontalRootHeaderItem() const;
    //! Корневой узел вертикального иерархического заголовка по его идентификатору
    HeaderItem* verticalRootHeaderItem() const;
    //! Узел горизонтального иерархического заголовка по его идентификатору
    HeaderItem* horizontalHeaderItem(int id) const;
    //! Узел вертикального иерархического заголовка по его идентификатору
    HeaderItem* verticalHeaderItem(int id) const;

    //! Виджет в левом верхнем углу
    QWidget* cornerWidget() const;
    void setCornerWidget(QWidget* widget);

    //! Установить текст в верхний левый угол
    void setCornerText(const QString& text, Qt::Alignment alignment = Qt::AlignCenter, int margin_left = 4,
        int margin_right = 4, int margin_top = 4, int margin_bottom = 4);
    void setCornerTextOptions(bool bold, const QColor& text_color);

    //! Количество зафиксированных групп (узлов верхнего уровня)
    int frozenGroupCount() const override;
    //! Установить количество зафиксированных групп (узлов верхнего уровня)
    void setFrozenGroupCount(int count) override;

    //! Использовать html форматирование
    void setUseHtml(bool b) override;

    //! Запросить подгонку строк по высоте
    void requestResizeRowsToContents() override;

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
        //! Если TableView подключена к наследнику QAbstractProxyModel, то это номер строки source
        int row) const;
    //! Задать выделение строки чекбоксом
    void checkRow(
        //! Если TableView подключена к наследнику QAbstractProxyModel, то это номер строки source
        int row, bool checked);
    //! Выделенные строки
    QSet<int> checkedRows() const;
    //! Все строки выделены чекбоксами
    bool isAllRowsChecked() const;
    //! Выделить все строки чекбоксами
    void checkAllRows(bool checked);

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
    //! Задать состояние чекбокса ячейки
    void setCellChecked(const QModelIndex& index, bool checked) override;
    //! Все выделенные чеками ячейки
    //! Если TableView подключена к наследнику QAbstractProxyModel, то это индексы source
    QModelIndexList checkedCells() const override;
    //! Очистить все выделение чекбоксами
    void clearCheckedCells() override;

public:
    //! Автоматически растягивать высоту под количество строк
    bool isAutoShrink() const final;
    void setAutoShring(bool b);
    //! Минимальное количество строк при автоподгоне высоты
    int shrinkMinimumRowCount() const final;
    void setShrinkMinimumRowCount(int n);
    //! Максимальное количество строк при автоподгоне высоты
    int shrinkMaximumRowCount() const final;
    void setShrinkMaximumRowCount(int n);
    //! Автоматически подгонять высоту строк под содержимое
    bool isAutoResizeRowsHeight() const final;
    void setAutoResizeRowsHeight(bool b);

public:
    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    void updateGeometries() override;
    void setModel(QAbstractItemModel* model) override;

    //! Выводим в паблик
    using TableViewBase::viewOptions;

    //! Встроенная таблица с фиксированными колонками
    TableViewBase* frozenTableView() const;

protected:
    //! Смена ширины колонки
    void onColumnResized(int column, int oldWidth, int newWidth) override;

    void paintEvent(QPaintEvent* event) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;    
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;
    void doItemsLayout() override;
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;
    void customEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

    //! Ширина бокового сдвига
    int leftPanelWidth() const override;

signals:
    //! Изменилось выделение строк чекбоксами
    void sg_checkedRowsChanged();
    //! Изменилась видимость панели выделения строк чекбоксами
    void sg_checkPanelVisibleChanged();
    //! Изменилось выделение ячеек чекбоксами
    void sg_checkedCellChanged(
        //! Если TableView подключена к наследнику QAbstractProxyModel, то это индекс source
        const QModelIndex& index, bool checked) override;

private slots:
    void sl_horizontalGeometriesChanged();    
    void sl_verticalSectionResized(int logicalIndex, int oldSize, int newSize);
    void sl_verticalGeometriesChanged();

    //! Изменилась видимость
    void sl_rootItemHiddenChanged(
        //! Список узлов нижнего уровня, для которых изменилась видимость
        const QList<zf::HeaderItem*>& bottom_items, bool is_hide);
    void sl_rootItemColumnsMoved(
        //! Какой узел перемещен
        zf::HeaderItem* moved,
        //! Из какой позиции относительно своего родителя
        int visual_pos_from,
        //! В какую позицию относительно своего родителя
        int visual_pos_to,
        //! True, если перемещается между (visual_pos_to-1) и (visual_pos_to)
        //! False, если перемещается между (visual_pos_to) и (visual_pos_to+1)
        bool before);

    void sl_frozen_horizontalSectionResized(int logicalIndex, int oldSize, int newSize);
    void sl_frozen_currentChanged(const QModelIndex& current, const QModelIndex& previous);
    //! Одиночное нажатие на фиксированные колонки
    void sl_frozenClicked(const QModelIndex& index);
    //! Двойное нажатие на фиксированные колонки
    void sl_frozenDoubleClicked(const QModelIndex& index);

    //! Завершена правка в ячейке
    void sl_frozenCloseEditor(QWidget* w);

    void sl_layoutChanged();
    void sl_rowsRemoved(const QModelIndex& parent, int first, int last);
    void sl_rowsInserted(const QModelIndex& parent, int first, int last);

private:
    using TableViewBase::onColumnResized;

    void init();
    void updateCornerWidget();

    //! Обновить положение фиксированной таблицы
    void updateFrozenTableGeometry();
    //! Крайняя правая х-координата фиксированной таблицы
    int frozenRightPos() const;
    //! Обновить положение текущей ячейки
    void updateFrozenCurrentCellPosition();

    //! Обновить количество фиксированных колонок
    void updateFrozenCount();

    void blockUpdateFrozenGeometry();
    void unblockUpdateFrozenGeometry();

    QModelIndex sourceIndex(const QModelIndex& index) const;

    //! Виджет в левом верхнем углу
    QWidget* _corner_widget = nullptr;

    QLabel* _corner_label = nullptr;
    QString _corner_text;
    Qt::Alignment _corner_alignment = Qt::AlignCenter;
    int _corner_margin_left = 4;
    int _corner_margin_right = 4;
    int _corner_margin_top = 4;
    int _corner_margin_bottom = 4;
    bool _corner_bold = false;
    QColor _corner_text_color;

    //! Встроенная таблица с фиксированными колонками
    FrozenTableView* _frozen_table_view = nullptr;
    QFrame* _frozen_table_line = nullptr;
    int _frozen_group_count = 0;

    bool _block_select_current = false;
    //! Флаг обновления свойств фиксированной таблицы
    bool _need_update_frozen_properties = false;

    int _block_update_geometry_count = 0;
    bool _geometry_recursion_block = false;

    //! Панель с чекбоксами
    CheckBoxPanel* _check_panel;
    //! Выделенные строки
    QSet<int> _checked;
    //! Все строки выделены
    int _all_checked = false;

    //! Автоподгон высоты виджета
    bool _auto_shrink = false;
    //! Минимальное количество строк при автоподгоне высоты
    int _shrink_minimum_row_count = 2;
    //! Максимальное количество строк при автоподгоне высоты
    int _shrink_maximum_row_count = 10;

    //! Автовысота строк
    bool _auto_resize_rows = false;
    bool _request_resize = false;

    //! Колонки, в ячейках которых, находятся чекбоксы (ключ - логические индексы колонок, значение - можно менять)
    QMap<int, bool> _cell_check_columns;
    //! Состояние чекбокса ячейки. Если TableView подключена к наследнику QAbstractProxyModel, то это номер индекс source
    mutable QList<QPersistentModelIndex> _cell_checked;
};

} // namespace zf
