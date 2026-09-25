#ifndef GAMEPLAY_3FB8_H
#define GAMEPLAY_3FB8_H

#include "common.h"

#include "gameplay/coord.h"
#include "gameplay/message.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>
#include "gameplay/3CD8.h"

struct _GsCOORDINATE2;
struct _GpObjDirRec;

/// One body an actor puts on the world's object lists: a sphere of `radius`
/// whose centre is `pos`, a local offset under `coord`, and which the contacts
/// it takes part in name by `key`.
///
/// `flags` bits 0-2 select what `ctx` points at, which is how the collision
/// passes reach the `GpRec18` table recording that body's contacts: 0 nothing,
/// 1 the table itself, 2 a node whose own table is used, 3 the `GpActorD4Rec`
/// shape the body carries, 4 a `GpObjDirRec`. Bit 3 marks a node sitting on a `Gp_ObjLists` list, bit
/// 0x800 makes the contacts it produces name the node instead of a direction,
/// and bits 0x4000 and 0x8000 enable the grid and pair passes, which skip a
/// node whose bit is clear.
typedef struct _GpObj {
    struct _GpObj* next;            // next on the list
    struct _GpObj* prev;            // previous on the list
    GsCOORDINATE2* coord;           // transform `pos` is an offset under
    union {
        GpRec18*             recs;  // kind 1: the body's own contact table
        struct _GpObj*       node;  // kind 2: the node whose table is used
        GpActorD4Rec*        d4rec; // kind 3: the shape the body carries
        struct _GpObjDirRec* dir;   // kind 4: the record whose `field_8` table is used
    } ctx;                          // the body's collision context; see the kind bits
    SVECTOR pos;                    // centre, in the `coord` frame
    s32     key;                    // identity in the contact records: class << 16 | id
    u16     radius;                 // collision radius
    u16     flags;                  // kind, list membership and pass enables; see above
} GpObj;
STATIC_ASSERT_SIZEOF(GpObj, 0x20);

/// 0x10-byte spawn argument for `Gp_SpawnAlly` / `Gp_SpawnPlayer`. `field_0`
/// is copied to `GameActor.field_52`; `field_4` / `field_8` / `field_C` are
/// copied to the extra coordinate translation.
typedef struct _GpActorArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ s32  field_4;
    /* 0x8 */ s32  field_8;
    /* 0xC */ s32  field_C;
} GpActorArg;
STATIC_ASSERT_SIZEOF(GpActorArg, 0x10);

/// Companion spawn record for `Gp_SpawnPlayer`. `field_0` is copied to
/// `GameActor.field_93C`. Nonzero `field_2` sets `field_954` to 2.
typedef struct _GpActorFlags {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ byte pad_3;
} GpActorFlags;
STATIC_ASSERT_SIZEOF(GpActorFlags, 0x4);

/// The companion block `Gp_SpawnAlly` allocates (`Mem_Set` size 0xD4) and
/// `GameActor.field_910` holds: the collision body a companion carries with it,
/// and the counters its own AI drives. Nothing outside the companion overlays
/// reads the block itself, only whether the pointer is set, which is how the
/// rest of gameplay tells a companion from any other actor.
///
/// `coord` / `obj` / `shape` / `contact` are that body. `Gp_BindActorD4` fills
/// them in: the actor's model coordinate copied into `coord`, a kind-3 `obj`
/// hung off it, and the one-entry `contact` table `shape` records its
/// collisions in. Only the companion that walks a scripted route binds one, so
/// the others carry the body around unused and their `contact` table stays
/// empty - which is why the helpers that read it take a zero to mean nothing is
/// touching the companion.
///
/// The rest is the AI's: `decisionTimer` paces when the companion picks its
/// next action, `scanAngle` / `targetHeading` / `scanDist` steer the turn it
/// makes then, `repeatCount` / `actionCount` bound the burst of work it is in
/// the middle of, and the last three walk it along its route.
typedef struct GpActorD4 {
    /* 0x00 */ byte          pad_0[0x18];
    /* 0x18 */ GsCOORDINATE2 coord;         // the body's transform, a copy of the actor's model coordinate
    /* 0x68 */ GpObj         obj;           // the body: a kind-3 node whose `ctx.d4rec` is `shape`
    /* 0x88 */ GpActorD4Rec  shape;         // the capsule the body's collisions are tested with
    /* 0xA0 */ GpRec18       contact;       // the one-entry table `shape` records its contacts in
    /* 0xB8 */ byte          pad_B8[0xC];
    /* 0xC4 */ s16           decisionTimer; // frames left before the companion picks its next action
    /* 0xC6 */ s16           scanAngle;     // sweep angle: 0x80 a tick, and past 0x1000 the sweep is over
    /* 0xC8 */ s16           targetHeading; // heading being turned to, in the 0..0xFFF angle unit
    /* 0xCA */ s16           scanDist;      // the contact distance the sweep compares its candidates by
    /* 0xCC */ u8            repeatCount;   // swings left in the attack burst, or the flinch interval of the companion that does not fight
    /* 0xCD */ u8            actionCount;   // attacks left before the fighting companions stop, or flinches taken by the other one
    /* 0xCE */ s8            pathStep;      // waypoint the companion is walking to
    /* 0xCF */ s8            turnDir;       // +1 or -1: the way it turns to `targetHeading`
    /* 0xD0 */ s8            pathDone;      // 1 once the last waypoint is reached
    /* 0xD1 */ byte          pad_D1[3];
} GpActorD4;
STATIC_ASSERT_SIZEOF(GpActorD4, 0xD4);

