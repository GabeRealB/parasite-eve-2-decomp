#ifndef GAMEPLAY_COLLISION_H
#define GAMEPLAY_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// One attack delivered through a collision body's key.
///
/// The packed identity has contact category 4 in the high halfword
/// (`DAMAGE_ATTACK_CATEGORY`), the low 12 bits of `power` below that, and the
/// low 4 bits of `reaction` in bits 12..15. Enemy parameter records and actor
/// spawn tables hold these entries, and several bodies may share one. A null
/// pointer packs as the zero key, which omits the body from pair contacts; a
/// stored `{ 0, 0 }` still packs as category 4.
///
/// `reaction` selects the player's hit reaction. 0 is the ordinary hit and
/// applies no status. 1 darkness, 2 paralysis, 8 silence, 10 confusion and
/// 11 berserker use that same presentation and also apply that status. 3
/// poison uses its own presentation and applies poison. 9 uses the ordinary
/// presentation and applies status flag 0x20, whose gameplay effect is
/// unproven. 4 sets the parasite-energy fade mask to 8, applies no status,
/// and has no hit presentation of its own. 5, 6 and 7 each use their own hit
/// presentation and set no status flag.
typedef struct DamageAttack {
    u16 power;    // Base damage before the HP-band scale; low 12 bits of the identity
    u16 reaction; // Hit reaction in the low 4 bits; values listed above
} DamageAttack;
STATIC_ASSERT_SIZEOF(DamageAttack, 0x4);

/// Low bits of `DamageAttack::power` copied into an attack identity.
#define DAMAGE_ATTACK_POWER_MASK 0xFFF
/// Low bits of `DamageAttack::reaction` copied into an attack identity.
#define DAMAGE_ATTACK_REACTION_MASK 0xF
/// Bit position of the reaction nibble in the identity halfword.
#define DAMAGE_ATTACK_REACTION_SHIFT 12
/// Packed attack identity's contact category: 4 in the high halfword.
#define DAMAGE_ATTACK_CATEGORY 0x40000

/// Fixed-damage hazard contact category, with its damage-table row in the low halfword.
#define DAMAGE_HAZARD_CATEGORY 0x50000

/// Geometry kinds and list state in `WorldCollisionTrigger::flags`.
///
/// View boundaries require movement against the face normal. Action quads
/// require sphere overlap on the negative side of the plane; kind 2 also
/// requires facing against `facingNormal`. Kind 4 accepts a centre distance
/// below 500 game units, otherwise requiring facing toward the origin and
/// quad overlap. All tests apply the broad-phase radius check.
/// Bits 3 and 4 have no observed consumers and are cleared along with list
/// state on unlink. Enabling/disabling a trigger does not clear its hit latch.
enum {
    WORLD_COLLISION_TRIGGER_KIND_MASK = 0x07,
    /// Identifies a directed quad requesting a view change within the current room.
    ///
    /// Value 1 occupies the kind bits of `WorldCollisionTrigger::flags`.
    /// Place the records in the room's view-boundary array, ending with LAST.
    /// Room setup binds their coordinate, links them to the view list and enables
    /// them. List membership selects the directed test; this kind adds no gate.
    /// The scan requires a motion sphere with PAIR_ENABLED and VIEW_TRIGGER_ENABLED
    /// set, and the session's view-trigger suppression must be clear.
    ///
    /// An enabled boundary latches a hit when the player moves against `normal`,
    /// passes the radius check about the origin and overlaps the quad's negative
    /// side. The sphere centre must project strictly inside all four edges, with
    /// computed signed plane distance in [-body radius, 0). The test uses current
    /// overlap and movement direction. Positions and radii use game units;
    /// normals use 4096 per unit.
    ///
    /// `parameter0` and `parameter1` are valid 1-based source and destination
    /// views in that room. `control` and `facingNormal` are unused by the view test.
    /// Consuming a hit clears its latch and, when the source matches the current
    /// session view, requests the destination in the live save's location.
    WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY = 1,
    /// An action quad requiring the body to face against its configured normal.
    ///
    /// The kind occupies `WorldCollisionTrigger::flags` bits selected by
    /// `WORLD_COLLISION_TRIGGER_KIND_MASK`. Supply `facingNormal` in the space
    /// of the querying body's coordinate parent, with 4096 per unit.
    /// Its dot product with column 2 (+Z) of the body's
    /// `coord->coord.m` must be <= -12582912: at least 3/4 opposing alignment
    /// for unit vectors. The gate uses both vectors without transformation or
    /// normalization. A hit also requires the broad-phase radius check,
    /// sphere overlap on the quad plane's negative side, and the body's
    /// projected centre strictly inside all four edges.
    WORLD_COLLISION_TRIGGER_FACING_QUAD         = 2,
    WORLD_COLLISION_TRIGGER_QUAD                = 3,
    WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD = 4,
    WORLD_COLLISION_TRIGGER_LINKED              = 0x20,
    WORLD_COLLISION_TRIGGER_ENABLED             = 0x40,
    /// Marks the final record included in a contiguous room-trigger array scan.
    ///
    /// Set this bit in the last element of each non-NULL view-boundary or action
    /// array. Room setup binds, links and enables that element before stopping;
    /// no separate count or dummy terminator is supplied. Linking, unlinking and
    /// clearing a list preserve the bit so its array can be bound again.
    /// Runtime lists terminate at a NULL `next`, independently of this marker.
    WORLD_COLLISION_TRIGGER_LAST             = 0x80,
    WORLD_COLLISION_TRIGGER_PERSISTENT_FLAGS = WORLD_COLLISION_TRIGGER_KIND_MASK | WORLD_COLLISION_TRIGGER_LAST,
};

