#pragma once

#include <QScrollBar>

class QAbstractSliderPrivate;

namespace zf
{
//! Скролбар для TableView
class TableViewScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    TableViewScrollBar(Qt::Orientation orientation, QWidget* parent);

    // доступ к протектед методам
    void hack_sliderChange();
    QAbstractSliderPrivate* privatePtr() const;
};

class TableView;

//! Панель для отображения чекбоксов
class CheckBoxPanel : public QWidget
{
    Q_OBJECT
public:
    CheckBoxPanel(TableView* parent);

    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

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
    //! Строка для положения курсора
    int cursorRow(const QPoint& c) const;

    TableView* _view;
    bool _ignore_group_check = false;
    bool _is_group_checked = false;
};
} // namespace zf
