#include "zf_item_delegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QHeaderView>
#include <QHelpEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTableView>
#include <QTextDocument>
#include <QTimer>
#include <QToolTip>
#include <QTreeView>
#include <float.h>
#include <QTextOption>
#include <QTextLayout>
#include <QStylePainter>

#include "zf_utils.h"
#include "zf_html_tools.h"

namespace zf
{
HintItemDelegate::HintItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QString HintItemDelegate::displayText(const QVariant& value, const QLocale& locale) const
{
    return QStyledItemDelegate::displayText(value, locale);
}

bool HintItemDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* v, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    if (!opt.text.isEmpty()) {
        QRect rect = v->visualRect(index);

        opt.features = (opt.features | QStyleOptionViewItem::HasDisplay);
        QSize size = sizeHint(opt, index);
        if (size.height() > rect.height()) {
            // Активен перенос строк, но высота строки этого не позволяет, поэтому убираем перенос
            opt.features = (opt.features & (~QStyleOptionViewItem::WrapText));
            size = sizeHint(opt, index);
        }

        if (rect.width() < size.width()) {
            QToolTip::showText(event->globalPos(),
                QStringLiteral("<div>%1</div>")
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                    .arg(Qt::escape(opt.text)),
                v);
#else
                    .arg(opt.text.toHtmlEscaped()),
                v);
#endif
            return true;
        }
    }
    if (!QStyledItemDelegate::helpEvent(event, v, opt, index))
        QToolTip::hideText();
    return true;
}

void HintItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    option->textElideMode = Qt::ElideRight;
    //    option->features = option->features & ~QStyleOptionViewItem::WrapText;
}

ItemDelegate::ItemDelegate(QAbstractItemView* item_view, QAbstractItemView* main_item_view, I_ItemDelegateCheckInfo* check_info_interface, QObject* parent)
    : HintItemDelegate(parent)
    , _item_view(item_view)
    , _main_item_view(main_item_view)
    , _check_info_interface(check_info_interface)
    , _view_mode(false)
{
    Q_ASSERT(_item_view != nullptr);
}

void ItemDelegate::setUseHtml(bool b)
{
    if (b == _use_html)
        return;
    _use_html = b;
}

bool ItemDelegate::isUseHtml() const
{
    return _use_html;
}

QAbstractItemView* ItemDelegate::itemView() const
{
    lazyInit();
    return _item_view;
}

QAbstractItemView* ItemDelegate::mainItemView() const
{
    lazyInit();
    return _main_item_view;
}

QWidget* ItemDelegate::currentEditor() const
{
    return _current_editor;
}

QRect ItemDelegate::checkBoxRect(const QModelIndex& index, bool expand) const
{
    QRect rect;

    if (_check_info_interface != nullptr) {
        bool show_check = false;
        bool checked = false;
        _check_info_interface->delegateGetCheckInfo(_item_view, index, show_check, checked);
        if (show_check) {
            const int shift = 2;

            QStyle* style = _item_view->style() ? _item_view->style() : QApplication::style();
            QRect item_rect = _item_view->visualRect(index);

            int check_width = style->pixelMetric(QStyle::PM_IndicatorWidth);
            int check_height = style->pixelMetric(QStyle::PM_IndicatorHeight);

            if (expand)
                rect = {item_rect.left(), item_rect.top(), check_width + shift * 3, item_rect.height()};
            else
                rect = {item_rect.left() + shift, item_rect.top() + shift, check_width, check_height};

            rect.moveCenter({rect.center().x(), item_rect.center().y()});
        }
    }

    return rect;
}

QString ItemDelegate::getDisplayText(const QModelIndex& index, QStyleOptionViewItem* option) const
{
    Q_UNUSED(option)

    if (!index.isValid() || _item_view == nullptr)
        return QString();

    QString text = Utils::variantToString(index.data(Qt::DisplayRole));

    // не надо отображать текст true/false если задан CheckStateRole
    if (index.model()->itemData(index).contains(Qt::CheckStateRole) && (text == QStringLiteral("true") || text == QStringLiteral("false")))
        text.clear();

    return text;
}

void ItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    lazyInit();

    Q_UNUSED(option)

    if (_item_view == nullptr) {
        HintItemDelegate::updateEditorGeometry(editor, option, index);
        return;
    }

    // позиционируем Memo поля
    QPlainTextEdit* pe = qobject_cast<QPlainTextEdit*>(editor);
    if (pe != nullptr) {
        HintItemDelegate::updateEditorGeometry(editor, option, index);

        QRect r = pe->geometry();
        r.setHeight(pe->geometry().height() * 4); // высота 4 строки

        // Преобразуем координаты, т.к. QPlainTextEdit имеет окно в качестве родителя
        QWidget* base = Utils::getTopWindow();
        QPoint p = _item_view->viewport()->mapTo(base, r.topLeft());
        r.moveTopLeft(p);
        // Ограничение по размеру
        r.setWidth(qMin(r.width(), base->width()));
        r.setHeight(qMin(r.height(), base->height()));
        // Ограничение по положению
        if (r.right() > base->width())
            r.moveRight(base->width());
        if (r.left() < 0)
            r.moveLeft(0);
        if (r.bottom() > base->height())
            r.moveBottom(base->height());
        if (r.top() < 0)
            r.moveTop(0);

        pe->setGeometry(r);

    } else {
        QRect rect = _item_view->visualRect(index);
        rect.setRight(qMin(rect.right(), _item_view->viewport()->geometry().right() - 1));
        editor->setGeometry(rect);

        if (_main_item_view != nullptr && editor->parent() != _main_item_view) {
            // избавляемся от обрезания ширины виджета при редактировании фиксированных колонок
            editor->setParent(_main_item_view);
            QPoint pos = editor->pos();
            pos.setX(pos.x() + _main_item_view->viewport()->pos().x());
            pos.setY(pos.y() + _main_item_view->viewport()->pos().y());
            editor->move(pos);
        }
    }
}

QWidget* ItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)

    lazyInit();
    _current_editor = createEditorInternal(parent, option, index);

    if (_current_editor != nullptr) {
        if (QCheckBox* w = qobject_cast<QCheckBox*>(_current_editor)) {
            connect(w, &QCheckBox::stateChanged, this, &ItemDelegate::sl_checkboxChanged, Qt::QueuedConnection);
        }
    }

    if (_current_editor != nullptr && _current_editor->parent() == nullptr)
        _current_editor->setParent(parent);

    return _current_editor;
}

void ItemDelegate::destroyEditor(QWidget* editor, const QModelIndex& index) const
{
    lazyInit();
    HintItemDelegate::destroyEditor(editor, index);
}

void ItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    lazyInit();

    setEditorDataInternal(editor, index);

    if (QComboBox* w = qobject_cast<QComboBox*>(editor))
        w->showPopup();
}

void ItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    lazyInit();

    setModelDataInternal(editor, model, index);
}

bool ItemDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* v, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    return HintItemDelegate::helpEvent(event, v, option, index);
}

void ItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid()) {
        HintItemDelegate::paint(painter, option, index);
        return;
    }

    lazyInit();

    if (_item_view->indexWidget(index) != nullptr && index == _item_view->currentIndex()) {
        // чтобы не было видно выделения по краям редактора
        painter->fillRect(option.rect, _item_view->palette().base());
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    // отрисовка дополнительного чекбокса
    int check_shift = 0;
    if (_check_info_interface != nullptr) {
        bool show_check = false;
        bool checked = false;
        _check_info_interface->delegateGetCheckInfo(_item_view, index, show_check, checked);
        if (show_check) {
            QRect check_rect = checkBoxRect(index, false);
            QRect check_rect_expanded = checkBoxRect(index, true);

            painter->save();
            QStyleOptionButton check_option;

            check_option.rect = check_rect;
            check_option.state = QStyle::State_Enabled;
            check_option.state |= checked ? QStyle::State_On : QStyle::State_Off;

            QRect item_rect = _item_view->visualRect(index);
            bool mouse_over = false;
            if (item_rect.isValid()) {
                QPoint cursor_screen_pos = QCursor::pos();
                QPoint checkbox_screen_top_left = _item_view->viewport()->mapToGlobal(check_rect.topLeft());
                QRect checkbox_screen_rect = {checkbox_screen_top_left.x(), checkbox_screen_top_left.y(), check_rect.width(), check_rect.height()};
                mouse_over = checkbox_screen_rect.contains(cursor_screen_pos);
            }

            check_option.state.setFlag(QStyle::State_MouseOver, mouse_over);

            painter->save();
            painter->fillRect(opt.rect, opt.backgroundBrush);
            painter->restore();

            style->drawControl(QStyle::ControlElement::CE_CheckBox, &check_option, painter, opt.widget);

            painter->restore();

            check_shift += check_rect_expanded.width();
        }
    }

    painter->save();
    opt.rect.setLeft(opt.rect.left() + check_shift);
    paintCellContent(style, painter, &opt, index, opt.widget);
    painter->restore();
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    lazyInit();

    if (!index.isValid())
        return HintItemDelegate::sizeHint(option, index);

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QSize size;
    if (!opt.icon.isNull() && opt.text.isEmpty())
        size = iconSize(opt.icon);
    else {
        // криворукие программисты Qt не учитывают span при расчете sizeHint
        QTableView* tw = qobject_cast<QTableView*>(_item_view);
        if (tw) {
            int span = tw->columnSpan(index.row(), index.column());
            if (span > 1) {
                int span_col = index.column() + span - 1;
                if (span_col > tw->horizontalHeader()->count() - 1)
                    span_col = tw->horizontalHeader()->count() - 1;

                int real_width = 0;
                for (int col = index.column(); col < span_col; col++) {
                    if (!tw->horizontalHeader()->isSectionHidden(col))
                        real_width += tw->horizontalHeader()->sectionSize(col);
                }
                if (real_width > 0)
                    opt.rect.setWidth(real_width);
            }

            span = tw->rowSpan(index.row(), index.column());
            if (span > 1) {
                opt.rect.setWidth(9999);
            }
        }

        if (_use_html && HtmlTools::isHtml(opt.text)) {
            QTextDocument doc;
            initTextDocument(&opt, index, doc);
            size = QSize(doc.size().width(), doc.size().height()); //QSize(doc.idealWidth(), doc.size().height());
        } else {
            size = HintItemDelegate::sizeHint(opt, index);
        }
    }

    // не меньше минимального размера
    size.setHeight(qMax(size.height(), 22));

    return size;
}