/// Direction-action selectors and activation gates in a trigger's control word.
///
/// The low byte selects an action (0..6, or 255 to cancel). Bits 8..13 are
/// action-specific settings; bit 8 selects alternate facing/surface data for
/// action 1, and the upper byte masked with 0x7F selects callback 0 or 1 for
/// action 3. AUTOMATIC bypasses the interaction-button requirement;
/// OUTSIDE_BATTLE rejects activation during an engaged battle.
enum {
    WORLD_COLLISION_TRIGGER_ACTION_MASK = 0xFF,
    /// Requests a room-resolved warp within the active stage.
    ///
    /// This zero-valued selector occupies the low byte of
    /// `WorldCollisionTrigger::control`; activation gates may be ORed into
    /// the upper byte. A latched hit supplies the request.
    /// `parameter0` requests a 1-based destination area. `parameter1` packs
    /// a 1-based current-area warp descriptor slot in the high nibble and a
    /// 1-based destination arrival slot in the low nibble (each 1..15).
    /// Both slots must exist in their respective area's warp table.
    /// The current-area descriptor supplies departure facing, sounds and an
    /// optional event flag. The room handler first receives a query and may
    /// block the warp, redirect its destination or run a deferred room event.
    WORLD_COLLISION_TRIGGER_ACTION_WARP   = 0,
    WORLD_COLLISION_TRIGGER_ACTION_FACING = 1,
    /// Requests a CAP command or a room-selected interaction.
    ///
    /// This selector occupies the low byte of `WorldCollisionTrigger::control`;
    /// the upper-byte activation gates still apply. `parameter0` is a zero-based index
    /// in the loaded CAP command table, and `parameter1` supplies flags: bit 0
    /// prepares the player's weapon and pauses scene actors; bit 1 hides the
    /// player models until playback finishes; bit 2 selects presentation mode 2.
    /// Without bit 2, the mode is 0 with bit 0 and 3 without it. Bits 3..7 are
    /// otherwise ignored. Supply a non-NULL command entry within the loaded
    /// table's extent; the dispatch does not check the index or entry.
    /// `WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE` in `parameter1` instead sends
    /// `parameter0` to the current room's message 0x13F0 handler, with a zero
    /// second payload word; that handler defines the interaction IDs.
    /// Activation consumes the request even when an event or CAP playback is
    /// already active, so a blocked request is discarded rather than deferred.
    WORLD_COLLISION_TRIGGER_ACTION_CAP        = 2,
    WORLD_COLLISION_TRIGGER_ACTION_CALLBACK   = 3,
    WORLD_COLLISION_TRIGGER_ACTION_CLEAR      = 4,
    WORLD_COLLISION_TRIGGER_ACTION_ROOM       = 5,
    WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON = 6,
    WORLD_COLLISION_TRIGGER_ACTION_CANCEL     = 0xFF,
    WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE    = 0x4000,
    WORLD_COLLISION_TRIGGER_AUTOMATIC         = 0x8000,
};

/// Room-event region ID in parameter0 for an unflagged room-action trigger.
enum { WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID = 0xFF };

/// CAP parameter1 value that routes parameter0 to room message 0x13F0.
enum { WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE = 0xFF };

