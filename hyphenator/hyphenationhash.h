#pragma once

#include <QMultiHash>
#include <QDataStream>
#include <QFile>
#include <QDate>
#include "hyphenator.h"
#include "hyphenatorfast.h"

namespace Hyphenation
{
class HyphenationHash;

HYPHENATOR_DLL_API QDataStream& operator<<(QDataStream& stream, const HyphenationHash& data);
HYPHENATOR_DLL_API QDataStream& operator>>(QDataStream& stream, HyphenationHash& data);

//! Позволяет не разбивать слова на слоги каждый раз, а получать готовый результат из хэша используя findWord
//! Экземпляр класса HyphenationHash должен быть один на все приложение
//! При желании может использоваться как производный класс от Hyphenator. В таком случае используется
//! Hyphenator::hyphenate
class HYPHENATOR_DLL_API HyphenationHash : public Hyphenator
{
    //! Данные в хэше. Служебный класс для HyphenationHash
    class HashData
    {
    public:
        HashData();
        HashData(const HyphenatedWord& word);
        ~HashData();

        HyphenatedWord& word();
        QDate dateUsed() const;
        void setCurrentDate();

        HashData& operator=(const HashData& data);

    private:
        QDate _dateUsed;
        HyphenatedWord _word;

        friend QDataStream& operator<<(QDataStream& stream, const HyphenationHash::HashData& data);
        friend QDataStream& operator>>(QDataStream& stream, HyphenationHash::HashData& data);
    };

    friend QDataStream& operator<<(QDataStream& stream, const HyphenationHash::HashData& data);
    friend QDataStream& operator>>(QDataStream& stream, HyphenationHash::HashData& data);

    friend HYPHENATOR_DLL_API QDataStream& operator<<(QDataStream& stream, const HyphenationHash& data);
    friend HYPHENATOR_DLL_API QDataStream& operator>>(QDataStream& stream, HyphenationHash& data);

public:
    HyphenationHash(Hyphenator* hyphenator);
    ~HyphenationHash();
    //! Разбить слово на слоги. При наличии подобного слова в хэше, значение берется оттуда. Иначе разбивается на слоги
    //! и добавляется в хэш
    HyphenatedWord findWord(const QString& text);
    //! Сохранить хэш в файл
    bool saveToFile(QFile& file);
    //! Загрузить хэш из файла
    bool loadFromFile(QFile& file);
    //! Очистить старые записи в хэше - старше чем date
    void clearOldHash(const QDate& date);
    //! Прикрепленный к классу Hyphenator
    Hyphenator* hyphenator() const { return _hyphenator; }
    //! Реализация интерфейса Hyphenator
    QStringList doHyphenate(const QString& s);

private:
    QMultiHash<uint, HashData> _hash;
    Hyphenator* _hyphenator;
};

//! Статический класс с глобальным объектом HyphenationHash.
//! Автоматически инициализируется объектом класса HyphenatorFast.
//! HyphenatorTeX в настоящий момент не используется из-за медленной работы и неполного,
//! для корректной работы, набора правил.
class HYPHENATOR_DLL_API GlobalHyphenationHash
{
    //! Внутренний класс-инициализатор статических переменных в globalHyphenationHash
    //! Обходное решение из-за отсутствия в с++ статических конструкторов
    class Initializer
    {
    public:
        Initializer();
        ~Initializer();
    };

    friend class Initializer;

public:
    static HyphenationHash* hash();

private:
    static HyphenationHash* _hyphenationHash; //! Глобальный хэш переносов
    static Hyphenator* _hyphenator; //! Механизм расстановки переносов для _hyphenationHash

    static Initializer _initializer; //! Обходное решение из-за отсутствия в с++ статических конструкторов
};

QDataStream& operator<<(QDataStream& stream, const HyphenationHash::HashData& data);
QDataStream& operator>>(QDataStream& stream, HyphenationHash::HashData& data);
} // namespace Hyphenation