bool ItemDelegate::eventFilter(QObject* object, QEvent* event)
{
    lazyInit();

    if (object == _item_view) {
        if (event->type() == QEvent::Resize) {
            if (!_close_widget.isNull())
                _close_editor_timer->start();
            return false;
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (_current_editor == nullptr && keyEvent->matches(QKeySequence::Cancel))
            return false; // борьба с багом при котором базовый делегат Qt жрет нажатия клавишь

        if (_current_editor == nullptr && (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)) {
            // Борьба с багом, при котором Qt вызывает сигнал закрытия редактора при открытии его через enter
            _fix_enter_key = true;
        }

        if (_current_editor != nullptr && (_current_editor == object || Utils::hasParent(object, _current_editor))) {
            // Для QPlainTextEdit необходима обработка закрытия
            QPlainTextEdit* pte = qobject_cast<QPlainTextEdit*>(_current_editor);
            if (pte != nullptr) {
                if ((keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) && keyEvent->modifiers() == Qt::ControlModifier) {
                    // Ctrl + Enter закрывает QPlainTextEdit
                    _close_widget = pte;
                    _close_editor_timer->start();
                    return false;
                }
            }
        }
    }

    return HintItemDelegate::eventFilter(object, event);
}

QWidget* ItemDelegate::createEditorInternal(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QWidget* result = nullptr;
    if (result == nullptr) {
        result = HintItemDelegate::createEditor(parent, option, index);
        if (result->parent() == nullptr)
            result->setParent(parent);
    }

    return result;
}

void ItemDelegate::setEditorDataInternal(QWidget* editor, const QModelIndex& index) const
{
    HintItemDelegate::setEditorData(editor, index);
}

void ItemDelegate::setModelDataInternal(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    HintItemDelegate::setModelData(editor, model, index);
}

void ItemDelegate::sl_closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
{
    // Борьба с багом, при котором Qt вызывает сигнал закрытия редактора при открытии его через enter
    if (_fix_enter_key) {
        _fix_enter_key = false;
        return;
    }

    lazyInit();

    Q_UNUSED(editor)
    Q_UNUSED(hint)

    _current_editor = nullptr;

    if (_close_widget.isNull())
        return;

    emit commitData(_close_widget.data());

    _close_widget = nullptr;
}

void ItemDelegate::sl_currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    lazyInit();

    if (current.isValid() || previous.isValid()) {
        for (int i = 0; i < _item_view->model()->columnCount(); i++) {
            if (current.isValid())
                _item_view->update(_item_view->model()->index(current.row(), i, current.parent()));
            if (previous.isValid())
                _item_view->update(_item_view->model()->index(previous.row(), i, previous.parent()));
        }
    }
}