/// Overlay of `GameActor` for the three s16s at 0x418 (`GsCOORDINATE2.param`
/// as vx/vy/vz). `Gp_AttachActorObj` zeros them after `Gfx_RotMatrixX` of
/// `field_3D4.workm`.
typedef struct _GpActorSvec {
    /* 0x000 */ byte pad[0x418];
    /* 0x418 */ s16  field_418;
    /* 0x41A */ s16  field_41A;
    /* 0x41C */ s16  field_41C;
} GpActorSvec;

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
    GsCOORDINATE2* coord;      // coordinate the effects are placed under, filled in on first use
    s16            spawnArgLo; // low half of the spawned effect's `Task::spawnArg1`
    s16            spawnArgHi; // high half; also the repeat count of an effect spawned in a series
} GpEffArg;
STATIC_ASSERT_SIZEOF(GpEffArg, 0x8);

extern GpEffArg D_80112C74;
extern s32      D_80112C7C[];

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
    struct Task*           task;    // the effect's own task, which carries this block as its `spawnArg2`
    s32                    field_4; // role unproven: zeroed by the spawn path, never read
    struct _GsCOORDINATE2* parent;  // coordinate the effect hangs off, copied onto `GsCOORDINATE2.sub`
    SVECTOR*               field_C; // role unproven: the offset vector the spawn was called with, never read
    SVECTOR                move;    // vector the owning task moves the effect by
    SVECTOR                pos;     // where the effect sits under `parent`, seeded from the spawn's offset vector
    s16                    index;   // the owning task's index into the table that picks the effect's frame or level
    s16                    age;     // frames since the effect was spawned
    s16                    scale;   // magnitude the task animates: a brightness for a ring or flash, a billboard size for a sprite
    s16                    angle;   // rotation the task spins the effect by, or the radius a ring effect draws it at
    s16                    period;  // frames the task's current phase lasts, or the size it holds while it lasts
    s16                    step;    // per-frame step the task advances another slot by, or a packed draw parameter
} GpEffWork;
STATIC_ASSERT_SIZEOF(GpEffWork, 0x2C);

/// The leading rotation entries of a `MATRIX`, paired into words, so a
/// rotation can be reset to identity with five aligned word stores instead of
/// nine halfword ones.
typedef struct _GpMtxWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} GpMtxWords;

/// 4-byte row of `D_8011291C`, indexed by `Task::spawnArg1`.
/// `Gp_EffPolyTask9C` copies `field_0` / `field_2` into `GpEffWork.period` /
/// `field_2A` (draw param for `Gp_DrawEffShard` and per-frame `field_26` step).
typedef struct _GpEffRec {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpEffRec;
STATIC_ASSERT_SIZEOF(GpEffRec, 4);

extern GpEffRec D_8011291C[];

/// 0xC-byte sprite frame of `Gp_EffSprRecs`, indexed by `GpEffWork.age`.
/// `w` is both the UV quad size and the billboard scale factor. `u` / `v` are
/// the UV origin. `clutX` / `clutY` feed `getClut`; `tpageX` feeds
/// `getTPage(0, 1, tpageX, 0)`.
typedef struct _GpEffSprRec {
    /* 0x0 */ u16 w;
    /* 0x2 */ u8  u;
    /* 0x3 */ u8  pad3;
    /* 0x4 */ u8  v;
    /* 0x5 */ u8  pad5;
    /* 0x6 */ u16 clutX;
    /* 0x8 */ u16 clutY;
    /* 0xA */ u16 tpageX;
} GpEffSprRec;
STATIC_ASSERT_SIZEOF(GpEffSprRec, 0xC);

extern GpEffSprRec Gp_EffSprRecs[];

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

extern GpEffUv8 D_80111E48[];

/// Overlay of `D_80112964` at `u16` index `GpEffWork.step`.
/// `Gp_DrawEffSprite81` loads `field_4`, shifts it into a CLUT X nibble, and
/// ORs `0x4280`. `Gp_DrawEffQuadT29` uses the same table at byte offset 8.
typedef struct _GpEffClutOff {
    /* 0x0 */ u16 pad_0;
    /* 0x2 */ u16 pad_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 pad_6;
    /* 0x8 */ u16 field_8;
} GpEffClutOff;

/// u16 CLUT-source table. Indexed as `&D_80112964[step]` then overlaid
/// with `GpEffClutOff` so the load is `lhu 4(base + step * 2)`.
extern u16 D_80112964[];

/// Spawn-id words indexed by the 3-digit packing of `Gp_StateC08.field_0`
/// `(hundreds-1)*9 + (tens-1)*3 + ones - 1`. `Gp_EffTask07State1` uses this
/// when `field_3 == 1`, and `D_80112A50` when `field_3 == -1`.
extern s32 D_80112978[];
extern s32 D_80112A50[];

/// Spawn-id words for `Gp_EffCtlTaskAE`, indexed with the same 3-digit packing
/// of `Gp_StateC08.field_0` as `D_80112978`; the value becomes the task's
/// `Task::spawnArg1` sound id.
extern s32 D_80112B94[];

/// `GsCOORDINATE2` index parallel to `D_80112978`. `Gp_EffTask07State1` adds
/// it onto `TmdObject.coords` when `field_3 == 1`.
extern u16 D_80112B28[];

/// 4 packed RGB-nibble colors. `Gp_EffCtlTaskC1` indexes with
/// `GpEffSpawnArg.field_2 & 3` and stores the halfword in `GpEffWork.period`.
extern u16 D_80112C6C[];

/// Message-handler table stored in `Task::msgTable` by `Gp_InitPlayerWork`.
extern s32 Gp_PlayerMsgTable[];

/// Overlay of `Task::spawnArg1` for `Gp_EffSprTask46` / `Gp_EffCtlTaskC1` /
/// `Gp_EffCtlTaskF4`. `Gp_EffSprTask46` uses `field_0 & 0xFFF` as the target
/// scale and `field_2 & 0xF` as the draw parameter; the parent word's
/// `0x20000000` / `0x10000000` bits pick the start state. `Gp_EffCtlTaskC1`
/// uses the parent word's low 12 bits as a Z rotation and `field_2 & 3` as
/// an index into `D_80112C6C`. `Gp_EffCtlTaskF4` copies `field_0 & 0xFFF` to
/// `GpEffWork.angle` and `field_0 & 0xF000` to `period`.
typedef struct _GpEffSpawnArg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} GpEffSpawnArg;
STATIC_ASSERT_SIZEOF(GpEffSpawnArg, 4);

