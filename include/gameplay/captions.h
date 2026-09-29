#ifndef GAMEPLAY_CAPTIONS_H
#define GAMEPLAY_CAPTIONS_H

#include "types.h"

#include "gameplay/cap.h"

#include "main/task_types.h"
#include "main/text.h"

// CAP dialogue commands, text state, relocation and task control.

extern GlyphUvwh D_8010FB70[4];

void Gp_RunCapCmd(s32 arg0, s16 arg1);

void Gp_MsgPlayer3F3(s32 arg0);

void Gp_MsgPlayerWeapon(s32 arg0);

void Gp_SpawnIfCapIdle(s32 arg0, s32 arg1);

/// Enqueues a type-6 sound event, substituting the current stage number into
/// the packed id when its stage nibble is set. `arg1` / `arg2` are the pan and
/// volume bytes.
void Gp_EnqueueStageSnd6(s32 arg0, s32 arg1, s32 arg2);

void Gp_MsgAllyWeapon(s32 arg0);

void Gp_RunCapCmd1(s32 arg0);

void Gp_MsgAlly3F3(s32 arg0);

/// Dispatches 0x7D0 to the slot-4 task to resolve a chained task for the
/// current stage/room, then forwards 0x7D5 with `arg1` to it.
void Gp_MsgSlot4Chain(s32 arg0, s32 arg1);

void func_800E3FAC(s32 arg0, s32 arg1);

void Gp_AllyAnimId(s32* arg0);

void Gp_FillAllyHp(void);

void Gp_FillPlayerHpMp(void);

void Gp_SetNibbleIf(s32 arg0, s32 arg1);

s32 Gp_PackStageSndId(s32 arg0);

void Gp_EnqueueStageSnd7(s32 arg0, s32 arg1);

void Gp_PlayerWeaponId(s32* arg0);

void func_800E4028(Task* arg0);

s32 func_800E3FCC(s32 arg0);

void func_800E7570(Task* arg0);

extern u8 D_80115680;

void Gp_EndWaitTask(Task* task);

void func_800E70AC(Task* task);

extern GpCapFile* Gp_CapFile;

extern u8 D_80115690;

extern s32 D_80115694;

extern GpCapEntry* Gp_CapCmds;

extern u8 D_801156A4;

extern s32 D_801156A8;

s32 Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2);

s32 Gp_CapBusy(void);

s32 Gp_AbortCap(void);

void Gp_ResetCap(void);

void Gp_LoadCapFile(s32 arg0);

void func_800E6E44(GpCapTextCb arg0);

/// Key of the event the running cap script stopped on (`Gp_CapEventKey`).
s32 Gp_GetCapEventKey(void);

void func_800E6D4C(s16 arg0, s16 arg1);

#endif // GAMEPLAY_CAPTIONS_H