void ItemDelegate::popupClosedInternal(bool applied)
{
    auto w = qobject_cast<QWidget*>(sender());
    if (w == nullptr)
        return;

    if (applied)
        emit commitData(w);

    emit closeEditor(w);
    if (_item_view)
        _item_view->setFocus();
}

void ItemDelegate::paintCellContent(QStyle* style, QPainter* p, const QStyleOptionViewItem* opt, const QModelIndex& index, const QWidget* widget) const
{
    Q_UNUSED(index)

    if (_use_html && HtmlTools::isHtml(opt->text)) {
        // код выдран из QCommonStyle::drawControl

        p->setClipRect(opt->rect);

        QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, opt, widget);
        QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, opt, widget);
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, opt, widget);

        // draw the background
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, opt, p, widget);

        // draw the check mark
        if (opt->features & QStyleOptionViewItem::HasCheckIndicator) {
            QStyleOptionViewItem option(*opt);
            option.rect = checkRect;
            option.state = option.state & ~QStyle::State_HasFocus;

            switch (opt->checkState) {
                case Qt::Unchecked:
                    option.state |= QStyle::State_Off;
                    break;
                case Qt::PartiallyChecked:
                    option.state |= QStyle::State_NoChange;
                    break;
                case Qt::Checked:
                    option.state |= QStyle::State_On;
                    break;
            }
            style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &option, p, widget);
        }

        // draw the icon
        QIcon::Mode mode = QIcon::Normal;
        if (!(opt->state & QStyle::State_Enabled))
            mode = QIcon::Disabled;
        else if (opt->state & QStyle::State_Selected)
            mode = QIcon::Selected;
        QIcon::State state = opt->state & QStyle::State_Open ? QIcon::On : QIcon::Off;
        opt->icon.paint(p, iconRect, opt->decorationAlignment, mode, state);

        // draw the text
        if (!opt->text.isEmpty()) {
            QPalette::ColorGroup cg = opt->state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
            if (cg == QPalette::Normal && !(opt->state & QStyle::State_Active))
                cg = QPalette::Inactive;

            if (opt->state & QStyle::State_Selected) {
                p->setPen(opt->palette.color(cg, QPalette::HighlightedText));
            } else {
                p->setPen(opt->palette.color(cg, QPalette::Text));
            }
            if (opt->state & QStyle::State_Editing) {
                p->setPen(opt->palette.color(cg, QPalette::Text));
                p->drawRect(textRect.adjusted(0, 0, -1, -1));
            }

            viewItemDrawText(style, p, opt, textRect);
        }

        // draw the focus rect
        if (opt->state & QStyle::State_HasFocus) {
            QStyleOptionFocusRect o;
            o.QStyleOption::operator=(*opt);
            o.rect = style->subElementRect(QStyle::SE_ItemViewItemFocusRect, opt, widget);
            o.state |= QStyle::State_KeyboardFocusChange;
            o.state |= QStyle::State_Item;
            QPalette::ColorGroup cg = (opt->state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
            o.backgroundColor = opt->palette.color(cg, (opt->state & QStyle::State_Selected) ? QPalette::Highlight : QPalette::Window);
            style->drawPrimitive(QStyle::PE_FrameFocusRect, &o, p, widget);
        }

    } else {
        style->drawControl(QStyle::CE_ItemViewItem, opt, p, opt->widget);
    }
}

void ItemDelegate::viewItemDrawText(QStyle* style, QPainter* p, const QStyleOptionViewItem* option, const QRect& rect) const
{
    const QWidget* widget = option->widget;
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;

    QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0); // remove width padding

    // Рисуем иконку
    if (!option->icon.isNull()) {
        QStyleOptionViewItem optIcon = *option;
        optIcon.text = QString();
        optIcon.state = option->state & ~(QStyle::State_HasFocus | QStyle::State_Active | QStyle::State_Selected);
        style->drawControl(QStyle::CE_ItemViewItem, &optIcon, p, option->widget);
    }

    p->save();
    p->translate(textRect.topLeft());
    QRect clip = textRect.translated(-textRect.topLeft());
    p->setClipRect(clip);

    QTextDocument doc;
    initTextDocument(option, option->index, doc);

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = clip;
    if (option->state & QStyle::State_Selected) {
        // Рисуем обводную линию вокруг текста
        QTextCharFormat format;
        format.setTextOutline(QPen(QColor("#fafafa"), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        QTextCursor cursor(&doc);
        cursor.select(QTextCursor::Document);
        cursor.mergeCharFormat(format);
        doc.documentLayout()->draw(p, ctx);

        format.setTextOutline(QPen(Qt::NoPen));
        cursor.mergeCharFormat(format);
    }
    // Рисуем текст без обводной линии
    ctx.palette.setColor(QPalette::Text, option->palette.color(QPalette::Text));
    doc.documentLayout()->draw(p, ctx);
    p->restore();

    if (option->rect.width() < doc.size().width() || option->rect.height() < doc.size().height()) {
        // отрисовка многоточия
        p->drawText(option->rect.adjusted(0, 0, -1, 2), Qt::AlignRight | Qt::AlignBottom, "...");
    }
}

void ItemDelegate::initTextDocument(const QStyleOptionViewItem* option, const QModelIndex& index, QTextDocument& doc) const
{
    QStyleOptionViewItem optionV4 = *option;
    initStyleOption(&optionV4, index);

    QStyle* style = optionV4.widget ? optionV4.widget->style() : QApplication::style();
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &optionV4).adjusted(2, 0, -2, -2);
    //    doc.setDefaultFont(qApp->font());
    doc.setTextWidth(textRect.width());
    doc.setHtml(optionV4.text);
}

