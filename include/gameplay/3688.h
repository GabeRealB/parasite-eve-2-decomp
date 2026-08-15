#ifndef GAMEPLAY_3688_H
#define GAMEPLAY_3688_H

#include "common.h"

#include "main/task.h"

struct _UiObject;

/// Callback for UiObject + Task state handlers (e.g. entries in `D_80096F7C`).
typedef void (*UiObjectTaskFunc)(struct _UiObject* arg0, Task* arg1);

/// Fixed-size table of `UiObjectTaskFunc` callbacks. Copied onto the stack by
/// `func_800CE498` so the call uses a local jump table.
typedef struct {
    UiObjectTaskFunc funcs[3];
} UiObjectTaskFuncTable3;

/// Three-entry dispatcher table: `func_800CE3B4`, `func_800C010C`, `func_800C02A0`.
extern UiObjectTaskFuncTable3 D_80096F7C;

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

/// Current room id copied from `GpMapRec.field_C` by `func_800D1FD4`.
extern u8 D_80114DF0;

u8 func_800D1FD4(void);

#endif // GAMEPLAY_3688_H