/// Overlay of `Task::spawnArg1` when the high byte is an `lb` nibble.
/// `Gp_EffSprTask5C` copies `field_3 & 0xF` into `GpEffWork.index`.
typedef struct _GpEffSpawnArgHi {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  pad_2;
    /* 0x3 */ s8  field_3;
} GpEffSpawnArgHi;
STATIC_ASSERT_SIZEOF(GpEffSpawnArgHi, 4);

/// Record whose word at 0x4 is the id `func_8010B2D4` passes to
/// `Gp_ScaleDamage` (and `func_8010B348` passes to `Gp_LookupIdField`).
/// `func_8010B348` also switches on the low 16 bits (2/4 vs 3) before
/// that call.
typedef struct _GpIdRec {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  field_4;
} GpIdRec;

/// One corner of the unit quad in `D_80111E38`: a signed XZ pair scaled by
/// the caller's half-size before being rotated into world space.
typedef struct _GpQuadCorner {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
} GpQuadCorner;
STATIC_ASSERT_SIZEOF(GpQuadCorner, 0x4);

/// Unit quad corners `(-1, 1)`, `(1, 1)`, `(-1, -1)`, `(1, -1)`.
extern GpQuadCorner D_80111E38[4];

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
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} GpEffFlareScratch;
STATIC_ASSERT_SIZEOF(GpEffFlareScratch, 0x1C);

/// 0x78-byte scratch for `func_800FCD00`: twelve alternating outer/inner
/// ring vertices, four projected screen positions, depth, and GTE flags.
typedef struct _GpEffRingScratch {
    /* 0x00 */ SVECTOR vec[12];
    /* 0x60 */ s16     sx0;
    /* 0x62 */ s16     sy0;
    /* 0x64 */ s16     sx1;
    /* 0x66 */ s16     sy1;
    /* 0x68 */ s16     sx2;
    /* 0x6A */ s16     sy2;
    /* 0x6C */ s16     sx3;
    /* 0x6E */ s16     sy3;
    /* 0x70 */ s32     otz;
    /* 0x74 */ s32     flag;
} GpEffRingScratch;
STATIC_ASSERT_SIZEOF(GpEffRingScratch, 0x78);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `Gp_EffTileTaskA4`.
/// `vec` is the coordinate's `workm.t[]` truncated to s16 and fed to
/// `gte_ldv0`. `otz` receives `gte_stszotz`, `flag` `gte_stflg` and `sxy`
/// `gte_stsxy` of the single RTPS that places the spark `TILE`.
typedef struct _GpEffTileScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ DVECTOR sxy;
} GpEffTileScratch;
STATIC_ASSERT_SIZEOF(GpEffTileScratch, 0x14);

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `Gp_EffLineTask92` and
/// `Gp_EffLineTaskA3`.
/// `vec0` is the coordinate's current `workm.t[]` truncated to s16.
/// `Gp_EffLineTask92` puts the previous-frame position (`GpEffWork.pos`) in
/// `vec1`. `Gp_EffLineTaskA3` rotates `move` through
/// `parent->coord` and `Gfx_ViewWorldMtx`, scales by `age << 11 + 0x1000`,
/// and adds `vec0` into `vec1`. Each vector is projected with its own RTPS:
/// `sxy0` / `sxy1` receive `gte_stsxy`, `flag` `gte_stflg` and `otz`
/// `gte_stszotz`, giving the two endpoints of a trail `LINE_F2` /
/// `LINE_G2`.
typedef struct _GpEffLineScratch {
    /* 0x00 */ SVECTOR vec0;
    /* 0x08 */ SVECTOR vec1;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ DVECTOR sxy0;
    /* 0x1C */ DVECTOR sxy1;
} GpEffLineScratch;
STATIC_ASSERT_SIZEOF(GpEffLineScratch, 0x20);

/// 0x10-byte scratch from `G_SCRATCH_HEAD` used by `func_8010133C`.
/// `field_0` / `field_4` are the outer/inner loop counters. `field_8` is
/// a color word (`0x808008`, then `0x37A78`). `field_C` / `field_E` are
/// stepped s16 coordinates (`x += 0x40`, `y -= 0x50`).
typedef struct _GpScratch10 {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s16 field_E;
} GpScratch10;
STATIC_ASSERT_SIZEOF(GpScratch10, 0x10);

