#pragma once

#include "hyphenator.h"

namespace Hyphenation
{
//! Алгоритм Ляна-Кнута (редактор TeX)
//! http://habrahabr.ru/post/138088/
//! Работает не быстро, но при наличии корректных правил - точно
//! Реализована поддержка только русского языка
class HYPHENATOR_DLL_API HyphenatorTeX : public Hyphenator
{
    //! Шаблон TeX (cлужебный класс для Hyphenator)
    struct PatternItemTeX
    {
        PatternItemTeX()
            : str("")
        {
        }
        void clear()
        {
            str = "";
            levels.clear();
        }

        QString str;
        QVector<int> levels;
    };
    typedef QVector<PatternItemTeX> PatternListTeX;

    friend bool pattern_compare(const PatternItemTeX& a, const PatternItemTeX& b);

public:
    HyphenatorTeX() {}
    ~HyphenatorTeX() {}
    //! Реализация конкретного алгоритма разбития
    QStringList doHyphenate(const QString& s);

private:
    //! Загрузить шаблоны
    void loadPatternsTeX();
    //! Инициализировать шаблон
    void createPattern(PatternItemTeX& pattern, const QString& str);
    //! Список шаблонов
    PatternListTeX _patterns;
};
}
