// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Miguel Angel Exposito Sanchez

#include <Arduino.h>

//#define DEBUG

#define DbgSerial Serial1
#ifdef DEBUG
template <typename T>
inline void dbgPrint(const T& t)
{
    if(DbgSerial.availableForWrite())
        DbgSerial.print(t);
}

template <typename T>
inline void dbgPrintln(const T& t)
{
    if(DbgSerial.availableForWrite())
        DbgSerial.println(t);
}
#else
#define dbgPrint(...)
#define dbgPrintln(...)
#endif