/// A 16.16 fixed-point word, read whole or as its fraction and integer halves.
typedef union {
    s32 w;
    struct {
        u16 lo;
        s16 hi;
    } h;
} GpFixed16;

/// 0x10-byte scratch from `G_SCRATCH_HEAD` used by `func_801011D0`.
/// Words at 0/4/8 are the 16.16 deltas from `func_800E0FEC`; if the
/// fractional half is nonzero they are stepped away from zero by 0x10000
/// and the high half is added onto `GsCOORDINATE2.coord.t[]`.
typedef struct _GpDeltaScratch {
    /* 0x00 */ GpFixed16 vx;
    /* 0x04 */ GpFixed16 vy;
    /* 0x08 */ GpFixed16 vz;
    /* 0x0C */ s32       pad;
} GpDeltaScratch;
STATIC_ASSERT_SIZEOF(GpDeltaScratch, 0x10);

/// 0xC-byte scratch from `G_SCRATCH_HEAD` used by `func_80103E7C`.
/// `field_0` / `field_4` / `field_8` are the wrap candidates
/// `tgt - cur`, `tgt - cur + 0x1000`, and `tgt - cur - 0x1000`.
/// The function returns the candidate with the smallest absolute value.
typedef struct _GpAngleScratch {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
} GpAngleScratch;
STATIC_ASSERT_SIZEOF(GpAngleScratch, 0xC);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `Gp_PlayerMode2State4`.
/// `field_0` is the clamped `func_80103E7C` turn delta applied to
/// `GameActor.field_52`. `vec` is the target-minus-current offset
/// (`GameActor.field_20/24/28` minus `GsCOORDINATE2.coord.t`).
typedef struct _GpApproachScratch {
    /* 0x00 */ s32     field_0;
    /* 0x04 */ VECTOR3 vec;
    /* 0x10 */ s32     pad;
} GpApproachScratch;
STATIC_ASSERT_SIZEOF(GpApproachScratch, 0x14);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `func_8010BD88`.
/// `vx`/`vy`/`vz` overlay a `VECTOR3` for `func_80103C74`; `angle` holds
/// the `ratan2` result and the clamped turn delta applied to
/// `GameActor.field_52`.
typedef struct _GpTurnScratch {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
    /* 0x10 */ s32 angle;
} GpTurnScratch;
STATIC_ASSERT_SIZEOF(GpTurnScratch, 0x14);

/// `GameActor.field_17C` collision record (a `GpRec18`) as `func_80109BB4`
/// reads it: `flags` bit 0 marks the slot occupied and its high nibble is
/// the index of the `GpObj` node in `GameActor.field_AC` that produced the
/// hit, `dist` is the contact radius, and `id` / `kind` are the low / high
/// halves of `GpRec18.key`. `kind` 3 is the push-back case (only for
/// `id < 0x46` entries whose `D_80113F9C` entry is 1), 4 and 5 dispatch to
/// `func_8010B2D4` / `func_8010B348`. `x` / `y` / `z` are the world-space
/// contact point (`GpRec18.point`).
typedef struct _GpHitRec {
    /* 0x00 */ u16  flags;
    /* 0x02 */ s16  dist;
    /* 0x04 */ u16  id;
    /* 0x06 */ u16  kind;
    /* 0x08 */ s16  x;
    /* 0x0A */ s16  y;
    /* 0x0C */ s16  z;
    /* 0x0E */ byte pad_E[0xA];
} GpHitRec;
STATIC_ASSERT_SIZEOF(GpHitRec, 0x18);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `Gp_StepPlayerMove`.
/// `scale` is `D_80112E10[field_958]` (signed, stored as a word). `angle`
/// holds `0x640000` then the yaw passed to `Gfx_RotMatrixY`. `saved` is a
/// copy of `GsCOORDINATE2.coord` around that rotate. `vec` is the matrix
/// column from `Gfx_MatrixCol2` / `VectorNormalSS`, later the Manhattan
/// `|dx|+|dz|` to the lock point. `lock` is `Gp_GetLockPos` output.
typedef struct _GpMoveScratch {
    /* 0x00 */ s32     scale;
    /* 0x04 */ s32     angle;
    /* 0x08 */ MATRIX  saved;
    /* 0x28 */ SVECTOR vec;
    /* 0x30 */ VECTOR3 lock;
    /* 0x3C */ s32     pad;
} GpMoveScratch;
STATIC_ASSERT_SIZEOF(GpMoveScratch, 0x40);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_80109BB4`.
/// `pos` is the world position of the colliding `GpObj` (`pos` rotated by
/// `coord->workm`, plus that matrix's translation), later
/// reused to save the actor's pre-push `coord.t[0]` / `t[2]`. `delta` is
/// `pos` minus the contact point, `unit` its `VectorNormal`, and `local`
/// that direction in grid space via `Gp_GridParams->field_0->workm`.
typedef struct _GpPushBackScratch {
    /* 0x00 */ VECTOR pos;
    /* 0x10 */ VECTOR delta;
    /* 0x20 */ VECTOR local;
    /* 0x30 */ VECTOR unit;
} GpPushBackScratch;
STATIC_ASSERT_SIZEOF(GpPushBackScratch, 0x40);

/// Scratch-pad block `func_8010BE5C` aims with: a coordinate placed at an
/// offset from one of the actor's parts, the zero rotation it is placed with,
/// and the vector from it to the target that the heading is taken from.
typedef struct _GpAimScratch {
    /* 0x00 */ VECTOR3       vec;
    /* 0x0C */ s32           pad_C;
    /* 0x10 */ SVECTOR       rot;
    /* 0x18 */ GsCOORDINATE2 coord;
} GpAimScratch;
STATIC_ASSERT_SIZEOF(GpAimScratch, 0x68);

/// 0x68-byte scratch from `G_SCRATCH_HEAD` used by `Gp_PickNearestRec18`.
/// The first 0x10 bytes are the `GpDeltaScratch` passed to `func_800E0FEC`.
/// Offset 0x10 is a temp `GsCOORDINATE2` (`flg` at 0x10, `workm.t[]` at
/// 0x48, `sub` at 0x5C) passed to `Gp_SpawnEff`. `offset` is three
/// `rand() & 7` halfwords passed as that call's last argument and added
/// onto `arg2->workm.t[]` when `arg2` is non-NULL.
typedef struct _GpPickScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ u32     flg;
    /* 0x14 */ byte    pad_14[0x34];
    /* 0x48 */ s32     t[3];
    /* 0x54 */ byte    pad_54[8];
    /* 0x5C */ void*   sub;
    /* 0x60 */ SVECTOR offset;
} GpPickScratch;
STATIC_ASSERT_SIZEOF(GpPickScratch, 0x68);

/// 8-byte rotation row (`SVECTOR` layout). `D_801131B4` is indexed by
/// `Gp_AimPitchRec` arg1 (`D_80167218[Mc_SaveData.companionVariant]`) and by
/// `Player_Status.weapon` in `Gp_AimYawToLock`.
typedef struct _GpAimRot {
    /* 0x0 */ s16 vx;
    /* 0x2 */ s16 vy;
    /* 0x4 */ s16 vz;
    /* 0x6 */ s16 pad;
} GpAimRot;
STATIC_ASSERT_SIZEOF(GpAimRot, 8);

/// 0x6C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_AimYawToLock`.
/// The first 0x50 bytes are a temp `GsCOORDINATE2`. `delta` is
/// `Gp_GetLockPos` output minus that coord's translation (computed in
/// place). `rot` is the `SVECTOR` passed to `Gp_PlaceCoordOffset` (table row
/// `D_801131B4[Player_Status.weapon]`). `angle` holds `ratan2` then
/// the wrapped, clamped yaw delta applied to `GameActor.field_52`.
typedef struct _GpYawScratch {
    /* 0x00 */ byte     pad_0[0x50];
    /* 0x50 */ VECTOR3  delta;
    /* 0x5C */ s32      pad_5C;
    /* 0x60 */ SVECTOR3 rot;
    /* 0x66 */ s16      pad_66;
    /* 0x68 */ s32      angle;
} GpYawScratch;
STATIC_ASSERT_SIZEOF(GpYawScratch, 0x6C);

