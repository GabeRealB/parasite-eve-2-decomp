#ifndef BOOT_H
#define BOOT_H

#include "common.h"

#include "main/task.h"

// =============================================================================
// Types — cold-boot image slots (src/main/boot.c)
// =============================================================================

/// 8-byte VRAM/heap slot: pointer + size. Tables of these are selected via Gfx_ImageSlotTables.
typedef struct _GfxImageSlot {
    /* 0x0 */ u_long* pixels;
    /* 0x4 */ s32     size;
} GfxImageSlot;
STATIC_ASSERT_SIZEOF(GfxImageSlot, 0x8);

// =============================================================================
// Functions — src/main/boot.c
// Boot_LoadInitialFile / Fs_StageCdfIsAvailable are declared in main/fs.h.
// =============================================================================

void Boot_WaitCdAudioReady(void);
void Boot_InitCdAudio(void);
void Gfx_StoreImageSlot(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void Gfx_LoadImageSlot(s32 arg0, s32 arg1, s32 arg2);
void Boot_InitCd(void);
void Boot_ResetCd(s32 mode);
void Boot_LoadTask(Task* task);
void Boot_DispatchCdCmd(void);
void Mem_ConfigureAuxHeap(s32 arg0, s32 arg1);

// =============================================================================
// Globals
// =============================================================================

/// Early-image build stamp string @ VA 0x80012750 ("2000/05/01 19:24 ver2.49").
extern const char Boot_BuildStamp[];

extern GfxImageSlot* Gfx_ImageSlotTables[];

#endif // BOOT_H
