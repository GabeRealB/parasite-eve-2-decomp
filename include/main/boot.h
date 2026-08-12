#ifndef BOOT_H
#define BOOT_H

#include "common.h"

#include "main/task.h"

// =============================================================================
// Functions — src/main/boot.c
// Boot_LoadInitialFile / Fs_StageCdfIsAvailable are declared in main/fs.h.
// Gfx image-slot APIs live in main/gfx.h.
// Mem_ConfigureAuxHeap lives in main/mem.h (implemented in boot.c).
// =============================================================================

void Boot_WaitCdAudioReady(void);
void Boot_InitCdAudio(void);
void Boot_InitCd(void);
void Boot_ResetCd(s32 mode);
void Boot_LoadTask(Task* task);
void Boot_DispatchCdCmd(void);

// =============================================================================
// Globals
// =============================================================================

/// Early-image build stamp string @ VA 0x80012750 ("2000/05/01 19:24 ver2.49").
extern const char Boot_BuildStamp[];

#endif // BOOT_H