/// 0x2C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_PlayerMode2State3`.
/// `mtx` receives a copy of the actor coordinate's `coord` matrix, pitched by
/// `Gfx_RotMatrixX`; `dir` (at `head - 0xC`) is that matrix's third column
/// normalized by `VectorNormalSS`, and `div` is the frame count the direction
/// is divided by to produce `GameActor.field_0` / `field_4` / `field_8`.
typedef struct _GpDashScratch {
    /* 0x00 */ MATRIX  mtx;
    /* 0x20 */ SVECTOR dir;
    /* 0x28 */ s32     div;
} GpDashScratch;
STATIC_ASSERT_SIZEOF(GpDashScratch, 0x2C);

/// 0x84-byte scratch from `G_SCRATCH_HEAD` used by `Gp_AimPitchToLock`,
/// `Gp_AimPitchToLockAlt`, `Gp_AimPitchRec`, and `Gp_AimPitchDirect`. The first 0x50 bytes are a
/// temp `GsCOORDINATE2`. `delta` is lock position minus that coord's
/// translation; `lock` is `Gp_GetLockPos` output; `rot` is the
/// `SVECTOR` passed to `Gp_PlaceCoordOffset` (zeros then table row in
/// `Gp_AimPitchToLockAlt`, table row in `Gp_AimPitchRec`, zeros in
/// `Gp_AimPitchDirect`). `angle` holds `ratan2` then the clamped pitch
/// delta applied to `GameActor.field_58` / `field_5C` / `field_60` /
/// `field_64` / `field_70` / `field_78`; `dist` is the XZ length of
/// `delta`. `Gp_AimPitchToLock` also derives `field_5C` / `field_64` from
/// the updated `field_58` / `field_60` (`/ 5` scaled by 3 then 2).
typedef struct _GpPitchScratch {
    /* 0x00 */ byte     pad_0[0x50];
    /* 0x50 */ VECTOR3  delta;
    /* 0x5C */ s32      pad_5C;
    /* 0x60 */ VECTOR3  lock;
    /* 0x6C */ s32      pad_6C;
    /* 0x70 */ SVECTOR3 rot;
    /* 0x76 */ s16      pad_76;
    /* 0x78 */ s32      angle;
    /* 0x7C */ s32      dist;
    /* 0x80 */ s32      pad_80;
} GpPitchScratch;
STATIC_ASSERT_SIZEOF(GpPitchScratch, 0x84);

/// Argument for `func_801052B8`. `field_0` is copied onto
/// `GameActor.field_93E`; `field_4` is copied onto `GameActor.field_934`.
typedef struct _GpCountArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ s32  field_4;
} GpCountArg;
STATIC_ASSERT_SIZEOF(GpCountArg, 8);

