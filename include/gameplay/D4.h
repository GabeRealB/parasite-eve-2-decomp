#ifndef GAMEPLAY_D4_H
#define GAMEPLAY_D4_H

#include "common.h"

#include "main/task.h"

struct _GameSessionFrom4;
struct _GpActorArg;

/// 8-byte id/handler record. `Task::field_24` points at a table of these
/// (`D_8010D208`, `D_8010FB90`, …). `func_800AC464` walks it and calls the
/// matching handler with the same four arguments. Terminator id is
/// `0x7FFFFFFF`.
typedef s32 (*GpMsgHandler)(Task* task, s32 msgId, s32 arg2, s32 arg3);

typedef struct _GpMsgEntry {
    /* 0x0 */ s32          id;
    /* 0x4 */ GpMsgHandler handler;
} GpMsgEntry;
STATIC_ASSERT_SIZEOF(GpMsgEntry, 8);

/// Per-index flag object pointed to by `D_80060A30`. Words at 0x4 / 0x8 are
/// bitmasks (ids 1–32 and 33–64) cleared by `func_800ABEF8`.
typedef struct _GpFlagBank {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
} GpFlagBank;
STATIC_ASSERT_SIZEOF(GpFlagBank, 0xC);

/// Main-executable table of `GpFlagBank*`, indexed by slot / session field_7.
extern GpFlagBank* D_80060A30[];

/// Per-stage wrapper. `field_0` is a 3-level table of bytes, indexed
/// 1-based by `GameSession.field_6` / `field_5` / `field_4`.
/// `func_800AD284` returns the innermost byte (camera / view index).
typedef struct _GpCb54Tbl {
    /* 0x0 */ u8*** field_0;
} GpCb54Tbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb54Tbl* D_8010CB54[];

/// 12-byte per-view record in tables pointed to by `D_8010CB68`.
/// Indexed 1-based by the `D_8010CB54` camera / view byte.
/// `func_800AD2E8` returns `field_8`.
typedef struct _GpCb68Rec {
    /* 0x0 */ void* field_0;
    /* 0x4 */ void* field_4;
    /* 0x8 */ void* field_8;
} GpCb68Rec;
STATIC_ASSERT_SIZEOF(GpCb68Rec, 0xC);

/// Per-stage wrapper. `field_0` is an array of `GpCb68Rec*`, indexed
/// 1-based by `GameSession.field_6` / `GameSessionFrom4.field_2`.
typedef struct _GpCb68Tbl {
    /* 0x0 */ GpCb68Rec** field_0;
} GpCb68Tbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb68Tbl* D_8010CB68[];

/// 5-byte table at `D_800938CC`. `func_800A9B3C` copies it to the stack and
/// indexes it 1-based by `Wip_SysConfig.field_26`; the byte is CdCmd 0x21
/// param2[0].
typedef struct _GpTbl5 {
    /* 0x0 */ u8 field_0[5];
} GpTbl5;
STATIC_ASSERT_SIZEOF(GpTbl5, 5);

extern GpTbl5 D_800938CC;

void func_800A954C(Task* task);
void func_800A9DF0(Task* task);
void func_800AB980(struct _GameSessionFrom4* arg0);
void func_800ABE68(struct _GpActorArg* arg0, u16* arg1);
void func_800ABEF8(s32 arg0);
s32   func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
s32   func_800AD284(void);
void* func_800AD2E8(void);

#endif // GAMEPLAY_D4_H
