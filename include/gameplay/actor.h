#ifndef GAMEPLAY_ACTOR_H
#define GAMEPLAY_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/geometry.h"
#include "gameplay/world_collision_types.h"

#include "main/coord.h"
#include "main/session_types.h"

/// Probe sweep step in angle units and clearance-distance sentinels.
enum {
    COMPANION_SCAN_UNTESTED   = -1,
    COMPANION_SCAN_CLEAR      = 0,
    COMPANION_SCAN_ANGLE_STEP = 0x80
};

/// Companion behavior state and its forward collision probe.
///
/// `GameActor.companionWork` owns this separately allocated, zeroed primary-heap
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
/// `GameActor.rotation.vy`. `vec` is the target-minus-current offset
/// (`GameActor.destination` minus `GfxCoord.coord.t`).
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
    WorldCollisionDelta delta;
    GfxCoord            coord;
    SVECTOR             offset;
} GpPickScratch;
STATIC_ASSERT_SIZEOF(GpPickScratch, 0x68);

#endif // GAMEPLAY_ACTOR_H
