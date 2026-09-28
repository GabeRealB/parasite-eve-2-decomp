#ifndef MAIN_PRIVATE_BOOT_H
#define MAIN_PRIVATE_BOOT_H

#include "types.h"

#include "main/task_types.h"

void Boot_WaitCdAudioReady(void);

void Boot_InitCdAudio(void);

void Boot_InitCd(void);

void Boot_ResetCd(s32 mode);

void Boot_LoadTask(Task* task);

void Boot_DispatchCdCmd(void);

#endif // MAIN_PRIVATE_BOOT_H
