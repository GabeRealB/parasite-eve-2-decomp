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
extern void  Gp_ApplyPadReplay(s32 arg0, PadScratch* arg1);
extern void  func_8002C1D8(void);
extern void  Gp_DrawActorTmdFlagged(GpuOtBuf* arg0);
extern void  Gp_DrawActorTmdActive(GpuOtBuf* arg0);
extern void  Gp_SpawnCurView(s32 arg0);
extern void* Gp_AttachDisp2d(Task* task);
extern void* Gp_AttachTmdFlags(Task* task, TmdSource* src, s32 flags);
extern void  Gp_UnlinkTmd(TmdListHead* arg0);
extern void  Gp_FreeTmd(TmdObject* arg0);
extern void  Gp_UnlinkDisp2d(TmdListHead* arg0);
extern void  Gp_FreeDisp2d(void* arg0);
extern void  Gp_ClearRec18Occupied(GpRec18* arg0);
extern s32   func_801011D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32* arg3);
extern void  Gp_LinkViewSprts(void);
extern void  Gp_AllocSprtLists(void);
extern s32   Gp_GetViewIndex(void);
extern void  func_80179954(void* arg0);
extern void  func_80179988(void* arg0);
extern void  func_801799BC(void* arg0);
extern s32   func_80179BE4(u16 arg0, u8 arg1, LinInterp* arg2);
extern void  func_80030AB0(McWork* work);
extern s16   Gp_FindStreamSlot(u16 arg0, u16 arg1, u16 arg2, u16 arg3);
extern void  Gp_StepCdAudioCmd(void);
extern void  Gp_ApplySndBankMasks(u16 arg0);
extern void  Gp_RestoreStreamRng(void);
extern void  func_8017D6D4(void);
extern void  func_801D4B64(Task* arg0);
extern void  func_800330D8(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern s32   func_80042500(void);
extern void  func_80046B34(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, s32 arg6);
extern void  func_80046EEC(void* arg0, void* arg1, s32 arg2);
extern s16   func_8004DE18(void* arg0);
extern void  func_8004E200(void);
extern u8*   func_80052488(s32 arg0, u8* arg1, MidiSong* arg2, MidiTrack* arg3);
extern s32   CdAudio_DriveNull(void);
extern void  func_8001F430(void);
extern u32   func_8001F180(u32);
extern s32   func_8001FAE0(s32 arg0, s32 arg1);
extern void  func_80059EE0(void);
extern s32   func_800AF590(s32 arg0, s32 arg1);
extern s32   func_800B0118(s32 arg0, s32 arg1);

extern GBytes4       D_80013F18;
extern const GBytes6 D_80014124;
extern s8            D_800138BC[]; // "99999999"
extern s8            D_800138C8[]; // "0"
extern s8            D_800138CC[]; // "999999999"

extern u16  D_8005ED8A;
extern s32* D_8005DCB4[];

extern volatile s32 D_800689E4;
extern volatile s32 D_800689E8;
extern s16          D_800689EC;
extern u8           D_800689F0[];
extern s8           D_80082748;
extern s8           D_80082749;
extern s8           D_8008274A;
extern s8           D_8008274B;
extern u8           D_800680A4;
extern s8           D_800680AC[];
extern s8           D_800680BB;
extern u32          D_800680BC;
extern u8           D_80068184;
extern volatile u32 D_800680C0;
extern volatile s32 D_80068B58;
extern volatile u8  D_80068B5C;
extern volatile u8  D_80068B5D;
extern volatile u8  D_80068B5F;
extern volatile u8  D_80068B62;
extern volatile u8  D_80068B63;
extern volatile u8  D_80068B64;
extern volatile u8  D_80068B65;
extern u8           D_80068B66;
extern volatile s16 D_80068B6A;
extern void*        D_80068B6C;
extern s32          D_80068B70;
extern s32          D_80068B74;
extern volatile u16 D_80068B78;
extern CdlLOC       D_800827F8;
extern u8           D_8007F2F0;
extern u8           D_8007F8E0[0x2800];
extern u8           D_80068A4A;
extern u8           D_80068A54[];
extern s32          D_80068A78;
extern s32          D_800820E0;
extern s16          D_800820E4;
extern u8           D_800820E8;
extern s8           D_800820E9;
extern volatile u8  D_80082121;
extern volatile u8  D_80082122;
extern volatile s32 D_80082124;
extern volatile s32 D_80082128;
extern volatile u8  D_8008212C;
extern volatile s32 D_80082130;
extern volatile s8  D_80082134;
extern volatile u8  D_80082135;
extern volatile u8  D_80082136;
extern u8           D_80082138[0x10];
extern s32          D_80082548[0x80];
extern volatile s32 D_8008274C;
extern volatile s32 D_80082750;
extern u8           D_80082754;
extern volatile s32 D_80082770;
extern s32          D_80082778;
extern volatile u8  D_8008277C;
extern u8           D_800827B0[];
extern volatile u8  D_800827E4;
extern volatile u16 D_80082808;
extern volatile u16 D_80082810;

extern void*        D_8006AC00;
extern u16          D_8006EBB8;
extern s8           D_8006EBBA;
extern s16          D_8006EBF2;
extern volatile s32 D_8006EBF4;

extern u8  D_80061170;
extern u8  D_800626E8;
extern u8  D_80062734;
extern u8  D_80062735;
extern u8  D_80062736;
extern u8  D_80062737;
extern u8  D_80062738;
extern u8  D_80062739;
extern u8  D_80062764[];
extern u8  D_8006276C[];
extern s32 D_8006763C[];
extern s32 D_80067648;
extern s32 D_8006764C;

extern s32    D_8007A0E4;
extern u_long D_8007A120[0x80];
extern s32    D_8007A118;
extern s32    D_8007A358;
extern u16    D_8007A35E;
extern void*  D_8007A360;
extern u8     D_8007A392;
extern u8     D_8007A394;
extern s16    D_8007A396;
extern u8     D_8007A398;
extern s16    D_8007A39A;
extern u16    D_8007A39C;
extern u8     D_8007E0CC;
extern s32    D_8007E0D4;

extern u8 D_80725C54[];

#endif // UNKNOWN_SYMS_H
