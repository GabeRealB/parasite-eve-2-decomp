#ifndef GAMEPLAY_ACTOR_H
#define GAMEPLAY_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/geometry.h"
#include "gameplay/world_collision_types.h"

#include "main/coord.h"
#include "main/session_types.h"

/// Collision-body kinds and flags stored together in one unsigned halfword.
///
/// KIND_MASK selects the context interpretation; bits 4..7 are the receiving
/// body index (0..15) copied to contact flags. FLOOR_QUERY adds the motion sphere's
/// vertical floor test. CLIP_TO_GRID_CONTACT shortens a capsule at its grid
/// contact; SINGLE_CONTACT also clips it at a pair contact and replaces the
/// first contact while retaining the other body's encoded address.
/// ROOM_TRIGGER_ENABLED tests room-transition quads, and VIEW_TRIGGER_ENABLED
/// tests saved-view quads. GRID_ENABLED and PAIR_ENABLED gate the collision
/// passes independently. FLAGS_MASK preserves the width of explicit masks.
enum {
    WORLD_COLLISION_BODY_NONE                 = 0,
    WORLD_COLLISION_BODY_SPHERE               = 1,
    WORLD_COLLISION_BODY_CONTACT_PROXY        = 2,
    WORLD_COLLISION_BODY_CAPSULE              = 3,
    WORLD_COLLISION_BODY_MOTION_SPHERE        = 4,
    WORLD_COLLISION_BODY_KIND_MASK            = 7,
    WORLD_COLLISION_BODY_LINKED               = 8,
    WORLD_COLLISION_BODY_FLOOR_QUERY          = 0x200,
    WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT = 0x400,
    WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED = 0x1000,
    WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED = 0x2000,
    WORLD_COLLISION_BODY_GRID_ENABLED         = 0x4000,
    WORLD_COLLISION_BODY_PAIR_ENABLED         = 0x8000,
    WORLD_COLLISION_BODY_FLAGS_MASK           = 0xFFFF
};

/// A borrowed collision body linked into one of the world's object lists.
///
/// `pos` is a signed local offset in game-coordinate units. Spheres use it as
/// their centre; capsules add it to both local endpoints. The cached `coord`
/// transform determines the collision calculation's composition space.
/// `key` is the packed contact category in the high halfword and identity in
/// the low halfword; zero suppresses recording this body in pair contacts.
///
/// Kind 0 has no contact storage, 1 is a sphere with a direct table, 2 borrows
/// a direct-table body's contacts, 3 supplies capsule/segment geometry and its
/// table, and 4 is a sphere with motion direction and contacts. Kind 2 has no
/// grid or pair test in the current dispatch tables. PAIR_ENABLED requires
/// kind 1..4. Contact tables retain
/// their final-entry marker and may be shared by several bodies.
///
/// Owners initialize the body, context and contact storage before linking,
/// and keep all borrowed pointers alive until unlinking. Unlinking clears
/// every flag except the kind, so pass enables and the body index must be
/// restored before reuse. A SINGLE_CONTACT result retaining this body's
/// address additionally requires it to stay alive until that result is reset.
typedef struct WorldCollisionBody {
    struct WorldCollisionBody*  next;              // Next body on the list; NULL at the tail
    struct WorldCollisionBody** prev;              // Link containing this body: list head or preceding body's next
    GfxCoord*                   coord;             // Borrowed transform for the local offset and shape
    union {
        WorldCollisionContact*       contacts;     // Kind 1: initialized contact table
        struct WorldCollisionBody*   contactOwner; // Kind 2: body whose context.contacts supplies the table
        WorldCollisionCapsule*       capsule;      // Kind 3: local endpoints, end radii and contacts
        WorldCollisionMotionContext* motion;       // Kind 4: motion direction and contact table
    } context;                                     // Borrowed payload selected by flags & KIND_MASK
    SVECTOR pos;                                   // Local sphere centre or capsule origin, in game-coordinate units
    s32     key;                                   // Packed contact category << 16 | identity; 0 omits pair-contact recording
    u16     radius;                                // Sphere/trigger radius and floor-query half-height, in game-coordinate units
    u16     flags;                                 // Kind, LINKED, body index and independent pass options; see above
} WorldCollisionBody;
STATIC_ASSERT_SIZEOF(WorldCollisionBody, 0x20);

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
    /* 0x68 */ WorldCollisionBody    obj;           // the body: a kind-3 node whose `context.capsule` is `shape`
    /* 0x88 */ WorldCollisionCapsule shape;         // the capsule the body's collisions are tested with
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
