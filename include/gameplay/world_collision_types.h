#ifndef GAMEPLAY_WORLD_COLLISION_TYPES_H
#define GAMEPLAY_WORLD_COLLISION_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// Contact-table flags and packed-key categories; LAST also marks an empty final element.
enum {
    WORLD_COLLISION_BODY_SINGLE_CONTACT      = 0x800,
    WORLD_COLLISION_CONTACT_OCCUPIED         = 1,
    WORLD_COLLISION_CONTACT_LAST             = 2,
    WORLD_COLLISION_CONTACT_BODY_INDEX_MASK  = 0xF0,
    WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT = 4,
    WORLD_COLLISION_CONTACT_GRID             = 0x100000,
    WORLD_COLLISION_CONTACT_GRID_FLOOR       = 0x100100,
    WORLD_COLLISION_CONTACT_GRID_EDGE        = 0x200
};

/// Mask selecting a packed contact key's high halfword; keeps unsigned word arithmetic.
#define WORLD_COLLISION_CONTACT_KIND_MASK 0xFFFF0000

/// One result in a body's collision-contact table.
///
/// The body owner supplies and initializes the storage before linking the body.
/// OCCUPIED marks an entry in use and LAST marks the table's final element;
/// some consumers also stop at a zero key. Pair contacts retain the receiving
/// body's index in flags bits 4..7. The key copies the contacted body's identity,
/// or combines grid kind 0x10 with the room's surface class and response bits
/// (0 ordinary overlap, 1 floor distance, 2 edge overlap in bits 8..11).
///
/// Positions and distances use world-coordinate units, truncated to signed
/// halfwords. Grid normals and capsule-axis directions use 4096 for one unit.
/// A capsule with `WORLD_COLLISION_BODY_SINGLE_CONTACT` stores the contacted
/// body's address as two halfwords in `response.node` instead. That body must
/// remain alive while the address is consumed. Sphere-pair contacts use a zero response vector.
typedef struct {
    u16 flags;        // OCCUPIED, LAST and receiving-body index; see above
    s16 distance;     // Grid penetration, floor travel distance, summed sphere radii, or 0 for capsule/segment hits
    union {
        s32 value;    // Packed identity; 0 is the no-key sentinel
        struct {
            u16 id;   // Body identity, or room surface class plus grid response bits
            u16 kind; // Contact category (body/interaction category, or 0x10 room grid)
        } parts;
    } key;
    SVECTOR point;      // World contact position or other body's centre; zero for normal-only grid overlaps
    union {
        SVECTOR normal; // Grid normal or capsule-axis direction, with 4096 representing one unit
        struct {
            u16 low;    // Low halfword of the contacted body's address
            s16 high;   // High halfword, retained signed for the address reconstruction
        } node;
    } response;
} WorldCollisionContact;
STATIC_ASSERT_SIZEOF(WorldCollisionContact, 0x18);

/// Segment-and-radii shape carried by a kind-3 collision body.
///
/// `ends[0]` and `ends[1]` are offsets from the body's position, in the body's
/// own frame, so the shape moves with it. The grid walk uses the XZ of both
/// endpoints; the hit test uses the full segment. Pair passes test the cylinder
/// that segment and the two end radii describe: a capsule when the radii are
/// equal, and a taper between them when they are not. Endpoint components and
/// radii are game-coordinate units.
///
/// `contacts` is the table the passes fill as this body makes contacts. The
/// owner initializes it, including its final-entry flag, and keeps it alive
/// until the body is unlinked. Several bodies may share one table. A weapon
/// re-arms the shape as it unfolds, moving one endpoint out along its reach
/// and widening that end's radius with the spread.
typedef struct {
    SVECTOR                ends[2];    // Local segment endpoints, [0] then [1]
    s16                    end0Radius; // Radius at ends[0], in game-coordinate units
    s16                    end1Radius; // Radius at ends[1]; equal to end0Radius unless the cylinder tapers
    WorldCollisionContact* contacts;   // Borrowed contact table, terminated by the final-entry flag
} WorldCollisionCapsule;
STATIC_ASSERT_SIZEOF(WorldCollisionCapsule, 0x18);

/// Motion direction and contact storage for a kind-4 spherical collision body.
///
/// The direction uses the body's cached transform's composition space, with
/// 4096 representing one unit. It is the forward axis times the movement sign
/// (zero when stopped), or the normalized push-back direction. Grid passes use
/// it to filter surfaces and select the floor-query footprint.
///
/// The body borrows this context and an initialized contact table whose final
/// entry has `WORLD_COLLISION_CONTACT_LAST` set. Multiple bodies can share the
/// table. The owner keeps both alive until the bodies are unlinked.
typedef struct {
    SVECTOR                motionDirection; // Movement or push-back heading; 4096 per unit
    WorldCollisionContact* contacts;        // Borrowed table, terminated by the final-entry flag
} WorldCollisionMotionContext;
STATIC_ASSERT_SIZEOF(WorldCollisionMotionContext, 0xC);

#endif // GAMEPLAY_WORLD_COLLISION_TYPES_H
