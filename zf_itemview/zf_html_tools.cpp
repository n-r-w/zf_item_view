#include "zf_html_tools.h"
#include <QSet>
#include "zf_utils.h"
#include "zf_error.h"

namespace zf
{
QRecursiveMutex HtmlTools::_mutex;
std::shared_ptr<QString> HtmlTools::_parsedCorrect;
std::shared_ptr<QString> HtmlTools::_parsedPlain;
std::shared_ptr<HtmlNodes> HtmlTools::_parsed;

struct HtmlNode
{
    enum Type
    {
        Tag, //! Корректный html тэг
        BadTag, //! Некорректный html тэг
        Escaped, //! Необходимо экранировать при выводе в html
        Data //! обычные данные
    };

    HtmlNode(Type type, const QString& text);
    //! Тип
    Type type;
    //! Содержимое
    QString text;
};

class HtmlNodes
{
    friend class HtmlTools;

public:
    HtmlNodes()
        : _isCorrect(true)
        , _hasHtmlNodes(false)
    {
    }
    ~HtmlNodes() {}

    //! Преобразовать в строку исключая все корректные html тэги
    QString toPlain() const;

    const QList<std::shared_ptr<HtmlNode>>& nodes() const { return _nodes; }

    //! Содержит хотя бы один некорректный тэг, то FALSE
    bool isCorrect() const { return _isCorrect; }
    //! Содержит хотя бы один корректный HTML тег
    bool isHasHtmlNodes() const { return _hasHtmlNodes; }

private:
    QList<std::shared_ptr<HtmlNode>> _nodes;
    bool _isCorrect;
    bool _hasHtmlNodes;
};

HtmlTools::HtmlTools()
{
}

// Известные html теги
const QSet<QString> HtmlTools::_supportedHtmlTags
        = {"!doctype", "style", "ag", "a", "address", "b", "big", "blockquote", "body", "br", "center", "cite", "code",
                "dd", "dfn", "div", "dl", "dt", "em", "font", "h1", "h2", "h3", "h4", "h5", "h6", "head", "hr", "html",
                "i", "img", "kbd", "meta", "li", "nobr", "ol", "p", "pre", "qt", "s", "samp", "small", "span", "strong",
                "sub", "sup", "table", "tbody", "td", "tfoot", "th", "thead", "title", "tr", "tt", "u", "ul", "var"};

// Известные теги, которые надо экранировать
const QSet<QString> HtmlTools::_escapedTags = {"zuid"};

// Заменяем '<' и '>' на ковычки, чтобы не было проблем при установке текста в html
#define Z_HTML_REPLACE_OPEN "\""
#define Z_HTML_REPLACE_CLOSE "\""

QString HtmlTools::plain(const QString& s, bool keepNewLine)
{
    QMutexLocker lock(&_mutex);

    QString sTmp = s;
    sTmp.replace(QStringLiteral("<b>'"), QStringLiteral("<b>"), Qt::CaseInsensitive)
            .replace(QStringLiteral("'</b>"), QStringLiteral("</b>"), Qt::CaseInsensitive)
            .replace(QStringLiteral("<b>"), QStringLiteral("<b>'"), Qt::CaseInsensitive)
            .replace(QStringLiteral("</b>"), QStringLiteral("'</b>"), Qt::CaseInsensitive);
    parse(sTmp);
    return plain(keepNewLine);
}

bool HtmlTools::plainIfHtml(QString& s, bool keepNewLine)
{
    QMutexLocker lock(&_mutex);

    if (isHtml(s)) {
        s = HtmlTools::plain(keepNewLine);
        return true;
    }

    return false;
}

QString HtmlTools::plain(bool keepNewLine)
{        
    /* Корректные html теги исключаются
     * Некорректные теги остаются без изменений */

    Q_ASSERT(_parsed != nullptr);
    if (!_parsedPlain) {
        _parsedPlain = std::make_shared<QString>();
        for (const std::shared_ptr<HtmlNode>& n : _parsed->nodes()) {
            if (n->type == HtmlNode::Tag) {
                if (n->text == QStringLiteral("<br>") || n->text == QStringLiteral("<hr>")) {
                    if (keepNewLine)
                        *_parsedPlain += QStringLiteral("\n");
                    else
                        *_parsedPlain += QStringLiteral(",");
                }

            } else if (n->type == HtmlNode::Escaped) {
                *_parsedPlain += n->text;

            } else if (n->type == HtmlNode::BadTag) {
                *_parsedPlain += n->text;

            } else
                *_parsedPlain += n->text;
        }
    }
    return *_parsedPlain;
}

bool HtmlTools::isHtml(const QString& s)
{
    QMutexLocker lock(&_mutex);

    parse(s);
    return isHtml();
}

bool HtmlTools::isHtml()
{
    Q_ASSERT(_parsed != nullptr);
    return _parsed->isCorrect() && _parsed->isHasHtmlNodes();
}

QString HtmlTools::correct(const QString& s)
{
    QMutexLocker lock(&_mutex);

    parse(s);
    return correct();
}

bool HtmlTools::correntIfHtml(QString& s)
{
    QMutexLocker lock(&_mutex);

    if (isHtml(s)) {
        s = correct(s);
        return true;
    }

    return false;
}

QString HtmlTools::correct()
{
    /* Корректные html теги остаются
     * Некорректные теги исключаются */

    Q_ASSERT(_parsed != nullptr);
    _parsedCorrect = std::make_shared<QString>();
    for (const std::shared_ptr<HtmlNode>& n : _parsed->nodes()) {
        QString s;

        if (n->type == HtmlNode::Escaped) {
            s = n->text.toHtmlEscaped();

        } else if (n->type == HtmlNode::BadTag) {
            s = Z_HTML_REPLACE_OPEN + n->text.mid(1, n->text.length() - 2) + Z_HTML_REPLACE_CLOSE;

        } else
            s = n->text;

        *_parsedCorrect += s;
    }

    return *_parsedCorrect;
}

QString HtmlTools::color(const QString& s, const QColor& color)
{
    return QStringLiteral("<font color=\"%2\">%1</font>").arg(s, color.name());
}

QString HtmlTools::bold(const QString& s)
{
    return QStringLiteral("<b>%1</b>").arg(s);
}

QString HtmlTools::italic(const QString& s)
{
    return QStringLiteral("<i>%1</i>").arg(s);
}

QString HtmlTools::font(const QString& s, int size)
{
    return QStringLiteral("<span style=\" font-size:%2pt;\">%1</span>").arg(s).arg(size);
}

QString HtmlTools::table(const QStringList& rows)
{
    if (rows.isEmpty())
        return QString();

    if (rows.count() == 1)
        return rows.at(0);

    QString res;
    for (QString row : qAsConst(rows)) {
        if (!res.isEmpty())
            res += QStringLiteral("<hr>");

        if (row.startsWith("<?xml version", Qt::CaseInsensitive))
            row = row.toHtmlEscaped();
        else
            HtmlTools::plainIfHtml(row);
        res += QStringLiteral("<tr><td>%1</td></tr>").arg(row);
    }

    return QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">") + res
            + QStringLiteral("</table>");
}

