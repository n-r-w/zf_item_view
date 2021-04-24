#pragma once

#include "hyphenator.h"

namespace Hyphenation
{
//! Алгоритм П.Христова в модификации Дымченко и Варсанофьева
//! http://shy.dklab.ru/new/js/Autohyphen.htc
//! https://sites.google.com/site/foliantapp/project-updates/hyphenation
//! Работает в несколько раз быстрее алгоритма TeX, но может иногда разбивать странно (пока не замечено)
//! Работает с английским и русским языком
class HYPHENATOR_DLL_API HyphenatorFast : public Hyphenator
{
    //! Служебный класс для HyphenatorFast
    struct HyphenPairFast
    {
        HyphenPairFast();
        HyphenPairFast(QString pattern, int position);
        ~HyphenPairFast();
        QString pattern;
        int position;
    };
    typedef QVector<HyphenPairFast> RulesFast;

public:
    HyphenatorFast();
    ~HyphenatorFast();
    //! Реализация конкретного алгоритма разбития
    QStringList doHyphenate(const QString& s);

private:
    //! Правила
    RulesFast _rules;
    //! Сначала doHyphenate делит слово по дефисам, а затем вызывает этот алгоритм разбиения на слоги
    QStringList doHyphenateInternal(const QString& s);

    static const QString _x;
    static const QString _g;
    static const QString _s;
};
}
