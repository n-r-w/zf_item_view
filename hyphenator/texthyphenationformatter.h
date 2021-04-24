#ifndef TEXTHYPHENATIONFORMATTER_H
#define TEXTHYPHENATIONFORMATTER_H

#include "hyphenationhash.h"

#include <QCache>
#include <QFontMetrics>
#include <QPainter>

namespace Hyphenation
{
//! Разбиение строки текста на линии по правилам переноса русского и английского языка
class HYPHENATOR_DLL_API TextHyphenationFormatter
{
public:
    TextHyphenationFormatter(Hyphenator* hyphenator);

    //! Режим переноса
    enum WrapMode
    {
        NoWrap = 0x0, //! Не переносить. Текст будет в одну линию
        HyphenateWrap = 0x1, //! Переносить по слогам
        WordWrap = 0x2, //! Переносить по словам
        WrapAnywhere = 0x4, //! Переносить по по достижению максимальной ширины
        WrapAbbreviation = 0x8 //! Переносить по слогам аббревиатуры
    };

    //! Набор флагов переноса текста
    Q_DECLARE_FLAGS(WrapFlags, WrapMode)

    //! Разбить на строки
    const QStringList split(const QString& text, int width, const QFontMetrics& fontMetrics, int average_char_width = 0,
                            TextHyphenationFormatter::WrapFlags wrapMode = TextHyphenationFormatter::HyphenateWrap);
    //! Разбить на строки и отрисовать текст в QPainter (вывод в точку QPoint)
    void draw(QPainter& p, const QPoint& pos, const QString& text,
            int width, //! Максимальная ширина текста без переносов
            int leading = 0, //! Расстояние между строками. 0 - значение для шрифта по умолчанию
            TextHyphenationFormatter::WrapFlags wrapMode = TextHyphenationFormatter::HyphenateWrap, //! Правила переноса
            Qt::Alignment alignment = Qt::AlignLeft, //! Qt::AlignLeft, Qt::AlignCenter, Qt::AlignRight
            QRect* boundingRect = nullptr);
    //! Разбить на строки и отрисовать текст в QPainter (вывод в QRect)
    void draw(QPainter& p, const QRect& rect, const QString& text,
            int leading = 0, //! Расстояние между строками. 0 - значение для шрифта по умолчанию
            TextHyphenationFormatter::WrapFlags wrapMode = TextHyphenationFormatter::HyphenateWrap, //! Правила переноса
            Qt::Alignment alignment = Qt::AlignCenter, QRect* boundingRect = nullptr);

    //! Отрисовать заранее разбитый текст в QPainter (вывод в точку QPoint)
    void draw(QPainter& p, const QPoint& pos, const QStringList& text, int leading = 0,
            Qt::Alignment alignment = Qt::AlignLeft, QRect* boundingRect = nullptr);
    //! Отрисовать заранее разбитый текст в QPainter (вывод в точку QRect)
    void draw(QPainter& p, const QRect& rect, const QStringList& text, int leading = 0,
            Qt::Alignment alignment = Qt::AlignCenter, QRect* boundingRect = nullptr);
    //! Возвращает размер строки текста
    QRect boundingRect(const QString& text,
            int width, //! Максимальная ширина текста без переносов
            const QFontMetrics& fontMetrics,
            int leading = 0, //! Расстояние между строками. 0 - значение для шрифта по умолчанию
            TextHyphenationFormatter::WrapFlags wrapMode = TextHyphenationFormatter::HyphenateWrap);
    //! Возвращает размер разбитой строки текста
    QRect boundingRect(const QStringList& text, const QFontMetrics& fontMetrics,
            int leading = 0 //! Расстояние между строками. 0 - значение для шрифта по умолчанию
            ) const;

private:
    QStringList splitHelper(const QString& text, int width, const QFontMetrics& fontMetrics, int average_char_width,
                            TextHyphenationFormatter::WrapFlags wrapMode);

    Hyphenator* _hyphenator;

    // Кэширование разбитых строк на основании ширины текста
    struct SplitInfo
    {
        int width;
        QStringList split;
    };
    QCache<QString, SplitInfo> _splitCache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextHyphenationFormatter::WrapFlags)

//! Статический класс с глобальным объектом TextHyphenationFormatter
class HYPHENATOR_DLL_API GlobalTextHyphenationFormatter
{
    //! Внутренний класс-инициализатор статических переменных в GlobalTextHyphenationFormatter
    class Initializer
    {
    public:
        Initializer();
        ~Initializer();
    };

    friend class Initializer;

public:
    static TextHyphenationFormatter* formatter();
    static QString stringToMultiline(const QFontMetrics& fm, const QString& value, int width);

private:
    static TextHyphenationFormatter* _textHyphenationFormatter;
    static Initializer _initializer;
};
} // namespace Hyphenation

#endif // TEXTHYPHENATIONFORMATTER_H