/// A linked room quad that latches an action contact or a view transition.
///
/// Storage is borrowed and mutable: keep the records and `coord` alive while
/// linked. Room setup binds `coord` to `gGfxViewCoord`. Contiguous resource
/// arrays end at the record with LAST set; the runtime list instead ends at a
/// null `next`. A node belongs to at most one list and `prevLink` addresses
/// either the list head or the preceding node's `next`.
///
/// The composed coordinate matrix maps the origin and its relative vertices
/// into collision query space. Positions and radius use game coordinates;
/// normals use 4096 per unit. The four corners have strip order: the boundary
/// walks 1, 0, 2, 3, 1. `facingNormal` is compared directly with the querying
/// body's local forward axis for FACING_QUAD.
///
/// On the view-boundary list, parameter0 is the source view and parameter1
/// the destination view; control is unused. A hit changes the saved view
/// only when the source equals the current view, then clears the latch.
/// On the action list, the first hit supplies control and both full bytes:
/// - WARP: requested area and packed departure/arrival descriptor slots, as
///   documented by `WORLD_COLLISION_TRIGGER_ACTION_WARP`.
/// - FACING: packed surface/facing selector, then yaw in 16-angle-unit steps
///   (256 steps per turn); selector bit 7 chooses indexed surface data.
/// - CAP / CAP_WEAPON: CAP command ID and presentation flags, or the room
///   message sentinel above. CAP_WEAPON includes a weapon-animation sequence.
/// - CALLBACK: two bytes passed to the selected callback.
/// - CLEAR / CANCEL: parameters unused.
/// - ROOM: room action ID and its full-byte argument, as in `DirectionActionRequest`.
/// Action hits remain latched until the action list is cleared; taking a hit
/// requests that clear on the next collision pass.
typedef struct WorldCollisionTrigger {
    struct WorldCollisionTrigger*  next;         // Next linked trigger, or NULL
    struct WorldCollisionTrigger** prevLink;     // Link that points to this node; NULL when unlinked
    GfxCoord*                      coord;        // Borrowed transform into collision query space; NULL before room binding
    SVECTOR                        origin;       // Quad origin in coordinate-local game units
    SVECTOR                        vertices[4];  // Corner offsets from origin, in strip order
    SVECTOR                        normal;       // Coordinate-local plane normal, 4096 per unit
    SVECTOR                        facingNormal; // Local facing-test normal for kind 2, 4096 per unit
    u16                            radius;       // Broad-phase radius about origin, in game units
    u16                            control;      // Action selector and gates above; unused for view boundaries
    u8                             parameter0;   // Source view or action-specific first byte, as listed above
    u8                             parameter1;   // Destination view or action-specific second byte, as listed above
    u8                             flags;        // Kind (1 view, 2 facing quad, 3 quad, 4 near/facing), LINKED, ENABLED, LAST
    s8                             hit;          // Latched contact (0 none, 1 hit), cleared by the list's consumer
} WorldCollisionTrigger;
STATIC_ASSERT_SIZEOF(WorldCollisionTrigger, 0x4C);

/// Runtime list state and resource-array termination in an occluder's flags.
///
/// The low three bits have no observed interpretation; resource records set
/// bit 0. Unlinking and clearing preserve those bits and LAST, clearing bits
/// 3..6. LAST marks the final included record in the array.
enum {
    WORLD_COLLISION_OCCLUDER_LINKED           = 0x20,
    WORLD_COLLISION_OCCLUDER_ENABLED          = 0x40,
    WORLD_COLLISION_OCCLUDER_LAST             = 0x80,
    WORLD_COLLISION_OCCLUDER_PERSISTENT_FLAGS = 0x07 | WORLD_COLLISION_OCCLUDER_LAST,
};

/// A linked room-space quad that blocks line of sight through its face.
///
/// Storage is borrowed and mutable: keep the room records alive while linked.
/// Room setup links and enables every element through the one with LAST set.
/// Runtime traversal instead ends at a NULL next. A record belongs to at most
/// one list, and `prevLink` addresses the head or the preceding record's `next`.
///
/// The current view transform maps the origin and its relative corners into
/// query space. Positions and radius use game units; normals use 4096 per unit.
/// The four corners have strip order, with boundary 1, 0, 2, 3, 1. An enabled
/// quad blocks a segment when the plane intersection lies strictly between
/// its endpoints and inside or on all four edges; both crossing directions
/// are accepted. The resource radius is unused by the segment test.
typedef struct WorldCollisionOccluder {
    struct WorldCollisionOccluder*  next;        // Next linked occluder, or NULL
    struct WorldCollisionOccluder** prevLink;    // Link pointing to this record; NULL when unlinked
    SVECTOR                         origin;      // Quad origin in room-local game units
    SVECTOR                         vertices[4]; // Corner offsets from origin, in strip order
    SVECTOR                         normal;      // Room-local plane normal, 4096 per unit
    u16                             radius;      // Approximate maximum corner distance from origin; unused by sight tests
    u8                              flags;       // LINKED, ENABLED, LAST and uninterpreted low three bits
    s8                              unknown_3B;  // No observed accesses; resource values are zero, role unproven
} WorldCollisionOccluder;
STATIC_ASSERT_SIZEOF(WorldCollisionOccluder, 0x3C);

