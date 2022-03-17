#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QHeaderView>
#include <QObject>
#include <QPair>
#include <QSize>
#include <QTimer>
#include <QVariant>

#include "zf_itemview.h"

namespace zf
{
class ItemViewHeaderModel;

//! Узел иерархических заголовков. Для получения корневого узла таблиц и деревьев использовать
//! zf::TableView::rootHeaderItem и zf::TreeView::rootHeaderItem
class ZF_ITEMVIEW_DLL_API HeaderItem : public QObject
{
    Q_OBJECT
public:
    ~HeaderItem() override;

    //! Корневой узел
    bool isRoot() const;

    //! Узел находится внизу иерархии
    bool isBottom() const;
    //! Узел находится вверху иерархии
    bool isTop() const;

    //! Начать изменение структуры заголовка (только для root)
    void beginUpdate();    
    //! Закончить изменение структуры заголовка (только для root)
    void endUpdate();
    bool isUpdating() const;

    //! Стереть все данные об узлах нижнего уровня (для root о всех)
    void clear();

    //! Корневой узел. Имеет идентификатор: -1
    HeaderItem* root() const;

    //! Найти узел по его идентификатору. Только для корневого узла. Ищет рекурсивно во всех дочерних узлах
    HeaderItem* item(
        //! Идентификатор. Если -1, то возвращает самого себя
        int id,
        //! Если не найден - критическая ошибка
        bool halt_if_not_found = true) const;
    //! Содержит ли узел с указанным идентификатором. Только для корневого узла. Ищет рекурсивно во всех дочерних узлах
    bool containsItem(
        //! Идентификатор
        int id) const;

    //! Идентификатор узла. Уникален в рамках всего заголовка
    int id() const { return _id; }

    QString label(
        //! Получить текст, заданный при конфигурации, без учета наличия рисунка или hide_label
        bool original_text = false) const;
    HeaderItem* setLabel(const QString& s);

    QFont font() const;
    HeaderItem* setFont(const QFont& f);

    HeaderItem* setBold(bool b);
    bool isBold() const;

    HeaderItem* setItalic(bool b);
    bool isItalic() const;

    HeaderItem* setUnderline(bool b);
    bool isUnderline() const;

    QColor foreground() const;
    HeaderItem* setForeground(const QColor& color);

    QColor background() const;
    HeaderItem* setBackground(const QColor& color);

    QIcon icon() const;
    HeaderItem* setIcon(const QIcon& icon,
        //! Если истина, то при заданной иконке, текст будет скрыт
        bool hide_label = false);

    //! Есть ли сортировка
    bool isSorting() const;
    //! Порядок сортировки
    Qt::SortOrder sortOrder() const;

    //! Преобразовать визуальную позицию колонки в логическую (только для root). Результат всегда >= 0
    int visualToLogical(int pos) const;
    //! Преобразовать логическую позицию колонки в визуальную (только для root). Может вернуть -1, если колонка скрыта
    int logicalToVisual(int pos) const;

    //! Сортировать (только для узлов нижнего уровня). Генерирует сигналы, на которые реагирует представление
    void sort(Qt::SortOrder order);
    //! Сортировать по номеру секции нижнего уровня (только для root). Генерирует сигналы, на которые реагирует представление
    void sort(
        //! Если -1, то сортировка отключается
        int section, Qt::SortOrder order);
    //! Отключить сортировку (только для root). Генерирует сигналы, на которые реагирует представление
    void sortOff();

    //! Задать размер колонки в количестве символов для всех дочерних колонок
    HeaderItem* setDefaultSectionSizeCharCount(int c);
    //! Задать размер колонки в пикселях для всех дочерних колонок
    HeaderItem* setDefaultSectionSize(int sectionSize);
    //! Размер секции по умолчанию
    int defaultSectionSize() const;

    //! Задать размер секции в количестве символов (только для горизонтального заголовка)
    HeaderItem* setSectionSizeCharCount(int c);
    //! Задать размер секции в пикселях
    HeaderItem* setSectionSize(int sectionSize);
    //! Размер секции (для горизонтального - ширина колонки, вертикального - высота строки)
    int sectionSize() const;
    //! Размер суммы секций нижнего уровня
    int bottomSectionsSize(int start_section, int end_section, bool only_visible = false) const;

