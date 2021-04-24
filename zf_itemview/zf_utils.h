#pragma once

#include <QModelIndex>
#include <QColor>
#include <QDataStream>
#include <QLocale>

#include "zf_error.h"

class QMainWindow;
class QIODevice;
class QAbstractScrollArea;

namespace zf
{
class HeaderItem;
class HeaderView;

class Utils
{
public:
    //! Количество знаков после запятой для дробных чисел
    static const int DOUBLE_DECIMALS;

    //! Перевод контейнера (QList, QSet) в строку
    template <typename T>
    static QString containerToString(
        //! Массив Qt
        const T& c,
        //! Максимальное количество отображаемых элементов
        int max_count = -1)
    {
        int count = max_count > 0 ? qMin(max_count, c.count()) : c.count();
        QString text;
        int n = 0;
        for (auto i = c.constBegin(); i != c.constEnd(); ++i) {
            if (n >= count)
                break;
            n++;

            if (!text.isEmpty())
                text += QStringLiteral(", ");
            text += variantToString(QVariant::fromValue(*i));
        }
        if (count < c.count())
            text += QStringLiteral("...(%1)").arg(c.count());

        return text;
    }

    //! Цвет линий UI
    static QColor uiLineColor(bool bold);
    //! Цвет фона окон UI
    static QColor uiWindowColor();
    //! Цвет фона кнопок UI
    static QColor uiButtonColor();
    //! Темный цвет UI
    static QColor uiDarkColor();
    //! Альтернативный фон ячеек в таблицах (например при чередовании строк)
    static QColor uiAlternateTableBackgroundColor();
    //! Цвет выделения фона в таблицах
    static QColor uiInfoTableBackgroundColor();
    //! Цвет выделения текста в таблицах
    static QColor uiInfoTableTextColor();

    //! Возвращает самый верхий индекс по цепочке прокси
    static QModelIndex getTopSourceIndex(const QModelIndex& index);
    //! Поиск по модели следующего или предыдущего элемента. Возвращает всегда индекс в с колонкой 0
    static QModelIndex getNextItemModelIndex(const QModelIndex& index, bool forward);
    //! Возвращает самый верхюю itemModel по цепочке прокси
    static QAbstractItemModel* getTopSourceModel(const QAbstractItemModel* model);
    //! Преобразует уровень прокси индекса в уровень model. Если не может, то invalid
    static QModelIndex alignIndexToModel(const QModelIndex& index, const QAbstractItemModel* model);

    //! Получить все индексы нулевой колонки для модели
    static void getAllIndexes(const QAbstractItemModel* m, QModelIndexList& indexes);

    //! Найти окно приложения
    static QMainWindow* getMainWindow();
    //! Найти верхнее окно приложения (диалог или MainWindow)
    static QWidget* getTopWindow();

    //! Есть ли такой родитель у объекта
    static bool hasParent(const QObject* obj, const QObject* parent);

    //! Сохранить состояние заголовка
    static Error saveHeader(QIODevice* device, HeaderItem* root_item, int frozen_group_count,
                            int data_stream_version = QDataStream::Qt_5_6);
    //! Восстановить состояние заголовка
    static Error loadHeader(QIODevice* device, HeaderItem* root_item, int& frozen_group_count,
                            int data_stream_version = QDataStream::Qt_5_6);

    //! Отрисовка индикации куда будет вставлен перетаскиваемый заголовок
    static void paintHeaderDragHandle(QAbstractScrollArea* area, HeaderView* header);

    //! Преобразование QVariant в строку для отображения
    static QString variantToString(const QVariant& value, QLocale::Language language = QLocale::AnyLanguage,
                                   //! Максимальное количество выводимых элементов для списка
                                   int max_list_count = -1);

private:
    //! Получить все индексы нулевой колонки для модели
    static void getAllIndexesHelper(const QAbstractItemModel* m, const QModelIndex& parent, QModelIndexList& indexes);
    //! Преобразование QVariant в строку для отображения
    static QString variantToStringHelper(const QVariant& value, const QLocale* locale,
                                         //! Максимальное количество выводимых элементов для списка
                                         int max_list_count);
};

} // namespace zf
