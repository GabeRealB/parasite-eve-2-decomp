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

/// 0x118-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawBandEx`. Holds
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

/// Argument record for `func_800FDB18`, the id-dispatched effect spawner: the
/// coordinate its effects are placed under, and the word they are spawned with.
///
/// The spawner fills `coord` in on first use -- while it is NULL it takes the
/// coordinate the call was handed, or the view coordinate if it was handed none
/// -- so a record that outlives one call carries its coordinate into the next.
/// Every effect the call spawns reads the two halves back as its own
/// `Task::spawnArg1`, where what they mean is that effect's business; the
/// effects that come in a series are spawned `spawnArgHi` times.
///
/// A record is either a global of the overlay that spawns the effects or a
/// member of the work block the spawner keeps beside it.
typedef struct {
    GfxCoord* coord;      // coordinate the effects are placed under, filled in on first use
    s16       spawnArgLo; // low half of the spawned effect's `Task::spawnArg1`
    s16       spawnArgHi; // high half; also the repeat count of an effect spawned in a series
} GpEffArg;
STATIC_ASSERT_SIZEOF(GpEffArg, 0x8);

/// Per-effect work area. `Gp_SpawnEff` allocates one (`memCalloc(0x2C)`) for
/// every effect it spawns and parks it in that task's `Task::spawnArg2`, which
/// is the only handle the rest of the engine has on it: the block is freed
/// with the task that carries it.
///
/// Each effect task reads the block in its own terms, so most of its slots
/// hold whatever that task animates - a billboard's size and spin, a ring's
/// brightness and radius, a palette blend - and the comments below give the
/// reading its users most often give them. `task`, `parent`, `pos` and `age`
/// are the part they all agree on.
///
/// The trailing halfwords are signed: the effect tasks that own the block
/// compare, divide and shift them as signed values. A task that wants one of
/// them unsigned converts it where it reads it.
typedef struct GpEffWork {
    struct Task*     task;    // the effect's own task, which carries this block as its `spawnArg2`
    s32              field_4; // role unproven: zeroed by the spawn path, never read
    struct GfxCoord* parent;  // coordinate the effect hangs off, copied onto `GfxCoord.parent`
    SVECTOR*         field_C; // role unproven: the offset vector the spawn was called with, never read
    SVECTOR          move;    // vector the owning task moves the effect by
    SVECTOR          pos;     // where the effect sits under `parent`, seeded from the spawn's offset vector
    s16              index;   // the owning task's index into the table that picks the effect's frame or level
    s16              age;     // frames since the effect was spawned
    s16              scale;   // magnitude the task animates: a brightness for a ring or flash, a billboard size for a sprite
    s16              angle;   // rotation the task spins the effect by, or the radius a ring effect draws it at
    s16              period;  // frames the task's current phase lasts, or the size it holds while it lasts
    s16              step;    // per-frame step the task advances another slot by, or a packed draw parameter
} GpEffWork;
STATIC_ASSERT_SIZEOF(GpEffWork, 0x2C);

/// 8-byte sprite frame of `D_80111E48`, indexed by
/// `GpEffWork.age / GpEffWork.period` in `Gp_EffSprTask5C`.
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

/// 0x38-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawEffSprite7C` and
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