    //! Для вертикального - ширина ячейки
    int verticalCellWidth() const;
    //! Задать ширину ячейки для вертикального заголовка
    HeaderItem* setVerticalCellWidth(int sectionSize);
    //! Задать ширину ячейки для вертикального заголовка в количестве символов
    HeaderItem* setVerticalCellCharCount(int c);

    //! Задать режим отображения
    HeaderItem* setResizeMode(QHeaderView::ResizeMode mode);
    QHeaderView::ResizeMode resizeMode() const;

    //! Скрыть/показать узел
    HeaderItem* setHidden(bool b);
    HeaderItem* setVisible(bool b) { return setHidden(!b); }
    HeaderItem* show() { return setHidden(false); }
    HeaderItem* hide() { return setHidden(true); }
    bool isHidden() const;
    bool isVisible() const { return !isHidden(); }

    //! Разрешить/запретить перенос колонок пользователем
    HeaderItem* setMovable(bool b);
    bool isMovable() const;

    //! Скопировать дочернюю структуру в QByteArray
    QByteArray toByteArray(int data_stream_version = QDataStream::Qt_5_6) const;
    //! Восстановить дочернюю структуру из QByteArray
    void fromByteArray(const QByteArray& data, int data_stream_version = QDataStream::Qt_5_6);

    /*! Переместить узел с точки зрения визуального отображения. Фактический порядок не меняется
     * Допустимы перемещения только в рамках одной группы (общий parent) */
    bool move(
        //! Идентификатор другого узла
        int id,
        //! Переместить перед или после
        bool before);
    /*! Переместить узел с точки зрения визуального отображения. Фактический порядок не меняется
     * Допустимы перемещения только в рамках одной группы (общий parent) */
    bool move(
        //! Идентификатор другого узла
        HeaderItem* item,
        //! Переместить перед или после
        bool before);

    //! Узел полностью скрыт для отображения и не будет показан пользователю (колонки таблицы для хранения служебной
    //! информации)
    HeaderItem* setPermanentHidden(bool b);
    bool isPermanentHidded() const;

    //! Добавить дочерний узел
    HeaderItem* append(
        //! Идентификатор. Должен быть уникальным в рамках всего заголовка
        int id,
        //! Текст заголовка
        const QString& label = QString());
    //! Добавить дочерний узел. ID узла устанавливается в count()+1
    HeaderItem* append(
        //! Текст заголовка
        const QString& label = QString());    
    //! Вставить дочерний узел в указанную позицию
    HeaderItem* insert(
        //! Позиция вставки
        int pos,
        //! Идентификатор. Должен быть уникальным в рамках всего заголовка
        int id,
        //! Текст заголовка
        const QString& label = QString());

    //! Удалить дочерний узел. Только для прямых потомков. Если не найден - критическая ошибка
    void remove(HeaderItem* child);
    //! Удалить дочерний узел в указанной позиции
    void remove(int pos);

    //! Нарушен ли порядок колонок по умолчанию
    bool isOrderChanged() const;
    //! Восстановить порядок по умолчанию
    void resetOrder();
    //! Показать все скрытые
    void resetHidden();

    //! Родительский узел
    HeaderItem* parent() const;
    //! Родительский узел верхнего уровня (root не используется). Возвращает самого себя, если это уже верхний уровень
    HeaderItem* topParent() const;

    //! Список дочерних узлов
    QList<HeaderItem*> children() const;
    //! Дочерний узел по индексу
    HeaderItem* child(int pos) const;

    //! Количество дочерних узлов
    int count() const;
    //! Содержит дочерние узлы
    bool hasChildren() const;

    //! Позиция в списке дочерних узлов родителя
    int pos() const;
    //! Позиция в списке узлов нижнего уровня
    int bottomPos() const;
    //! Позиция в списке узлов нижнего уровня с точки зрения визуального отображения
    int bottomVisualPos(Qt::SortOrder order = Qt::AscendingOrder,
        //! Учитывать только видимые узлы
        bool visible_only = false) const;

