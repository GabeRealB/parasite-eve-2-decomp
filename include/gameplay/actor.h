#ifndef GAMEPLAY_ACTOR_H
#define GAMEPLAY_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

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

/// Scratch-stack block for a companion actor's per-frame movement step.
///
/// Every companion package runs that step once per frame: it reserves one
/// block, stages the heading the three `GameActor.collisionMotionContexts`
/// records are given, draws the ground shadow, and releases the block before
/// returning. The player's own movement step stages only the heading, in a
/// bare `SVECTOR`.
///
/// The heading is the root coordinate's Z axis scaled by
/// `GameActor.movementSign`, or `GameActor.pushbackDirection` while
/// `GameActor.usesPushbackDirection` is set. The shadow centre is written only
/// when a floor is found under the model; a package that centres the shadow
/// on its root coordinate instead leaves it untouched.
typedef struct {
    VECTOR3 shadowCentre;    // Floor point under the model that the ground shadow is centred on, in world coordinate units
    byte    field_C[4];      // Never accessed; role unproven
    SVECTOR motionDirection; // Movement or push-back heading for this frame; 4096 per unit
} CompanionMoveScratch;
STATIC_ASSERT_SIZEOF(CompanionMoveScratch, 0x18);

#endif // GAMEPLAY_ACTOR_H
