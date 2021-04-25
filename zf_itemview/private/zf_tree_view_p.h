#pragma once

#include <QWidget>

namespace zf
{
class TreeView;

//! Панель для отображения чекбоксов
class TreeCheckBoxPanel : public QWidget
{
    Q_OBJECT
public:
    TreeCheckBoxPanel(TreeView* parent);

    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* event) override;

    //! Ширина панели
    static int width();

private:
    //! Смещение по вертикали
    int offset() const;
    //! Положения чекбоксов. Первый rect - размер ячейки, второй - чекбокса
    QList<QPair<QRect, QRect>> checkboxRects() const;
    //! Положение чекбокса в заголовке. Первый rect - размер ячейки, второй - чекбокса
    QPair<QRect, QRect> headerCheckboxRect() const;
    //! Размер одного чекбокса
    static QSize checkboxSize();
    //! Индекс для положения курсора
    QModelIndex cursorIndex(const QPoint& c) const;

    TreeView* _view;
    bool _ignore_group_check = false;
    bool _is_group_checked = false;
};

} // namespace zf
