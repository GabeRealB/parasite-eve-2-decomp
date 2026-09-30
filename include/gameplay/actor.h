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

/// Probe sweep step in angle units and clearance-distance sentinels.
enum {
    COMPANION_SCAN_UNTESTED   = -1,
    COMPANION_SCAN_CLEAR      = 0,
    COMPANION_SCAN_ANGLE_STEP = 0x80
};

/// Companion behavior state and its forward collision probe.
///
/// `GameActor.field_910` owns this separately allocated, zeroed primary-heap
/// block; a NULL pointer identifies an ordinary actor. The armed and scripted
/// companions bind the probe; the noncombatant leaves it unbound. Its embedded
/// transform, capsule and single-result table must stay alive while the body
/// is linked. A zero contact key means the probe has no recorded obstruction.
///
/// Angles use 4096 units per turn. The sweep chooses the greatest planar contact
/// distance, preferring a direction with no contact. `activity.combat` holds
/// action repetitions and a weapon's remaining attack allowance;
/// `activity.distress` holds the noncombatant's flinch interval and count.
/// Keep the byte storage and explicit signed comparisons: the initial interval
/// is 0x96, and interpreting it as s8 is part of the behavior.
typedef struct CompanionWork {
    byte unknown_0[0x18];                  // Zeroed allocation bytes; role unproven
    struct {
        GfxCoord              coord;       // Actor model transform; armed companion additionally rotates it by scanAngle
        WorldCollisionBody    body;        // Linked capsule body borrowing this probe's transform and shape
        WorldCollisionCapsule shape;       // Forward segment and radii, with contacts pointing to the table below
        WorldCollisionContact contacts[1]; // Single result; key 0 means no obstruction, LAST terminates the table
    } probe;                               // Collision storage used to detect obstructions ahead of the companion
    byte unknown_B8[0xC];                  // Zeroed allocation bytes; role unproven
    s16  decisionTimer;                    // Active behavior ticks left before the next idle decision
    s16  scanAngle;                        // Relative probe yaw (0..4096); 4096 marks the completed sweep
    s16  targetHeading;                    // Selected relative yaw during scanning, absolute yaw (0..4095) when turning
    s16  scanClearance;                    // Best planar distance in game units (-1 untested, 0 clear direction)
    union {
        struct {
            u8 repeatsRemaining; // Repetitions left in the current action burst; stop checks use s8
            u8 attacksRemaining; // Weapon attacks left before refresh; stop checks use s8
        } combat;
        struct {
            u8 flinchInterval; // Interval byte compared as s8; initially 0x96, later reset to 60
            u8 flinchCount;    // Flinches taken; signed comparison at 5 selects the severe reaction
        } distress;
    } activity;                // Counter interpretation selected by the companion kind
    s8   waypointIndex;        // Index into the current scripted route; must be within that table
    s8   turnDir;              // Signed turn direction (-1 negative yaw, +1 positive yaw)
    s8   routeComplete;        // Scripted route completion latch (0 pending, 1 complete)
    byte unknown_D1[3];        // Zeroed allocation bytes; role unproven
} CompanionWork;
STATIC_ASSERT_SIZEOF(CompanionWork, 0xD4);

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
