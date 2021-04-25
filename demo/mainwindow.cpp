#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    _table_model.setColumnCount(COL_COUNT);
    _table_model.setRowCount(ROW_COUNT);
    for (int row = 0; row < ROW_COUNT; row++) {
        for (int col = 0; col < COL_COUNT; col++) {
            _table_model.setData(_table_model.index(row, col), QString("Row %1, Column %2").arg(row + 1).arg(col + 1), Qt::EditRole);
        }
    }

    _tree_model.setColumnCount(COL_COUNT);
    for (int row = 0; row < ROW_COUNT; row++) {
        QList<QStandardItem*> items;
        for (int col = 0; col < COL_COUNT; col++) {
            items << new QStandardItem(QString("Row %1, Column %2").arg(row + 1).arg(col + 1));
        }
        _tree_model.invisibleRootItem()->appendRow(items);

        for (int c_row = 0; c_row < 10; c_row++) {
            QList<QStandardItem*> c_items;
            for (int col = 0; col < COL_COUNT; col++) {
                c_items << new QStandardItem(QString("Child row %1, Column %2").arg(c_row + 1).arg(col + 1));
            }
            items.constFirst()->appendRow(c_items);

            for (int c_row1 = 0; c_row1 < 10; c_row1++) {
                QList<QStandardItem*> c_items1;
                for (int col = 0; col < COL_COUNT; col++) {
                    c_items1 << new QStandardItem(QString("Child row %1, Column %2").arg(c_row1 + 1).arg(col + 1));
                }
                c_items.constFirst()->appendRow(c_items1);
            }
        }
    }

    _table_view = new zf::TableView;
    _table_view->setAutoResizeRowsHeight(false);
    _table_view->setModel(&_table_model);
    configureHeader(_table_view->horizontalRootHeaderItem());

    _tree_view = new zf::TreeView;
    _tree_view->setModel(&_tree_model);
    configureHeader(_tree_view->rootHeaderItem());

    ui->main_la->addWidget(_table_view);
    ui->main_la->addWidget(_tree_view);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::configureHeader(zf::HeaderItem* parent)
{
    for (int col = 0; col < COL_COUNT; col++) {
        zf::HeaderItem* item = parent->append(QString("Header %1").arg(col + 1));
        item->append("Child header 1");
        item->append("Child header 2");
    }
}

void MainWindow::on_auto_height_clicked()
{
    _table_view->setAutoResizeRowsHeight(!_table_view->isAutoResizeRowsHeight());
}

void MainWindow::on_frozen_clicked()
{
    if (_table_view->frozenGroupCount() == 0)
        _table_view->setFrozenGroupCount(2);
    else
        _table_view->setFrozenGroupCount(0);
}

void MainWindow::on_check_panel_clicked()
{
    _table_view->showCheckRowPanel(!_table_view->isShowCheckRowPanel());
    _tree_view->showCheckRowPanel(!_tree_view->isShowCheckRowPanel());
}

void MainWindow::on_check_cell_clicked()
{
    if (_table_view->cellCheckColumns().count() == 0)
        _table_view->setCellCheckColumn(2, true, true);
    else
        _table_view->setCellCheckColumn(2, false, false);

    if (_tree_view->cellCheckColumns(0).count() == 0) {
        _tree_view->setCellCheckColumn(2, true, true, 0);
        _tree_view->setCellCheckColumn(2, true, true, 2);
    } else {
        _tree_view->setCellCheckColumn(2, false, false, 0);
        _tree_view->setCellCheckColumn(2, false, false, 2);
    }
}
