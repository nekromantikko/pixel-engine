#pragma once
#include "typedef.h"

template <typename T>
struct MemoryPool
{
private:
    T* objs;
    u32* handles;
    u32 maxCount;
    u32 count;
public:
    void Init(u32 size) {
        maxCount = size;
        count = 0;

        objs = (T*)calloc(size, sizeof(T));
        handles = (u32*)calloc(size, sizeof(u32));

        for (u32 i = 0; i < size; i++)
        {
            handles[i] = i;
        }
    }

    void Free() {
        free(objs);
        free(handles);
    }

    T* AllocObject()
    {
        if (count >= maxCount)
            return nullptr;

        u32 handle = handles[count++];
        return &objs[handle];
    }

    T& operator[](u32 index)
    {
        s32 handle = GetHandle(index);
        if (handle == -1) {
            DEBUG_ERROR("U dun fuk up\n");
        }

        return objs[handle];
    }

    s32 GetHandle(u32 i)
    {
        if (i >= count)
            return -1;

        return handles[i];
    }

    u32 Count() const
    {
        return count;
    }

    void FreeObject(u32 index) {
        s32 handle = GetHandle(index);
        if (handle == -1) {
            return;
        }

        handles[index] = handles[--count];
        handles[count] = handle;
    }
};