/// Animation data pointed to by `Gp_PlayerAnimBlkTbl` / `Gp_AnimBlkTbl` /
/// `GameActor.field_928`. `Gp_CopyPlayerAnim` and `Gp_CopyAllyAnim` copy up to
/// 0x20 words onto `field_BC`.
typedef struct _GpAnimBlk {
    /* 0x00 */ byte pad_0[0x1C];
    /* 0x1C */ s32  field_1C; // copied out by func_actor_503500_80143AC0 before message 0x3FF
    /* 0x20 */ byte pad_20[0x4];
    /* 0x24 */ s32  field_24;
    /* 0x28 */ byte pad_28[0x94];
    /* 0xBC */ s32  field_BC[0x20];
} GpAnimBlk;

/// 4-byte pad-event template indexed by `func_801041FC`. `field_0` / `field_2`
/// are passed to `Pad_PostEvent` (`lbu` / `lh`).
typedef struct _GpPadEvt {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} GpPadEvt;
STATIC_ASSERT_SIZEOF(GpPadEvt, 0x4);

/// The tasks of the two player-side actors, whose `work` is each actor's
/// `GameActor`. Slot 0 is the player's: `Gp_InitPlayerWork` claims it and
/// `Gp_TeardownSlot0` releases it. Slot 1 is the companion's, which that
/// actor's set-up claims the same way. The node and lock-on helpers walk
/// both slots.
extern Task* Gp_ActorSlots[2];

/// Flag byte cleared by `func_800A7DE0` / `Gp_SpawnPlayer`.
extern u8 D_80115768;

/// Four-entry `Task::state` dispatcher: `Gp_InitPlayerWork`, `Gp_PlayerWorkState1`,
/// `Gp_PlayerWorkState2`, `Gp_TeardownSlot0`.
extern TaskFuncTable4 Gp_PlayerWorkStates;

/// Four-entry `Task::state` dispatcher: `func_8010B590`, `func_8010B5C0`,
/// `func_8010B5E4`, `func_8010B5F0`.
extern TaskFuncTable4 D_80097AB0;

/// `Player_Status.weapon` dispatcher copied by `func_8010615C`. Unused
/// slots are `func_801065A0`; others are weapon-overlay entry points.
extern TaskFuncTable33 D_800978BC;

/// `field_954` dispatcher: `Gp_TickPlayerNormal`, `Gp_TickPlayerMode1`, `Gp_TickPlayerMode2`.
extern TaskFuncTable3 Gp_PlayerModeFns;

/// `field_956` dispatcher copied by `Gp_TickPlayerNormal`.
extern TaskFuncTable8 D_8009794C;

/// `field_96C` dispatcher: three slots of `Gp_PlayerMode1State0`, then `Gp_PlayerMode1State3`.
extern TaskFuncTable4 Gp_PlayerMode1States;

/// `field_956` dispatcher copied by `Gp_TickPlayerMode2`.
extern TaskFuncTable12 Gp_PlayerMode2States;

/// u8 Task_Spawn type bases. `func_80104258` indexes
/// `D_80112DFC[arg2 + Player_Status.field_26 - 2]`.
extern u8 D_80112DFC[];

/// Pad-event templates for `func_801041FC` (`D_80112E28[arg1 & 0xFFFF]`).
extern GpPadEvt D_80112E28[];

/// 2-wide rows indexed by `Mc_SaveData.characterId`. `Gp_PlayerMode2StateB` passes
/// `D_80112E04[field_22][1]` to `func_80105894`.
extern u8 D_80112E04[][2];

/// s16 scale rows indexed by `GameActor.field_958`. `Gp_StepPlayerMove` divides
/// the normalized matrix-column by `D_80112E10[field_958]`.
extern s16 D_80112E10[];

/// u16 facing-step rows indexed by `GameActor.field_95A`. `Gp_TurnPlayer`
/// adds `D_80112E20[field_95A] * field_975` onto `field_52` (masked `0xFFF`).
extern u16 D_80112E20[];

/// 2-wide rows of `GsCOORDINATE2` indices. `func_8010403C` indexes
/// `D_80112E2C[Mc_SaveData.characterId - 1][arg0]`.
extern u8 D_80112E2C[][2];

/// u16 turn-rate rows indexed by `Player_Status.weapon`. `Gp_AimYawToLock`
/// clamps the wrapped yaw delta to this value (or 1.5x when
/// `func_800B9D80(0x2000)` is set).
extern u16 D_80112E30[];

/// NULL-terminated `GpImgRec*` lists for `func_801030CC`. Indexed as
/// `table[type * 4 + Player_Status.field_26 - 5][frame]`. `D_80112E74` is
/// the `field_987` sequence; `D_80112EB4` is the `field_98A` sequence.
extern struct _GpImgRec** D_80112E74[];
extern struct _GpImgRec** D_80112EB4[];

/// Per-item flag byte indexed by `Player_Status.weapon`. Nonzero makes
/// `Gp_PlayerNormalState2` / `Gp_PlayerMode2StateA` pass `GameActor.field_97F` (the current
/// aim direction) to `func_80106264` instead of the default 1.
extern u8 D_80112EF8[];

/// 2-wide rows indexed by `Player_Status.weapon`. Zero at `[i][0]`
/// makes `func_801088D4` abort the item-use path (`field_95E = 0x3E8`).
extern u8 D_80112F1C[][2];