    //! Список дочерних узлов с точки зрения визуального отображения
    QList<HeaderItem*> childrenVisual(Qt::SortOrder order = Qt::AscendingOrder,
        //! Учитывать только видимые узлы
        bool visible_only = false) const;
    //! Позиция в списке дочерних узлов родителя с точки зрения визуального отображения
    int visualPos(
        //! Учитывать только видимые узлы
        bool visible_only = false) const;
    //! Дочерний узел по порядку отображения на экране
    HeaderItem* childVisual(int visual_pos, Qt::SortOrder order = Qt::AscendingOrder) const;

    //! Список всех дочерних узлов
    QList<HeaderItem*> allChildren(
        //! На сколько уровней искать
        int depth = -1) const;
    //! Список всех родительских узлов без root
    QList<HeaderItem*> allParent() const;
    //! Список всех узлов нижнего уровня
    QList<HeaderItem*> allBottom() const;
    //! Список всех узлов нижнего уровня, отсортированных с точки зрения отображения на экране
    QList<HeaderItem*> allBottomVisual(Qt::SortOrder order = Qt::AscendingOrder,
        //! Учитывать только видимые узлы
        bool visible_only = false) const;

    //! Узел нижнего уровня по порядковому номеру
    HeaderItem* bottomItem(int section) const;
    //! Количество узлов нижнего уровня
    int bottomCount(bool visible_only = false) const;
    //! Количество узлов нижнего уровня, для которых не установлено свойство permanentHidden
    int bottomPermanentVisibleCount() const;
    //! Узлы нижнего уровня, для которых не установлено свойство permanentHidden
    QList<HeaderItem*> bottomPermanentVisible() const;

    //! Максимальная сумма размеров в глубину по всем дочерним узлам + levelsSize этого узла
    int levelSizeToBottom() const;
    //! Сумма размеров по всем родительским узлам + levelsSize этого узла
    int levelSizeToTop() const;

    //! Содержит ли такой дочерний узел. Проверяет только прямых потомков
    bool contains(HeaderItem* child) const;
    //! Содержит ли дочерний узел с указанным ID. Проверяет только прямых потомков
    bool contains(int child_id) const;

    //! Позиция дочернего узла. Проверяет только прямых потомков. Если не найден, то -1
    int childPos(HeaderItem* child) const;
    //! Позиция дочернего узла по его id. Проверяет только прямых потомков. Если не найден, то -1
    int childPos(int child_id) const;

    //! Пересчитать состояние
    void recalc();

    //! Начальный уровень (для горизонтального заголовка - строка, для вертикального - колонка)
    int levelFrom() const;
    //! Начальная секция (для горизонтального заголовка - колонка, для вертикального - строка)
    int sectionFrom(
        //! Если false, то номер в порядке визуального отображения
        bool logical = true,
        //! Истина только для (logical = false)
        //! Если true, то при подсчете порядка учитываются только видимые секции
        bool visible_only = false) const;
    //! На сколько уровней распространяется
    int levelSpan() const;
    //! На сколько секций распространяется
    int sectionSpan(
        //! Если true, то при подсчете порядка учитываются только видимые секции
        bool visible_only = false) const;
    //! Конечный уровень (для горизонтального заголовка - строка, для вертикального - колонка)
    int levelTo() const;
    //! Конечная секция (для горизонтального заголовка - колонка, для вертикального - строка)
    int sectionTo(
        //! Если false, то номер в порядке визуального отображения
        bool logical = true,
        //! Истина только для (logical = false)
        //! Если true, то при подсчете порядка учитываются только видимые секции
        bool visible_only = false) const;

    //! Начальная строка (для горизонтального: levelFrom, вертикального: sectionFrom)
    int rowFrom() const;
    //! Начальная колонка (для горизонтального: sectionFrom, вертикального: levelFrom)
    int columnFrom() const;
    //! На сколько строк распространяется (для горизонтального: levelSpan, вертикального: sectionSpan)
    int rowSpan() const;
    //! На сколько колонок распространяется (для горизонтального: sectionSpan, вертикального: levelSpan)
    int columnSpan() const;
    //! Начальная строка (для горизонтального: levelTo, вертикального: sectionTo)
    int rowTo() const;
    //! Начальная колонка (для горизонтального: sectionTo, вертикального: levelTo)
    int columnTo() const;

