#include "common.h"

#include "main/mem.h"

void Mem_Set(void* dest, u32 ch, u32 count)
{
    u32 i;
    u8* ptr;
    u8  v8;
    u16 v16;
    u32 v32;
    u32 remaining;
    u32 alignment;

    ptr       = (u8*)dest;
    remaining = count;

    v8  = ch & 0xFF;
    v16 = (u16)(v8 | (v8 << 8));
    v32 = (v8 << 24) + (v8 << 16) + (v8 << 8) + v8;

    while (remaining >= 4) {
        alignment = (uintptr)ptr & 3;

        switch (alignment) {
            case 0:
                *(u32*)ptr = v32;
                ptr       += 4;
                remaining -= 4;
                break;

            case 1:
                *ptr++     = v8;
                *(u16*)ptr = v16;
                ptr       += 2;
                remaining -= 3;
                break;

            case 2:
                *(u16*)ptr = v16;
                ptr       += 2;
                remaining -= 2;
                break;

            case 3:
                *ptr       = v8;
                ptr       += 1;
                remaining -= 1;
                break;
        }
    }

    i = 0;
    while ((i & 0xFFFF) < remaining) {
        *ptr++ = v8;
        i++;
    }
}
