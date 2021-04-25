#pragma once

#include <QMainWindow>
#include <QStandardItemModel>

#include <zf_table_view.h>
#include <zf_tree_view.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_auto_height_clicked();

    void on_frozen_clicked();

    void on_check_panel_clicked();

    void on_check_cell_clicked();

    void on_add_row_clicked();

    void on_remove_row_clicked();

private:
    void configureHeader(zf::HeaderItem* parent);

    void addShrinkRow(int count);
    void removeShrinkRow();

    Ui::MainWindow* ui;

    QStandardItemModel _table_model;
    zf::TableView* _table_view = nullptr;

    QStandardItemModel _tree_model;
    zf::TreeView* _tree_view = nullptr;

    const int COL_COUNT = 10;
    const int ROW_COUNT = 100;

    QStandardItemModel _shrink_table_model;
    zf::TableView* _shrink_table_view = nullptr;
};
