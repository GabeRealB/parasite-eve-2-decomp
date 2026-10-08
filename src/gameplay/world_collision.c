#include "gameplay/world_collision.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "collision.h"
#include "item_use.h"
#include "player_actor.h"
#include "world_collision.h"

#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"

/// A body's address in the two readings a contact record needs: the pointer,
/// and the word whose halves `WorldCollisionContactResponse::bodyAddress` holds.
typedef union {
    WorldCollisionBody* object;
    s32                 address;
} _WorldCollisionBodyAddress;

/// Claims an entry and marks its receiving-body index for a pair-contact writer.
///
/// `rec` must be a modifiable pointer into `obj`'s initialized contact table.
/// Both arguments must be stable expressions: they are evaluated repeatedly.
/// This macro advances `rec`, and returns from its enclosing void function on
/// exhaustion or a missing reciprocal table. Single-contact mode replaces the
/// first entry and clears the previous body contact using its encoded address.
/// Its inline helper and fixed labels require one expansion per function.
#define WORLD_COLLISION_CLAIM_CONTACT(rec, obj)                                                                                           \
    do {                                                                                                                                  \
        _WorldCollisionBodyAddress _otherAddress;                                                                                         \
        WorldCollisionContact*     _other;                                                                                                \
        u16                        _recFlags;                                                                                             \
                                                                                                                                          \
        if ((obj)->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {                                                                         \
            _recFlags = (rec)->flags;                                                                                                     \
            if (!(_recFlags & WORLD_COLLISION_CONTACT_OCCUPIED)) {                                                                        \
                (rec)->flags = _recFlags | (((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED); \
            } else {                                                                                                                      \
                if (((rec)->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_GRID) {                             \
                    _otherAddress.address = (((rec)->response.bodyAddress.high << 16) & 0xFFFF0000) | (rec)->response.bodyAddress.low;    \
                    _other                = _worldCollisionGetBodyContacts(_otherAddress.object);                                         \
                    if (_other == NULL) {                                                                                                 \
                        return;                                                                                                           \
                    }                                                                                                                     \
                    for (;;) {                                                                                                            \
                        if (_other->key.value == (obj)->key) {                                                                            \
                            goto _found;                                                                                                  \
                        }                                                                                                                 \
                        if (_other->flags & WORLD_COLLISION_CONTACT_LAST) {                                                               \
                            return;                                                                                                       \
                        }                                                                                                                 \
                        _other++;                                                                                                         \
                    }                                                                                                                     \
                _found:                                                                                                                   \
                    _other->key.value             = 0;                                                                                    \
                    _other->distance              = 0;                                                                                    \
                    _other->point.vx              = 0;                                                                                    \
                    _other->point.vy              = 0;                                                                                    \
                    _other->point.vz              = 0;                                                                                    \
                    _other->response.direction.vx = 0;                                                                                    \
                    _other->response.direction.vy = 0;                                                                                    \
                    _other->response.direction.vz = 0;                                                                                    \
                    _other->flags                &= ~WORLD_COLLISION_CONTACT_OCCUPIED;                                                    \
                }                                                                                                                         \
                (rec)->flags |= ((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED;              \
            }                                                                                                                             \
        } else {                                                                                                                          \
            for (;;) {                                                                                                                    \
                _recFlags = (rec)->flags;                                                                                                 \
                if (!(_recFlags & WORLD_COLLISION_CONTACT_OCCUPIED)) {                                                                    \
                    goto _free;                                                                                                           \
                }                                                                                                                         \
                if (_recFlags & WORLD_COLLISION_CONTACT_LAST) {                                                                           \
                    return;                                                                                                               \
                }                                                                                                                         \
                (rec)++;                                                                                                                  \
            }                                                                                                                             \
        _free:                                                                                                                            \
            (rec)->flags = _recFlags | (((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED);     \
        }                                                                                                                                 \
    } while (0)

/// What one body of a colliding pair learns about the other, as a pair test hands it to the contact writer.
///
/// These are the fields of a `WorldCollisionContact` that depend on the shapes
/// tested. The writer copies them unchanged into the entry it claims and takes
/// the key and flags from the two bodies. A test fills this once per body,
/// because the two sides of one contact are described differently:
///
/// - Sphere against sphere, either body: `point` is the other sphere's centre,
///   `response` is zero and `distance` is the sum of both radii.
/// - Sphere against capsule, for the sphere: `point` is the capsule's second
///   endpoint and `response.direction` its unit axis towards the first endpoint.
/// - Sphere against capsule, for the capsule: `point` is the axis point nearest
///   the sphere, or the sphere's centre when the capsule tapers. `response.bodyAddress`
///   is the sphere body's address under `WORLD_COLLISION_BODY_SINGLE_CONTACT`,
///   and zero otherwise.
///
/// The capsule's first endpoint is its earlier clip contact when it has one.
typedef struct {
    SVECTOR                       point;    // World position, truncated to signed halfwords
    WorldCollisionContactResponse response; // Axis direction (4096 per unit), encoded body address, or zero
    s16                           distance; // Summed sphere radii in world units; 0 for capsule pairs
} _WorldCollisionPairContact;
STATIC_ASSERT_SIZEOF(_WorldCollisionPairContact, 0x12);

/// Scratch for one sphere-against-sphere pair test.
///
/// The scratch stack reserves this block for the test and releases it on
/// every exit. `centre0` and `centre1` are the two bodies' world centres in
/// game units, in argument order. Each SDK vector's fourth component is
/// unused and left uninitialized; placing a centre writes only the first
/// three. `centreDelta` is `centre0` minus `centre1`. A pair whose X or Z
/// separation exceeds a signed halfword is rejected before the squared
/// separation is compared with the square of `radiusSum`. `contact` is
/// filled once for each body: `point` is the other centre, `response` is
/// zero and `distance` is the radius sum, truncated to a signed halfword.
typedef struct {
    _WorldCollisionPairContact contact;     // Per-body result copied into the contact table
    VECTOR                     centre0;     // First body's world centre
    VECTOR                     centre1;     // Second body's world centre
    VECTOR                     centreDelta; // centre0 minus centre1, in game units
    s32                        radiusSum;   // Sum of both radii, in game units
} _WorldCollisionSphereScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionSphereScratch, 0x48);

/// Scratch for one sphere-against-capsule pair test.
///
/// The scratch stack reserves this block for the test and releases it on
/// every exit. Positions use game units. `segmentDirection` uses 4096 per
/// unit and points from endpoint 1 toward endpoint 0. Endpoint 0 is an
/// earlier clip contact when the capsule has one, and the geometric end
/// otherwise. The sphere centre must lie between `extendedEnd0` and
/// `extendedEnd1`, those endpoints moved apart by the sphere radius.
/// `axisPoint` is the centre's projection onto that axis. A taper measures
/// the segment with `segmentDelta` and interpolates the capsule radius,
/// restoring geometric endpoint 0 from the capsule when a contact replaced
/// it. `work` reuses one vector for the radius displacement, the restored
/// local endpoint, the projection offset and the centre-to-axis vector.
/// Each SDK vector's fourth component is unused and left uninitialized.
/// `contact` is filled once for each body of the pair.
typedef struct {
    _WorldCollisionPairContact contact;          // Per-body result copied into the contact table
    VECTOR                     sphereCenter;     // Sphere body's world centre
    VECTOR                     ends[2];          // World-space capsule segment, [0] then [1]
    VECTOR                     extendedEnd0;     // Endpoint 0 moved away from endpoint 1 by the sphere radius
    VECTOR                     extendedEnd1;     // Endpoint 1 moved away from endpoint 0 by the sphere radius
    VECTOR                     segmentDelta;     // Endpoint 0 minus endpoint 1, game units; tapered length only
    SVECTOR                    segmentDirection; // From endpoint 1 toward endpoint 0; 4096 per unit
    SVECTOR                    axisPoint;        // Sphere centre projected onto the axis, truncated to signed 16 bits
    union {
        SVECTOR radiusAlongSegment;              // Sphere radius as a displacement along segmentDirection
        SVECTOR localEndpoint;                   // Local endpoint 0 plus the body's position, before rotation
        SVECTOR projectionOffset;                // segmentDirection times the distance from extendedEnd1
        SVECTOR centreToAxis;                    // axisPoint minus the sphere centre
    } work;
} _WorldCollisionCapsuleScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionCapsuleScratch, 0x8C);

/// Transformed grid-face geometry and edge work for a segment intersection test.
///
/// Corners and directions use the query space given by the grid's cached
/// transform; only corners receive its translation. Triangle tests initialize
/// three corners, quad tests all four. The scratch stack reserves this entire
/// block for one face test and releases it on every exit; its contents are not
/// retained. `edgeWork` changes units when the edge-plane normal replaces the
/// displacement, without changing its signed 32-bit component representation.
typedef struct {
    VECTOR corners[4];    // Face corners in query space, in game-coordinate units
    VECTOR faceNormal;    // Rotated face normal, with 4096 representing one unit
    VECTOR edgeDirection; // Normalized end-minus-start edge direction, with 4096 per unit
    VECTOR edgeWork;      // Edge displacement in game units, then outward edge-plane normal with 4096 per unit
} _WorldCollisionGridRayScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGridRayScratch, 0x70);

/// Scratch for testing one sphere against the faces of its collision-grid cell.
///
/// The scratch stack reserves this block for the test and releases it on
/// every exit; its contents are not retained. `gridCell` selects the cell:
/// X and Z are the indices, Y is zero, and the fourth halfword is unused.
/// `centre` is the sphere centre in the query space of the grid's cached
/// view transform, the same space as `geometry`, and its fourth word is
/// unused. `geometry` holds one face. Triangle tests fill three corners and
/// quad tests all four. A centre past an edge test's slack on the positive
/// side of that edge's plane is outside the face.
typedef struct {
    SVECTOR                       gridCell; // Cell indices; Y is 0 and the pad halfword is unused
    VECTOR                        centre;   // Query-space sphere centre in game units; pad word unused
    _WorldCollisionGridRayScratch geometry; // This face's corners, normal and outward edge planes
} _WorldCollisionGridSphereScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGridSphereScratch, 0x88);

// Sphere-query normals, directions and taper ratios use 12 fractional bits.
enum {
    WORLD_COLLISION_SPHERE_FRACTION_BITS         = 12,
    WORLD_COLLISION_SPHERE_UNIT                  = 1 << WORLD_COLLISION_SPHERE_FRACTION_BITS,
    WORLD_COLLISION_SPHERE_GRID_TRIANGLE_CORNERS = 3
};

/// Builds an outward edge-plane normal in a sphere query's geometry scratch.
///
/// `sphereScratch` is a live `_WorldCollisionGridSphereScratch` pointer with face
/// corners and normal initialized in one query frame. `edgePairIndex` selects
/// `Gp_FaceEdgePairs`: 0..2 for triangles or 1..4 for quads. Edge XYZ deltas must
/// fit signed halfwords with squared length in 1..0x7FFFFFFF for normalization.
/// Writes the Q12 unit edge direction and outward cross-product normal into
/// geometry.edgeDirection/edgeWork; pad words are untouched. Clobbers GTE
/// rotation and arithmetic state. Both arguments must be stable expressions,
/// as they are evaluated repeatedly. Expands to multiple statements: invoke
/// only within a braced block. No caller-local identifiers are captured.
#define WORLD_COLLISION_BUILD_SPHERE_GRID_EDGE_PLANE(sphereScratch, edgePairIndex)                                                                                                         \
    (sphereScratch)->geometry.edgeWork.vx =                                                                                                                                                \
        (sphereScratch)->geometry.corners[Gp_FaceEdgePairs[(edgePairIndex)].endCornerIndex].vx - (sphereScratch)->geometry.corners[Gp_FaceEdgePairs[(edgePairIndex)].startCornerIndex].vx; \
    (sphereScratch)->geometry.edgeWork.vy =                                                                                                                                                \
        (sphereScratch)->geometry.corners[Gp_FaceEdgePairs[(edgePairIndex)].endCornerIndex].vy - (sphereScratch)->geometry.corners[Gp_FaceEdgePairs[(edgePairIndex)].startCornerIndex].vy; \
    (sphereScratch)->geometry.edgeWork.vz =                                                                                                                                                \
        (sphereScratch)->geometry.corners[Gp_FaceEdgePairs[(edgePairIndex)].endCornerIndex].vz - (sphereScratch)->geometry.corners[Gp_FaceEdgePairs[(edgePairIndex)].startCornerIndex].vz; \
    VectorNormal(&(sphereScratch)->geometry.edgeWork, &(sphereScratch)->geometry.edgeDirection);                                                                                           \
    gte_ldopv1(&(sphereScratch)->geometry.faceNormal);                                                                                                                                     \
    gte_ldopv2(&(sphereScratch)->geometry.edgeDirection);                                                                                                                                  \
    gte_op12();                                                                                                                                                                            \
    gte_stlvnl(&(sphereScratch)->geometry.edgeWork);

/// Prepares the other sphere's centre and radius sum for a pair-contact writer.
///
/// `contact` is writable `_WorldCollisionPairContact` storage, `otherCentre` a
/// disjoint `VECTOR` in the receiving body's query frame, and `radiusSum` game
/// units. Narrows XYZ/radius to signed halfwords and zeroes response XYZ;
/// pad halfwords are untouched. Arguments must be stable, side-effect-free
/// expressions; evaluated repeatedly. Expands to multiple statements: invoke
/// only within a braced block. Captures no caller-local identifiers.
#define WORLD_COLLISION_PREPARE_SPHERE_PAIR_CONTACT(contact, otherCentre, radiusSum) \
    (contact)->point.vx              = (otherCentre)->vx;                            \
    (contact)->point.vy              = (otherCentre)->vy;                            \
    (contact)->point.vz              = (otherCentre)->vz;                            \
    (contact)->response.direction.vx = 0;                                            \
    (contact)->response.direction.vy = 0;                                            \
    (contact)->response.direction.vz = 0;                                            \
    (contact)->distance              = (radiusSum);

/// Extends a placed capsule segment by the sphere radius along its reverse axis.
///
/// `capsuleScratch` is writable `_WorldCollisionCapsuleScratch` storage with the
/// placed ends and Q12 axis initialized; `sphereRadius` is in game units and
/// disjoint from the output. The axis displacement narrows to signed halfwords
/// before extending the VECTOR endpoints. Pad components are untouched.
/// Arguments must be stable, side-effect-free expressions; evaluated repeatedly.
/// Expands to multiple statements: invoke only within a braced block. Captures
/// no caller-local identifiers.
#define WORLD_COLLISION_EXTEND_CAPSULE_SEGMENT(capsuleScratch, sphereRadius)                                                                         \
    (capsuleScratch)->work.radiusAlongSegment.vx = ((capsuleScratch)->segmentDirection.vx * (sphereRadius)) >> WORLD_COLLISION_SPHERE_FRACTION_BITS; \
    (capsuleScratch)->work.radiusAlongSegment.vy = ((capsuleScratch)->segmentDirection.vy * (sphereRadius)) >> WORLD_COLLISION_SPHERE_FRACTION_BITS; \
    (capsuleScratch)->work.radiusAlongSegment.vz = ((capsuleScratch)->segmentDirection.vz * (sphereRadius)) >> WORLD_COLLISION_SPHERE_FRACTION_BITS; \
    (capsuleScratch)->extendedEnd0.vx            = (capsuleScratch)->ends[0].vx + (capsuleScratch)->work.radiusAlongSegment.vx;                      \
    (capsuleScratch)->extendedEnd0.vy            = (capsuleScratch)->ends[0].vy + (capsuleScratch)->work.radiusAlongSegment.vy;                      \
    (capsuleScratch)->extendedEnd0.vz            = (capsuleScratch)->ends[0].vz + (capsuleScratch)->work.radiusAlongSegment.vz;                      \
    (capsuleScratch)->extendedEnd1.vx            = (capsuleScratch)->ends[1].vx - (capsuleScratch)->work.radiusAlongSegment.vx;                      \
    (capsuleScratch)->extendedEnd1.vy            = (capsuleScratch)->ends[1].vy - (capsuleScratch)->work.radiusAlongSegment.vy;                      \
    (capsuleScratch)->extendedEnd1.vz            = (capsuleScratch)->ends[1].vz - (capsuleScratch)->work.radiusAlongSegment.vz;

s32 Gp_PendingObj4CFlag;

static s32 _worldCollisionCollideSpherePair(WorldCollisionBody* firstBody, WorldCollisionBody* secondBody, s32 handlerIndex);

static s32 _worldCollisionCollideSphereCapsulePair(WorldCollisionBody* sphereBody, WorldCollisionBody* capsuleBody, s32 handlerIndex);

static void _worldCollisionCollideListPairs(WorldCollisionBody* body);

static void _worldCollisionRecordPairContact(const WorldCollisionBody* receivingBody, const WorldCollisionBody* contactedBody, const _WorldCollisionPairContact* pairContact);

/// Borrows the mutable contact table selected by a body's kind.
///
/// Returns NULL for NONE, unsupported kinds, or a NULL table. A proxy follows
/// exactly one contactOwner to its direct table; that owner and shape contexts
/// must be live and correctly initialized. Does not validate or clear entries,
/// retain the body, or transfer ownership.
static inline WorldCollisionContact* _worldCollisionGetBodyContacts(const WorldCollisionBody* body)
{
    WorldCollisionContact* contacts = NULL;

    switch (body->flags & WORLD_COLLISION_BODY_KIND_MASK) {
        case WORLD_COLLISION_BODY_NONE:
            break;
        case WORLD_COLLISION_BODY_SPHERE:
            contacts = body->context.contacts;
            break;
        case WORLD_COLLISION_BODY_CONTACT_PROXY:
            contacts = body->context.contactOwner->context.contacts;
            break;
        case WORLD_COLLISION_BODY_CAPSULE:
            contacts = body->context.capsule->contacts;
            break;
        case WORLD_COLLISION_BODY_MOTION_SPHERE:
            contacts = body->context.motion->contacts;
            break;
    }
    return contacts;
}

WorldCollisionPairHandler Gp_PairHandlers[5] = {
    worldCollisionPairNop,
    _worldCollisionCollideSpherePair,
    worldCollisionPairNop,
    _worldCollisionCollideSphereCapsulePair,
    worldCollisionPairNop,
};
WorldCollisionPairRule D_8010FA4C[4][4] = {
    { { WORLD_COLLISION_PAIR_HANDLER_SPHERES, false },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERE_PROXY, false },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERE_CAPSULE, false },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERES, false } },
    { { WORLD_COLLISION_PAIR_HANDLER_SPHERE_PROXY, true },
      { WORLD_COLLISION_PAIR_HANDLER_NONE, false },
      { WORLD_COLLISION_PAIR_HANDLER_CAPSULE_PROXY, true },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERE_PROXY, true } },
    { { WORLD_COLLISION_PAIR_HANDLER_SPHERE_CAPSULE, true },
      { WORLD_COLLISION_PAIR_HANDLER_CAPSULE_PROXY, false },
      { WORLD_COLLISION_PAIR_HANDLER_NONE, false },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERE_CAPSULE, true } },
    { { WORLD_COLLISION_PAIR_HANDLER_SPHERES, false },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERE_PROXY, false },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERE_CAPSULE, false },
      { WORLD_COLLISION_PAIR_HANDLER_SPHERES, false } },
};

