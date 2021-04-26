#include "texthyphenationformatter.h"

#include <QCryptographicHash>
#include <QDebug>

static QString generateHasgString(const QString& data)
{
    return QString(QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Sha3_224).toHex());
}

namespace Hyphenation
{
//! Развернуть строку
static QString rotateString(const QString& str)
{
    QString res = str;
    int i, count;
    int len = str.length() - 1;
    for (i = len, count = 0; i > count; i--, count++) {
        QChar a = str.at(i);
        res[i] = str[count];
        res[count] = a;
    }
    return res;
}

QStringList TextHyphenationFormatter::splitHelper(const QString& text, int width, const QFontMetrics& fontMetrics,
                                                  int average_char_width, TextHyphenationFormatter::WrapFlags wrapMode)
{
    QStringList splittedLines;
    int average = average_char_width > 0 ? average_char_width : fontMetrics.averageCharWidth();

    if (width < average) {
        // Вообще не влезает - разбиваем по одному символу
        for (int i = 0; i < text.count(); i++) {
            QString c = QString(text.at(i));
            if (c == QStringLiteral("\n"))
                c = QStringLiteral(" ");

            splittedLines << c;
        }

        return splittedLines;
    }

    QStringList splittedByN = text.split(QStringLiteral("\n"),
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                                         Qt::SkipEmptyParts
#else
                                         QString::SkipEmptyParts
#endif
    );
    if (splittedByN.count() > 1) {
        // В строке уже есть переносы, заданные в ручную
        for (auto& s : splittedByN) {
            QStringList splitted = split(s, width, fontMetrics, wrapMode);
            splittedLines += splitted;
        }
        return splittedLines;
    }

    QString restText = text;
    if (restText.length() < 2 || wrapMode & TextHyphenationFormatter::NoWrap) {
        // Вообще не переносить
        splittedLines.append(restText);
        return splittedLines;
    }

    // Вычисляем максимальную длину строки текста без переноса
    int maxWidth = width / average;
    if (maxWidth < 2) {
        // Слишком маленькая ширина даже для одного символа
        width = fontMetrics.maxWidth() * 2;
        maxWidth = width / average;
    }

    // Небольшая оптимизация. Выносим определение всех переменных из цикла,
    // чтобы избежать постоянных вызовов конструктора/деструктора
    QString rotatedLeft;
    QString word;
    QString leftText;
    QString line;
    QString tempRest;
    QString leftPart, rightPart;
    QStringList hyphenation;

    while (!restText.isEmpty()) {
        // Т.к. width/fontMetrics.averageCharWidth() дает лишь примерную длину максимальной строки текста в линии,
        // то находим какая еще часть неразбитого по строкам текста влезает в линию
        restText = restText.trimmed();
        int tempMaxWidth = qMin(maxWidth, restText.length());
        int realPixelWidth = fontMetrics.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                             horizontalAdvance
#else
                             width
#endif
                             (restText, tempMaxWidth);
        while (realPixelWidth > width) {
            tempMaxWidth--;
            if (restText.at(tempMaxWidth - 1) == ' ') {
                // Исключаем пробелы между словами
                tempMaxWidth--;
            }
            realPixelWidth = fontMetrics.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                             horizontalAdvance
#else
                             width
#endif
                             (restText, tempMaxWidth);
            if (tempMaxWidth <= 1) {
                tempMaxWidth = 1;
                break;
            }
        }
        if (tempMaxWidth >= restText.length()) {
            // Последняя часть текста влезает полностью
            splittedLines.append(restText);
            return splittedLines;
        }
        // Получаем новую максимальную строку текста без переносов и с потенциально обрезанными словами
        line = restText.left(tempMaxWidth);

        if (wrapMode & TextHyphenationFormatter::WrapAnywhere) {
            // Переносить по достижению максимальной ширины
            splittedLines.append(line);
            restText.remove(0, tempMaxWidth);
            continue;
        }
        // Проверяем не разрезали ли мы строку по слову (т.е. если нет пробелов ни с одной из сторон разреза)
        tempRest = restText.right(restText.length() - tempMaxWidth);

        if (!tempRest.isEmpty() && (line.at(line.length() - 1) != ' ') && (tempRest.at(0) != ' ')) {
            // Разрез по слову. Получаем его
            // Левая часть. Ищем первый пробел справа
            rotatedLeft = rotateString(line);
            int leftPos = rotatedLeft.indexOf(' ');
            leftPos = (leftPos == -1) ? 0 : line.length() - leftPos;
            // Правая часть. Ищем первый пробел слева
            int rightPos = tempRest.indexOf(' ');
            rightPos = (rightPos == -1) ? tempRest.length() : rightPos + 1;
            // Получаем слово
            leftPart = line.right(line.length() - leftPos).trimmed();
            rightPart = tempRest.left(rightPos).trimmed();
            word = leftPart + rightPart;

            TextHyphenationFormatter::WrapFlags tempWrapMode = wrapMode;
            if (!rightPart.isEmpty() && (rightPart.at(0) == '.' || rightPart.at(0) == '-')) {
                // Если правая часть слова начинается на точку или дефис, то переносить его не надо
                tempWrapMode |= TextHyphenationFormatter::WordWrap;
            }

            if (!(wrapMode & TextHyphenationFormatter::WrapAbbreviation) && isAbbreviation(word)) {
                // Это аббревиатура - переносим целиком
                tempWrapMode |= TextHyphenationFormatter::WordWrap;
            }

            if (tempWrapMode & TextHyphenationFormatter::WordWrap) {
                // Переносить по словам
                if (leftPos == 0) {
                    // Допустимая ширина не позволяет влезть слову целиком - режем слово
                    splittedLines.append(line.trimmed());
                    restText.remove(0, tempMaxWidth);
                    continue;
                }
                splittedLines.append(line.left(leftPos - 1));
                restText.remove(0, leftPos);
                continue;
            }

            // Разбиваем на слоги
            hyphenation = _hyphenator->doHyphenate(word);
            // Получаем строку слева, за исключением невлезающего слова
            leftText = line.mid(0, leftPos);
            // Ищем какой слог влезает в остатки линии
            bool isCanSplit = false;
            bool isSplitChar = false;
            for (int i = 0; i < hyphenation.count(); i++) {
                QString tempPart;
                bool tempSplitChar = false;
                // Проверяем нет ли дефиса на конце слога
                if (hyphenation.at(i).at(hyphenation.at(i).count() - 1) == '-') {
                    tempPart = hyphenation.at(i);
                    tempSplitChar = true;
                } else {
                    tempPart = hyphenation.at(i) + '-';
                }
                // Замеряем влезает ли
                if (fontMetrics.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                    horizontalAdvance
#else
                    width
#endif
                    (leftText + tempPart)
                    > width) {
                    break;
                }
                leftText += hyphenation.at(i);
                isSplitChar = tempSplitChar;
                isCanSplit = true;
            }
            if (isCanSplit) {
                // Влезло на переносе
                if (isSplitChar || restText.length() == leftText.length()) {
                    // В конце слога уже есть дефис - добавлять еще не надо
                    splittedLines.append(leftText);
                } else {
                    splittedLines.append(leftText + '-');
                }
                restText.remove(0, leftText.length());
                continue;
            } else {
                // Разбиение на слоги не помогает. Разрезанное слово не влезает даже по слогам - переносим полностью
                if (leftPos == 0
                    || fontMetrics.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                           horizontalAdvance
#else
                           width
#endif
                           (word)
                           > width) {
                    // Допустимая ширина не позволяет влезть слову целиком - режем слово
                    splittedLines.append(line.left(line.length()) + '-');
                    restText.remove(0, tempMaxWidth);
                    continue;
                }

                if (leftPos > 1)
                    splittedLines.append(line.left(leftPos - 1).trimmed());
                restText.remove(0, leftPos);
                continue;
            }
        } else {
            // Слово не разрезано, переносим как есть
            splittedLines.append(line.trimmed());
            restText.remove(0, tempMaxWidth);
            continue;
        }
    }
    return splittedLines;
}

TextHyphenationFormatter::TextHyphenationFormatter(Hyphenator* hyphenator)
    : _hyphenator(hyphenator)
{
    _splitCache.setMaxCost(1000);
}

const QStringList TextHyphenationFormatter::split(const QString& text, int width, const QFontMetrics& fontMetrics,
                                                  int average_char_width, TextHyphenationFormatter::WrapFlags wrapMode)
{
    if (!_hyphenator) {
        qDebug("TextHyphenationFormatter::split - hyphenator not defined");
        return QStringList();
    }

    if (text.simplified().isEmpty()) {
        return QStringList();
    }

    QString key = generateHasgString(text);
    SplitInfo* si = _splitCache.object(key);
    if (si && si->width == width)
        return si->split;

    si = new SplitInfo;
    si->width = width;
    si->split = splitHelper(text, width, fontMetrics, average_char_width, wrapMode);
    Q_ASSERT(_splitCache.insert(key, si));

    return si->split;
}

void TextHyphenationFormatter::draw(QPainter& p, const QPoint& pos, const QString& text, int width, int leading,
        WrapFlags wrapMode, Qt::Alignment alignment, QRect* boundingRect)
{
    if (text.isEmpty())
        return;
    draw(p, pos, split(text, width, p.fontMetrics(), wrapMode), leading, alignment, boundingRect);
}

void TextHyphenationFormatter::draw(QPainter& p, const QRect& rect, const QString& text, int leading,
        WrapFlags wrapMode, Qt::Alignment alignment, QRect* boundingRect)
{
    if (text.isEmpty())
        return;
    draw(p, rect, split(text, rect.width() - p.fontMetrics().averageCharWidth(), p.fontMetrics(), wrapMode), leading,
         alignment, boundingRect);
}

void TextHyphenationFormatter::draw(QPainter& p, const QPoint& pos, const QStringList& text, int leading,
        Qt::Alignment alignment, QRect* boundingRect)
{
    p.save();

    QFont font = p.font();
    font.setStyleStrategy(QFont::PreferAntialias);
    p.setFont(font);

    int yPos = pos.y();
    int xPos = pos.x();

    for (int i = 0; i < text.count(); i++) {
        int textWidth = p.fontMetrics().
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                        horizontalAdvance
#else
                        width
#endif
                        (text.at(i));
        if (alignment & Qt::AlignRight) {
            xPos = pos.x() - textWidth;
        } else if (alignment & Qt::AlignCenter) {
            xPos = pos.x() - textWidth / 2;
        }
        p.drawText(xPos, yPos, text.at(i));
        if (boundingRect) {
            QRect boundingRectTmp;
            boundingRectTmp.setLeft(xPos);
            boundingRectTmp.setTop(yPos);
            boundingRectTmp.setHeight(p.fontMetrics().height() + leading);
            boundingRectTmp.setWidth(textWidth);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            *boundingRect = boundingRect->unite(boundingRectTmp);
#else
            *boundingRect = boundingRect->united(boundingRectTmp);
#endif
        }
        yPos += p.fontMetrics().height() + leading;
    }
    p.restore();
}

void TextHyphenationFormatter::draw(QPainter& p, const QRect& rect, const QStringList& text, int leading,
        Qt::Alignment alignment, QRect* boundingRect)
{
    if (text.isEmpty()) {
        return;
    }
    p.save();

    QFont font = p.font();
    font.setStyleStrategy(QFont::PreferAntialias);
    p.setFont(font);

    int xPos;
    int yPos;
    int height = p.fontMetrics().ascent() + p.fontMetrics().descent() + leading;

    if (alignment & Qt::AlignBottom) {
        yPos = rect.y() + rect.height() - height * text.count();
    } else if (alignment & Qt::AlignTop) {
        yPos = rect.y();
    } else {
        yPos = rect.y() + (rect.height() - height * text.count()) / 2;
    }

    for (int i = 0; i < text.count(); i++) {
        int width = p.fontMetrics().
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                    horizontalAdvance
#else
                    width
#endif
                    (text.at(i));
        if (alignment & Qt::AlignLeft) {
            xPos = rect.x();
        } else if (alignment & Qt::AlignRight) {
            xPos = rect.x() + rect.width() - width;
        } else {
            xPos = rect.x() + (rect.width() - width) / 2;
        }
        QRect boundingRectTmp;
        p.drawText(xPos, yPos, width, height, Qt::AlignTop | Qt::AlignLeft | Qt::TextDontClip | Qt::TextSingleLine,
                text.at(i), &boundingRectTmp);
        if (boundingRect) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            *boundingRect = boundingRect->unite(boundingRectTmp);
#else
            *boundingRect = boundingRect->united(boundingRectTmp);
#endif
        }
        yPos += height;
    }

