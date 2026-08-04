#ifndef BOOT_H
#define BOOT_H

#include "common.h"

#include "main/task.h"

// =============================================================================
// Types — cold-boot image slots (src/main/boot.c)
// =============================================================================

/// 8-byte VRAM/heap slot: pointer + size. Tables of these are selected via D_8005C37C.
typedef struct _F04CF8_ImageSlot {
    /* 0x0 */ u_long* field_0;
    /* 0x4 */ s32     field_4;
} F04CF8_ImageSlot;
STATIC_ASSERT_SIZEOF(F04CF8_ImageSlot, 0x8);

// =============================================================================
// Functions — src/main/boot.c
// Boot_LoadInitialFile / Fs_StageCdfIsAvailable are declared in main/fs.h.
// =============================================================================

void F04CF8_800148A0(void);
void F04CF8_800148EC(void);
void func_800149E8(s32 arg0, s32 arg1, s32 arg2);
void F04CF8_80014A50(void);
void F04CF8_80014A98(s32 mode);
void func_80014B38(Task* task);
void func_80014C2C(void);
void func_800144F8(s32 arg0, s32 arg1);

// =============================================================================
// Globals
// =============================================================================

extern F04CF8_ImageSlot* D_8005C37C[];

#endif // BOOT_H
