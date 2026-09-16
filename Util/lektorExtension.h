#pragma once

//based on Lucius and Genpretz lektor.h extension

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

    template<typename T>
    void removeAt(lektor<T>& lek, int index)
    {
        if (index + 1 < lek.count)
        {
            for (uint32_t i = index + 1; i < lek.count; ++i)
            {
                lek[i - 1] = lek[i];
            }
        }
        --lek.count;
    }

    template<typename T>
    void remove(lektor<T>& lek, const T& val)
    {
        for (uint32_t i = 0; i < lek.count; ++i)
        {
            if (lek[i] == val)
            {
                removeAt(lek, i);
                break;
            }
        }
    }


}
