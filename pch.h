#pragma once
#include <QString>

inline QString readableTimeString(qint64 time)
{
   return QString("%1:%2.%3").arg(time / 1000 / 60).arg(time / 1000 % 60, 2, 10, QLatin1Char('0')).arg(time % 1000, 3, 10, QLatin1Char('0'));
}
