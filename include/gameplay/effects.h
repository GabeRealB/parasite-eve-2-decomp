#ifndef GAMEPLAY_EFFECTS_H
#define GAMEPLAY_EFFECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

struct GfxCoord;
struct Task;

/// The scratch-pad block of a shape drawn about one projected point: `vec` is
/// the point, and one RTPS fills `sx`, `sy`, `flag` and `otz`. `step` is a
/// length scaled by the depth, the radius a ring is swept at or the half size
/// of a billboard sprite.
typedef struct _GpRingScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     step;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} GpRingScratch;
STATIC_ASSERT_SIZEOF(GpRingScratch, 0x18);

/// The scratch-pad block of a billboard quad spun about one projected point:
/// `vec` is the point, taken from a coordinate's world translation, and one
/// RTPS fills `sx`, `sy`, `flag` and `otz`. `dx` and `dy` are the rotated half
/// extents scaled by the depth; they are added to and subtracted from the
/// projected point to place the four corners of the quad, and only their low
/// halves are read back.
typedef struct _GpFxQuadScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} GpFxQuadScratch;
STATIC_ASSERT_SIZEOF(GpFxQuadScratch, 0x1C);

/// The scratch-pad block of an annulus drawn about one projected point: `vec`
/// is the point, and one RTPS fills `sx`, `sy`, `flag` and `otz`. `inner` and
/// `outer` are the two radii the annulus is swept between, each scaled by the
/// depth.
typedef struct _GpArcScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     inner;
    /* 0x14 */ s32     outer;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} GpArcScratch;
STATIC_ASSERT_SIZEOF(GpArcScratch, 0x1C);

/// 0x118-byte scratch from the scratch stack used by `Gp_DrawBandEx`. Holds
/// the two 16-vertex rings of a shaded band: `inner[i]` is the ring of
/// radius `arg1` and `outer[i]` the ring of radius `arg1 + arg2`, both built
/// in the XZ plane by `rsin` / `rcos`, rotated by the coordinate's `workm`
/// and offset by its `workm.t[]`. The second pass projects each segment:
/// `sxy0` is the `gte_stsxy` of `inner[i]` and `sxy1` / `sxy2` / `sxy3` the
/// `gte_stsxy3` of `inner[i + 1]` / `outer[i]` / `outer[i + 1]`, with `otz`
/// from `gte_stszotz` (then incremented) and `flag` from `gte_stflg`.
typedef struct _GpBandScratch {
    /* 0x000 */ SVECTOR inner[16];
    /* 0x080 */ SVECTOR outer[16];
    /* 0x100 */ s32     otz;
    /* 0x104 */ s32     flag;
    /* 0x108 */ DVECTOR sxy0;
    /* 0x10C */ DVECTOR sxy1;
    /* 0x110 */ DVECTOR sxy2;
    /* 0x114 */ DVECTOR sxy3;
} GpBandScratch;
STATIC_ASSERT_SIZEOF(GpBandScratch, 0x118);

/// Argument record for `func_800FDB18`, the id-dispatched effect spawner.
///
/// `coord` is the coordinate the effects hang off, and it is borrowed: the
/// record does not own it. While it is NULL the spawner copies in the
/// coordinate the call passed, or the view coordinate when the call passed
/// none, and a later call that passes no coordinate reuses it. Some effect
/// ids place the effect on this stored coordinate even when the call also
/// passes one.
///
/// `spawnArgLo` and `spawnArgHi` are the signed halves of one spawn-argument
/// word, the low half in bits 0..15. Effect ids that hand the word on pack it
/// into the spawned effect's `Task::spawnArg1`, and that effect decides what
/// the halves mean. Ids that spawn a series use `spawnArgHi` as the repeat
/// count, one of them triples it, and do not pass `spawnArgLo`. Some ids
/// pass only `spawnArgLo` and force the high half to 1. One id reads neither
/// half. Callers store non-negative magnitudes in the low half and small
/// positive counts in the high half.
///
/// A caller keeps the record as a global or as a member of the work block
/// beside the spawner. One effect task also allocates its own into `Task::work`.
typedef struct {
    GfxCoord* coord;      // borrowed coordinate the effects hang off; NULL until a call fills it
    s16       spawnArgLo; // signed low half of the spawn-argument word; series spawns leave it unused
    s16       spawnArgHi; // signed high half of that word, or the repeat count of a series spawn
} EffectSpawnArg;
STATIC_ASSERT_SIZEOF(EffectSpawnArg, 0x8);

