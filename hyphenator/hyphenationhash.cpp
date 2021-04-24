#include "hyphenationhash.h"

#include <QDebug>

namespace Hyphenation
{
HyphenationHash::HyphenationHash(Hyphenator* hyphenator)
{
    _hyphenator = hyphenator;
}

HyphenationHash::~HyphenationHash()
{
}

HyphenatedWord HyphenationHash::findWord(const QString& text)
{
    QString t = text.simplified();
    if (t.isEmpty()) {
        return HyphenatedWord();
    }

    uint hashValue = qHash(text);
    //    QList<HashData> dataList = _hash.values(hashValue);

    // Проверяем что выдал запрос по хэшу
    for (QMultiHash<uint, HashData>::iterator i = _hash.begin(); i != _hash.end(); ++i) {
        if (QString::compare(i->word().text(), t, Qt::CaseSensitive) == 0) {
            i->setCurrentDate();
            return i->word();
        }
    }

    // Ничего не найдено. Создаем новое слово в хэше
    HyphenatedWord newWord(text);
    HashData newData(newWord);
    _hyphenator->hyphenate(newData.word());
    _hash.insert(hashValue, newData);
    return newData.word();
}

bool HyphenationHash::saveToFile(QFile& file)
{
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QDataStream ds;
        ds.setDevice(&file);
        ds << *this;
        file.close();
        return true;
    }
    return false;
}

bool HyphenationHash::loadFromFile(QFile& file)
{
    file.close();
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream ds;
        ds.setDevice(&file);
        ds >> *this;
        file.close();
        return true;
    }
    return false;
}

void HyphenationHash::clearOldHash(const QDate& date)
{
    QMultiHash<uint, HashData>::iterator i = _hash.begin();
    while (i != _hash.end()) {
        if (i->dateUsed() < date) {
            i = _hash.erase(i);
        } else {
            i++;
        }
    }
}

HyphenationHash::HashData& HyphenationHash::HashData::operator=(const HyphenationHash::HashData& data)
{
    if (&data != this) {
        _dateUsed = data._dateUsed;
        _word = data._word;
    }
    return *this;
}

QStringList HyphenationHash::doHyphenate(const QString& s)
{
    return findWord(s).splitInfo();
}

QDataStream& operator<<(QDataStream& stream, const HyphenationHash& data)
{
    return stream << data._hash;
}

QDataStream& operator>>(QDataStream& stream, HyphenationHash& data)
{
    stream >> data._hash;
    if (stream.status() != QDataStream::Ok) {
        data._hash.clear();
        qDebug() << "HyphenationHash: load from stream error";
    }
    return stream;
}

HyphenationHash* GlobalHyphenationHash::_hyphenationHash = nullptr;
Hyphenator* GlobalHyphenationHash::_hyphenator = nullptr;
GlobalHyphenationHash::Initializer GlobalHyphenationHash::_initializer;

GlobalHyphenationHash::Initializer::Initializer()
{
}

GlobalHyphenationHash::Initializer::~Initializer()
{
    //    if (GlobalHyphenationHash::_hyphenationHash) {
    //        delete GlobalHyphenationHash::_hyphenationHash;
    //        delete GlobalHyphenationHash::_hyphenator;
    //    }
}

HyphenationHash* GlobalHyphenationHash::hash()
{
    if (!_hyphenationHash) {
        _hyphenator = new HyphenatorFast();
        _hyphenationHash = new HyphenationHash(_hyphenator);
    }
    return GlobalHyphenationHash::_hyphenationHash;
}

QDataStream& operator<<(QDataStream& stream, const HyphenationHash::HashData& data)
{
    return stream << data._dateUsed << data._word;
}

QDataStream& operator>>(QDataStream& stream, HyphenationHash::HashData& data)
{
    return stream >> data._dateUsed >> data._word;
}
} // namespace Hyphenation