/// Builds a grid-face edge's outward plane normal for a query-space half-space test.
///
/// `edgePairIndex` selects `Gp_FaceEdgePairs` (0..2 for triangles, 1..4 for
/// quads). Both indexed corners must be initialized in game units, and
/// `faceNormal` must be the face's normal in the same space, with 4096 per unit.
/// The end-minus-start edge delta must fit signed halfwords and have squared
/// length in 1..0x7FFFFFFF, as required by the SDK normalization routine.
///
/// Writes the normalized edge to `edgeDirection` and the face-normal cross
/// edge-direction product to `edgeWork`, both with 4096 per unit. The cross
/// product is not normalized again; the caller supplies the plane offset.
/// Only XYZ are read or written; vector pad words are untouched. Clobbers GTE
/// rotation-matrix registers and arithmetic state, and retains no pointers.
static __inline__ void _worldCollisionBuildGridEdgePlaneNormal(_WorldCollisionGridRayScratch* scratch, s32 edgePairIndex)
{
    const WorldCollisionFaceEdge* edge = &Gp_FaceEdgePairs[edgePairIndex];

    scratch->edgeWork.vx = scratch->corners[edge->endCornerIndex].vx - scratch->corners[edge->startCornerIndex].vx;
    scratch->edgeWork.vy = scratch->corners[edge->endCornerIndex].vy - scratch->corners[edge->startCornerIndex].vy;
    scratch->edgeWork.vz = scratch->corners[edge->endCornerIndex].vz - scratch->corners[edge->startCornerIndex].vz;
    VectorNormal(&scratch->edgeWork, &scratch->edgeDirection);

    // Cross in this order so the positive half-space lies outside the face.
    gte_ldopv1(&scratch->faceNormal);
    gte_ldopv2(&scratch->edgeDirection);
    gte_op12();
    gte_stlvnl(&scratch->edgeWork);
}

