#pragma once

#include <QHeaderView>
#include <QStack>
#include <memory>

#include "zf_itemview.h"
#include "zf_itemview_header_model.h"

namespace zf
{
class HeaderItem;

//! Иерархический заголовок. Не использовать напрямую, только через HeaderItem
class ZF_ITEMVIEW_DLL_API HeaderView : public QHeaderView
{
    Q_OBJECT

public:
    //! Корневой узел
    HeaderItem* rootItem() const;
    //! Модель
    ItemViewHeaderModel* model() const;

    //! Указать связанный заголовок
    void setJoinedHeader(HeaderView* header_view);
    //! Связанный заголовок
    HeaderView* joinedHeader() const;

    //! Задать ограничение на отображение групп секций (каждая группа - это узел верхнего уровня и его дочерние узлы)
    //! Только для горизонтального заголовка
    void setLimit(int group_count);
    int limit() const;
    //! Количество секций, на которые задан лимит отображения (без учета скрытых)
    int limitSectionCount() const;

    //! Разрешать сортировку
    bool isAllowSorting() const;
    void setAllowSorting(bool b);

    //! Разрешать диалог конфигурации
    bool isConfigMenuEnabled() const;
    void setConfigMenuEnabled(bool b);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    HeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~HeaderView() override;

    void setModel(QAbstractItemModel* model) override;
    void setJoinedModel(ItemViewHeaderModel* model);

