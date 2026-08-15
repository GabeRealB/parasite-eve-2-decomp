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

void func_800FC6C0(void);
void func_80101408(GpActorWork* arg0);
void func_80109170(GpActorWork* arg0);
void func_8010A1B0(s32 arg0, s32 arg1);
void func_8010A42C(GpActorWork* arg0, s32 arg1);
void func_80103B5C(GpActorWork* arg0);
void func_8010B210(GpActorWork* arg0);

#endif // GAMEPLAY_3FB8_H
