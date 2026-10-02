#ifndef GAMEPLAY_PRIVATE_CAPTIONS_H
#define GAMEPLAY_PRIVATE_CAPTIONS_H

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "gameplay/direction.h"
#include "message.h"

#include "main/task_types.h"
#include "main/text.h"

// CAP dialogue commands, text state, relocation and task control.

extern GpCmdReply D_801155A0;

extern TaskDesc D_8010FB4C[3];

extern s32 D_8010FB80;

extern s32 D_8010FB84;

extern s32 Gp_CapCaretGrey;

extern s32 Gp_CapCaretDir;

extern const char Gp_StrCapMagic[];

extern const _GpCapLayout D_80097518;

extern const char Gp_StrEvsFmt[];

extern const TaskFuncTable3 Gp_CapTaskStates;

void Gp_ClearAllFlagNibbles(void);

void Gp_SpawnEvt1(s32 arg0, s32 arg1);

/// Location-message fallback of `D_8010FAD4`, the table installed on pointer
/// slot 7: copies the requested location onto the outgoing record and answers
/// 1, leaving the decision to whoever reads the reply.
s32 func_800E3FF0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst);

s32 func_800E4018(void);

extern s16 D_801156BC;

void Gp_InitCapTask(Task* task);

void Gp_CapTaskState1(Task* task);

extern GpCapChoice D_801155D0[15];

extern u8 D_80115648;

extern s16 D_8011564A;

extern u16 Gp_CapCaretX;

extern u16 Gp_CapCaretY;

extern s16 D_80115650;

extern s16 D_80115652;

extern s16 D_80115654;

extern s16 D_80115656;

extern u8 Gp_CapCaretDelay;

extern u8 D_80115659;

extern u8 D_8011565A;

extern u16 D_8011565C;

extern CapTextUpdateCallback D_80115660;

extern s16 D_80115664;

extern s16 D_80115666;

extern s16 Gp_CapEventKey;

extern s16 D_8011566A;

extern u8 D_8011566C;

extern u8 D_8011566D;

extern u8 D_8011566E;

extern u8 D_8011566F;

extern u8 D_80115670;

extern Task* Gp_CapTask;

extern s16 D_80115678;

extern s16 D_8011567A;

extern TextGlyphCell* Gp_CapGlyphs;

s32 Gp_RelocCapFile(CapFileAddress base);

extern CapSequenceRecord* Gp_CapTable;

extern s16 D_801155AC;

extern u16 D_801155AE;

extern s16 D_801155B0;

extern s16 D_801155B2;

extern s16 D_801155B4;

extern s16 D_801155B6;

extern u8 D_801155B8;

extern s8 D_801155B9;

extern u8 D_801155BA;

extern u8 D_801155BB;

extern s16 D_801155BC;

extern s16 D_801155BE;

extern s16 D_801155C0;

s32 Gp_StartCap(CapSequenceRecord* sequence, s16 arg1, s16 arg2);

/// Pointer to the loaded `.pe2cap2` blob (folder slot type 3).
extern u8 D_80115688;

extern s16 D_80115698;

extern s16 D_8011569A;

extern u8 D_8011569C;

void func_800E44A0(Task* task);

s16 Gp_CapTextHeight(const u16* arg0);

s16 Gp_CapTextTopY(const u16* arg0);

void Gp_ApplyCapEvtFlags(void);

s32 Gp_FindCapEvt(s32 arg0);

void func_800E6EF4(Task* task);

/// `spawnArg1` packs three bytes: bits 0-7 are the message argument, bits
/// 8-15 the delay in frames, and bits 16-23 the recipient - 0 for slot 3, 1
/// for slot 0xA, otherwise `Gp_LookupSlot4(n - 2)`.
void Gp_DelayedMsgTask(Task* task);

#endif // GAMEPLAY_PRIVATE_CAPTIONS_H