/// A triangle's missing fourth vertex index.
enum { WORLD_COLLISION_GRID_FACE_NO_VERTEX = 0xFFFF };

/// An indexed triangle or quad in a room's collision grid.
///
/// Indices refer to the owning `WorldCollisionGrid` vertex and normal pools, whose
/// storage must remain alive with the face table. Vertices use grid-local game
/// coordinates; normals use 4096 for unit length. The fourth vertex is
/// `WORLD_COLLISION_GRID_FACE_NO_VERTEX` for a triangle. Sphere-grid passes
/// skip a face whose first two vertex indices are both zero.
///
/// The surface class selects the current room's collision and footstep
/// properties and is copied into the low bits of grid-contact keys. Class
/// meanings are local to the room.
typedef struct {
    u16 vertexIndices[4]; // Vertex-pool indices; only slot 3 may be NO_VERTEX
    u16 normalIndex;      // Index into the grid's normal pool
    s16 surfaceClass;     // Room collision/footstep property index (0..7), copied into contact keys
} WorldCollisionGridFace;
STATIC_ASSERT_SIZEOF(WorldCollisionGridFace, 0xC);

/// End of a cell's signed face-index list; NULL denotes an empty cell.
enum { WORLD_COLLISION_GRID_CELL_END = -1 };

/// Cell coordinate returned when the position lies below the grid's origin.
enum { WORLD_COLLISION_GRID_INVALID_CELL = -1 };

/// A room collision mesh and the XZ cell index used to select its faces.
///
/// Storage is borrowed: the descriptor, mesh pools, cell table and face-index
/// lists must remain live while the grid is active. Templates may have a NULL
/// `viewCoord`; activation binds it to `gGfxViewCoord`. Its composed matrix
/// maps room vertices and normals into view space. Room and actor updates may
/// rebuild the mesh pools in place without changing the cell lists.
///
/// Cell coordinates are `(roomX + xBias) / cellSize` and
/// `(roomZ + zBias) / cellSize`, with `WORLD_COLLISION_GRID_INVALID_CELL` for
/// a negative numerator. `cellSize` must be nonzero. View-space queries first
/// apply the transpose of `viewCoord->workm` and subtract `viewCoord->coord.t`
/// on X and Z. A cell needs `0 <= x < cellCountX` and `0 <= z < cellCountZ`;
/// the cell table
/// has `cellCountX * cellCountZ` entries, indexed as `x * cellCountZ + z`.
///
/// Nonempty cells hold signed indices in `[0, faceCount)`, terminated by
/// `WORLD_COLLISION_GRID_CELL_END`. Active grids must fit the collision pass's
/// 256-face candidate array. Vertex and normal pool lengths are asset-specific,
/// are not stored here, and need not equal `faceCount`; face indices and any
/// runtime edits must fit those pools.
typedef struct WorldCollisionGrid {
    GfxCoord*               viewCoord;   // Borrowed view transform; NULL before binding to the current view
    SVECTOR*                normals;     // Writable room-space normals, 4096 for unit length
    SVECTOR*                vertices;    // Writable room-space vertices in signed game coordinates
    WorldCollisionGridFace* faces;       // Writable indexed triangles/quads; faceCount entries
    s16**                   cellFaceIds; // X-major cell table of face-index lists; NULL entries are empty
    s32                     xBias;       // Additive X offset in game coordinates; negative of the grid origin
    s32                     zBias;       // Additive Z offset in game coordinates; negative of the grid origin
    u16                     cellCountX;  // Number of cells along X
    u16                     cellCountZ;  // Number of cells along Z; stride between X rows
    u16                     cellSize;    // Square cell side length in game coordinates, nonzero
    u16                     faceCount;   // Face-table length; at most 256 for an active grid
} WorldCollisionGrid;
STATIC_ASSERT_SIZEOF(WorldCollisionGrid, 0x24);

/// The setup argument of a `D_8010FABC` descriptor: the location whose entry
/// starts the task, packed in decimal as `stage * 10000 + area * 100 + room`,
/// with room 0 matching the whole area.
#define GP_TASK_LOC_KEY(stage, area, room) ((stage) * 10000 + (area) * 100 + (room))

/// `GpuImageUpload.operation` of the record that ends a list.
#define GP_IMG_REC_END 0xFF

#endif // GAMEPLAY_COLLISION_H