/// u16 table indexed by `Gp_AttachActorObj` arg1: the reach a weapon of that
/// attach id adds to the shape's `end1` to give its `end0`.
extern u16 D_80112F60[];

/// `D_80112F60[0x17]`, the row for attach id 0x17. Spelled out for the same
/// reason as `D_80112F94`; `func_gunblade_8011E040` is the only user.
extern u16 D_80112F8E;

/// `D_80112F60[0x1A]`, the row for attach id 0x1A. The weapons overlays import
/// the entry under its own address rather than the table's, so it is spelled
/// out here; `func_m4a1_bayonet_8011DA34` is the only user.
extern u16 D_80112F94;

/// 0x10-byte `VECTOR` rows indexed by `Gp_AttachActorObj` arg1: where the
/// weapon of that attach id sits on the actor. Copied through scratch; the low
/// 16 bits of `vx`/`vy`/`vz` seed the shape's `end1`.
extern VECTOR D_80112FA4[];

/// Overlay-imported s16 table indexed by `Mc_SaveData.companionVariant` and passed
/// to `func_80106350` (`func_8010C46C` / `func_8010C4F0` / `func_8010C75C`).
extern s16 D_80167218[];

/// Overlay-imported s16 table indexed by `Mc_SaveData.companionVariant` and passed
/// as the third argument of `Gp_AttachActorObj` (`Gp_SetupAllyWeapon`).
extern s16 D_80167224[];

/// Overlay-imported u8 table indexed by `Mc_SaveData.companionVariant` and stored
/// at `GpActorD4.actionCount` (`Gp_SetupAllyWeapon`).
extern u8 D_80167230[];

/// 8-byte `GpAimRot` rows copied onto `GpPitchScratch.rot`.
extern GpAimRot D_801131B4[];

/// u8 table indexed by `Mc_SaveData.companionVariant`. Non-zero selects
/// `Gp_AimPitchToLock`; zero uses `D_80167218` with `Gp_AimPitchRec`.
extern u8 D_80113388[];