    p.restore();
}

QRect TextHyphenationFormatter::boundingRect(const QString& text, int width, const QFontMetrics& fontMetrics,
        int leading, TextHyphenationFormatter::WrapFlags wrapMode)
{
    return boundingRect(split(text, width, fontMetrics, wrapMode), fontMetrics, leading);
}

QRect TextHyphenationFormatter::boundingRect(
        const QStringList& text, const QFontMetrics& fontMetrics, int leading) const
{
    if (!text.count())
        return QRect();

    QRect res;
    QRect r;
    for (int i = 0; i < text.count(); i++) {
        r = fontMetrics.boundingRect(text.at(i));
        if (r.width() > res.width())
            res.setWidth(r.width());
    }

    int height = fontMetrics.ascent() + fontMetrics.descent() + leading;
    res.setBottom(height * (text.count() - 1) + r.height());
    return res;
}

TextHyphenationFormatter* GlobalTextHyphenationFormatter::_textHyphenationFormatter = nullptr;
GlobalTextHyphenationFormatter::Initializer GlobalTextHyphenationFormatter::_initializer;

GlobalTextHyphenationFormatter::Initializer::Initializer()
{
}

GlobalTextHyphenationFormatter::Initializer::~Initializer()
{
    //    if (GlobalTextHyphenationFormatter::_textHyphenationFormatter)
    //        delete GlobalTextHyphenationFormatter::_textHyphenationFormatter;
}

TextHyphenationFormatter* GlobalTextHyphenationFormatter::formatter()
{
    if (!_textHyphenationFormatter)
        _textHyphenationFormatter = new TextHyphenationFormatter(GlobalHyphenationHash::hash());

    return _textHyphenationFormatter;
}

QString GlobalTextHyphenationFormatter::stringToMultiline(const QFontMetrics& fm, const QString& value, int width)
{
    width = qMax(fm.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                     horizontalAdvance
#else
                     width
#endif
                     ('W')
                     * 4,
                 width);
    QString v = value;
    v.replace('\n', ' ');

    using namespace Hyphenation;
    QStringList sl = formatter()->split(v, width, fm, fm.averageCharWidth(), TextHyphenationFormatter::HyphenateWrap);
    return sl.join('\n');
}
} // namespace Hyphenation