    //! В процессе перетаскивания колонок
    bool isColumnsDragging() const { return _drag_from_begin >= 0; }
    //! Визуальный индекс колонки начала перемещаемой группы
    int dragFromBegin() const { return _drag_from_begin; }
    //! Визуальный индекс колонки окончания перемещаемой группы
    int dragFromEnd() const { return _drag_from_end; }
    //! Визуальный индекс колонки куда произошло перемещение
    int dragTo() const { return _drag_to; }
    //! Визуальный индекс колонки куда произошло перемещение (с учетом скрытых колонок)
    int dragToHidden() const { return _drag_to_hidden; }
    //! Если истина, то вставка слева от dragTo, иначе справа
    bool dragLeft() const { return _drag_left; }
    //! Перемещение колонки разрешено
    bool dragAllowed() const { return _drag_allowed; }

signals:
    //! Колонки горизонтального заголовка перемещаются через Drag&Drop
    void sg_columnsDragging(
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

    //! Перемещение колонок начато
    void sg_columnsDragStarted();
    //! Перемещение колонок завершено
    void sg_columnsDragFinished();
    //! Запрошено меню конфигурации заголовка
    void sg_configMenuRequested(const QPoint& pos);

    //! Вызывается перед началом перезагрузки данных из rootItem
    void sg_beforeLoadDataFromRootHeader();
    //! Вызывается после окончания перезагрузки данных из rootItem
    void sg_afterLoadDataFromRootHeader();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;    
    void customEvent(QEvent* event) override;
    bool event(QEvent* event) override;
    QModelIndex indexAt(const QPoint& pos) const override;
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;

private slots:    
    void sl_sectionResized(int logical_index, int old_size, int new_size);
    void sl_columnsInserted(const QModelIndex& parent, int first, int last);
    void sl_rowsInserted(const QModelIndex& parent, int first, int last);

    void sl_itemDataChanged(zf::HeaderItem* item, int role);
    void sl_modelReset();

    //! Изменился режим resizeMode для данного узла (только для узлов нижнего уровня)
    void sl_itemResizeModeChanged(int section, QHeaderView::ResizeMode mode);
    //! Изменилась видимость
    void sl_rootItemHiddenChanged(
        //! Список узлов нижнего уровня, для которых изменилась видимость
        const QList<zf::HeaderItem*>& bottom_items, bool is_hide);
    //! Секции были перемещены с точки зрения отображения
    void sl_rootItemVisualMoved();
    //! Изменился размер секции, находящейся все диапазона управляемых секций. Генерируется root для секций
    //! вертикального заголовка, номер которых превышает количество секций заголовка
    void sl_rootItemOutOfRangeResized(int section, int size);
    //! Изменилась сортировка. Генерируется root для секций нижнего уровня горизонтального заголовка
    void sl_rootItemSortChanged(
        //! -1 если раньше не было сортировки
        int old_section,
        //! Если new_section -1, то сортировка отключена
        int new_section, Qt::SortOrder order);

    //! Перемещение колонок начато
    void sl_joinedColumnsDragStarted();
    //! Перемещение колонок завершено
    void sl_joinedColumnsDragFinished();
    //! Колонки горизонтального заголовка перемещаются через Drag&Drop
    void sl_joinedColumnsDragging(
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

    //! Сменилось состояние автопоиска
    void sl_autoSearchStatusChanged(bool is_active);
    //! Сменился текст автопоиска
    void sl_autoSearchTextChanged(const QString& text);

private:
    using QHeaderView::isSortIndicatorShown;
    using QHeaderView::resizeSection;
    using QHeaderView::resizeSections;
    using QHeaderView::setSectionResizeMode;
    using QHeaderView::setSortIndicator;
    using QHeaderView::setSortIndicatorShown;
    using QHeaderView::sortIndicatorOrder;
    using QHeaderView::sortIndicatorSection;

    void paintSectionHelper(QPainter* painter, const QRect& rect, int logicalIndex, bool transparent) const;

    //! Вью к которому относится заголовок
    QAbstractItemView* itemView() const;

    //! Скопировать свойства из связанного заголовка
    void copyJoinedProperties();
    //! Скопировать данные о перетаскивании со связанного заголовка
    void takeDragDataFromJoined();

    //! Переключить сортировку
    void flipSortIndicator(int section);

    //! Перезагрузить данные из rootItem
    void reloadDataFromRootItem();
    void reloadDataFromRootItemHelper();
    //! Обновить порядок отображения столбцов на основании HeaderItem
    void updateVisualOrder();

    //! Получить размеры всех секций, которые относятся к данной секции (если -1, то для всех)
    QMap<int, int> getSectionsSizes(int logicalIndex = -1,
        //! Принудительно задать new_size для logicalIndex
        int new_size = -1) const;

    QMimeData* encodeMimeData(const QPoint& pos, const QModelIndex& index) const;
    void decodeMimeData(
        const QMimeData* data, const QObject* source_object, QPoint& source_pos, QModelIndex& source_index) const;

    /*! Информация о ячейке
     * Номера колонок и строк заданы в соответствии с ориентацией, т.е. для горизонтального заголовка - строка=уровень,
     * колонка=секция, для вертикального - строка=секция, колонка=уровень */
    struct CellInfo
    {
        const HeaderView* header_view;

        HeaderItem* header_item = nullptr;

        //! Ячейка (включая ее spaDataProperty _dataset_propertyn)
        QRect cell_rect;
        //! Все ячейки группы
        QRect group_rect;

        //! Индекс ячейки
        QModelIndex cell_index;

        //! Индекс ячейки с которой начинается спан
        QModelIndex span_begin_index;

        //! Индекс родительской группы
        QModelIndex group_index;

        //! Индекс начала
        QModelIndex span_start_index;
        //! Индекс окончания
        QModelIndex span_finish_index;

        //! Начальная колонка
        int col_span_from = -1;
        //! Конечная колонка
        int col_span_to = -1;
        //! Размер спана по колонкам
        int col_span = 0;

        //! Начальная строка
        int row_span_from = -1;
        //! Конечная строка
        int row_span_to = 0;
        //! Размер спана по строкам
        int row_span = 0;

        //!  Начальная колонка с точки зрения отображения на экране
        int visual_col_from = -1;
        //!  Конечная колонка с точки зрения отображения на экране
        int visual_col_to = -1;

        //!  Начальная строка с точки зрения отображения на экране
        int visual_row_from = -1;
        //!  Конечная строка с точки зрения отображения на экране
        int visual_row_to = -1;

        //!  Начальная колонка с точки зрения отображения на экране с учетом скрытых
        int visual_col_from_hidden = -1;
        //!  Конечная колонка с точки зрения отображения на экране с учетом скрытых
        int visual_col_to_hidden = -1;
        //! Размер спана по колонкам с учетом скрытых
        int col_span_hidden = 0;

        //!  Начальная строка с точки зрения отображения на экране с учетом скрытых
        int visual_row_from_hidden = -1;
        //!  Конечная строка с точки зрения отображения на экране с учетом скрытых
        int visual_row_to_hidden = -1;
        //! Размер спана по строкам с учетом скрытых
        int row_span_hidden = 0;
    };
    std::shared_ptr<CellInfo> cellInfo(HeaderItem* item) const;
    std::shared_ptr<CellInfo> cellInfo(const QModelIndex& cell_index) const;
    static std::shared_ptr<CellInfo> cellInfo(const HeaderView* header, HeaderItem* item);

    //! Информация о перетаскивании
    struct DragInfo
    {
        //! Индекс для исходной колонки
        QModelIndex source_index;
        //! Информация об исходной колонке
        std::shared_ptr<CellInfo> source_info;

        //! Положение курсора для целевой колонки
        QPoint target_pos;
        //! Индекс для целевой колонки
        QModelIndex target_index;
        //! Информация о целевой колонке
        std::shared_ptr<CellInfo> target_info;

        //! Разрешить это действие
        bool allow = false;

        //! Визуальный индекс колонки куда происходит перемещение
        int visual_drop_to = -1;
        //! Визуальный индекс колонки куда происходит перемещение с учетом скрытых
        int visual_drop_to_hidden = -1;
        //! Если истина, то вставка слева от to, иначе справа
        bool drop_left = false;
    };
    std::shared_ptr<DragInfo> dragInfo(const QModelIndex& source_index,
                                       //! Интересует только информация о source_index
                                       bool source_only, const QModelIndex& target_index, const QPoint& target_pos) const;

    //! Индекс разделителя если мышь не на разделителе, то -1
    int sectionHandleAt(const QPoint& point) const;
    //! Разрешать ли изменение размера колонок в данной точке
    bool allowResize(const QPoint& point) const;

    ItemViewHeaderModel* _model = nullptr;
    HeaderView* _joined_header = nullptr;
    int _limit = 0;
    bool _allow_sorting = false;
    bool _allow_config = true;

    static const QString MimeType;
    QPoint _drag_start_pos;
    QPoint _sort_mouse_press_pos;

    //! Визуальный индекс колонки начала перемещаемой группы
    int _drag_from_begin = -1;
    //! Визуальный индекс колонки окончания перемещаемой группы
    int _drag_from_end = -1;
    //! Визуальный индекс колонки куда произошло перемещение
    int _drag_to = -1;
    //! Визуальный индекс колонки куда произошло перемещение с учетом скрытых
    int _drag_to_hidden = -1;
    //! Если истина, то вставка слева от to, иначе справа
    bool _drag_left = false;
    //! Перемещение колонки разрешено
    bool _drag_allowed = false;

    int _block_change_header_items_counter = 0;
    bool _block_emit_joined_flag = false;
    bool _need_update_joined_properties = false;

    QTimer* _reload_data_from_root_item_timer = nullptr;

    //! Для исключения двойной отрисовки ячеек
    mutable QList<QRect> _painted_rect;

    friend class Utils;
    friend class TableViewBase;
    friend class TreeView;
    friend class TableView;
};

} // namespace zf
