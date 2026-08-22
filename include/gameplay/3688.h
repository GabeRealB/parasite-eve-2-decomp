#ifndef GAMEPLAY_3688_H
#define GAMEPLAY_3688_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

struct _UiObject;

/// Callback for UiObject + Task state handlers (e.g. entries in `D_80096F7C`).
typedef void (*UiObjectTaskFunc)(struct _UiObject* arg0, Task* arg1);

/// Fixed-size table of `UiObjectTaskFunc` callbacks. Copied onto the stack by
/// `func_800CE498` so the call uses a local jump table.
typedef struct {
    UiObjectTaskFunc funcs[3];
} UiObjectTaskFuncTable3;

/// Five-entry dispatcher table: `func_800BF624`, `func_800CDFA8`, `func_800CE094`,
/// `func_800CE188`, `func_800CE1E0`. Copied onto the stack by `func_800CE22C`.
extern TaskFuncTable5 D_80096E70;

/// Three-entry dispatcher table: `func_800CE3B4`, `func_800C010C`, `func_800C02A0`.
extern UiObjectTaskFuncTable3 D_80096F7C;

/// Four-entry dispatcher table: `func_800D1D5C`, `func_800D1E28`, `func_800CFF04`,
/// `func_800D1EB8`.
extern TaskFuncTable4 D_800971C0;

/// 0xE-byte per-room record in tables pointed to by `D_8010F0F4`.
/// Indexed by `GameSession.field_7 - 1` then `GameSession.field_6`.
/// field_0/field_2 are signed coords, field_4/field_6 unsigned extents,
/// field_8/field_A signed scales (`func_800D02A4`); field_C is the
/// room id stored in `D_80114DF0` (`func_800D1FD4`).
typedef struct _GpMapRec {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  field_C;
    /* 0x0D */ u8  field_D;
} GpMapRec;
STATIC_ASSERT_SIZEOF(GpMapRec, 0xE);

/// Per-stage table of `GpMapRec` arrays. Index is `GameSession.field_7 - 1`.
extern GpMapRec* D_8010F0F4[];

/// 0x20-byte per-room name string in tables pointed to by `D_8010F0B8`.
/// Indexed by `GameSession.field_7 - 1` then `GameSession.field_6 - 1`.
typedef struct _GpMapName {
    /* 0x00 */ u8 text[0x20];
} GpMapName;
STATIC_ASSERT_SIZEOF(GpMapName, 0x20);

/// Per-stage table of `GpMapName` arrays. Index is `GameSession.field_7 - 1`.
/// A NULL entry skips the name draw (`func_800D1BAC`).
extern GpMapName* D_8010F0B8[];

/// 8-byte map marker in tables pointed to by `D_8010F11C`.
/// Indexed by loop `i` in `func_800D08D4`. `field_0` is the icon object
/// (`func_800D4270`); `field_4` is the room id (`D_80114DF0`); `field_5`
/// is an extra bit id (`0xFF` = none).
typedef struct _GpMapMark {
    /* 0x0 */ void* field_0;
    /* 0x4 */ u8    field_4;
    /* 0x5 */ u8    field_5;
    /* 0x6 */ byte  pad_6[2];
} GpMapMark;
STATIC_ASSERT_SIZEOF(GpMapMark, 8);

/// Per-stage table of `GpMapMark` arrays. Index is `GameSession.field_7 - 1`.
extern GpMapMark* D_8010F11C[];

/// Per-stage table of GameFlag nibble ids, indexed by room (`D_80114DF0`).
/// Index is `GameSession.field_7 - 1`.
extern u8* D_8010F108[];

/// Per-stage `GpMapMark` counts. Index is `GameSession.field_7 - 1`.
extern u8 D_8010F138[];

/// Current room id copied from `GpMapRec.field_C` by `func_800D1FD4`.
extern u8 D_80114DF0;

/// Room-id offset applied by `func_800D131C` (0, or 1 / 3 for two flagged rooms).
extern u8 D_80114DF1;

void func_800CD924(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void func_800CDBEC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_800CDE80(s32 arg0, s32 arg1);
void func_800CEB40(s32 arg0);
void func_800CF7C4(DialogPrompt* arg0, UiObject* arg1);
void func_800D4E40(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
u8   func_800D1FD4(void);

#endif // GAMEPLAY_3688_H