void Gp_TickWorldCollision(Task* unused)
{
    if (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER) != NULL) {
        playerActorUpdateMove();
        worldCollisionCollideBodyListGrid(Gp_ObjList0);
        worldCollisionCollideBodyListGrid(Gp_ObjList1);
        worldCollisionCollideBodyListGrid(Gp_ObjList2);
        worldCollisionCollideBodyListGrid(Gp_ObjList3);
        worldCollisionCollideBodyListGrid(Gp_ObjList4);
        worldCollisionCollideBodyListGrid(Gp_ObjList7);
        worldCollisionCollideBodyListGrid(Gp_ObjList8);
        worldCollisionCollideBodyLists(Gp_ObjList0, Gp_ObjList2);
        worldCollisionCollideBodyLists(Gp_ObjList0, Gp_ObjList3);
        worldCollisionCollideBodyLists(Gp_ObjList0, Gp_ObjList4);
        worldCollisionCollideBodyLists(Gp_ObjList0, Gp_ObjList8);
        _worldCollisionCollideListPairs(Gp_ObjList0);
        worldCollisionCollideBodyLists(Gp_ObjList1, Gp_ObjList2);
        worldCollisionCollideBodyLists(Gp_ObjList1, Gp_ObjList4);
        worldCollisionCollideBodyLists(Gp_ObjList1, Gp_ObjList6);
        worldCollisionCollideBodyLists(Gp_ObjList2, Gp_ObjList4);
        worldCollisionCollideBodyLists(Gp_ObjList2, Gp_ObjList8);
        _worldCollisionCollideListPairs(Gp_ObjList2);
        worldCollisionCollideBodyLists(Gp_ObjList3, Gp_ObjList4);
        worldCollisionCollideBodyLists(Gp_ObjList4, Gp_ObjList8);
        if (Gp_PendingObj4CFlag != 0) {
            worldCollisionClearActionHits();
        }
        func_800E0608(Gp_ObjList0, WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_KIND_MASK,
                      WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_MOTION_SPHERE);
        if (gGameSession->suppressViewTriggers == 0) {
            worldCollisionScanViewBoundaries(Gp_ObjList0, WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_KIND_MASK,
                                             WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_MOTION_SPHERE);
        }
    }
}

