#ifndef GAMEPLAY_WORLD_COLLISION_TYPES_H
#define GAMEPLAY_WORLD_COLLISION_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// Contact-table flags and packed-key categories; LAST also marks an empty final element.
enum {
    WORLD_COLLISION_BODY_SINGLE_CONTACT      = 0x800,
    WORLD_COLLISION_CONTACT_OCCUPIED         = 1,
    WORLD_COLLISION_CONTACT_LAST             = 2,
    WORLD_COLLISION_CONTACT_BODY_INDEX_MASK  = 0xF0,
    WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT = 4,
    WORLD_COLLISION_CONTACT_PLAYER_BODY      = 0x10000, // Player or companion body
    WORLD_COLLISION_CONTACT_ATTACK           = 0x20000, // Weapon or other attack that can hit enemy bodies
    WORLD_COLLISION_CONTACT_ENEMY_BODY       = 0x30000, // Enemy body, body part or detached boss chunk
    WORLD_COLLISION_CONTACT_GRID             = 0x100000,
    WORLD_COLLISION_CONTACT_GRID_FLOOR       = 0x100100,
    WORLD_COLLISION_CONTACT_GRID_EDGE        = 0x200
};

/// Mask selecting a packed contact key's high halfword; keeps unsigned word arithmetic.
#define WORLD_COLLISION_CONTACT_KIND_MASK 0xFFFF0000

/// Response half of a collision contact: a direction, or the contacted body's address.
///
/// `direction` is the usual reading. A grid contact holds the face's normal as
/// the room's grid stores it. A sphere touched by a capsule holds the capsule's
/// axis, pointing from the capsule's second endpoint to its first. Both use
/// 4096 for one unit. Contacts with no direction - sphere pairs, and a capsule's
/// own entry for a sphere it touched - hold zero in all three components.
///
/// `bodyAddress` takes the direction's place in one case: the entry a capsule
/// with `WORLD_COLLISION_BODY_SINGLE_CONTACT` receives for a sphere it touched.
/// It is that sphere's body, kept so that the capsule's next pair contact can
/// release the reciprocal entry in the sphere's table before replacing this
/// one. The third halfword stays zero. Nothing in the entry marks this reading:
/// it is assumed of the occupied non-grid entry a single-contact body holds.
///
/// `direction.pad` carries no value. Component-wise writers and clears leave
/// it alone, and a pair contact copies whatever its scratch block held there.
typedef union {
    SVECTOR direction; // Unit direction (4096 per unit): grid face normal or capsule axis; zero when the contact has none
    struct {
        u16 low;       // Bits 0..15 of the contacted body's address
        s16 high;      // Bits 16..31; signed storage, masked back to 16 bits on decoding
    } bodyAddress;
} WorldCollisionContactResponse;
STATIC_ASSERT_SIZEOF(WorldCollisionContactResponse, 8);

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
/// body's address as two halfwords in `response.bodyAddress` instead. That body must
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
    SVECTOR                       point;    // World contact position or other body's centre; zero for normal-only grid overlaps
    WorldCollisionContactResponse response; // Direction or encoded body address; see the type
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

/// Collision-body kinds and flags stored together in one unsigned halfword.
///
/// KIND_MASK selects the context interpretation; bits 4..7 are the receiving
/// body index (0..15) copied to contact flags. FLOOR_QUERY adds the motion sphere's
/// vertical floor test. CLIP_TO_GRID_CONTACT shortens a capsule at its grid
/// contact; SINGLE_CONTACT also clips it at a pair contact and replaces the
/// first contact while retaining the other body's encoded address.
/// ROOM_TRIGGER_ENABLED tests room-transition quads, and VIEW_TRIGGER_ENABLED
/// tests saved-view quads.
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

    /// Enables a body's shape tests in the list-driven room-grid pass.
    ///
    /// Tests run only with a room grid installed. Supply initialized shape and
    /// contact storage for the body's kind.
    /// Spheres, capsules and motion spheres have grid tests; kinds NONE and
    /// CONTACT_PROXY do not. A motion sphere with FLOOR_QUERY runs its floor
    /// test before its directed overlap test. This bit is independent of the
    /// pair and trigger enables. Setting it does not link the body; clearing it
    /// keeps links and existing contacts intact. Direct shape tests bypass this bit.
    WORLD_COLLISION_BODY_GRID_ENABLED = 0x4000,
    /// Enables a body in list-driven pair tests and room/view trigger scans.
    ///
    /// Pair tests require this bit on both bodies and kinds 1..4; kind 2
    /// currently has no pair handler. Trigger scans additionally require a
    /// motion sphere and the corresponding trigger-enable bit. Clearing this
    /// bit leaves the body linked and existing contacts intact; grid tests
    /// use `WORLD_COLLISION_BODY_GRID_ENABLED` independently.
    WORLD_COLLISION_BODY_PAIR_ENABLED = 0x8000
};

/// All storage bits of a collision body's 16-bit kind-and-flags word.
///
/// Includes the kind, list membership, receiving-body index and pass options.
/// XOR with bits drawn from this word produces a nonnegative int clearing mask
/// that preserves every other stored bit. Used alone, it preserves the whole
/// word when a body-flags update only sets bits.
enum { WORLD_COLLISION_BODY_FLAGS_MASK = 0xFFFF };

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

#endif // GAMEPLAY_WORLD_COLLISION_TYPES_H