    /*! Размер секции для отображения на экране
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QSize itemSize() const;
    /*! Размер всей группы для отображения на экране (этот узел и его дочерние узлы)
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QSize itemGroupSize() const;

    /*! Область секции для отображения на экране. При отображении надо учесть сдвиг в QScrollView
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QRect sectionRect() const;
    /*! Область группы для отображения на экране. При отображении надо учесть сдвиг в QScrollView
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QRect groupRect() const;

    //! Пустой узел (для внутреннего использования)
    bool isEmpty() const;

    //! Отобразить все скрытые секции за исключением isSectionPermanentHidden
    void showHiddenSections();
    //! Можно скрыть колонки (должно остаться минимум одна видимая колонка)
    bool canHideSection(HeaderItem* item) const;
    //! Имеет скрытые колонки (без учета постоянно скрытых
    bool hasHiddenSections() const;

    Qt::Orientation orientation() const;
    QFontMetrics fontMetrics() const;
    ItemViewHeaderModel* model() const;

    //! Первая видимая секция относительно root
    int firstVisibleSection() const;
    //! Последняя видимая секция относительно root
    int lastVisibleSection() const;

    //! Сдвиг иконки относительно левой части ячейки заголовка
    static const int ICON_LEFT_SHIFT;

signals:
    //! Поменялись свойства узла, требующие перестройки структуры
    void sg_structureChanged();
    //! Вызывается перед удалением узла
    void sg_beforeDelete();

    //! Изменился режим resizeMode для данного узла (только для узлов нижнего уровня). Генерируется root
    void sg_resizeModeChanged(int section, QHeaderView::ResizeMode mode);
    //! Генерируется root
    void sg_hiddenChanged(
        //! Список узлов нижнего уровня, для которых изменилась видимость
        const QList<zf::HeaderItem*>& bottom_items, bool is_hide);
    //! Секции были перемещены с точки зрения отображения. Генерируется root
    void sg_visualMoved(
        //! Какой узел перемещен
        zf::HeaderItem* moved,
        //! Из какой позиции относительно своего родителя
        int visual_pos_from,
        //! В какую позицию относительно своего родителя
        int visual_pos_to,
        //! True, если перемещается между (visual_pos_to-1) и (visual_pos_to)
        //! False, если перемещается между (visual_pos_to) и (visual_pos_to+1)
        bool before);
    //! Изменился размер секции, находящейся все диапазона управляемых секций. Генерируется root для секций
    //! вертикального заголовка, номер которых превышает количество секций заголовка
    void sg_outOfRangeResized(int section, int size);
    //! Изменилась сортировка. Генерируется root для секций нижнего уровня горизонтального заголовка
    void sg_sortChanged(
        //! -1 если раньше не было сортировки
        int old_section,
        //! Если new_section -1, то сортировка отключена
        int new_section, Qt::SortOrder order);

private slots:
    //! Удалены дочерние узлы
    void sl_childDeleted();
    //! Добавлены или удалены дочерние узлы, поменялись их свойства, требующие перестройки структуры
    void sl_childStructureChanged();

private:
    enum class Type
    {
        Root,
        Item,
        Empty
    };

    //! Создать корневой узел
    static HeaderItem* createRoot(Qt::Orientation orientation, ItemViewHeaderModel* model);
    //! Создать пустой узел
    static HeaderItem* createEmpty(Qt::Orientation orientation, ItemViewHeaderModel* model, int level, int section);

    //! корневой или пустой
    HeaderItem(Type type, Qt::Orientation orientation, ItemViewHeaderModel* header);
    //! дочерний
    HeaderItem(HeaderItem* parent, int id, const QString& label);

    //! Генерация сигналов модели
    void dataChanged(int role);

    //! Список всех узлов нижнего уровня, отсортерованных с точки зрения отображения на экране
    QList<HeaderItem*> allBottomVisualHelper(Qt::SortOrder order, bool visible_only) const;

    //! Задать секции, которые всегда должны оставаться скрытыми
    void setPermanentHiddenSection(int logicalIndex, bool hidden);
    bool isSectionPermanentHidden(int logicalIndex) const;

    void sortHelper(HeaderItem* item, Qt::SortOrder order = Qt::AscendingOrder);
    HeaderItem* currentSortingItem() const;

    QList<HeaderItem*> setHiddenHelper(bool b);
    QList<HeaderItem*> setPermanentHiddenHelper(bool b);

    //! Аналог QHeaderView::isSectionHidden
    bool isSectionHidden(int section) const;

    //! Аналог QHeaderView::visualIndex
    int visualSection(int logical_section) const;
    //! Аналог QHeaderView::logicalIndex
    int logicalSection(int visual_section) const;
    //! Порядковый номер слева с учетом видимых колонок
    int realVisualSection(int visual_section) const;

    //! Задать размеры нескольких секций сразу. Размеры задаются для секций нижнего уровня. Верхние секции просто
    //! наследуют размер от нижних
    void setSectionsSizes(
        //! Ключ - номер секции, значение - размер
        const QMap<int, int>& sizes);

    //! Предыдущий узел на одном уровне с точки зрения отображения на экране
    HeaderItem* previousVisual(bool use_cache) const;
    //! Слудующий узел на одном уровне с точки зрения отображения на экране
    HeaderItem* secondVisual(bool use_cache) const;

    //! Левый верхний угол секции относительно самой первой
    QPoint sectionCorner(bool use_cache) const;

    //! Отступы по бокам ячеек
    int margin() const;

    //! Текст, разбитый по слогам
    QString labelMultiline() const;
    //! Разбить текст по слогам на основании текущего размера секции по горизонтали. Истина, если разбиение изменилось
    bool calculateLabelMultiline();

    //! Для горизонтального заголовка - высота ячейки, для вертикального - ширина ячейки
    int levelSize() const;
    //! Для горизонтального заголовка - высота ячеек всех подчиненных узлов, для вертикального - ширина ячеек всех
    //! подчиненных узлов
    int groupLevelSize() const;
    //! Задать ширину ячейки (только для вертикального заголовка)
    HeaderItem* setLevelSize(int sectionSize);

    //! Задать размер секции в пикселях и разбить размер на дочерние узлы
    void setSizeSplitHelper(int sectionSize, bool split_size);

    //! Найти по уровню и секции
    HeaderItem* findByPos(int level, int section,
        //! Учитывать спан или точно искать совпадение уровня и секции
        bool use_span) const;
    //! Найти по индексу модели
    HeaderItem* findByPos(const QModelIndex& index,
        //! Учитывать спан или точно искать совпадение уровня и секции
        bool use_span) const;

    //! Расчитать спан для узлов. Возвращает количество дочерних элементов нижнего уровня
    int calculateSpan(int level, int section);
    //! Расчет спана по уровням
    void calculateLevelSpan(int level_count);
    //! Найти количество уровней
    int calcLevelCount() const;
    //! Найти количество секций
    int calcSectionCount() const;

    //! Рассчитать размеры секций
    void calculateSectionsSize();
    void calculateSectionsSizeHelper();

    //! Позиция в списке дочерних узлов родителя с точки зрения визуального отображения
    int visualPosHelper(
        //! Учитывать только видимые узлы
        bool visible_only) const;

    /*! Размер секции для отображения на экране
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QSize itemSizeHelper() const;
    /*! Размер всей группы для отображения на экране (этот узел и его дочерние узлы)
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QSize itemGroupSizeHelper() const;

    /*! Область секции для отображения на экране. При отображении надо учесть сдвиг в QScrollView
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QRect sectionRectHelper() const;
    /*! Область группы для отображения на экране. При отображении надо учесть сдвиг в QScrollView
     * width - по горизонтали, height - повертикали (вне зависимости от ориентации заголовка) */
    QRect groupRectHelper() const;