/// Tests each unordered pair of enabled bodies within one collision list.
///
/// The borrowed list must be live and acyclic, and enabled bodies must have
/// kinds 1..4 with initialized shape/context storage. Each body is compared
/// only with its successors, excluding itself and avoiding duplicate tests.
/// Rules choose the callback and argument order; callback results are ignored.
/// The callbacks may write contacts but must preserve this list's links.
static void _worldCollisionCollideListPairs(WorldCollisionBody* body)
{
    WorldCollisionBody*           other;
    const WorldCollisionPairRule* rule;
    s32                           rowOffsetBytes;
    s32                           ruleOffsetBytes;
    u16                           flags;
    u16                           handlerIndex;
    u16                           swapBodies;
    u8                            rowIndex;
    u8                            columnIndex;

    for (; body != NULL; body = body->next) {
        flags = body->flags;
        other = body->next;
        if (flags & WORLD_COLLISION_BODY_PAIR_ENABLED) {
            rowIndex = (body->flags & WORLD_COLLISION_BODY_KIND_MASK) - WORLD_COLLISION_BODY_SPHERE;
            if (other != NULL) {
                rowOffsetBytes = rowIndex * (s32)sizeof(D_8010FA4C[0]);
                for (; other != NULL; other = other->next) {
                    if (other->flags & WORLD_COLLISION_BODY_PAIR_ENABLED) {
                        columnIndex     = (other->flags & WORLD_COLLISION_BODY_KIND_MASK) - WORLD_COLLISION_BODY_SPHERE;
                        ruleOffsetBytes = columnIndex * (s32)sizeof(WorldCollisionPairRule) + rowOffsetBytes;
                        // Complete the byte offset before adding the matrix base.
                        rule         = (const WorldCollisionPairRule*)((const u8*)&D_8010FA4C + ruleOffsetBytes);
                        swapBodies   = rule->swapBodies;
                        handlerIndex = rule->handlerIndex;
                        if (swapBodies == false) {
                            Gp_PairHandlers[handlerIndex](body, other, handlerIndex);
                        } else {
                            Gp_PairHandlers[handlerIndex](other, body, handlerIndex);
                        }
                    }
                }
            }
        }
    }
}

/// Copies a pair-test result into a claimed receiving-body contact entry.
///
/// The bodies, their contexts and LAST-terminated writable tables must be live.
/// A zero contacted-body key or missing receiving table makes no change.
/// Ordinary mode uses the first unoccupied entry; a full table makes no change.
/// SINGLE_CONTACT replaces entry zero, first clearing the previous reciprocal
/// body contact when present. Its encoded body address and reciprocal table
/// must remain valid; a missing table or absent matching key aborts the new write.
/// The receiving body's index is ORed into the claimed flags, and key comes
/// from contactedBody. Copies distance, point and response unchanged, including
/// vector pad halfwords. Pair coordinates must share the cached transform frame.
static void _worldCollisionRecordPairContact(const WorldCollisionBody* receivingBody, const WorldCollisionBody* contactedBody, const _WorldCollisionPairContact* pairContact)
{
    WorldCollisionContact* contactSlot;

    if (contactedBody->key == 0) {
        return;
    }

    contactSlot = _worldCollisionGetBodyContacts(receivingBody);
    if (contactSlot == NULL) {
        return;
    }

    // Replacing a single body contact also releases its reciprocal entry.
    WORLD_COLLISION_CLAIM_CONTACT(contactSlot, receivingBody);

    contactSlot->key.value = contactedBody->key;
    contactSlot->distance  = pairContact->distance;
    contactSlot->point     = pairContact->point;
    contactSlot->response  = pairContact->response;
}

/// Tests two spherical bodies and offers a reciprocal contact on overlap.
///
/// Both bodies may be spheres or motion spheres; their live composed transforms
/// must use the same frame. Centres and radii use game units. Rejects X/Z
/// separation above 32767 before comparing squared separation strictly below
/// the squared radius sum. Returns 1 for overlap even when a zero key, missing
/// table or exhausted table prevents a contact write; tangency returns 0.
/// Each receiving entry gets the other centre, zero response and the radius
/// sum, narrowed to signed halfwords. `handlerIndex` is unused dispatch metadata.
/// Squared distance and squared radius sum must fit signed 32-bit arithmetic.
/// Inputs and live contact tables must be clear of the initialized scratch
/// stack's 72-byte block and nested position-query reservations. Releases its
/// block on every exit, changes GTE state and retains no new storage.
static s32 _worldCollisionCollideSpherePair(WorldCollisionBody* firstBody, WorldCollisionBody* secondBody, s32 handlerIndex)
{
    enum { WORLD_COLLISION_SPHERE_PAIR_MAX_HORIZONTAL_DELTA = 0x7FFF };
    _WorldCollisionSphereScratch* scratch;
    s32                           overlaps;

    // Place both centres in their shared cached-transform frame.
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionSphereScratch);
    worldCollisionGetBodyComposedPosition(firstBody, &scratch->centre0);
    worldCollisionGetBodyComposedPosition(secondBody, &scratch->centre1);

    overlaps                = 0;
    scratch->centreDelta.vx = scratch->centre0.vx - scratch->centre1.vx;
    scratch->centreDelta.vy = scratch->centre0.vy - scratch->centre1.vy;
    scratch->centreDelta.vz = scratch->centre0.vz - scratch->centre1.vz;
    // Reject an X or Z separation beyond a signed halfword.
    if ((ABS(scratch->centreDelta.vx) > WORLD_COLLISION_SPHERE_PAIR_MAX_HORIZONTAL_DELTA) || (ABS(scratch->centreDelta.vz) > WORLD_COLLISION_SPHERE_PAIR_MAX_HORIZONTAL_DELTA)) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionSphereScratch);
        return 0;
    }

    scratch->radiusSum = firstBody->radius + secondBody->radius;
    if (scratch->centreDelta.vx * scratch->centreDelta.vx + scratch->centreDelta.vy * scratch->centreDelta.vy + scratch->centreDelta.vz * scratch->centreDelta.vz < scratch->radiusSum * scratch->radiusSum) {
        // Each body records the other centre, a zero response and the radius sum.
        WORLD_COLLISION_PREPARE_SPHERE_PAIR_CONTACT(&scratch->contact, &scratch->centre1, scratch->radiusSum);
        _worldCollisionRecordPairContact(firstBody, secondBody, &scratch->contact);
        overlaps = 1;

        WORLD_COLLISION_PREPARE_SPHERE_PAIR_CONTACT(&scratch->contact, &scratch->centre0, scratch->radiusSum);
        _worldCollisionRecordPairContact(secondBody, firstBody, &scratch->contact);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionSphereScratch);
    return overlaps;
}

