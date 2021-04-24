#include "zf_tree_view_p.h"
#include "zf_tree_view.h"
#include "zf_header_view.h"
#include "zf_utils.h"

#include <QStylePainter>
#include <QMouseEvent>
#include <QApplication>

namespace zf
{
TreeCheckBoxPanel::TreeCheckBoxPanel(TreeView* parent)
    : QWidget(parent)
    , _view(parent)
{
    setMouseTracking(true);
}

void TreeCheckBoxPanel::paintEvent(QPaintEvent* e)
{
    QWidget::paintEvent(e);

    if (_view->model() == nullptr)
        return;

    QStylePainter painter(this);

    QStyleOptionButton check_option;
    auto rects = checkboxRects();

    QStyleOptionViewItem grid_option;
    grid_option.initFrom(_view);

    for (auto& rect : qAsConst(rects)) {
        if (rect.first.isNull())
            continue;

        QModelIndex row_index = _view->indexAt({0, rect.first.center().y() - offset()});
        Q_ASSERT(row_index.isValid());
        row_index = Utils::getTopSourceIndex(row_index);

        // чекбокс
        bool checked = _view->isRowChecked(row_index);
        painter.save();
        check_option.rect = rect.second;
        check_option.state = QStyle::State_Enabled;
        check_option.state |= checked ? QStyle::State_On : QStyle::State_Off;

        QPoint mouse_pos = mapFromGlobal(QCursor::pos());
        check_option.state.setFlag(QStyle::State_MouseOver, check_option.rect.contains(mouse_pos));

        painter.drawControl(QStyle::ControlElement::CE_CheckBox, check_option);
        painter.restore();

        // линии
        painter.save();
        painter.setPen(Utils::uiLineColor(true));
        painter.drawLine(width() - 1, rect.first.top() - 1, width() - 1, rect.first.bottom());
        painter.restore();
    }

    // заголовок
    if (!_view->header()->isHidden()) {
        painter.save();
        QRect header_rect
            = QRect(1, _view->horizontalHeader()->geometry().top(), width() - 1, _view->horizontalHeader()->geometry().bottom());
        painter.fillRect(header_rect, palette().brush(QPalette::Button));

        painter.setPen(Utils::uiLineColor(true));
        painter.drawLine(header_rect.right(), header_rect.top(), header_rect.right(), header_rect.bottom());
        painter.drawLine(header_rect.left(), header_rect.bottom(), header_rect.right(), header_rect.bottom());

        painter.restore();

        // чекбокс в заголовке
        painter.save();
        bool checked = _view->isAllRowsChecked();
        check_option.rect = headerCheckboxRect().second;

        check_option.state = QStyle::State_Enabled;
        check_option.state |= checked ? QStyle::State_On : QStyle::State_Off;
        QPoint mouse_pos = mapFromGlobal(QCursor::pos());
        check_option.state.setFlag(QStyle::State_MouseOver, check_option.rect.contains(mouse_pos));

        painter.drawControl(QStyle::ControlElement::CE_CheckBox, check_option);
        painter.restore();
    }

    // вертикальная линия
    painter.save();
    painter.setPen(Utils::uiLineColor(true));
    painter.drawLine(width() - 1, 0, width() - 1, rect().bottom());
    painter.restore();
}

void TreeCheckBoxPanel::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() == Qt::LeftButton && !_ignore_group_check) {
        QModelIndex index = cursorIndex(e->pos());
        if (index.isValid())
            _view->checkRow(index, _is_group_checked);
    }

    update();
}

void TreeCheckBoxPanel::mousePressEvent(QMouseEvent* e)
{
    if (_view->model() == nullptr)
        return;

    if (headerCheckboxRect().second.contains(e->pos())) {
        _view->checkAllRows(!_view->isAllRowsChecked());
        _ignore_group_check = true;
        return;
    }

    QModelIndex index = cursorIndex(e->pos());
    if (index.isValid()) {
        bool checked = !_view->isRowChecked(index);
        _view->checkRow(index, checked);
        _is_group_checked = checked;
        _ignore_group_check = false;

    } else {
        _ignore_group_check = true;
    }
}

void TreeCheckBoxPanel::mouseReleaseEvent(QMouseEvent* e)
{
    Q_UNUSED(e)
    _ignore_group_check = false;
}

int TreeCheckBoxPanel::width()
{
    return checkboxSize().width() + 10;
}

int TreeCheckBoxPanel::offset() const
{
    return _view->viewport()->geometry().top() + 1;
}

QList<QPair<QRect, QRect>> TreeCheckBoxPanel::checkboxRects() const
{
    int first_col = _view->horizontalHeader()->logicalIndexAt(0);
    int offset = this->offset();
    QSize check_size = checkboxSize();
    QList<QPair<QRect, QRect>> res;

    QModelIndex first_visual_index = _view->indexAt({first_col, 0});
    bool found = false;

    for (QModelIndex visual_index = first_visual_index; visual_index.isValid();) {
        QRect cell_rect = _view->visualRect(visual_index);

        if (cell_rect.isNull())
            break;

        QPair<QRect, QRect> rects;

        cell_rect.adjust(0, offset, 0, offset);
        rects.first = cell_rect;

        cell_rect.setLeft((width() - check_size.width()) / 2);
        cell_rect.setRight(cell_rect.left() + check_size.width());

        int cell_height = cell_rect.height();
        cell_rect.setTop(cell_rect.top() + (cell_height - check_size.height()) / 2);
        cell_rect.setBottom(cell_rect.top() + check_size.height());
        rects.second = cell_rect;

        if (_view->viewport()->geometry().top() > rects.first.bottom() || _view->viewport()->geometry().bottom() < rects.first.top()) {
            // возвращаем только видимые части
            if (found)
                break;

        } else {
            res << rects;
            found = true;
        }

        visual_index = Utils::getNextItemModelIndex(visual_index, true);
        if (visual_index.isValid())
            visual_index = visual_index.model()->index(visual_index.row(), first_col, visual_index.parent());
    }

    return res;
}

QPair<QRect, QRect> TreeCheckBoxPanel::headerCheckboxRect() const
{
    if (!_view->horizontalHeader()->isVisible())
        return {};

    QPair<QRect, QRect> res;

    QStyleOptionHeader opt_header_section;

    QRect rect = QRect(0, _view->horizontalHeader()->geometry().top(), width(), _view->horizontalHeader()->geometry().bottom());
    res.first = rect;

    QSize check_size = checkboxSize();
    rect.setLeft((width() - check_size.width()) / 2);
    rect.setRight(rect.left() + check_size.width());
    rect.setTop((width() - check_size.width()) / 2);

    int cell_height = rect.height();
    rect.setTop(rect.top() + (cell_height - check_size.height()) / 2);
    rect.setBottom(rect.top() + check_size.height());
    res.second = rect;

    return res;
}

QSize TreeCheckBoxPanel::checkboxSize()
{
    QStyleOptionButton check_option;
    return (qApp->style()->sizeFromContents(QStyle::CT_CheckBox, &check_option, QSize()).expandedTo(QApplication::globalStrut()));
}

QModelIndex TreeCheckBoxPanel::cursorIndex(const QPoint& c) const
{    
    auto rects = checkboxRects();
    int shift = 0;
    for (auto& rect : qAsConst(rects)) {
        if (rect.second.contains(c)) {
            return Utils::getTopSourceIndex(_view->indexAt({0, c.y() - offset()}));
        }

        shift++;
    }
    return QModelIndex();
}

} // namespace zf
