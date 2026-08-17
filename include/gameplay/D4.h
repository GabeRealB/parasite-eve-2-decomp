#ifndef GAMEPLAY_D4_H
#define GAMEPLAY_D4_H

#include "common.h"

#include "main/task.h"

struct _GameSessionFrom4;
struct _GpActorArg;
struct _GpAreaKey;

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

/// Per-index flag object pointed to by `D_80060A30`. `field_4[0]` / `[1]` are
/// bitmasks (ids 1–32 and 33–64) cleared by `func_800ABEF8` and set by
/// `func_800ABF1C`.
typedef struct _GpFlagBank {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ s32  field_4[2];
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

/// 8-byte command record. `GpCb68Rec.field_4` points at a 0xFFFF-terminated
/// list of these. `func_800ACF8C` returns whether `field_2` is zero; when it
/// is, `func_800AC688` skips the first record, otherwise it clears
/// `Display_State.field_100`. `field_5` nonzero skips `func_800AD410`.
typedef struct _GpCb68Obj {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ u8   field_5;
    /* 0x6 */ byte pad_6[2];
} GpCb68Obj;
STATIC_ASSERT_SIZEOF(GpCb68Obj, 8);

/// 12-byte per-view record in tables pointed to by `D_8010CB68`.
/// Indexed 1-based by the `D_8010CB54` camera / view byte.
/// `func_800AD2E8` returns `field_8`. `func_800ACF8C` reads `field_4`.
typedef struct _GpCb68Rec {
    /* 0x0 */ void*      field_0;
    /* 0x4 */ GpCb68Obj* field_4;
    /* 0x8 */ void*      field_8;
} GpCb68Rec;
STATIC_ASSERT_SIZEOF(GpCb68Rec, 0xC);

/// Per-stage wrapper. `field_0` is an array of `GpCb68Rec*`, indexed
/// 1-based by `GameSession.field_6` / `GameSessionFrom4.field_2`.
typedef struct _GpCb68Tbl {
    /* 0x0 */ GpCb68Rec** field_0;
} GpCb68Tbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb68Tbl* D_8010CB68[];

/// Dual-buffer primitive list heads, indexed by `Display_State.field_1f`.
extern void* D_8010CAE8[];

/// Cursor into the current `D_8010CAE8` list. Set by `func_800AC688`,
/// advanced by `func_800AD410`.
extern void* D_80114CC8;

/// 5-byte table at `D_800938CC`. `func_800A9B3C` copies it to the stack and
/// indexes it 1-based by `Wip_SysConfig.field_26`; the byte is CdCmd 0x21
/// param2[0].
typedef struct _GpTbl5 {
    /* 0x0 */ u8 field_0[5];
} GpTbl5;
STATIC_ASSERT_SIZEOF(GpTbl5, 5);

extern GpTbl5 D_800938CC;

/// 8-byte pair of byte-table pointers at `D_801149FC`. `func_800AE1F0`
/// indexes by `(D_80114CD8 & 0x70) >> 4`. `D_80114CD2 & 0x100` selects
/// `field_4` over `field_0`. The byte at `(D_80114CD8 & 0xF) -
/// GameActor.field_82` is stored into `GameActor.field_930`.
typedef struct _GpDirPair {
    /* 0x0 */ u8* field_0;
    /* 0x4 */ u8* field_4;
} GpDirPair;
STATIC_ASSERT_SIZEOF(GpDirPair, 8);

extern GpDirPair D_801149FC[];

void func_800A954C(Task* task);
void func_800A9DF0(Task* task);
void func_800AB980(struct _GameSessionFrom4* arg0);
void func_800ABE68(struct _GpActorArg* arg0, u16* arg1);
void func_800ABEF8(s32 arg0);
void func_800ABF1C(struct _GpAreaKey* arg0);
s32   func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_800AC688(void);
/// 1-based index of `(u8)arg0` in the current room's `D_8010CB54` byte
/// list. Length is the `D_8010CB40` cell as an s16. Returns 0 if absent.
s8    func_800ACEBC(s32 arg0);
s32   func_800ACF8C(void);
s32   func_800AD284(void);
void* func_800AD2E8(void);

#endif // GAMEPLAY_D4_H