    //! Найти по уровню и секции
    HeaderItem* findByPosHelper(int level, int section,
                                //! Учитывать спан или точно искать совпадение уровня и секции
                                bool use_span) const;

    //! Скопировать дочернюю структуру в QByteArray
    void toByteArrayHelper(QDataStream& ds) const;
    //! Восстановить дочернюю структуру из QByteArray
    void fromByteArrayHelper(QDataStream& ds);

    Type _type = Type::Empty;
    ItemViewHeaderModel* _model = nullptr;
    Qt::Orientation _orientation;
    HeaderItem* _parent = nullptr;
    HeaderItem* _root = nullptr;

    QList<HeaderItem*> _children;
    //! Дочерние узлы в порядке отображени
    QList<HeaderItem*> _children_visual_order;

    int _update_counter = 0;
    bool _is_initialized = false;
    QTimer* _recalc_timer = nullptr;

    //! Идентификатор
    int _id = -1;

    QString _label;
    QString _label_multi_line;
    bool _hide_label = false;
    QFont* _font = nullptr;
    bool _bold = false;
    bool _italic = false;
    bool _underline = false;
    QColor _foreground;
    QColor _background;
    QIcon _icon;
    bool _sorting = false;
    Qt::SortOrder _sort_order = Qt::AscendingOrder;

