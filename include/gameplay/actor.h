#ifndef GAMEPLAY_ACTOR_H
#define GAMEPLAY_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/geometry.h"
#include "gameplay/world_collision_types.h"

#include "main/coord.h"
#include "main/session_types.h"

/// One body an actor puts on the world's object lists: a sphere of `radius`
/// whose centre is `pos`, a local offset under `coord`, and which the contacts
/// it takes part in name by `key`.
///
/// `flags` bits 0-2 select what `ctx` points at, which is how the collision
/// passes reach the `WorldCollisionContact` table recording that body's contacts: 0 nothing,
/// 1 the table itself, 2 a node whose own table is used, 3 the `GpActorD4Rec`
/// shape the body carries, 4 a `WorldCollisionMotionContext`. Bit 3 marks a node sitting on a `Gp_ObjLists` list, bit
/// 0x800 makes the contacts it produces name the node instead of a direction,
/// and bits 0x4000 and 0x8000 enable the grid and pair passes, which skip a
/// node whose bit is clear.
typedef struct _GpObj {
    struct _GpObj*  next;                   // next on the list
    struct _GpObj** prev;                   // address of the preceding next link
    GfxCoord*       coord;                  // transform `pos` is an offset under
    union {
        WorldCollisionContact*       recs;  // kind 1: the body's own contact table
        struct _GpObj*               node;  // kind 2: the node whose table is used
        GpActorD4Rec*                d4rec; // kind 3: the shape the body carries
        WorldCollisionMotionContext* dir;   // kind 4: motion direction and contact table
    } ctx;                                  // the body's collision context; see the kind bits
    SVECTOR pos;                            // centre, in the `coord` frame
    s32     key;                            // identity in the contact records: class << 16 | id
    u16     radius;                         // collision radius
    u16     flags;                          // kind, list membership and pass enables; see above
} GpObj;
STATIC_ASSERT_SIZEOF(GpObj, 0x20);

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
    /* 0x00 */ byte                  pad_0[0x18];
    /* 0x18 */ GfxCoord              coord;         // the body's transform, a copy of the actor's model coordinate
    /* 0x68 */ GpObj                 obj;           // the body: a kind-3 node whose `ctx.d4rec` is `shape`
    /* 0x88 */ GpActorD4Rec          shape;         // the capsule the body's collisions are tested with
    /* 0xA0 */ WorldCollisionContact contact;       // the one-entry table `shape` records its contacts in
    /* 0xB8 */ byte                  pad_B8[0xC];
    /* 0xC4 */ s16                   decisionTimer; // frames left before the companion picks its next action
    /* 0xC6 */ s16                   scanAngle;     // sweep angle: 0x80 a tick, and past 0x1000 the sweep is over
    /* 0xC8 */ s16                   targetHeading; // heading being turned to, in the 0..0xFFF angle unit
    /* 0xCA */ s16                   scanDist;      // the contact distance the sweep compares its candidates by
    /* 0xCC */ u8                    repeatCount;   // swings left in the attack burst, or the flinch interval of the companion that does not fight
    /* 0xCD */ u8                    actionCount;   // attacks left before the fighting companions stop, or flinches taken by the other one
    /* 0xCE */ s8                    pathStep;      // waypoint the companion is walking to
    /* 0xCF */ s8                    turnDir;       // +1 or -1: the way it turns to `targetHeading`
    /* 0xD0 */ s8                    pathDone;      // 1 once the last waypoint is reached
    /* 0xD1 */ byte                  pad_D1[3];
} GpActorD4;
STATIC_ASSERT_SIZEOF(GpActorD4, 0xD4);

/// 0x14-byte scratch from the scratch stack used by `Gp_PlayerMode2State4`.
/// `field_0` is the clamped `func_80103E7C` turn delta applied to
/// `GameActor.field_52`. `vec` is the target-minus-current offset
/// (`GameActor.field_20/24/28` minus `GfxCoord.coord.t`).
typedef struct _GpApproachScratch {
    /* 0x00 */ s32     field_0;
    /* 0x04 */ VECTOR3 vec;
    /* 0x10 */ s32     pad;
} GpApproachScratch;
STATIC_ASSERT_SIZEOF(GpApproachScratch, 0x14);

/// Scratch-pad block for picking the nearest collision record. `delta`
/// receives the push-back of the record being classified, which is
/// discarded (only the record mask returned alongside it is used), `coord`
/// is the node the pick effect is spawned on, and `offset` a small random
/// jitter added to that position.
typedef struct _GpPickScratch {
    GpDeltaScratch delta;
    GfxCoord       coord;
    SVECTOR        offset;
} GpPickScratch;
STATIC_ASSERT_SIZEOF(GpPickScratch, 0x68);

#endif // GAMEPLAY_ACTOR_H