QSizeF ItemDelegate::viewItemTextLayout(QTextLayout& textLayout, int lineWidth, int maxHeight, int* lastVisibleLine)
{
    if (lastVisibleLine)
        *lastVisibleLine = -1;
    qreal height = 0;
    qreal widthUsed = 0;
    textLayout.beginLayout();
    int i = 0;
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        widthUsed = qMax(widthUsed, line.naturalTextWidth());
        // we assume that the height of the next line is the same as the current one
        if (maxHeight > 0 && lastVisibleLine && height + line.height() > maxHeight) {
            const QTextLine nextLine = textLayout.createLine();
            *lastVisibleLine = nextLine.isValid() ? i : -1;
            break;
        }
        ++i;
    }
    textLayout.endLayout();
    return QSizeF(widthUsed, height);
}

void ItemDelegate::sl_popupClosed()
{
    popupClosedInternal(true);
}

void ItemDelegate::sl_popupClosed(bool applied)
{
    popupClosedInternal(applied);
}

void ItemDelegate::sl_checkboxChanged(int)
{
    auto w = qobject_cast<QWidget*>(sender());
    if (w == nullptr)
        return;

    emit commitData(w);
    emit closeEditor(w);
}

void ItemDelegate::lazyInit() const
{
    if (_initialized)
        return;
    _initialized = true;

    ItemDelegate* self = const_cast<ItemDelegate*>(this);

    self->_close_editor_timer = new QTimer(self);
    self->_close_editor_timer->setInterval(0);
    self->_close_editor_timer->setSingleShot(true);

    if (_main_item_view != nullptr)
        connect(_main_item_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &ItemDelegate::sl_currentChanged);
    else
        connect(_item_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &ItemDelegate::sl_currentChanged);

    _item_view->installEventFilter(self);

    connect(_close_editor_timer, &QTimer::timeout, this, [&]() {
        if (_close_widget.isNull())
            return;

        emit const_cast<ItemDelegate*>(this)->commitData(_close_widget.data());
        emit const_cast<ItemDelegate*>(this)->closeEditor(_close_widget.data(), QAbstractItemDelegate::SubmitModelCache);

        _close_widget = nullptr;
        _current_editor = nullptr;
    });

    connect(this, &ItemDelegate::closeEditor, this, &ItemDelegate::sl_closeEditor);
}

QSize ItemDelegate::iconSize(const QIcon& icon)
{
    QSize a_size = icon.actualSize(QSize(16, 16));
    if (a_size.width() > 16)
        a_size.setWidth(16);
    if (a_size.height() > 16)
        a_size.setHeight(16);
    return a_size;
}

void ItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    HintItemDelegate::initStyleOption(option, index);

    if (!index.isValid())
        return;

    lazyInit();

    QAbstractItemView* item_view = _main_item_view != nullptr ? _main_item_view : _item_view;
    QModelIndex current_index;
    QModelIndex source_index = index;
    // текущая ячейка
    bool is_current_cell = false;
    // строка выделена
    bool is_selected_row = false;
    // текущая строка
    bool is_current_row = false;

    if (item_view != nullptr) {
        // определение текущего индекса
        current_index = item_view->currentIndex();
        auto tree = qobject_cast<QTreeView*>(item_view);
        if (tree != nullptr && current_index.column() != 0 && tree->isFirstColumnSpanned(current_index.row(), current_index.parent())) {
            current_index = tree->model()->index(current_index.row(), 0, current_index.parent());
        }

// цвет фона текущей строки при условии что фокус не на таблице
#define COLOR_CURRENT_LINE_BACKGROUND_NOT_FOCUSED QColor(QStringLiteral("#ebf5ff"))
// цвет фона текущей строки при условии что фокус на таблице
#define COLOR_CURRENT_LINE_BACKGROUND_FOCUSED QColor(QStringLiteral("#ebf5ff"))
// цвет фона выделенной ячейки при условии что фокус не на таблице
#define COLOR_SELECTED_BACKGROUND_NOT_FOCUSED QColor(QStringLiteral("#bfdfff"))
// цвет фона выделенной ячейки при условии что фокус на таблице
#define COLOR_SELECTED_BACKGROUND_FOCUSED QColor(QStringLiteral("#04aa6d"))
// Надо ли менять цвет текст для текущей ячейки, на которой нет фокуса ввода
#define CHANGE_FONT_COLOR_FOR_NOT_FOCUSED false

        bool has_selection = false;
        // находится ли таблица в фокусе
        bool is_focused = item_view->hasFocus();

        // для оптимизации не используем selectedRows
        const QItemSelection selection = item_view->selectionModel()->selection();
        // выделено больше одной строки
        bool selected_more_one = selection.count() > 1 || (selection.count() == 1 && selection.at(0).bottom() > selection.at(0).top());
        QModelIndex first_selected;
        if (!selection.isEmpty())
            first_selected = item_view->model()->index(selection.at(0).top(), 0, selection.at(0).parent());

        if (!selection.isEmpty() && !selected_more_one) {
            bool is_select_current_row = first_selected.row() == current_index.row() && first_selected.parent() == current_index.parent();
            if (is_select_current_row) {
                // выделена одна строка и она же является текущей. делаем вид что вообще ничего не выделено
                is_selected_row = false;
                is_current_cell = index == current_index;

            } else {
                // выделена не текущая строка. выделяем ее и не показываем текущую
                is_selected_row = first_selected.row() == index.row() && first_selected.parent() == index.parent();
                is_current_cell = false;
                has_selection = true;
            }

        } else if (selected_more_one) {
            // выделено более одной строки
            is_selected_row = item_view->selectionModel()->isRowSelected(index.row(), index.parent());
            is_current_cell = false;
            has_selection = true;

        } else {
            // выделения нет
            is_selected_row = false;
            is_current_cell = current_index == index;
        }

        // текущая строка
        if (!has_selection && current_index.isValid() && item_view->selectionBehavior() == QAbstractItemView::SelectRows
            && current_index.parent() == index.parent()) {
            // Текущая активная строка в режиме QAbstractItemView::SelectRows
            // Проверяем span
            QTableView* tw = qobject_cast<QTableView*>(item_view);
            if ((!tw && current_index.row() == index.row())
                || (tw && index.row() >= current_index.row() && index.row() < current_index.row() + tw->rowSpan(current_index.row(), current_index.column()))) {
                is_current_row = true;
            }
        }

        if (is_current_cell || is_selected_row)
            option->backgroundBrush = is_focused ? COLOR_SELECTED_BACKGROUND_FOCUSED : COLOR_SELECTED_BACKGROUND_NOT_FOCUSED;
        else if (is_current_row)
            option->backgroundBrush = is_focused ? COLOR_CURRENT_LINE_BACKGROUND_FOCUSED : COLOR_CURRENT_LINE_BACKGROUND_NOT_FOCUSED;
        else
            option->backgroundBrush = QColor(Qt::white);

        QColor font_color;
        if ((is_current_cell || is_selected_row) && !index.data(Qt::CheckStateRole).isValid() && (CHANGE_FONT_COLOR_FOR_NOT_FOCUSED || is_focused))
            font_color = QColor(Qt::white);
        else
            font_color = QColor(Qt::black);
        option->palette.setColor(QPalette::Text, font_color);
    }

    // Убираем флаги выделения, чтобы Qt не перерисовывал фокус
    option->state = option->state & (~(QStyle::State_HasFocus | QStyle::State_Selected));

    // переопределяем текст
    option->text = getDisplayText(source_index, option);

    if (!option->icon.isNull()) {
        if (option->text.isEmpty()) {
            option->displayAlignment = Qt::AlignCenter;
            option->decorationAlignment = Qt::AlignCenter;
        }
        option->features |= QStyleOptionViewItem::HasDecoration;
        option->decorationSize = QSize(16, 16);
    }
}

} // namespace zf
