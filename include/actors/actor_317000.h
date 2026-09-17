#ifndef ACTOR_317000_H
#define ACTOR_317000_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// A 16.16 fixed-point word, read whole or as its fraction/integer halves.
typedef union Actor317000Fixed {
    s32 v;
    struct {
        u16 lo;
        s16 hi;
    } p;
} Actor317000Fixed;

/// Work block allocated by `func_actor_317000_8016267C` (`Mem_Calloc(0x4CC)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_317000_80162744` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350700_801624B4` does for `Actor350700Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the two `sb` bytes at 0x43D/0x43E and the `sh` at 0x4C8 are set to
/// -1, and the three words at 0x4A0..0x4A8 are cleared. `field_4C0` is the
/// state index `func_actor_317000_80161E68` dispatches on, `lh` scaled by 4
/// into the two function pointers it builds at 0x18/0x1C of its frame:
/// `func_actor_317000_80162458` latches 1 there, and
/// `func_actor_317000_801620BC` clears it together with `field_4C2` when the
/// actor is already facing its target.
///
/// The block opens with the actor's animation context -- `ctx` and the 0x13
/// `GpAnimSlot`s `func_800B3F84` is handed as its fourth and fifth arguments --
/// so the block pointer is also the `GpAnimCtx*` the animation helpers take:
/// `func_actor_317000_80162458` passes it that way to `func_800B3F84`,
/// `Gp_AnimResetSlot`, `Gp_AnimTickIndex` and `func_800B4114`, the same shape
/// `Actor342000Work` and `Actor141000Work` lay out.
///
/// `target` is the placement position `func_actor_317000_80162458` copies in;
/// `step` is the local-space offset `ApplyMatrixLV` rotates into world space.
typedef struct Actor317000Work {
    /* 0x000 */ GpAnimCtx        ctx;
    /* 0x014 */ GpAnimSlot       slots[0x13];  // slot array `func_800B3F84` is handed
    /* 0x30C */ byte             poses[0x130]; // pose buffer `func_800B3F84` is handed
    /* 0x43C */ s8               field_43C;    // non-zero while the animation slots tick
    /* 0x43D */ s8               field_43D;
    /* 0x43E */ s8               field_43E;
    /* 0x43F */ s8               field_43F; // animation state re-applied by `func_actor_317000_80162950`
    /* 0x440 */ MATRIX           light;
    /* 0x460 */ MATRIX           color;
    /* 0x480 */ VECTOR3          target;    // placement position copied in by `func_actor_317000_80162458`
    /* 0x48C */ byte             pad_48C[0x4];
    /* 0x490 */ VECTOR3          step;      // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x49C */ byte             pad_49C[0x4];
    /* 0x4A0 */ Actor317000Fixed pos[3];    // 16.16 position, integer part added to the root coordinate
    /* 0x4AC */ byte             pad_4AC[0xC];
    /* 0x4B8 */ u16              field_4B8; // placement rotation copied in by `func_actor_317000_80162458`
    /* 0x4BA */ u16              field_4BA; // target yaw `func_actor_317000_801627D0` steers toward
    /* 0x4BC */ u16              field_4BC; // placement rotation copied in by `func_actor_317000_80162458`
    /* 0x4BE */ byte             pad_4BE[0x2];
    /* 0x4C0 */ u16              field_4C0;
    /* 0x4C2 */ u16              field_4C2;
    /* 0x4C4 */ s8               field_4C4;
    /* 0x4C5 */ s8               field_4C5;
    /* 0x4C6 */ s16              field_4C6;
    /* 0x4C8 */ s16              field_4C8;
    /* 0x4CA */ byte             pad_4CA[0x2];
} Actor317000Work;
STATIC_ASSERT_SIZEOF(Actor317000Work, 0x4CC);