/// Tests a spherical body against a capsule segment or taper and offers contacts.
///
/// `sphereBody` may be a sphere or motion sphere; `capsuleBody` must be kind 3 with
/// a live capsule. Both cached transforms and any clipping contact must share
/// a composition frame. Positions/radii use game units; axis products and taper
/// ratios use 12 fractional bits. Segment placement may substitute an earlier
/// contact for endpoint 0. The centre must pass the axial bounds extended by
/// the sphere radius and the strict combined-radius test about the axis.
/// A taper requires a nonzero endpoint-1 radius and geometric segment length.
/// Segment deltas must meet the SDK normalization bounds; arithmetic must fit
/// its signed 32-bit intermediates. Returns 1 for geometric overlap regardless
/// of whether the contact tables accept writes; `handlerIndex` is unused.
///
/// The sphere receives endpoint 1 and the reverse segment axis. The capsule
/// receives the nearest axis point, or the sphere centre for a taper, and the
/// sphere body's encoded address in SINGLE_CONTACT mode. Contact components
/// narrow to signed halfwords. Both LAST-terminated tables and any retained
/// reciprocal body address must remain live. Storage must be clear of the
/// initialized scratch stack's 140-byte block and nested queries; the block
/// is released on every exit. Changes GTE state and borrows all input storage.
static s32 _worldCollisionCollideSphereCapsulePair(WorldCollisionBody* sphereBody, WorldCollisionBody* capsuleBody, s32 handlerIndex)
{
    _WorldCollisionCapsuleScratch* scratchEnd;
    _WorldCollisionCapsuleScratch* scratch;
    const WorldCollisionCapsule*   capsule;
    s32                            axisWork; // Axis distance, segment length, Q12 taper fraction, then combined radius
    s32                            overlaps;
    s32                            isTapered;
    VECTOR*                        sphereCenter;
    s32                            radiusSquared;
    s32                            end0ProjectionX;
    s32                            end0ProjectionY;
    s32                            end0ProjectionZ;
    s32                            end1ProjectionX;
    s32                            end1ProjectionY;
    s32                            end1ProjectionZ;
    s32                            segmentSquareX;
    s32                            segmentSquareY;
    s32                            segmentSquareZ;
    s32                            projectionSquareX;
    s32                            projectionSquareY;
    s32                            projectionSquareZ;
    s32                            separationSquareX;
    s32                            separationSquareY;
    s32                            separationSquareZ;
    s32                            segmentLength;
    s32                            projectionLength;
    s32                            radiusRatioQ12;
    s32                            radiusWork;    // Capsule endpoint-0 radius, then sphere radius; game units
    s32                            radiusOrSlope; // Combined radius in game units, or Q12 taper slope

    // Address the centre from the cursor before the reservation is stored.
    // Taking &scratch->sphereCenter after that store does not keep this order.
    scratchEnd                                          = SCRATCH_STACK_CURSOR(_WorldCollisionCapsuleScratch);
    sphereCenter                                        = &(scratchEnd - 1)->sphereCenter;
    SCRATCH_STACK_CURSOR(_WorldCollisionCapsuleScratch) = scratchEnd - 1;
    capsule                                             = capsuleBody->context.capsule;
    scratch                                             = scratchEnd - 1;
    // Place the sphere and segment in their shared cached-transform frame.
    worldCollisionGetBodyComposedPosition(sphereBody, sphereCenter);
    worldCollisionPlaceCapsuleSegment(capsuleBody, scratch->ends, &scratch->segmentDirection, WORLD_COLLISION_CAPSULE_SEGMENT_PAIR_TEST);

    WORLD_COLLISION_EXTEND_CAPSULE_SEGMENT(scratch, sphereBody->radius);

    // Reject a centre outside the segment extended by the sphere radius.
    end0ProjectionX = (scratch->sphereCenter.vx - scratch->extendedEnd0.vx) * scratch->segmentDirection.vx;
    end0ProjectionY = (scratch->sphereCenter.vy - scratch->extendedEnd0.vy) * scratch->segmentDirection.vy;
    end0ProjectionZ = (scratch->sphereCenter.vz - scratch->extendedEnd0.vz) * scratch->segmentDirection.vz;
    overlaps        = 0;
    if (end0ProjectionX + end0ProjectionY + end0ProjectionZ > 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
        return 0;
    }

    end1ProjectionX = (scratch->sphereCenter.vx - scratch->extendedEnd1.vx) * scratch->segmentDirection.vx;
    end1ProjectionY = (scratch->sphereCenter.vy - scratch->extendedEnd1.vy) * scratch->segmentDirection.vy;
    end1ProjectionZ = (scratch->sphereCenter.vz - scratch->extendedEnd1.vz) * scratch->segmentDirection.vz;
    axisWork        = (end1ProjectionX + end1ProjectionY + end1ProjectionZ) >> WORLD_COLLISION_SPHERE_FRACTION_BITS;
    if (axisWork <= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
        return 0;
    }

    radiusWork = capsule->end0Radius;
    isTapered  = radiusWork != capsule->end1Radius;
    // Equal radii share one combined radius. A taper restores geometric
    // endpoint 0 when a contact replaced it, then interpolates the radius.
    // Narrow translated positions before halfword sums, avoiding full-word overflow.
    if (!isTapered) {
        radiusOrSlope         = sphereBody->radius + radiusWork;
        scratch->axisPoint.vx = (u16)scratch->extendedEnd1.vx + ((scratch->segmentDirection.vx * axisWork) >> WORLD_COLLISION_SPHERE_FRACTION_BITS);
        scratch->axisPoint.vy = (u16)scratch->extendedEnd1.vy + ((scratch->segmentDirection.vy * axisWork) >> WORLD_COLLISION_SPHERE_FRACTION_BITS);
        scratch->axisPoint.vz = (u16)scratch->extendedEnd1.vz + ((scratch->segmentDirection.vz * axisWork) >> WORLD_COLLISION_SPHERE_FRACTION_BITS);
        axisWork              = radiusOrSlope;
    } else {
        if (capsuleBody->flags & (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT)) {
            gte_SetRotMatrix(&capsuleBody->coord->workm);
            scratch->work.localEndpoint.vx = capsule->ends[0].vx + capsuleBody->pos.vx;
            scratch->work.localEndpoint.vy = capsule->ends[0].vy + capsuleBody->pos.vy;
            scratch->work.localEndpoint.vz = capsule->ends[0].vz + capsuleBody->pos.vz;
            gte_ldv0(&scratch->work.localEndpoint);
            gte_rtv0();
            gte_stlvnl(&scratch->ends[0]);
            scratch->ends[0].vx += capsuleBody->coord->workm.t[0];
            scratch->ends[0].vy += capsuleBody->coord->workm.t[1];
            scratch->ends[0].vz += capsuleBody->coord->workm.t[2];
        }

        scratch->segmentDelta.vx = scratch->ends[0].vx - scratch->ends[1].vx;
        scratch->segmentDelta.vy = scratch->ends[0].vy - scratch->ends[1].vy;
        scratch->segmentDelta.vz = scratch->ends[0].vz - scratch->ends[1].vz;
        segmentSquareX           = scratch->segmentDelta.vx * scratch->segmentDelta.vx;
        segmentSquareY           = scratch->segmentDelta.vy * scratch->segmentDelta.vy;
        segmentSquareZ           = scratch->segmentDelta.vz * scratch->segmentDelta.vz;
        segmentLength            = SquareRoot0(segmentSquareX + segmentSquareY + segmentSquareZ);

        scratch->work.projectionOffset.vx = (scratch->segmentDirection.vx * axisWork) >> WORLD_COLLISION_SPHERE_FRACTION_BITS;
        scratch->work.projectionOffset.vy = (scratch->segmentDirection.vy * axisWork) >> WORLD_COLLISION_SPHERE_FRACTION_BITS;
        scratch->work.projectionOffset.vz = (scratch->segmentDirection.vz * axisWork) >> WORLD_COLLISION_SPHERE_FRACTION_BITS;
        projectionSquareX                 = scratch->work.projectionOffset.vx * scratch->work.projectionOffset.vx;
        projectionSquareY                 = scratch->work.projectionOffset.vy * scratch->work.projectionOffset.vy;
        projectionSquareZ                 = scratch->work.projectionOffset.vz * scratch->work.projectionOffset.vz;
        axisWork                          = segmentLength;
        projectionLength                  = SquareRoot0(projectionSquareX + projectionSquareY + projectionSquareZ);

        radiusRatioQ12        = (capsule->end0Radius << WORLD_COLLISION_SPHERE_FRACTION_BITS) / capsule->end1Radius;
        axisWork              = (projectionLength << WORLD_COLLISION_SPHERE_FRACTION_BITS) / axisWork;
        radiusOrSlope         = radiusRatioQ12 - WORLD_COLLISION_SPHERE_UNIT;
        radiusWork            = sphereBody->radius;
        axisWork              = radiusWork + ((((radiusOrSlope * axisWork) >> WORLD_COLLISION_SPHERE_FRACTION_BITS) * capsule->end1Radius >> WORLD_COLLISION_SPHERE_FRACTION_BITS) + capsule->end1Radius);
        scratch->axisPoint.vx = scratch->work.projectionOffset.vx + (u16)scratch->extendedEnd1.vx;
        scratch->axisPoint.vy = scratch->work.projectionOffset.vy + (u16)scratch->extendedEnd1.vy;
        scratch->axisPoint.vz = scratch->work.projectionOffset.vz + (u16)scratch->extendedEnd1.vz;
    }

    scratch->work.centreToAxis.vx = scratch->axisPoint.vx - (u16)scratch->sphereCenter.vx;
    scratch->work.centreToAxis.vy = scratch->axisPoint.vy - (u16)scratch->sphereCenter.vy;
    scratch->work.centreToAxis.vz = scratch->axisPoint.vz - (u16)scratch->sphereCenter.vz;
    separationSquareX             = scratch->work.centreToAxis.vx * scratch->work.centreToAxis.vx;
    separationSquareY             = scratch->work.centreToAxis.vy * scratch->work.centreToAxis.vy;
    separationSquareZ             = scratch->work.centreToAxis.vz * scratch->work.centreToAxis.vz;
    radiusSquared                 = axisWork * axisWork;
    if (separationSquareX + separationSquareY + separationSquareZ < radiusSquared) {
        // The sphere records endpoint 1 and the segment direction. The capsule
        // records the axis point, or the sphere centre when the capsule tapers.
        scratch->contact.distance              = 0;
        scratch->contact.point.vx              = scratch->ends[1].vx;
        scratch->contact.point.vy              = scratch->ends[1].vy;
        scratch->contact.point.vz              = scratch->ends[1].vz;
        scratch->contact.response.direction.vx = scratch->segmentDirection.vx;
        scratch->contact.response.direction.vy = scratch->segmentDirection.vy;
        scratch->contact.response.direction.vz = scratch->segmentDirection.vz;
        _worldCollisionRecordPairContact(sphereBody, capsuleBody, &scratch->contact);
        if (!isTapered) {
            scratch->contact.point = scratch->axisPoint;
        } else {
            scratch->contact.point.vx = scratch->sphereCenter.vx;
            scratch->contact.point.vy = scratch->sphereCenter.vy;
            scratch->contact.point.vz = scratch->sphereCenter.vz;
        }
        if (capsuleBody->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {
            s32 sphereAddress                          = (s32)sphereBody;
            scratch->contact.response.bodyAddress.low  = sphereAddress;
            scratch->contact.response.bodyAddress.high = sphereAddress >> 16;
        } else {
            scratch->contact.response.direction.vx = 0;
            scratch->contact.response.direction.vy = 0;
        }
        scratch->contact.response.direction.vz = 0;
        scratch->contact.distance              = 0;
        _worldCollisionRecordPairContact(capsuleBody, sphereBody, &scratch->contact);
        overlaps = 1;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
    return overlaps;
}

void worldCollisionCollideSphereGrid(const WorldCollisionBody* body)
{
    enum { WORLD_COLLISION_SPHERE_GRID_EDGE_TOLERANCE = 10 };
    _WorldCollisionGridSphereScratch* scratch;
    const WorldCollisionGridFace*     face;
    WorldCollisionContact*            contact;
    const s16*                        faceIds;
    s32                               faceIndex;
    s32                               geometryIndex;
    s32                               cornerCount;
    s32                               isOutside;
    s32                               edgeDistance;
    s32                               facePlaneOffset;
    s32                               edgePlaneOffset;
    u16                               planeDistanceBits;
    u16                               contactFlags;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridSphereScratch);
    worldCollisionGetBodyComposedPosition(body, &scratch->centre);
    worldCollisionViewToCell(&scratch->centre, &scratch->gridCell);

    if ((u16)scratch->gridCell.vx < Gp_GridParams->cellCountX && (u16)scratch->gridCell.vz < Gp_GridParams->cellCountZ) {
        faceIds = Gp_GridParams->cellFaceIds[scratch->gridCell.vx * Gp_GridParams->cellCountZ + scratch->gridCell.vz];
        if (faceIds != NULL) {
            for (;;) {
                faceIndex = *faceIds;
                if (faceIndex == WORLD_COLLISION_GRID_CELL_END) {
                    goto done;
                }
                face = &Gp_GridParams->faces[faceIndex];
                if (face->vertexIndices[0] == 0 && face->vertexIndices[1] == 0) {
                    faceIds++;
                    continue;
                }

                // Transform this face into the sphere's query space.
                gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
                gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[0]]);
                gte_rtv0();
                gte_stlvnl(&scratch->geometry.corners[0]);
                scratch->geometry.corners[0].vx += Gp_GridParams->viewCoord->workm.t[0];
                scratch->geometry.corners[0].vy += Gp_GridParams->viewCoord->workm.t[1];
                scratch->geometry.corners[0].vz += Gp_GridParams->viewCoord->workm.t[2];

                gte_ldv0(&Gp_GridParams->normals[face->normalIndex]);
                gte_rtv0();
                gte_stlvnl(&scratch->geometry.faceNormal);

                facePlaneOffset = (scratch->geometry.faceNormal.vx * scratch->geometry.corners[0].vx + scratch->geometry.faceNormal.vy * scratch->geometry.corners[0].vy +
                                   scratch->geometry.faceNormal.vz * scratch->geometry.corners[0].vz) >>
                                  12;
                planeDistanceBits = ((scratch->geometry.faceNormal.vx * scratch->centre.vx + scratch->geometry.faceNormal.vy * scratch->centre.vy +
                                      scratch->geometry.faceNormal.vz * scratch->centre.vz) >>
                                     12) -
                                    facePlaneOffset;
                if (body->radius >= ABS((s16)planeDistanceBits)) {
                    goto edges;
                }
                goto next_face;

            mark_outside:
                isOutside = 1;
                goto edges_done;

            fill:
                contact->flags              = contactFlags | WORLD_COLLISION_CONTACT_OCCUPIED;
                contact->distance           = body->radius - planeDistanceBits;
                contact->key.value          = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                contact->point.vx           = 0;
                contact->point.vy           = 0;
                contact->point.vz           = 0;
                contact->response.direction = Gp_GridParams->normals[face->normalIndex];
                goto next_face;

            edges:
                cornerCount = (face->vertexIndices[ARRAY_SIZE(face->vertexIndices) - 1] != WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? (s32)ARRAY_SIZE(face->vertexIndices) : WORLD_COLLISION_SPHERE_GRID_TRIANGLE_CORNERS;
                for (geometryIndex = 1; geometryIndex < cornerCount; geometryIndex++) {
                    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[geometryIndex]]);
                    gte_rtv0();
                    gte_stlvnl(&scratch->geometry.corners[geometryIndex]);
                    scratch->geometry.corners[geometryIndex].vx += Gp_GridParams->viewCoord->workm.t[0];
                    scratch->geometry.corners[geometryIndex].vy += Gp_GridParams->viewCoord->workm.t[1];
                    scratch->geometry.corners[geometryIndex].vz += Gp_GridParams->viewCoord->workm.t[2];
                }

                isOutside = 0;
                // Test the centre against the outward planes of the face edges.
                for (geometryIndex = cornerCount - WORLD_COLLISION_SPHERE_GRID_TRIANGLE_CORNERS; geometryIndex < cornerCount * 2 - WORLD_COLLISION_SPHERE_GRID_TRIANGLE_CORNERS; geometryIndex++) {
                    WORLD_COLLISION_BUILD_SPHERE_GRID_EDGE_PLANE(scratch, geometryIndex);

                    edgePlaneOffset = (scratch->geometry.edgeWork.vx * scratch->geometry.corners[Gp_FaceEdgePairs[geometryIndex].endCornerIndex].vx +
                                       scratch->geometry.edgeWork.vy * scratch->geometry.corners[Gp_FaceEdgePairs[geometryIndex].endCornerIndex].vy +
                                       scratch->geometry.edgeWork.vz * scratch->geometry.corners[Gp_FaceEdgePairs[geometryIndex].endCornerIndex].vz) >>
                                      12;
                    edgeDistance = (s16)(((scratch->geometry.edgeWork.vx * scratch->centre.vx + scratch->geometry.edgeWork.vy * scratch->centre.vy +
                                           scratch->geometry.edgeWork.vz * scratch->centre.vz) >>
                                          12) -
                                         edgePlaneOffset);
                    if (edgeDistance - WORLD_COLLISION_SPHERE_GRID_EDGE_TOLERANCE > 0) {
                        goto mark_outside;
                    }
                }
            edges_done:
                if (isOutside) {
                    faceIds++;
                    continue;
                }

                contact = body->context.contacts;
                for (;;) {
                    contactFlags = contact->flags;
                    if (!(contactFlags & WORLD_COLLISION_CONTACT_OCCUPIED)) {
                        goto fill;
                    }
                    if (contactFlags & WORLD_COLLISION_CONTACT_LAST) {
                        goto done;
                    }
                    contact++;
                }

            next_face:
                faceIds++;
            }
        }
    }

done:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_WorldCollisionGridSphereScratch));
}

/// Tests a motion sphere against its prepared grid cell and merges its contacts.
///
/// `body` must be kind 4 and `motionContext` its live context. `scratch` belongs to
/// the caller: its centre and gridCell are initialized, with centre, direction
/// and the grid's composed view transform in one query frame. This walk borrows
/// the geometry tail without reserving or releasing the caller's block.
/// Out-of-range cells and NULL lists are ignored. Face lists end at
/// `WORLD_COLLISION_GRID_CELL_END`;
/// mesh indices and triangle/quad edges must be valid and nondegenerate, with
/// edge deltas meeting the SDK normalization bounds. Contacts are writable and
/// LAST-terminated. Stops on exhaustion; changes GTE state, retains no pointers.
static inline void _worldCollisionCollideMotionSphereCell(const WorldCollisionBody* body, _WorldCollisionGridSphereScratch* scratch,
                                                          const WorldCollisionMotionContext* motionContext)
{
    // The direction/normal dot is Q24; the grid-normal component is Q12.
    // Contact matching ignores the low-byte surface class but retains response bits.
    enum {
        WORLD_COLLISION_MOTION_SPHERE_MIN_GRID_NORMAL_Y  = -0xDDA,
        WORLD_COLLISION_MOTION_SPHERE_MAX_RECEDING_DOT   = 0x280000,
        WORLD_COLLISION_MOTION_SPHERE_CONTACT_CLASS_MASK = ~0xFF
    };
    const WorldCollisionGridFace* face;
    WorldCollisionContact*        contact;
    const s16*                    faceIds;
    s32                           faceIndex;
    s32                           geometryIndex;
    s32                           cornerCount;
    s32                           isOutside;
    s32                           edgeDistance;
    s32                           facePlaneOffset;
    s32                           edgePlaneOffset;
    u16                           planeDistanceBits;
    s32                           edgeContactFlag;
    s32                           faceContactKey;
    u16                           contactFlags;

    if ((u16)scratch->gridCell.vx < Gp_GridParams->cellCountX && (u16)scratch->gridCell.vz < Gp_GridParams->cellCountZ) {
        faceIds = Gp_GridParams->cellFaceIds[scratch->gridCell.vx * Gp_GridParams->cellCountZ + scratch->gridCell.vz];
        if (faceIds != NULL) {
            for (;;) {
                faceIndex = *faceIds;
                if (faceIndex == WORLD_COLLISION_GRID_CELL_END) {
                    return;
                }
                face = &Gp_GridParams->faces[faceIndex];
                if (face->vertexIndices[0] == 0 && face->vertexIndices[1] == 0) {
                    faceIds++;
                    continue;
                }
                if (Gp_GridParams->normals[face->normalIndex].vy < WORLD_COLLISION_MOTION_SPHERE_MIN_GRID_NORMAL_Y) {
                    faceIds++;
                    continue;
                }

                // Transform this face into the sphere's query space.
                gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
                gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[0]]);
                gte_rtv0();
                gte_stlvnl(&scratch->geometry.corners[0]);
                scratch->geometry.corners[0].vx += Gp_GridParams->viewCoord->workm.t[0];
                scratch->geometry.corners[0].vy += Gp_GridParams->viewCoord->workm.t[1];
                scratch->geometry.corners[0].vz += Gp_GridParams->viewCoord->workm.t[2];

                gte_ldv0(&Gp_GridParams->normals[face->normalIndex]);
                gte_rtv0();
                gte_stlvnl(&scratch->geometry.faceNormal);

                if (motionContext->motionDirection.vx * scratch->geometry.faceNormal.vx + motionContext->motionDirection.vy * scratch->geometry.faceNormal.vy +
                        motionContext->motionDirection.vz * scratch->geometry.faceNormal.vz >
                    WORLD_COLLISION_MOTION_SPHERE_MAX_RECEDING_DOT) {
                    faceIds++;
                    continue;
                }

                facePlaneOffset = (scratch->geometry.faceNormal.vx * scratch->geometry.corners[0].vx + scratch->geometry.faceNormal.vy * scratch->geometry.corners[0].vy +
                                   scratch->geometry.faceNormal.vz * scratch->geometry.corners[0].vz) >>
                                  12;
                planeDistanceBits = ((scratch->geometry.faceNormal.vx * scratch->centre.vx + scratch->geometry.faceNormal.vy * scratch->centre.vy +
                                      scratch->geometry.faceNormal.vz * scratch->centre.vz) >>
                                     12) -
                                    facePlaneOffset;
                if (body->radius < ABS((s16)planeDistanceBits)) {
                    faceIds++;
                    continue;
                }

                cornerCount = (face->vertexIndices[ARRAY_SIZE(face->vertexIndices) - 1] != WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? (s32)ARRAY_SIZE(face->vertexIndices) : WORLD_COLLISION_SPHERE_GRID_TRIANGLE_CORNERS;
                for (geometryIndex = 1; geometryIndex < cornerCount; geometryIndex++) {
                    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[geometryIndex]]);
                    gte_rtv0();
                    gte_stlvnl(&scratch->geometry.corners[geometryIndex]);
                    scratch->geometry.corners[geometryIndex].vx += Gp_GridParams->viewCoord->workm.t[0];
                    scratch->geometry.corners[geometryIndex].vy += Gp_GridParams->viewCoord->workm.t[1];
                    scratch->geometry.corners[geometryIndex].vz += Gp_GridParams->viewCoord->workm.t[2];
                }

                edgeContactFlag = 0;
                isOutside       = 0;
                // Test the centre against the outward planes of the face edges.
                for (geometryIndex = cornerCount - WORLD_COLLISION_SPHERE_GRID_TRIANGLE_CORNERS; geometryIndex < cornerCount * 2 - WORLD_COLLISION_SPHERE_GRID_TRIANGLE_CORNERS; geometryIndex++) {
                    WORLD_COLLISION_BUILD_SPHERE_GRID_EDGE_PLANE(scratch, geometryIndex);

                    edgePlaneOffset = (scratch->geometry.edgeWork.vx * scratch->geometry.corners[Gp_FaceEdgePairs[geometryIndex].endCornerIndex].vx +
                                       scratch->geometry.edgeWork.vy * scratch->geometry.corners[Gp_FaceEdgePairs[geometryIndex].endCornerIndex].vy +
                                       scratch->geometry.edgeWork.vz * scratch->geometry.corners[Gp_FaceEdgePairs[geometryIndex].endCornerIndex].vz) >>
                                      12;
                    edgeDistance = (s16)(((scratch->geometry.edgeWork.vx * scratch->centre.vx + scratch->geometry.edgeWork.vy * scratch->centre.vy +
                                           scratch->geometry.edgeWork.vz * scratch->centre.vz) >>
                                          12) -
                                         edgePlaneOffset);
                    if (edgeDistance - body->radius > 0) {
                        isOutside = 1;
                        break;
                    }
                    if (edgeDistance > 0) {
                        if ((s16)planeDistanceBits < 0) {
                            isOutside = 1;
                            break;
                        }
                        edgeContactFlag = WORLD_COLLISION_CONTACT_GRID_EDGE;
                    }
                }
                // Merge equal normal/response classes, retaining the first surface key.
                if (!isOutside) {
                    contact = body->context.motion->contacts;
                    for (;;) {
                        contactFlags = contact->flags;
                        if (contactFlags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                            if ((contact->key.value & WORLD_COLLISION_MOTION_SPHERE_CONTACT_CLASS_MASK) == (edgeContactFlag | WORLD_COLLISION_CONTACT_GRID)) {
                                if (contact->response.direction.vx == Gp_GridParams->normals[face->normalIndex].vx &&
                                    contact->response.direction.vy == Gp_GridParams->normals[face->normalIndex].vy &&
                                    contact->response.direction.vz == Gp_GridParams->normals[face->normalIndex].vz) {
                                    if (contact->distance < (s32)body->radius - (s16)planeDistanceBits) {
                                        contact->distance = body->radius - planeDistanceBits;
                                    }
                                    break;
                                }
                            }
                        } else {
                            contact->flags              = contactFlags | WORLD_COLLISION_CONTACT_OCCUPIED;
                            contact->distance           = body->radius - planeDistanceBits;
                            faceContactKey              = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                            contact->key.value          = edgeContactFlag | faceContactKey;
                            contact->point.vx           = 0;
                            contact->point.vy           = 0;
                            contact->point.vz           = 0;
                            contact->response.direction = Gp_GridParams->normals[face->normalIndex];
                            break;
                        }
                        if (contact->flags & WORLD_COLLISION_CONTACT_LAST) {
                            return;
                        }
                        contact++;
                    }
                }
                faceIds++;
            }
        }
    }
}

