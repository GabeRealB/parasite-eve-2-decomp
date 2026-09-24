#ifndef ACTOR_207200_H
#define ACTOR_207200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// A `GpObj` list node followed by one `GpRec18`-sized record. The large
/// enemy keeps five and unlinks them one by one as it tears down; the record
/// of the one at 0x214 becomes the enemy's `recs` once it starts dying.
typedef struct Actor207200Obj {
    /* 0x00 */ GpObj obj;
    /* 0x20 */ byte  field_20[0x18];
} Actor207200Obj;
STATIC_ASSERT_SIZEOF(Actor207200Obj, 0x38);

/// Work block of either enemy the package spawns, as its tick handlers read
/// it. It describes two different allocations at once: the small enemy's
/// 0x2B0-byte block (the objects at 0xFC, 0x14C and 0x184 and the fields
/// from 0x264 to 0x2A6) and the large enemy's 0x4AC-byte block (the objects
/// from 0x1DC on and the fields from 0x44C on).
typedef struct Actor207200Work {
    /* 0x000 */ byte           pad_0[0xFC];
    /* 0x0FC */ Actor207200Obj field_FC;
    /* 0x134 */ byte           pad_134[0x18];
    /* 0x14C */ Actor207200Obj field_14C;
    /* 0x184 */ Actor207200Obj field_184;
    /* 0x1BC */ byte           pad_1BC[0x20];
    /* 0x1DC */ Actor207200Obj field_1DC;
    /* 0x214 */ Actor207200Obj field_214;
    /* 0x24C */ byte           pad_24C[0x18];
    /* 0x264 */ MATRIX         field_264; // transform folded onto the model part
    /* 0x284 */ byte           pad_284[2];
    /* 0x286 */ s16            field_286; // state the enemy work is running
    /* 0x288 */ s16            field_288;
    /* 0x28A */ s16            field_28A; // frames spent in the current state
    /* 0x28C */ s16            field_28C; // animation id the work is playing
    /* 0x28E */ u16            field_28E; // id the three helper slots last saw
    /* 0x290 */ u16            field_290; // frames spent on the current id
    /* 0x292 */ s16            field_292;
    /* 0x294 */ byte           pad_294[0xC];
    /* 0x2A0 */ s16            field_2A0; // angle the transform is scaled by
    /* 0x2A2 */ byte           pad_2A2[4];
    /* 0x2A6 */ s16            field_2A6;
    /* 0x2A8 */ byte           pad_2A8[0x1C];
    /* 0x2C4 */ Actor207200Obj field_2C4;
    /* 0x2FC */ byte           pad_2FC[0x78];
    /* 0x374 */ Actor207200Obj field_374;
    /* 0x3AC */ Actor207200Obj field_3AC;
    /* 0x3E4 */ GpEffArg       field_3E4; // `func_800FDB18` argument record
    /* 0x3EC */ GpEffArg       field_3EC; // `func_800FDB18` argument record
    /* 0x3F4 */ GpEffArg       field_3F4; // `func_800FDB18` argument record
    /* 0x3FC */ byte           pad_3FC[0x50];
    /* 0x44C */ SVECTOR        field_44C; // rotation `field_484` turns about y
    /* 0x454 */ s32            field_454; // model position while the actor idles
    /* 0x458 */ s32            field_458;
    /* 0x45C */ s32            field_45C;
    /* 0x460 */ byte           pad_460[4];
    /* 0x464 */ MATRIX         field_464; // last transform folded onto the model
    /* 0x484 */ s16            field_484; // turn step, +/-25 per frame
    /* 0x486 */ s16            field_486; // state the six helper slots are running
    /* 0x488 */ s16            field_488; // frames spent in `field_486`
    /* 0x48A */ u16            field_48A; // frames the current helper stage has run
    /* 0x48C */ s16            field_48C;
    /* 0x48E */ u16            field_48E; // id the six helper slots last saw
    /* 0x490 */ u16            field_490; // frames spent on the current id
    /* 0x492 */ s16            field_492;
    /* 0x494 */ s16            field_494;
    /* 0x496 */ byte           pad_496[2];
    /* 0x498 */ s16            field_498;
    /* 0x49A */ s16            field_49A;
    /* 0x49C */ s16            field_49C;
    /* 0x49E */ s16            field_49E;
    /* 0x4A0 */ s16            field_4A0;
    /* 0x4A2 */ s16            field_4A2;
    /* 0x4A4 */ s16            field_4A4;
    /* 0x4A6 */ s16            field_4A6;
    /* 0x4A8 */ s16            field_4A8;
    /* 0x4AA */ s16            field_4AA;
} Actor207200Work;

#endif
