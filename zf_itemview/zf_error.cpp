#include "zf_error.h"

namespace zf
{
Error::Error(const char* text)
    : _text(QString::fromUtf8(text))
{
    Q_ASSERT(!_text.isEmpty());
}

Error::Error(const QString& text)
    : _text(text)
{
    Q_ASSERT(!_text.isEmpty());
}

bool Error::isError() const
{
    return !_text.isEmpty();
}

bool Error::isOk() const
{
    return _text.isEmpty();
}

QString Error::text() const
{
    return _text;
}

void Error::clear()
{
    _text.clear();
}

} // namespace zf
