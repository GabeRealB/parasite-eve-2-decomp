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
    func_8004D5A8();
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

INCLUDE_ASM("main/nonmatchings/3D458", func_8004CFE8);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D008);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D0A0);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D0F0);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D150);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D19C);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D200);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D298);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D2EC);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D35C);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D460);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D5A8);

void* F3D458_Malloc(size_t size)
{
    // Simple first-fit allocator, using linked lists.
    // The allocator splits up the available space into variable-length blocks.
    // Each block starts with a header, which contains the total length of the
    // block in bytes (including the header), whether it is allocated, a magic
    // value, and pointers to the previous/next blocks. The header is followed
    // by a chunk of data, which is returned to the caller. The returned
    // pointer (and the block header) are aligned to 4 bytes.

    size_t    maxBlockSize;
    size_t    newBlockSize;
    size_t    allocSize;
    GStruct6* block;
    GStruct6* newBlock;

    // Start the search at the first header.
    maxBlockSize = 0;
    block        = D648E0_HeapStart;

    // Reserve additional space for the header and align to 4 bytes.
    allocSize = (size + sizeof(GStruct6) + 3) & ~3;

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
                newBlock     = (GStruct6*)((u8*)block + allocSize);

                // If there is enough space for a new block, we must link it
                // to the current block.
                if (sizeof(GStruct6) < newBlockSize) {
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

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D6C8);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D7D4);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D820);

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

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D8BC);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D94C);
