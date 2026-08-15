#ifndef GAMEPLAY_3FB8_H
#define GAMEPLAY_3FB8_H

#include "common.h"

#include "main/session.h"

struct _GsCOORDINATE2;

/// Work object whose `actor` pointer sits at 0x1C (same slot as `Task::idMap`).
typedef struct _GpActorWork {
    /* 0x00 */ byte       pad_0[0x18];
    /* 0x18 */ void*      field_18; // Task::exitCallback; cleared before self-kill
    /* 0x1C */ GameActor* actor;
} GpActorWork;

typedef void (*GpActorFunc)(GpActorWork* arg0);

/// 4-entry callback table copied onto the stack by `func_80108E40`.
typedef struct {
    GpActorFunc funcs[4];
} GpActorFuncTable4;

/// 0xD4-byte block allocated by `func_8010BAC8` (`Mem_Set` size 0xD4) and
/// stored at `GameActor.field_910`. `func_8010BF7C` writes `field_C4`.
typedef struct _GpActorD4 {
    /* 0x00 */ byte pad_0[0xC4];
    /* 0xC4 */ s16  field_C4;
    /* 0xC6 */ byte pad_C6[0xE];
} GpActorD4;
STATIC_ASSERT_SIZEOF(GpActorD4, 0xD4);

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8`. `flg` is the same
/// word cleared by `*field_8 = 0`. Offset 0x44 (`param` in libgs) is an s16
/// flag (`lh`) in `func_8010B590`.
typedef struct _GpCoordExt {
    /* 0x00 */ s32  flg;
    /* 0x04 */ byte pad_4[0x40];
    /* 0x44 */ s16  field_44;
    /* 0x46 */ byte pad_46[2];
} GpCoordExt;
STATIC_ASSERT_SIZEOF(GpCoordExt, 0x48);

/// 8-byte argument record for `func_800FDB18`. `field_0` is a coordinate
/// (fallback `D_80070F10`); `field_4` / `field_6` are packed into the
/// `func_800EA478` argument.
typedef struct _GpEffArg {
    /* 0x0 */ struct _GsCOORDINATE2* field_0;
    /* 0x4 */ s16                    field_4;
    /* 0x6 */ s16                    field_6;
} GpEffArg;
STATIC_ASSERT_SIZEOF(GpEffArg, 0x8);

/// Current actor-work pointer; cleared when the work task is torn down.
extern GpActorWork* volatile D_80115760;

/// `field_96C` dispatcher: three slots of `func_80109170`, then `func_80109208`.
extern GpActorFuncTable4 D_800979F8;

void func_800FC6C0(void);
void func_80101408(GpActorWork* arg0);
void func_80109170(GpActorWork* arg0);
void func_8010A1B0(s32 arg0, s32 arg1);
void func_8010A42C(GpActorWork* arg0, s32 arg1);
void func_80103B5C(GpActorWork* arg0);
void func_8010B210(GpActorWork* arg0);

#endif // GAMEPLAY_3FB8_H
