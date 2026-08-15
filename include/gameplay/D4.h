#ifndef GAMEPLAY_D4_H
#define GAMEPLAY_D4_H

#include "common.h"

struct _GameSessionFrom4;
struct _GpActorArg;

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

void func_800AB980(struct _GameSessionFrom4* arg0);
void func_800ABE68(struct _GpActorArg* arg0, u16* arg1);
void func_800ABEF8(s32 arg0);

#endif // GAMEPLAY_D4_H