std::shared_ptr<HtmlNodes> HtmlTools::parse(const QString& html)
{
    QMutexLocker lock(&_mutex);

    _parsedCorrect.reset();
    _parsedPlain.reset();
    _parsed = std::make_shared<HtmlNodes>();

    QString text;
    bool tagFound = false;

    if ( // Это результат QTextEdit::toHtml для пустого содержимого
            html != QStringLiteral("p, li { white-space: pre-wrap; }")) {
        for (int i = 0; i < html.length(); i++) {
            if (html[i] == '<') {
                if (tagFound) {
                    // Подряд две открывающие - это не html
                    text = QStringLiteral("<") + text + html[i];
                    _parsed->_isCorrect = false;
                    tagFound = false;

                } else {
                    // Начало тега. Все что было ранее рассматриваем как текст
                    if (!text.isEmpty()) {
                        auto node = std::make_shared<HtmlNode>(HtmlNode::Data, text);
                        _parsed->_nodes << node;

                        text.clear();
                    }

                    tagFound = true;
                }

            } else if (html[i] == '>') {
                if (!tagFound) {
                    // закрывающая без открывающего - это не html
                    text += html[i];
                    _parsed->_isCorrect = false;

                } else {
                    // Обнаружен какой-то тег
                    QString tag = text;
                    if (!tag.isEmpty()) {
                        if (tag.at(0) == '/')
                            tag = tag.right(tag.length() - 1);

                        int spacePos = tag.indexOf(' ', 0);
                        if (spacePos >= 0)
                            tag = tag.left(spacePos);
                    }
                    QString tag_prepared = tag.toLower().trimmed();

                    if (!tag.isEmpty() && _supportedHtmlTags.contains(tag_prepared)) {
                        // Корректный html тег
                        auto node = std::make_shared<HtmlNode>(HtmlNode::Tag, "<" + text + ">");
                        _parsed->_nodes << node;
                        _parsed->_hasHtmlNodes = true;

                    } else if (!tag.isEmpty() && _escapedTags.contains(tag_prepared)) {
                        // Надо экранировать содержимое
                        auto node = std::make_shared<HtmlNode>(HtmlNode::Escaped, "<" + text + ">");
                        _parsed->_nodes << node;

                    } else {
                        // Некорректный html тег
                        auto node = std::make_shared<HtmlNode>(HtmlNode::BadTag, "<" + text + ">");
                        _parsed->_nodes << node;
                        _parsed->_isCorrect = false;
                    }

                    text.clear();
                    tagFound = false;
                }
            } else
                text += html[i];
        }
    }

    if (tagFound)
        // Была открывающая скобка, но закрывающей не было
        _parsed->_isCorrect = false;

    if (!text.isEmpty()) {
        auto node = std::make_shared<HtmlNode>(HtmlNode::Data, text);
        _parsed->_nodes << node;
    }

    return _parsed;
}

QString HtmlTools::underline(const QString& s)
{
    return QStringLiteral("<span style=\" text-decoration: underline;\">%1</span>").arg(s);
}

QString HtmlTools::align(const QString& s, Qt::Alignment align)
{
    QString a;
    if (align == Qt::AlignLeft)
        a = "left";
    else if (align == Qt::AlignRight)
        a = "right";
    else if (align == Qt::AlignCenter)
        a = "center";
    else
        Q_ASSERT(false);

    return QStringLiteral("<p align=\"%1\">%2</p>").arg(a, s);
}

HtmlNode::HtmlNode(HtmlNode::Type type, const QString& text)
    : type(type)
    , text(text)
{
    if (this->type == Data)
        Q_ASSERT(!this->text.isEmpty());
}

} // namespace zf
