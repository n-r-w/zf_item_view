#include "hyphenatorfast.h"

namespace Hyphenation
{
const QString HyphenatorFast::_x = QString::fromUtf8("йьъ");
const QString HyphenatorFast::_g = QString::fromUtf8("аеёиоуыэюяaeiouy");
const QString HyphenatorFast::_s = QString::fromUtf8("бвгджзклмнпрстфхцчшщbcdfghjklmnpqrstvwxz");

HyphenatorFast::HyphenatorFast()
{
    _rules.append(HyphenPairFast("xgg", 1));
    _rules.append(HyphenPairFast("xgs", 1));
    _rules.append(HyphenPairFast("xsg", 1));
    _rules.append(HyphenPairFast("xss", 1));
    _rules.append(HyphenPairFast("gssssg", 3));
    _rules.append(HyphenPairFast("gsssg", 3));
    _rules.append(HyphenPairFast("gsssg", 2));
    _rules.append(HyphenPairFast("sgsg", 2));
    _rules.append(HyphenPairFast("gssg", 2));
    _rules.append(HyphenPairFast("sggg", 2));
    _rules.append(HyphenPairFast("sggs", 2));
}

HyphenatorFast::~HyphenatorFast()
{
}

static const QChar _separator(0);

QStringList HyphenatorFast::doHyphenateInternal(const QString& s)
{
    QString hyphenatedText;
    QString text = s;

    for (QString::ConstIterator c = text.constBegin(); c != text.constEnd(); c++) {
        if (_x.indexOf(*c) != -1) {
            hyphenatedText.append('x');
        } else if (_g.indexOf(*c) != -1) {
            hyphenatedText.append('g');
        } else if (_s.indexOf(*c) != -1) {
            hyphenatedText.append('s');
        } else {
            hyphenatedText.append(*c);
        }
    }

    for (RulesFast::ConstIterator hp = _rules.constBegin(); hp != _rules.constEnd(); hp++) {
        int index = hyphenatedText.indexOf(hp->pattern);
        while (index != -1) {
            int actualIndex = index + hp->position;
            hyphenatedText = hyphenatedText.mid(0, actualIndex) + _separator + hyphenatedText.mid(actualIndex);
            text = text.mid(0, actualIndex) + _separator + text.mid(actualIndex);
            index = hyphenatedText.indexOf(hp->pattern);
        }
    }

    QStringList nodes;
    int index = text.indexOf(_separator);
    int i = 0;
    while (index >= 0) {
        nodes.append(text.mid(0, index));
        text = text.mid(index + 1);
        index = text.indexOf(_separator);
        i++;
    }
    nodes.append(text);
    return nodes;
}

QStringList HyphenatorFast::doHyphenate(const QString& s)
{
    // Проверка на наличие дефисов. Если они есть, то заранее разделяем по ним слово
    QStringList split = s.split('-', Qt::SkipEmptyParts, Qt::CaseInsensitive);

    QStringList nodes;
    for (int i = 0; i < split.count(); i++) {
        if (i != split.count() - 1) {
#if QT_VERSION >= 0x040500
            nodes.append(doHyphenateInternal(split.at(i) + '-'));
#else
            foreach (QString s, doHyphenateInternal(split.at(i) + '-')) {
                nodes.append(s);
            }
#endif
        } else {
#if QT_VERSION >= 0x040500
            nodes.append(doHyphenateInternal(split.at(i)));
#else
            foreach (QString s, doHyphenateInternal(split.at(i))) {
                nodes.append(s);
            }
#endif
        }
    }
    return nodes;
}

HyphenatorFast::HyphenPairFast::HyphenPairFast()
    : position(0)
{
}

HyphenatorFast::HyphenPairFast::HyphenPairFast(QString pattern, int position)
    : pattern(pattern)
    , position(position)
{
}

HyphenatorFast::HyphenPairFast::~HyphenPairFast()
{
}

} // namespace Hyphenation