/// 0x14-byte animation preset the overlay's state bodies build for
/// `func_actor_317000_80162A10` (`func_actor_317000_801627D0` and
/// `func_actor_317000_80162950`) and for its spawn body
/// `func_actor_317000_80162458`, which is that same install written out
/// in-line. `field_0` is compared against `Actor317000Work::field_43E` and
/// `field_4` against `field_43D`, and whichever differs is latched through
/// `func_800B3F84` and the `Gp_AnimResetSlot` / `Gp_AnimTickIndex` slot loops
/// -- `field_0` also selects the bank in `D_actor_317000_8016CF40`. `field_0`
/// is 0 and `field_8` is 1 and `field_C` is 5 in all three; `field_10` is 0 in
/// the two state bodies and 1 in the spawn body, whose `field_4` is its
/// optional start animation (the literal 2 when absent) -- the state bodies
/// take the `field_43F` byte and the literal 2 there. The same five-word shape
/// as `GpAnimArg` and `Actor141000AnimPreset`.
typedef struct Actor317000AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor317000AnimPreset;
STATIC_ASSERT_SIZEOF(Actor317000AnimPreset, 0x14);

/// Spawn placement `func_actor_317000_80162458` copies into its work block:
/// the position into `Actor317000Work::target`, the rotation into
/// `field_4B8..field_4BC` (the yaw being the turn-to-face target).
typedef struct Actor317000Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor317000Placement;
STATIC_ASSERT_SIZEOF(Actor317000Placement, 0x18);

/// Optional start animation for the same handler: the preset's `field_4` and
/// the `field_43F` byte. Absent, the defaults are anim 2 and 1 -- the same
/// 4-byte record as `Actor141000SpawnAnim`.
typedef struct Actor317000SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor317000SpawnAnim;

/// The animation banks `func_actor_317000_80162458` indexes with
/// `Actor317000Work::field_43E` for `func_800B3F84`'s second argument; each
/// entry is a `GpAnimSet`-backed bank pointer.
extern void* D_actor_317000_8016CF40[];

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_317000_801627D0` writes over the root coordinate before
/// `RotMatrix` overwrites the 3x3: five aligned stores rather than nine
/// halfword ones (the same shape as `Actor141000MatWords` and
/// `Actor350700MatWords`).
typedef struct Actor317000MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor317000MatWords;
STATIC_ASSERT_SIZEOF(Actor317000MatWords, 0x14);

/// The constant local-space offset `func_actor_317000_801628D8` rotates,
/// `{ 0, 0xFF800000, 0x400000, 0 }`. The overlay keeps its own copy in
/// `.rodata`, so the address comes from the per-overlay symbol map.
extern VECTOR D_actor_317000_80161E40;

/// Debug-print pool `func_actor_317000_80162CA0` uses for its unrecognised
/// message mode: `D_actor_317000_80161E50` is the `"%s=(%d,%d,%d)\n"` format
/// and the other two are the `"pos"` and `"rot"` labels it passes as that
/// format's `%s`. Kept in `.rodata`, so the addresses come from the
/// per-overlay symbol map.
extern char D_actor_317000_80161E50[];
extern char D_actor_317000_80161E60[];
extern char D_actor_317000_80161E64[];

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8`. Offset 0x44 (libgs
/// `param`) holds the facing `func_actor_317000_801620BC` derives from the
/// actor's own and the slot 3 task's translation, the rotation `RotMatrix` is
/// later rebuilt from -- the same reuse `ActorsShared80132860Coord`,
/// `ActorsShared8013231cCoord` and `GpCoordExt` document for their own
/// overlays.
typedef struct Actor317000Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor317000Coord;

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor335800Msg` and `Actor361100Msg`, whose
/// halfword at 0x2 selects the mode the handler latches.
typedef struct Actor317000Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor317000Msg;
STATIC_ASSERT_SIZEOF(Actor317000Msg, 0x4);

void func_actor_317000_80162744(Task* arg0);
s32  func_actor_317000_80162458(Task* task, s32 arg1, Actor317000Placement* place, Actor317000SpawnAnim* anim);
s32  func_actor_317000_80162CA0(Task* task, s32 arg1, Actor317000Msg* msg);

#endif
