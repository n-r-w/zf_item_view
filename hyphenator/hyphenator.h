/**
@mainpage Разбиение слов и предложений на слоги, в соответствие с морфологией русского и английского языка
Все классы библиотеки Hyphenator находятся в пространстве имен Hyphenation

Roman Nikulenkov, https://github.com/n-r-w
*/

#pragma once

#include "hyphenator_dll.h"

#include <QByteArray>
#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Hyphenation
{
class HyphenatedWord;

//! Базовый абстрактный класс для слов и предложений, которые будут разбиваться на слоги
class HYPHENATOR_DLL_API HyphenatedItem
{
    friend HYPHENATOR_DLL_API QDataStream& operator<<(QDataStream& stream, const HyphenatedWord& data);
    friend HYPHENATOR_DLL_API QDataStream& operator>>(QDataStream& stream, HyphenatedWord& data);

public:
    HyphenatedItem();
    HyphenatedItem(const QString& text);
    virtual ~HyphenatedItem();
    //! Исходный текст
    QString text() const;
    //! Текст разбит на слоги
    virtual bool isHyphenated() const = 0;
    //! Нет данных
    bool isEmpty() const;

    virtual HyphenatedItem& operator=(const HyphenatedItem& item);

private:
    QString _text;
};

//! Является ли слово аббревиатурой (т.е. разбивать на слоги его не надо)
bool HYPHENATOR_DLL_API isAbbreviation(const QString& s);

//! Слово на русском языке, которое будет разбито на слоги
//! Для разбиения необходимо вызвать Hyphenator.hyphenate
class HYPHENATOR_DLL_API HyphenatedWord : public HyphenatedItem
{
    friend class Hyphenator;
    friend HYPHENATOR_DLL_API QDataStream& operator<<(QDataStream& stream, const HyphenatedWord& data);
    friend HYPHENATOR_DLL_API QDataStream& operator>>(QDataStream& stream, HyphenatedWord& data);

public:
    HyphenatedWord();
    HyphenatedWord(const QString& text);
    ~HyphenatedWord();
    //! Слово разбито на слоги
    bool isHyphenated() const;
    //! Разбивка по слогам
    QStringList splitInfo() const;
    //! Является ли слово аббревиатурой (т.е. разбивать на слоги его не надо)
    bool isAbbreviation() const;

    HyphenatedWord& operator=(const HyphenatedWord& item);

private:
    QStringList _splitInfo;
};

//! Выгрузка HyphenatedWord в стрим
HYPHENATOR_DLL_API QDataStream& operator<<(QDataStream& stream, const HyphenatedWord& data);
//! Загрузка HyphenatedWord из стрима
HYPHENATOR_DLL_API QDataStream& operator>>(QDataStream& stream, HyphenatedWord& data);

//! Фраза, предназначенная для разбиения на слова, а слова на слоги
//! //! Для разбиения необходимо вызвать Hyphenator.hyphenate
class HYPHENATOR_DLL_API HyphenatedPhrase : public HyphenatedItem
{
    friend class Hyphenator;

public:
    HyphenatedPhrase();
    HyphenatedPhrase(const QString& text);
    ~HyphenatedPhrase();
    //! Фраза разбита на слова и слоги
    bool isHyphenated() const;
    //! Слова
    QVector<HyphenatedWord> hyphenatedWords() const;

    HyphenatedPhrase& operator=(const HyphenatedPhrase& item);

private:
    QVector<HyphenatedWord> _hyphenatedWords;
};

//! Базовый абстрактный класс для реализации конкретного алгоритма разбиения на слоги
class HYPHENATOR_DLL_API Hyphenator
{
public:
    Hyphenator();
    virtual ~Hyphenator();
    //! Разбить слово на слоги
    void hyphenate(HyphenatedWord& word);
    //! Разбить фразу на слова и слоги
    void hyphenate(HyphenatedPhrase& phrase);
    //! Реализация конкретного алгоритма разбития
    virtual QStringList doHyphenate(const QString& s) = 0;
};
}
