#pragma once

//based on Lucius modified lektor.h

#include <kenshi/util/lektor.h>

namespace lektorEx
{
    template<typename T>
    void reserve(lektor<T>& lek, uint32_t newSize)
    {
        if (newSize == 0)
            newSize = 10;

        if (lek.maxSize < newSize)
        {
            auto newStuff = new T[newSize];
            for (uint32_t i = 0; i < lek.count; ++i)
            {
                newStuff[i] = lek.stuff[i];
            }

            delete[] lek.stuff;
            lek.stuff = newStuff;
            lek.maxSize = newSize;
        }
    }
    template<typename T>
    void push_back(lektor<T>& lek, const T& val)
    {
        if (lek.maxSize <= lek.count)
            reserve(lek, lek.maxSize * 2);

        lek.stuff[lek.count] = val;
        ++lek.count;
    }

    template<typename T>
    void push_back_unique(lektor<T>& lek, const T& val)
    {
        if (lek.count != 0)
        {
            for (uint32_t i = 0; i < lek.count; ++i)
            {
                if (lek.stuff[i] == val)
                    return;
            }
        }

        if (lek.maxSize <= lek.count)
            reserve(lek, lek.maxSize * 2);

        lek.stuff[lek.count] = val;
        ++lek.count;
    }
}
