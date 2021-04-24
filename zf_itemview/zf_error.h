#pragma once

#include "zf_itemview.h"
#include <QString>

namespace zf
{
class ZF_ITEMVIEW_DLL_API Error
{
public:
    Error() = default;
    explicit Error(const char* text);
    explicit Error(const QString& text);

    bool isError() const;
    bool isOk() const;
    //! Текст ошибки
    QString text() const;
    //! Сбросить в ОК
    void clear();

private:
    QString _text;
};

} // namespace zf
