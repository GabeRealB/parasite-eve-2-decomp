#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

// Residual main-executable symbols with no module home yet:
//   - unmatched / overlay funcs (func_800*, func_801*)
//   - BSS/data not yet filed under a module header
//
// Named Module_ APIs and typed globals live in the matching main/*.h.
// Do not re-add those protos here.

#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "common.h"
#include "main/display.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"

extern u16 func_8001D82C(void);

// Overlay / dynamically loaded
extern void Gp_ApplyPadReplay(s32 arg0, PadScratch* arg1);
extern void func_8002C1D8(void);
extern void Gp_DrawActorTmdFlagged(GpuOtBuf* arg0);
extern void Gp_DrawActorTmdActive(GpuOtBuf* arg0);
extern void Gp_SpawnCurView(s32 arg0);
extern void Gp_ClearRec18Occupied(GpRec18* arg0);
extern s32  func_801011D0(GpCoord* arg0, s32 arg1, s32 arg2, s32* arg3);
extern void Gp_LinkViewSprts(void);
extern void Gp_AllocSprtLists(void);
extern s32  Gp_GetViewIndex(void);
extern void func_80179954(void* arg0);
extern void func_80179988(void* arg0);
extern void func_801799BC(void* arg0);
extern s32  func_80179BE4(u16 arg0, u8 arg1, LinInterp* arg2);
extern s16  Gp_FindStreamSlot(u16 arg0, u16 arg1, u16 arg2, u16 arg3);
extern void Gp_StepCdAudioCmd(void);
extern void Gp_ApplySndBankMasks(u16 arg0);
extern void Gp_RestoreStreamRng(void);
extern void func_8017D6D4(void);
extern void func_801D4B64(Task* arg0);
extern s32  func_80042500(void);
extern void func_8004E200(void);
extern s32  func_8001FAE0(u16 arg0, s32 arg1);
extern s32  func_800B0118(s32 arg0, s32 arg1);

extern u16 D_8005ED8A;

extern volatile s32 D_800689E4;
extern volatile s16 D_800689EC;
extern volatile u8  D_80068B5D;
extern volatile u8  D_80068B65;
extern CdlLOC       D_800827F8;
extern s32          D_80068A78;
#ifndef SNDEVT_C
extern s32          D_800820E0;
extern s16          D_800820E4;
extern s16          D_800820E6;
extern volatile u8  D_80082121;
extern volatile u8  D_80082122;
extern volatile s32 D_80082124;
extern volatile s32 D_80082128;
extern volatile u8  D_8008212C;
extern volatile s32 D_80082130;
extern volatile s8  D_80082134;
extern volatile u8  D_80082135;
extern volatile u8  D_80082136;
#endif
extern volatile s32 D_80082750;
extern u8           D_80082754;
extern volatile s32 D_80082770;
extern s32          D_80082778;
extern volatile u8  D_8008277C;
extern u8           D_800827B0[];
extern volatile u8  D_800827E4;
extern volatile u16 D_80082808;
extern volatile u16 D_80082810;

#ifndef LOADUI_C
extern u8  D_8007A394;
extern s16 D_8007A396;
#endif
#ifndef FONT_C
extern u16 D_8007A39C;
#endif
#ifndef SNDBANK_C
extern s32 D_8007E0D4;
#endif

extern u8 D_80725C54[];

#endif // UNKNOWN_SYMS_H
