#include "common.h"

#include <psyq/libspu.h>
#include <psyq/libapi.h>

#include "main/unknown_syms.h"

void func_8004CC58(s32 arg0)
{
    s32* temp_v0;

    D_800680C0 = 0;
    if (arg0 == 1) {
        goto wait_spu_transfer;
    }
    if (arg0 < 2) {
        if (arg0 == 0) {
            goto init_spu;
        }
        goto end;
    }
    if (arg0 == 2) {
        goto setup_events;
    }
    goto end;

init_spu:
    SpuInit();
    D58028_SpuTimerEnabled = false;
    goto unknown;

wait_spu_transfer:
    SpuIsTransferCompleted(1);

unknown:
    D_800680BC = 0;
    F16494_ResetSpuAttr();

setup_events:
    F3D458_ResetHeap();
    func_800509B4();
    func_8004DDF0();
    F3E48C_ConfigSpuReverb(3);
    func_8004DF10();
    func_8004D0A0();
    F3D458_8004D88C();
    func_80053E68();
    func_80053FF4(0);
    func_80050D20(0);

    temp_v0  = F3D458_Malloc(4);
    *temp_v0 = 0;

    func_8004D460(&func_80053F60, 0, 0x8801, temp_v0);
    if (D58028_SpuTimerEnabled) {
        DisableEvent(D648E0_SpuTimerED);
        CloseEvent(D648E0_SpuTimerED);
        StopRCnt(RCntCNT0);
        D58028_SpuTimerEnabled = false;
    }

    if (D_80070F68.field_124 == 1) {
        D_800680A4 = 0;
        D_8007E0CC = 0;
        SetRCnt(RCntCNT0, 0xffff, RCntMdINTR | RCntMdSC);
        ResetRCnt(RCntCNT0);
        StartRCnt(RCntCNT0);
        EnterCriticalSection();
        D648E0_SpuTimerED = OpenEvent(RCntCNT0, EvSpINT, EvMdINTR, func_8004D7D4);

        // HACK: What is this? The control flow of this function already
        // looks bad. To add insult to injury, This is the output that we
        // want:
        //
        // jal      OpenEvent
        // addiu    a3, a3, %lo(func_8004D7D4)
        // sw       v0, %lo(DE648E0_SpuTimerED)(s0)
        // jal      ExitCriticalSection
        // nop
        //
        // And this is the assembly that we get without this line:
        //
        // jal      OpenEvent
        // addiu    a3, a3, %lo(func_8004D7D4)
        // jal      ExitCriticalSection
        // sw       v0, %lo(DE648E0_SpuTimerED)(s0)
        //
        // Somehow the developers managed to insert the additional nop
        // instruction, and the only way I could think of is to insert
        // an empty assembler instruction. Maybe it has something to do with
        // the compiler/maspsx version, or rewriting the function with more
        // sensible control flow could fix it, but this matches.
        asm("");

        ExitCriticalSection();
        EnableEvent(D648E0_SpuTimerED);
        D58028_SpuTimerEnabled = true;
    }
    D_800680A4 = 0;
    D_8007E0CC = 0;

end:
    D_800680C0 = 1;
}

INCLUDE_ASM("main/nonmatchings/3D458", func_8004CE28);

void func_8004CFC8(void)
{
    func_8004CC58(0);
}

void func_8004CFE8(void)
{
    func_8004CC58(1);
}

void func_8004D008(void)
{
    if (D_800680C0 != 0) {
        D_800680C0 = 0;
        func_8004E200();
        func_800508B0();
        func_8004D8BC();
        F3E48C_8004E44C();
        D_800680BC += 1;
        if (D_80070F68.field_124 == 1) {
            D_8007E0CC = 6;
            ResetRCnt(RCntCNT0);
            D_800680A4 = 1;
        }
        D_800680C0 = 1;
    }
}

void func_8004D0A0(void)
{
    s32        i;
    s32*       p;
    GStruct42* ptr;
    u16        flag;

    p = (s32*)D_8007E0D8;
    i = 0;
    do {
        *p = 0;
        i++;
        p++;
    } while ((u32)i < 0x80);

    flag = 0xFFFF;
    i    = 0xF;
    ptr  = D_8007E0D8;
    ptr += 0xF;
    do {
        ptr->field_8 = flag;
        i--;
        ptr--;
    } while (i >= 0);

    D_8007E0D4 = 0;
}

