#include "common.h"

#include <psyq/malloc.h>
#include <psyq/stdio.h>

#include "main/mem.h"
#include "main/game.h"
#include "main/unknown_syms.h"

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

// `_freep` is exported by libapi, and points to the start of the heap.
// It is initialized by a call to `InitHeap3`, after which it is utilized
// by `malloc3`, `free3`, etc.. The developers decided to repurpose the
// existing heap utilities instead of writing a custom implementation.
// By writing to `_freep`, we can change the active heap.
//
// NOLINTNEXTLINE
extern u8* _freep;

void* Mem_Calloc(size_t size, bool auxHeap)
{
    u32   i;
    void* ptr;
    u32   remaining;
    u8*   dest;

    u32 zero16;
    u32 zero32;

    Mem_SetActiveHeap(auxHeap);
    ptr = malloc3(size);
    if (ptr != NULL) {
        dest      = (u8*)ptr;
        remaining = size;
        zero32    = 0;
        zero16    = 0;

        while (remaining >= 4) {
            switch ((uintptr)dest & 3) {
                case 0:
                    *(u32*)dest = zero32;
                    dest       += 4;
                    remaining  -= 4;
                    break;

                case 1:
                    *dest++     = 0;
                    *(u16*)dest = zero16;
                    dest       += 2;
                    remaining  -= 3;
                    break;

                case 2:
                    *(u16*)dest = zero16;
                    dest       += 2;
                    remaining  -= 2;
                    break;

                case 3:
                    *dest      = 0;
                    dest      += 1;
                    remaining -= 1;
                    break;
            }
        }

        i = 0;
        while ((i & 0xFFFF) < remaining) {
            *dest++ = 0;
            i++;
        }

        goto end;
    }

    printf("gmalloc2-->NULL\n");

end:
    return ptr;
}

void Mem_SetActiveHeap(bool auxHeap)
{
    if (auxHeap == true) {
        _freep = GActiveAuxHeap;
    } else {
        _freep = GHeap;
    }
}

void* Mem_Malloc(size_t size, bool auxHeap)
{
    void* ptr;

    if (auxHeap == true) {
        _freep = GActiveAuxHeap;
    } else {
        _freep = GHeap;
    }

    ptr = malloc3(size);
    if (ptr == NULL) {
        printf("gmalloc-->NULL\n");
    }
    return ptr;
}

void Mem_Free(void* ptr)
{
    _freep = GHeap;
    free3(ptr);
}

void Mem_Free2(void* ptr, bool auxHeap)
{
    if (auxHeap == true) {
        _freep = GActiveAuxHeap;
    } else {
        _freep = GHeap;
    }
    free3(ptr);
}

void Mem_InitAux(void)
{
    InitHeap3((ulong*)GActiveAuxHeap, GActiveAuxHeapSize);
}

void Mem_Init()
{
    InitHeap3((ulong*)GActiveAuxHeap, GActiveAuxHeapSize);
    InitHeap3((ulong*)GHeap, G_HEAP_SIZE);
}

// The rom contains an empty function that is never called.
// Might have been a debug utility that is not present in
// the release.
void Mem_Dummy0()
{
}

void Mem_SetActiveAuxHeap(bool aux0)
{
    switch (aux0 & 0xFFFF) {
        case false:
            GActiveAuxHeap     = D_800691F4;
            GActiveAuxHeapSize = D_800691F8;
            break;

        case true:
            GActiveAuxHeap     = GAuxHeap;
            GActiveAuxHeapSize = GAuxHeapSize;
            break;
    }
}