void Gp_EffPolyTask9C(Task* arg0);
void Gp_DrawEffShard(struct _GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3);
void Gp_EffSprTask46(Task* arg0);
void Gp_DrawEffSprite81(Task* arg0);
void Gp_DrawEffSprite46(struct _GsCOORDINATE2* arg0, s32 arg1, s16 arg2, u16 arg3);
void Gp_EffSprTask81(Task* arg0);
void Gp_EffSprTask55(Task* arg0);
void Gp_EffSprTask42(Task* arg0);
void func_800F91AC(Task* arg0);
void Gp_EffCtlTask9B(Task* arg0);
void Gp_EffSprTask30(Task* arg0);
void Gp_DrawEffSpark(Task* arg0, s32 arg1, u8* arg2);
void Gp_DrawEffQuadT29(struct _GsCOORDINATE2* arg0, s32 arg1, u16 arg2, u16 arg3);
void Gp_EffTask07State1(Task* arg0);
void Gp_EffCtlTaskC1(Task* arg0);
void Gp_EffCtlTaskF3(Task* arg0);
void Gp_EffCtlTaskF4(Task* arg0);
void Gp_EffCtlTaskAC(Task* arg0);
void Gp_EffCtlTask0E(Task* arg0);
void Gp_PulseState1C80(void);
void Gp_EffCtlTaskA5(Task* arg0);
void Gp_EffCtlTaskA6(Task* arg0);
void Gp_EffCtlTaskE3(Task* arg0);
void func_800FDB18(s32 arg0, struct _GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void func_800FF710(Task* arg0);
void Gp_EffSprTaskE0(Task* arg0);
void Gp_EffSprTaskE2(Task* arg0);
/// Hand-written GTE routine. `arg2` is a full 32-bit word: the high half picks
/// the CLUT (palette column) and the low 12 bits are the billboard size, so it
/// must not be declared `s16` (that makes callers emit a spurious `sll`/`sra`
/// truncation). It is unsigned because the size is divided by `otz` with `divu`.
void  Gp_DrawEffSpriteE2(struct _GsCOORDINATE2* arg0, u16 arg1, u32 arg2, s16 arg3);
s32   func_801011D0(struct _GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32* arg3);
void  Gp_InitPlayerWork(Task* arg0);
void  Gp_AttachActorObj(Task* arg0, s32 arg1, s32 arg2);
void  Gp_TeardownSlot0(Task* arg0);
void  Gp_BindActorAnim(Task* arg0);
void  Gp_AnimPlayChildSlotsEx(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
Task* func_80104258(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
/// `arg3` is unused; the actor-init caller passes 0 so the `jal` delay
/// slot of the `field_93A` load is `move a3, a1`.
s32  func_80104508(Task* arg0, s32 arg1, GpAnimArg* arg2, s32 arg3);
void func_801030CC(Task* arg0);
void func_801041FC(Task* arg0, s32 arg1);
s32  Gp_SpawnWeaponEff(void);
s32  Gp_SetupAllyWeapon(void);
void func_80106350(Task* arg0, s32 arg1, s32 arg2);
void func_801088D4(Task* arg0, s32 arg1, s32 arg2);
/// Overlay import. `func_801088D4` calls it with `gameGetPtrSlot(0xA)` when
/// `Mc_SaveData.companionType == 1`.
void  func_80166E94(void* arg0, s32 arg1);
void  Gp_PlayerMode1State0(Task* arg0);
s32   func_80109290(Task* arg0);
void  Gp_TriggerPeState(s32 arg0, s32 arg1);
void  func_8010A42C(Task* arg0, s32 arg1);
void  Gp_DetachLinkNode(Task* arg0);
s32   Gp_ApplyDirArg(Task* arg0, GpMoveArg* arg1);
s32   func_80104E00(Task* arg0, s32 arg1, GpXformArg* arg2);
s32   Gp_SetActorDest(Task* arg0, s32 arg1, GpXformArg* arg2, GpOverrideArg* arg3);
s32   Gp_MoveActorBy(Task* arg0, s32 arg1, GpMoveArg* arg2);
s32   Gp_PickNearestRec18(GpRec18* arg0, struct _GsCOORDINATE2* arg1, struct _GsCOORDINATE2* arg2);
void  Gp_MoveActorByKeep(Task* arg0, s32 arg1, GpMoveArg* arg2);
void  func_8010B210(Task* arg0);
void  Gp_BindActorD4(Task* arg0, SVECTOR3* arg1, s32 arg2);
s32   func_8010C30C(Task* arg0);
Task* Gp_SpawnPlayer(GpActorArg* arg0, u16 arg1, s32 arg2, GpActorFlags* arg3);
Task* Gp_SpawnAlly(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3);

// Functions defined in this module but not previously declared anywhere.
// Without a prototype m2c cannot type a call to them and the decompiled
// seed fails to compile ('invalid use of void expression') - the single
// largest cause of unusable seeds in the bulk m2c pass.

s32   func_80105894(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_80106238(Task* arg0, s32 arg1, s32 arg2);
void  Gp_AnimResetChildSlots(Task* arg0, s32 arg1);
void  func_80106550(Task* arg0);
void  Gp_AnimTickChildSlots(Task* arg0);
s32   func_80106264(s32 arg0);
void  Gp_ResetActorMove(Task* arg0, s16 arg1);
void  func_801066DC(Task* arg0, s16 arg1);
s16   func_80103E7C(s16 arg0, s16 arg1);
void  Gp_StepPlayerMove(Task* arg0);
s32   Gp_KillPlayerEffs(void);
void  Gp_TurnPlayer(Task* arg0);
s32   func_801060E0(Task* arg0);
void  func_80103C74(GsCOORDINATE2* arg0, VECTOR3* arg1, VECTOR3* arg2);
s32   func_80103D8C(s32 arg0, s32 arg1);
void  Gp_AnimPlayChildSlots(Task* arg0, s32 arg1, s32 arg2);
void  Gp_TickActorAnimState(Task* arg0);
void  Gp_TrackLockTarget(Task* arg0);
void  Gp_ResetActorAnimState(Task* arg0, s32 arg1);
void  Gp_StopPlayerAnim(Task* arg0, s32 arg1);
void  func_8010BF7C(Task* arg0, s32 arg1, s32 arg2);
void  func_80109374(Task* arg0);
s32   Gp_ApplyHpDamage(s32 arg0);
void  func_80109BB4(Task* arg0, GpRec18* arg1);
void  func_8010BFCC(Task* arg0);
Task* func_80104490(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_80106518(s32 arg0);
void  func_8010B9A4(Task* arg0);
void  Gp_TrackAllyLockTarget(Task* arg0, s32 arg1);
void  func_8010615C(Task* arg0);
void  func_801095BC(s32* arg0);
void  Gp_PlayerStepSfx(Task* arg0);
void  Gp_UpdateLockTarget(Task* arg0);
void  Gp_EndPlayerActorTask(Task* arg0);
Task* func_80104364(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
s32   func_801041B4(Task* arg0);
s32   func_8010583C(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_80108684(Task* arg0);
void  func_8010AAB4(Task* arg0);
void  Gp_AimYawToLock(Task* arg0, s32 arg1);
void  Gp_AimPitchToLock(Task* arg0);
void  Gp_PlayerMode2State4(Task* arg0);
s32   Gp_EnterActorMode2(Task* arg0, s32 arg1, s32 arg2);
void  func_8010A9D0(Task* arg0);
s32   Gp_HurtAlly(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_8010B2A0(s32 arg0, s32 arg1);
s32   func_8010C058(void);
void  func_8010C180(Task* arg0);
void  func_801061F0(void);
void  func_80105B0C(Task* arg0);
void  func_80105B74(VECTOR3* arg0);
void  Gp_TickPlayerActor(Task* arg0);
void  func_8010AC54(Task* arg0);
void  func_80109A1C(Task* arg0);
void  func_8010AD64(Task* arg0);
void  func_80109844(Task* arg0);
s32   func_80104B54(Task* arg0, s32 arg1, GpAnimArg* arg2);
void  func_8010B2D4(Task* arg0, GpIdRec* arg1, s32 arg2);
void  func_8010B348(Task* arg0, GpIdRec* arg1, s32 arg2);
s32   func_801062DC(Task* arg0, s32 arg1);
void  Gp_AimPitchRec(Task* arg0, s32 arg1, s32 arg2);
void  func_80104A4C(Task* arg0);
void  func_80109720(Task* arg0);
s32   func_801055D4(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void  Gp_AimPitchToLockAlt(Task* arg0);
void  func_8010A670(Task* arg0);
void  func_80109250(Task* arg0);
void  func_80109210(Task* arg0);
void  func_80109FC4(Task* arg0);
void  func_801065A8(Task* arg0);
void  func_80108620(Task* arg0);
s32   Gp_HpBand(void);

#endif // GAMEPLAY_3FB8_H