void func_8004D0F0(GStruct42* arg0)
{
    if ((arg0 != NULL) && ((arg0->field_8 & 0xF000) != 0xF000)) {
        F3D458_Free(arg0->field_1C);
        arg0->field_1C = NULL;
        arg0->field_0  = NULL;
        arg0->field_4  = NULL;
        arg0->field_10 = NULL;
        arg0->field_8  = 0xFFFF;
        arg0->field_14 = NULL;
    }
}

GStruct42* func_8004D150(u16 arg0)
{
    s32        i;
    GStruct42* ptr;
    s32        id;

    if (arg0 == 0xFFFF) {
        arg0 = 0;
    }
    id = arg0;

    for (i = 0, ptr = D_8007E0D8; i < 0x10; i++, ptr++) {
        if (ptr->field_8 == id) {
            return ptr;
        }
    }
    return NULL;
}

void func_8004D19C(GStruct42* arg0)
{
    u16*            table;
    GStruct42Group* data;
    s32             i;
    u8              count;

    table = arg0->field_10;
    if (table != NULL) {
        data   = arg0->field_0;
        *table = 0;
        count  = arg0->field_B;
        table++;
        i = count - 1;
        if (i > 0) {
            i = count - 2;
            if (i != -1) {
                do {
                    *table = table[-1] + data->field_0;
                    data++;
                    i--;
                    table++;
                } while (i != -1);
            }
        }
    }
}

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D200);

s32 func_8004D298(GStruct55* arg0, s32 arg1)
{
    s32 var_a1;

    var_a1 = arg1;
    if (arg0->field_E == 1) {
        if (arg0->field_0 == arg0->field_4) {
            arg0->field_8 = 0;
        }
        var_a1 = (s32)((u32)(var_a1 * arg0->field_0) / 65535);
    }
    return var_a1;
}

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D2EC);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D35C);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D460);

void F3D458_ResetHeap(void)
{
    D648E0_HeapStart              = (HeapBlockHeader*)D648E0_HeapBuffer;
    D648E0_HeapStart->size        = C3D458_HEAP_SIZE;
    D648E0_HeapStart->magic       = C3D458_HEAP_START_MAGIC;
    D648E0_HeapStart->isAllocated = false;
    D648E0_HeapStart->prev        = NULL;
    D648E0_HeapStart->next        = NULL;
}

void* F3D458_Malloc(size_t size)
{
    // Simple first-fit allocator, using linked lists.
    // The allocator splits up the available space into variable-length blocks.
    // Each block starts with a header, which contains the total length of the
    // block in bytes (including the header), whether it is allocated, a magic
    // value, and pointers to the previous/next blocks. The header is followed
    // by a chunk of data, which is returned to the caller. The returned
    // pointer (and the block header) are aligned to 4 bytes.
    size_t           maxBlockSize;
    size_t           newBlockSize;
    size_t           allocSize;
    HeapBlockHeader* block;
    HeapBlockHeader* newBlock;

    // Start the search at the first header.
    maxBlockSize = 0;
    block        = D648E0_HeapStart;

    // Reserve additional space for the header and align to 4 bytes.
    allocSize = (size + sizeof(HeapBlockHeader) + 3) & ~3;

    // Find the first suitable block.
    if (block != NULL) {
        // If we use a normal while(...) loop GCC decides to allocate the
        // registers in the order `t0`, `t1`, `t2`. Instead the registers
        // must be allocated in the order `t1`, `t2`, `t0`. To achieve this
        // we manually place the constants in the correct registers.
        size_t          heapStart         = (size_t)&D648E0_HeapBuffer;
        register size_t heapEnd asm("t1") = heapStart + C3D458_HEAP_SIZE;
        register size_t magic asm("t2")   = C3D458_HEAP_MAGIC;

        // Actual loop body.
        do {
            // Check that the block is still in bounds of our heap.
            if ((size_t)block < heapStart || heapEnd < (size_t)block) {
                return NULL;
            }

            // Skip allocated blocks.
            if (block->isAllocated) {
                goto next;
            }

            // Does not do anything, but is in the assembly for some reason.
            if (maxBlockSize < block->size) {
                maxBlockSize = block->size;
            }

            // If we found a block that is big enough, we can allocate from it.
            if (block->size >= allocSize) {
                // We allocate by splitting the block in two such that:
                //
                // [ block    | byte 0 | ... | byte blockSize ]
                //
                // Turns into the following if there is enough space
                // for a new block:
                //
                // [ block    | byte 0 | ... | byte allocSize ]
                // [ newBlock | byte 0 | ... | byte restSize  ]
                //
                // Or otherwise into:
                //
                // [ block    | byte 0 | ... | byte allocSize ]
                // [            byte 0 | ... | byte restSize  ]
                newBlockSize = block->size - allocSize;
                newBlock     = (HeapBlockHeader*)((u8*)block + allocSize);

                // If there is enough space for a new block, we must link it
                // to the current block.
                if (sizeof(HeapBlockHeader) < newBlockSize) {
                    newBlock->size        = newBlockSize;
                    newBlock->magic       = magic;
                    newBlock->isAllocated = false;

                    if (block->next == NULL) {
                        newBlock->next = NULL;
                    } else {
                        block->next->prev = newBlock;
                        newBlock->next    = block->next;
                    }
                    block->next    = newBlock;
                    newBlock->prev = block;
                    block->size    = allocSize;
                }

                // The allocated data is located just after the header.
                block->isAllocated = true;
                return (u8*)(block + 1);
            }

        next:
            block = block->next;
        } while (block != NULL);
    }

    return NULL;
}

