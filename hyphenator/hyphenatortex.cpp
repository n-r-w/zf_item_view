#include "hyphenatortex.h"

#include "tex_patterns.h"

namespace Hyphenation
{
static const QChar _marker('.');

static bool isdigit_func(const QChar& c)
{
    return c >= '0' && c <= '9';
}
static bool ismarker_func(const QChar& c)
{
    return c == _marker;
}
static int char2digit_func(const QChar& c)
{
    return c.unicode() - QChar('0').unicode();
}

bool pattern_compare(const HyphenatorTeX::PatternItemTeX& a, const HyphenatorTeX::PatternItemTeX& b)
{
    bool first = a.str.size() < b.str.size();
    int min_size = first ? a.str.size() : b.str.size();
    for (int i = 0; i < min_size; ++i) {
        if (a.str.at(i) < b.str.at(i))
            return true;
        else if (a.str.at(i) > b.str.at(i))
            return false;
    }
    return first;
}

QStringList HyphenatorTeX::doHyphenate(const QString& s)
{
    loadPatternsTeX();
    QString word_string;
    word_string.append(_marker);
    word_string.append(s);
    word_string.append(_marker);

    QVector<int> levels(word_string.size(), 0);

    for (int i = 0; i < word_string.size() - 2; ++i) {
        PatternListTeX::Iterator pattern_iter = _patterns.begin();
        for (int count = 1; count <= word_string.size() - i; ++count) {
            PatternItemTeX pattern_from_word;
            pattern_from_word.str = word_string.mid(i, count);
            if (pattern_compare(pattern_from_word, *pattern_iter))
                continue;
            pattern_iter = std::lower_bound(pattern_iter, _patterns.end(), pattern_from_word, pattern_compare);
            if (pattern_iter == _patterns.end())
                break;
            if (!pattern_compare(pattern_from_word, *pattern_iter)) {
                for (int level_i = 0; level_i < pattern_iter->levels.size(); ++level_i) {
                    int l = pattern_iter->levels.at(level_i);
                    if (l > levels.at(i + level_i))
                        levels[i + level_i] = l;
                }
            }
        }
    }

    QStringList res;
    int pos = 0;
    for (int i = 0; i < levels.size() - 2; ++i) {
        if (levels.at(i + 1) % 2 && i) {
            // В этом месте у слова перенос
            res.append(s.mid(pos, i - pos));
            pos = i;
        }
    }
    res.append(s.mid(pos));
    return res;
}

void HyphenatorTeX::loadPatternsTeX()
{
    if (_patterns.count() > 0)
        return;
    //Загрузка русских паттернов
    int i = 0;
    while (patterns[i]) {
        PatternItemTeX pi;
        createPattern(pi, QString::fromUtf8(patterns[i]));
        _patterns.append(pi);
        ++i;
    }
    std::sort(_patterns.begin(), _patterns.end(), pattern_compare);
}

void HyphenatorTeX::createPattern(PatternItemTeX& pattern, const QString& str)
{
    pattern.clear();
    bool wait_digit = true;

    for (QString::ConstIterator c = str.begin(); c != str.end(); c++) {
        if (isdigit_func(*c)) {
            pattern.levels.append(char2digit_func(*c));
            wait_digit = false;
        } else {
            if (!ismarker_func(*c) && wait_digit) {
                pattern.levels.append(0);
            }
            pattern.str.append(*c);
            wait_digit = true;
        }
    }

    if (wait_digit) {
        pattern.levels.append(0);
    }
}

HyphenatorTeX::PatternItemTeX::PatternItemTeX()
{
}

void HyphenatorTeX::PatternItemTeX::clear()
{
    str.clear();
    levels.clear();
}
} // namespace Hyphenation