    QHeaderView::ResizeMode _resize_mode = QHeaderView::Interactive;
    bool _is_hidden = false;
    bool _is_permananet_hidden = false;
    bool _is_movable = true;

    //! Размер в пикселях всего заголовка по направлению уровней (от начального до конечного уровня)
    int _level_size = 0;
    //! Размер в пикселях всего заголовка и его дочерних элементов по направлению уровней
    int _group_level_size = 0;
    //! Размер в пикселях всего заголовка по направлению секций (от начальной до конечной секции)
    int _section_size = 0;

    int _default_section_size = 0;

    //! Начальный уровень (для горизонтального заголовка - строка, для вертикального - колонка)
    int _level_from = -1;
    //! Начальная секция (для горизонтального заголовка - колонка, для вертикального - строка)
    int _section_from = -1;
    //! На сколько уровней распространяется (для корневого заголовка - максимальный уровень)
    int _level_span = -1;
    //! На сколько секций распространяется (для корневого заголовка - количество элементов нижнего уровня)
    int _section_span = -1;
    //! Конечный уровень (для горизонтального заголовка - строка, для вертикального - колонка)
    int _level_span_to = -1;
    //! Конечная секция (для горизонтального заголовка - колонка, для вертикального - строка)
    int _section_span_to = -1;

    //! Очистить кэшированные значения
    void clearCache() const;
    //! Расчитать кэшированные значения
    void updateCache() const;

    mutable bool _is_cache_updating = false;
    mutable bool _cached = false;
    //! Хэш по всем узлам для root. Ключ - id, значение - заголовок
    mutable QHash<int, HeaderItem*> _all_children_by_id;
    //! Хэш по всем узлам для root. Ключ - id, значение - visualPos(false)
    mutable QHash<int, int> _all_visual_pos;
    //! Хэш по всем узлам для root. Ключ - id, значение - visualPos(true)
    mutable QHash<int, int> _all_visual_pos_visible_only;
    //! Хэш по всем узлам для root. Ключ - id, значение - sectionRect
    mutable QHash<int, QRect> _all_sections_rect;
    //! Хэш по всем узлам для root. Ключ - id, значение - groupRect
    mutable QHash<int, QRect> _all_group_rect;
    //! Хэш по всем узлам для root. Ключ - id, значение - itemSize
    mutable QHash<int, QSize> _all_sizes;
    //! Хэш по всем узлам для root. Ключ - id, значение - itemGroupSize
    mutable QHash<int, QSize> _all_group_sizes;
    //! Хэш по всем узлам для root. Ключ - {level, section}, значение - HeaderItem (findByPos - use_span=false)
    mutable QHash<QPair<int,int>, HeaderItem*> _all_find_by_pos;
    //! Хэш по всем узлам для root. Ключ - {level, section}, значение - HeaderItem (findByPos - use_span=true)
    mutable QHash<QPair<int, int>, HeaderItem*> _all_find_by_pos_span;

    static const QChar AVERAGE_CHAR;

    friend class ItemViewHeaderModel;
    friend class HeaderView;
    friend class Utils;
};

} // namespace zf