void worldCollisionCollideMotionSphereGrid(const WorldCollisionBody* body)
{
    _WorldCollisionGridSphereScratch*  scratch;
    const WorldCollisionMotionContext* motionContext;

    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridSphereScratch);
    motionContext = body->context.motion;
    worldCollisionGetBodyComposedPosition(body, &scratch->centre);
    worldCollisionViewToCell(&scratch->centre, &scratch->gridCell);

    _worldCollisionCollideMotionSphereCell(body, scratch, motionContext);

    SCRATCH_STACK_RELEASE_BYTES(sizeof(_WorldCollisionGridSphereScratch));
}

s32 worldCollisionIntersectGridFace(s32 faceIndex, const VECTOR endpoints[2], SVECTOR directionAndHit[2], const WorldCollisionBody* bodyQuery)
{
    // Direction/normal products use Q12; edge tolerances use game units.
    enum {
        WORLD_COLLISION_GRID_FACE_FRACTION_BITS        = 12,
        WORLD_COLLISION_GRID_FACE_PROBE_EDGE_TOLERANCE = 5,
        WORLD_COLLISION_GRID_FACE_BODY_EDGE_TOLERANCE  = 10,
        WORLD_COLLISION_GRID_FACE_TRIANGLE_CORNERS     = 3
    };
    _WorldCollisionGridRayScratch* scratchEnd;
    _WorldCollisionGridRayScratch* scratch;
    const WorldCollisionGridFace*  face;
    s32                            polygonIndex;
    s32                            cornerCount;
    s16                            facePlaneOffset;
    s32                            directionNormalDot;
    s32                            distanceAlongReverseDirection;
    s32                            edgePlaneOffset;
    s32                            edgeDistance;
    s32                            edgeTolerance;

    scratchEnd                 = SCRATCH_STACK_CURSOR(_WorldCollisionGridRayScratch);
    SCRATCH_STACK_CURSOR(void) = scratchEnd - 1;
    face                       = &Gp_GridParams->faces[faceIndex];
    scratch                    = scratchEnd - 1;

    // Transform the face into the segment's query space.
    gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[0]]);
    gte_rtv0();
    gte_stlvnl(&scratch->corners[0]);
    scratch->corners[0].vx += Gp_GridParams->viewCoord->workm.t[0];
    scratch->corners[0].vy += Gp_GridParams->viewCoord->workm.t[1];
    scratch->corners[0].vz += Gp_GridParams->viewCoord->workm.t[2];

    gte_ldv0(&Gp_GridParams->normals[face->normalIndex]);
    gte_rtv0();
    gte_stlvnl(&scratch->faceNormal);

    // Accept only a crossing from the negative side towards the face normal.
    facePlaneOffset = (scratch->faceNormal.vx * scratch->corners[0].vx + scratch->faceNormal.vy * scratch->corners[0].vy +
                       scratch->faceNormal.vz * scratch->corners[0].vz) >>
                      WORLD_COLLISION_GRID_FACE_FRACTION_BITS;
    directionNormalDot = (scratch->faceNormal.vx * directionAndHit[0].vx + scratch->faceNormal.vy * directionAndHit[0].vy + scratch->faceNormal.vz * directionAndHit[0].vz) >> WORLD_COLLISION_GRID_FACE_FRACTION_BITS;

    if (directionNormalDot >= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
        return 0;
    }
    if ((((scratch->faceNormal.vx * endpoints[1].vx + scratch->faceNormal.vy * endpoints[1].vy + scratch->faceNormal.vz * endpoints[1].vz) >> WORLD_COLLISION_GRID_FACE_FRACTION_BITS) -
         facePlaneOffset) <= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
        return 0;
    }
    distanceAlongReverseDirection = -(((((scratch->faceNormal.vx * endpoints[0].vx + scratch->faceNormal.vy * endpoints[0].vy + scratch->faceNormal.vz * endpoints[0].vz) >> WORLD_COLLISION_GRID_FACE_FRACTION_BITS) -
                                        facePlaneOffset)
                                       << WORLD_COLLISION_GRID_FACE_FRACTION_BITS)) /
                                    directionNormalDot;
    if (distanceAlongReverseDirection >= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
        return 0;
    }

    // Publish the halfword-truncated plane hit before the edge tests can reject it.
    directionAndHit[1].vx = endpoints[0].vx + ((directionAndHit[0].vx * distanceAlongReverseDirection) >> WORLD_COLLISION_GRID_FACE_FRACTION_BITS);
    directionAndHit[1].vy = endpoints[0].vy + ((directionAndHit[0].vy * distanceAlongReverseDirection) >> WORLD_COLLISION_GRID_FACE_FRACTION_BITS);
    directionAndHit[1].vz = endpoints[0].vz + ((directionAndHit[0].vz * distanceAlongReverseDirection) >> WORLD_COLLISION_GRID_FACE_FRACTION_BITS);

    cornerCount = (face->vertexIndices[WORLD_COLLISION_GRID_FACE_TRIANGLE_CORNERS] == WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? WORLD_COLLISION_GRID_FACE_TRIANGLE_CORNERS : (s32)ARRAY_SIZE(scratch->corners);

    gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
    for (polygonIndex = 1; polygonIndex < cornerCount; polygonIndex++) {
        gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[polygonIndex]]);
        gte_rtv0();
        gte_stlvnl(&scratch->corners[polygonIndex]);
        scratch->corners[polygonIndex].vx += Gp_GridParams->viewCoord->workm.t[0];
        scratch->corners[polygonIndex].vy += Gp_GridParams->viewCoord->workm.t[1];
        scratch->corners[polygonIndex].vz += Gp_GridParams->viewCoord->workm.t[2];
    }

    // Triangle and quad edge ranges share the corner-pair table.
    // Distances past each outward plane are deliberately truncated to halfwords.
    for (polygonIndex = cornerCount - WORLD_COLLISION_GRID_FACE_TRIANGLE_CORNERS; polygonIndex < cornerCount * 2 - WORLD_COLLISION_GRID_FACE_TRIANGLE_CORNERS; polygonIndex++) {
        _worldCollisionBuildGridEdgePlaneNormal(scratch, polygonIndex);

        edgePlaneOffset = (scratch->edgeWork.vx * scratch->corners[Gp_FaceEdgePairs[polygonIndex].endCornerIndex].vx +
                           scratch->edgeWork.vy * scratch->corners[Gp_FaceEdgePairs[polygonIndex].endCornerIndex].vy +
                           scratch->edgeWork.vz * scratch->corners[Gp_FaceEdgePairs[polygonIndex].endCornerIndex].vz) >>
                          WORLD_COLLISION_GRID_FACE_FRACTION_BITS;
        edgeTolerance = WORLD_COLLISION_GRID_FACE_PROBE_EDGE_TOLERANCE;
        edgeDistance  = ((scratch->edgeWork.vx * directionAndHit[1].vx + scratch->edgeWork.vy * directionAndHit[1].vy + scratch->edgeWork.vz * directionAndHit[1].vz) >> WORLD_COLLISION_GRID_FACE_FRACTION_BITS) -
                       edgePlaneOffset;
        if (bodyQuery != NULL) {
            edgeTolerance = WORLD_COLLISION_GRID_FACE_BODY_EDGE_TOLERANCE;
        }
        if ((s16)edgeDistance - edgeTolerance > 0) {
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
            return 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
    return 1;
}