/// Work block of one spawned effect.
///
/// The effect spawner allocates one, `sizeof(EffectWork)` bytes, and stores it
/// in that task's `Task::spawnArg2`. The task frees the block when it exits.
/// A caller may also keep the returned pointer for the effect's lifetime.
///
/// `task` is the effect's own task. `parent` is the coordinate the spawn was
/// given, or the view coordinate when that argument was NULL. The effect's own
/// coordinate is parented to the view either way; a task copies `parent` onto
/// a coordinate only when it hangs the effect there, and some tasks read
/// `parent`'s matrix instead. `age` counts frames from zero. `pos` and `move`
/// are vectors. For most effects they are a position and a displacement; some
/// store a rotation triple in `pos` and a direction in `move`. Spawn copies
/// the caller's offset into `pos`, reading a NULL offset as zero, and zeroes
/// `move`.
///
/// The four trailing halfwords are signed parameters. Each effect stores its
/// own magnitudes there, and a reader that wants one unsigned converts it at
/// the use. The comments name the reading most tasks give them.
typedef struct {
    struct Task*     task;    // the effect task; this block is its `spawnArg2`
    s32              field_4; // zeroed at spawn; no decompiled reader; role unproven
    struct GfxCoord* parent;  // coordinate the spawn was given, or the view when that argument was NULL
    SVECTOR*         field_C; // offset pointer the spawn was given, NULL left as NULL; no decompiled reader; role unproven
    SVECTOR          move;    // displacement or direction; zero at spawn
    SVECTOR          pos;     // position or offset copied from the spawn argument; some effects store a rotation triple
    s16              index;   // selector: a frame, variant or sprite column
    s16              age;     // frames since spawn; tasks count it up from zero
    s16              scale;   // size, brightness or fixed-point scale, as the effect uses it
    s16              angle;   // spin, or another angular magnitude; some effects store a radius or a limit
    s16              period;  // frames the current phase lasts, or the value held for that phase
    s16              step;    // per-frame increment, or a packed draw parameter
} EffectWork;
STATIC_ASSERT_SIZEOF(EffectWork, 0x2C);

/// 8-byte sprite frame of `D_80111E48`, indexed by
/// `EffectWork.age / EffectWork.period` in `Gp_EffSprTask5C`.
/// `u` / `v` are the UV origin of a 0x28-wide quad; `clutX` / `clutY` feed
/// `getClut`. TPage is hardcoded to 0x29.
typedef struct _GpEffUv8 {
    /* 0x0 */ u8  u;
    /* 0x1 */ u8  pad1;
    /* 0x2 */ u8  v;
    /* 0x3 */ u8  pad3;
    /* 0x4 */ u16 clutX;
    /* 0x6 */ u16 clutY;
} GpEffUv8;
STATIC_ASSERT_SIZEOF(GpEffUv8, 8);

/// One corner of the unit quad in `D_80111E38`: a signed XZ pair scaled by
/// the caller's half-size before being rotated into world space.
typedef struct _GpQuadCorner {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
} GpQuadCorner;
STATIC_ASSERT_SIZEOF(GpQuadCorner, 0x4);

/// 0x38-byte scratch from the scratch stack used by `Gp_DrawEffSprite7C` and
/// `Room_Draw16`. `vec[]` holds the four rotated + translated quad corners
/// fed to the GTE; `otz` is `gte_stszotz` (then incremented by the sprite
/// helpers, not by `Room_Draw16`), `flag` is `gte_stflg`, and `sxy0` (RTPS
/// of `vec[0]`) plus `sxy1`..`sxy3` (RTPT of the rest) are the projected
/// screen positions copied into the `POLY_FT4`.
typedef struct _GpQuadScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ DVECTOR sxy0;
    /* 0x2C */ DVECTOR sxy1;
    /* 0x30 */ DVECTOR sxy2;
    /* 0x34 */ DVECTOR sxy3;
} GpQuadScratch;
STATIC_ASSERT_SIZEOF(GpQuadScratch, 0x38);

/// The scratch-pad block of a billboard quad spun about one projected point,
/// holding what `GpFxQuadScratch` holds in a different order: `vec` is the
/// point, and one RTPS fills `sx`, `sy`, `flag` and `otz`. `dx` and `dy` are
/// the rotated half extents scaled by the depth, added to and subtracted from
/// the projected point to place the corners of the quad; only their low
/// halves are read back.
typedef struct _GpEffFlareScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     dx;
    /* 0x08 */ s32     dy;
    /* 0x0C */ s32     flag;
    /* 0x10 */ SVECTOR vec;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} GpEffFlareScratch;
STATIC_ASSERT_SIZEOF(GpEffFlareScratch, 0x1C);

#endif // GAMEPLAY_EFFECTS_H
