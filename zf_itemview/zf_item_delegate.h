#pragma once

#include "zf_itemview.h"
#include <QPointer>
#include <QStyledItemDelegate>

class QTextOption;
class QTextLayout;
class QTextDocument;

namespace zf
{
/*! Умеет отображать всплывающую подсказку, если текст не помещается в ячейку */
class ZF_ITEMVIEW_DLL_API HintItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    HintItemDelegate(QObject* parent = nullptr);
    QString displayText(const QVariant& value, const QLocale& locale) const override;  
    bool helpEvent(QHelpEvent* event, QAbstractItemView* v, const QStyleOptionViewItem& option,
            const QModelIndex& index) override;

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;
};

//! Интерфейс для отображения чекбокса в делегате
class I_ItemDelegateCheckInfo
{
public:
    virtual ~I_ItemDelegateCheckInfo() { }
    virtual void delegateGetCheckInfo(QAbstractItemView* item_view, const QModelIndex& index, bool& show, bool& checked) const = 0;
};

/*! Базовый делегат для ячеек таблиц и деревьев */
class ZF_ITEMVIEW_DLL_API ItemDelegate : public HintItemDelegate
{
    Q_OBJECT
public:
    //! Для использования отдельно от представления набора данных View
    ItemDelegate(
        //! QAbstractItemView, который обслуживает делегат
        QAbstractItemView* item_view,
        //!  QAbstractItemView от которого наследуется состояние фокуса и т.п.
        QAbstractItemView* main_item_view,
        //! Интерфейс для отображения чекбокса в делегате
        I_ItemDelegateCheckInfo* check_info_interface, QObject* parent = nullptr);

    //! Использовать html форматирование
    void setUseHtml(bool b);
    bool isUseHtml() const;

    //! QAbstractItemView, который обслуживает делегат
    QAbstractItemView* itemView() const;
    //! QAbstractItemView от которого наследуется состояние фокуса и т.п.
    QAbstractItemView* mainItemView() const;

    //! Активный редактор
    QWidget* currentEditor() const;

    //! Место чекбокса для индекса
    QRect checkBoxRect(const QModelIndex& index,
                       //! Если истина, то возвращает всю область чекбокса включая до границ ячейки, а не только "квадратик"
                       bool expand) const;

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void destroyEditor(QWidget* editor, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;    
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* v, const QStyleOptionViewItem& option, const QModelIndex& index) override;

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;    
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;    
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool eventFilter(QObject* object, QEvent* event) override;

    //! Только создание редактора. Переопределять этот метод, а не createEditor
    virtual QWidget* createEditorInternal(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    virtual void setEditorDataInternal(QWidget* editor, const QModelIndex& index) const;
    virtual void setModelDataInternal(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;

private slots:
    void sl_closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint);
    void sl_currentChanged(const QModelIndex& current, const QModelIndex& previous);
    //! Закрылся popup (для комбобокса или т.п.)
    void sl_popupClosed();
    //! Закрылся popup (для комбобокса или т.п.)
    void sl_popupClosed(bool applied);
    //! Изменилось состояние чекбокса
    void sl_checkboxChanged(int);

private:
    //! Получить отображаемый на экране текст
    QString getDisplayText(const QModelIndex& index, QStyleOptionViewItem* option) const;

    //! Отложенная инициализация
    void lazyInit() const;    
    //! Размер иконки по умолчанию
    static QSize iconSize(const QIcon& icon);

    //! Закрылся popup (для комбобокса или т.п.)
    void popupClosedInternal(bool applied);

    //! Отрисовка содержимого ячейки (частично выдрано из QCommonStyle)
    void paintCellContent(QStyle* style, QPainter* painter, const QStyleOptionViewItem* option, const QModelIndex& index, const QWidget* widget) const;
    //! Отрисовка текста ячейки (частично выдрано из QCommonStyle)
    void viewItemDrawText(QStyle* style, QPainter* p, const QStyleOptionViewItem* option, const QRect& rect) const;

    //! Инициализация rich text
    void initTextDocument(const QStyleOptionViewItem* option, const QModelIndex& index, QTextDocument& doc) const;

    //! выдрано из QCommonStyle
    static QSizeF viewItemTextLayout(QTextLayout& textLayout, int lineWidth, int maxHeight = -1, int* lastVisibleLine = nullptr);

    //! QAbstractItemView, который обслуживает делегат
    QAbstractItemView* _item_view = nullptr;
    //!  QAbstractItemView от которого наследуется состояние фокуса и т.п.
    QAbstractItemView* _main_item_view = nullptr;

    //! Интерфейс для отображения чекбокса в делегате
    I_ItemDelegateCheckInfo* _check_info_interface = nullptr;

    bool _view_mode = false;
    QTimer* _close_editor_timer = nullptr;
    mutable QPointer<QWidget> _close_widget;
    mutable QPointer<QWidget> _current_editor;
    mutable bool _initialized = false;
    //! Борьба с багом, при котором Qt вызывает сигнал закрытия редактора при открытии его через enter
    bool _fix_enter_key = false;

    //! Использовать html форматирование
    bool _use_html = true;
};

} // namespace zf

Q_DECLARE_METATYPE(QAbstractItemDelegate::EndEditHint)
