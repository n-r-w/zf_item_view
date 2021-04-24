#ifndef ZF_HTML_TOOLS_H
#define ZF_HTML_TOOLS_H

#include <memory>

#include <QString>
#include <QColor>
#include <QRecursiveMutex>

#include "zf_itemview.h"

namespace zf
{
struct HtmlNode;
class HtmlNodes;
class Error;

//! Разные операции с HTML
class ZF_ITEMVIEW_DLL_API HtmlTools
{
public:
    HtmlTools();

public:
    //! Преобразовать HTML в текст. Корректные HTML теги исключаются
    //! Некорректные теги рассматриваются как обычный текст
    static QString plain(const QString& s,
        //! Сохранять переносы строки
        bool keepNewLine = true);

    //! Преобразовать HTML в текст, если это корректный HTML. Корректные Html теги исключаются
    //! Некорректные теги рассматриваются как обычный текст
    static bool plainIfHtml(QString& s,
        //! Сохранять переносы строки
        bool keepNewLine = true);

    //! Является ли строка HTML
    static bool isHtml(const QString& s);

    //! Проверить HTML и заменить некорректные теги на текст (<qwerty> на "qwerty")
    static QString correct(const QString& s);
    //! Проверить HTML и заменить некорректные теги на текст (<qwerty> на "qwerty") если это HTML текст
    static bool correntIfHtml(QString& s);

    //! Парсинг
    static std::shared_ptr<HtmlNodes> parse(const QString& html);

public:
    /* Оформление использовать как вложения. Например:
          bold(color("xxx",Qt::red)) - жирная красная строка  */
    //! Оформить строку цветом HTML
    static QString color(const QString& s, const QColor& color);
    //! Оформить строку жирным HTML
    static QString bold(const QString& s);
    //! Оформить строку курсивом HTML
    static QString italic(const QString& s);
    //! Оформить строку подчеркиванием HTML
    static QString underline(const QString& s);
    //! Оформить строку выравниванием текста HTML
    static QString align(const QString& s, Qt::Alignment align);

    //! Оформить строку размером шрифта HTML
    static QString font(const QString& s, int size);

    //! Создать таблицу из одной колонки
    static QString table(const QStringList& rows);

private:
    static QString plain(
        //! Сохранять переносы строки
        bool keepNewLine = true);
    static bool isHtml();
    static QString correct();

    enum NodeType
    {
        Tag, //! html тэг. Если тэг некорректный, то он рассматривается как Data
        Data //! обычные данные
    };

    struct Node
    {
        NodeType type;
        // Содержимое
        QString text;
    };

    static QRecursiveMutex _mutex;

    static std::shared_ptr<QString> _parsedCorrect;
    static std::shared_ptr<QString> _parsedPlain;
    static std::shared_ptr<HtmlNodes> _parsed;

    static const QSet<QString> _supportedHtmlTags;
    static const QSet<QString> _escapedTags;
};

} // namespace zf

#endif // ZF_HTML_TOOLS_H