void F3D458_Free(void* ptr)
{
    // This is the inverse of the allocation function. Given a pointer, that we
    // assume points to the start of the data region which was returned by the
    // allocation function, we insert it into the linked list of blocks. To
    // prevent fragmentation, we first try to merge neighboring blocks, if they
    // are not in use.
    size_t           heapStart;
    size_t           heapEnd;
    HeapBlockHeader* header;

    // If `ptr` is `NULL` we are done.
    if (ptr == NULL) {
        return;
    }

    // Safety check: Ensure that the pointer is contained in the heap region.
    // Otherwise we return.
    heapStart = (size_t)D648E0_HeapBuffer;
    if ((size_t)ptr < heapStart) {
        return;
    }

    heapEnd = heapStart + C3D458_HEAP_SIZE;
    if (heapEnd < (size_t)ptr) {
        return;
    }

    // As with the allocation, the data pointer is located directly after the
    // block header. For some reason, the original code first sets the
    // `isAllocated` flag to `false`, before checking the magic number.
    //
    // TODO: Maybe there same heap is reused with a block kind that is not
    // merged on free. Investigate!
    header              = ptr - sizeof(HeapBlockHeader);
    header->isAllocated = false;
    if (header->magic != C3D458_HEAP_MAGIC && header->magic != C3D458_HEAP_START_MAGIC) {
        return;
    }

    // If the preceding block is also free, we grow it to take up the
    // additional space of the current block and make it point to the
    // succeeding block. `header` will always point to the earliest block.
    if (header->prev != NULL && header->prev->isAllocated == false) {
        if (header->next != NULL) {
            header->next->prev = header->prev;
            header->prev       = header->prev;
        }
        header->prev->next  = header->next;
        header->prev->size += header->size;
        header              = header->prev;
    }

    // We do the same, in case the succeeding neighbor is also not in use.
    if (header->next != NULL && header->next->isAllocated == false) {
        if (header->next->next != NULL) {
            header->next->next->prev = header;
        }
        header->size += header->next->size;
        header->next  = header->next->next;
    }

    // This should not be required, since all headers already had the flag
    // set to `false`. Nevertheless, here it is.
    header->isAllocated = false;
}

long func_8004D7D4(void)
{
    if (D_800680A4 != 0) {
        D_8007E0CC--;
        if (D_8007E0CC == 0) {
            D_800680A4 = 0;
            func_8004D820();
        }
    }
    return 0;
}

s32 func_8004D820(void)
{
    if (D_800680C0 == 0) {
        return 0;
    }
    D_800680C0 = 0;
    func_8004E200();
    func_8004D8BC();
    F3E48C_8004E44C();
    D_800680C0  = 1;
    D_800680BC += 1;
    return 0;
}

void F3D458_8004D88C(void)
{
    D648E0_8007E0B0.field_0  = 0;
    D648E0_8007E0B0.field_4  = 0;
    D648E0_8007E0B0.field_8  = 0;
    D648E0_8007E0B0.field_c  = 0;
    D648E0_8007E0B0.field_14 = NULL;
    D648E0_8007E0B0.field_10 = 0;
    D648E0_8007E0C8          = 1;
}

void func_8004D8BC(void)
{
    GStruct8* head;
    GStruct8* node;
    s32       (*callback)(s32);

    head = &D648E0_8007E0B0;
    if (D648E0_8007E0C8 != 0) {
        if (head != NULL) {
            node = (GStruct8*)head->field_14;
            while (1) {
                if (node == NULL) {
                    break;
                }
                callback = (s32 (*)(s32))node->field_0;
                if (callback != NULL) {
                    if (callback(node->field_c) == -1) {
                        node = func_8004D94C(node);
                        continue;
                    }
                }
                node = (GStruct8*)node->field_14;
            }
        }
    }
}

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D94C);
