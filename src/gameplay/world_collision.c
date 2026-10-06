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
                    _other                = _worldCollisionGetObjectContacts(_otherAddress.object);                                       \
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

s32 Gp_PendingObj4CFlag;

s32 Gp_PairHandler1(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind);

s32 Gp_PairHandler3(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind);

static void Gp_RunPairHandler(WorldCollisionBody* node);

static void _worldCollisionRecordPairContact(WorldCollisionBody* receivingBody, WorldCollisionBody* contactedBody, _WorldCollisionPairContact* contact);

/// Contact table selected by the body's shape, or NULL for a body without one.
static inline WorldCollisionContact* _worldCollisionGetObjectContacts(WorldCollisionBody* obj)
{
    WorldCollisionContact* recs = NULL;

    switch (obj->flags & WORLD_COLLISION_BODY_KIND_MASK) {
        case WORLD_COLLISION_BODY_NONE:
            break;
        case WORLD_COLLISION_BODY_SPHERE:
            recs = obj->context.contacts;
            break;
        case WORLD_COLLISION_BODY_CONTACT_PROXY:
            recs = obj->context.contactOwner->context.contacts;
            break;
        case WORLD_COLLISION_BODY_CAPSULE:
            recs = obj->context.capsule->contacts;
            break;
        case WORLD_COLLISION_BODY_MOTION_SPHERE:
            recs = obj->context.motion->contacts;
            break;
    }
    return recs;
}

WorldCollisionPairHandler Gp_PairHandlers[5] = {
    worldCollisionPairNop,
    Gp_PairHandler1,
    worldCollisionPairNop,
    Gp_PairHandler3,
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
        Gp_UpdatePlayerMove();
        Gp_CollideListGrid(Gp_ObjList0);
        Gp_CollideListGrid(Gp_ObjList1);
        Gp_CollideListGrid(Gp_ObjList2);
        Gp_CollideListGrid(Gp_ObjList3);
        Gp_CollideListGrid(Gp_ObjList4);
        Gp_CollideListGrid(Gp_ObjList7);
        Gp_CollideListGrid(Gp_ObjList8);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList3);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList8);
        Gp_RunPairHandler(Gp_ObjList0);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList6);
        Gp_CollideLists(Gp_ObjList2, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList2, Gp_ObjList8);
        Gp_RunPairHandler(Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList3, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList4, Gp_ObjList8);
        if (Gp_PendingObj4CFlag != 0) {
            Gp_ClearPendingObj4C();
        }
        func_800E0608(Gp_ObjList0, WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_KIND_MASK,
                      WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_MOTION_SPHERE);
        if (gGameSession->suppressViewTriggers == 0) {
            func_800E06AC(Gp_ObjList0, WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_KIND_MASK,
                          WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_MOTION_SPHERE);
        }
    }
}

static void Gp_RunPairHandler(WorldCollisionBody* node)
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

    for (; node != NULL; node = node->next) {
        flags = node->flags;
        other = node->next;
        if (flags & WORLD_COLLISION_BODY_PAIR_ENABLED) {
            rowIndex = (node->flags & WORLD_COLLISION_BODY_KIND_MASK) - WORLD_COLLISION_BODY_SPHERE;
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
                            Gp_PairHandlers[handlerIndex](node, other, handlerIndex);
                        } else {
                            Gp_PairHandlers[handlerIndex](other, node, handlerIndex);
                        }
                    }
                }
            }
        }
    }
}

/// Records an identified pair contact in the receiving body's initialized table.
static void _worldCollisionRecordPairContact(WorldCollisionBody* receivingBody, WorldCollisionBody* contactedBody, _WorldCollisionPairContact* contact)
{
    WorldCollisionContact* rec;

    if (contactedBody->key == 0) {
        return;
    }

    rec = _worldCollisionGetObjectContacts(receivingBody);
    if (rec == NULL) {
        return;
    }

    WORLD_COLLISION_CLAIM_CONTACT(rec, receivingBody);

    rec->key.value = contactedBody->key;
    rec->distance  = contact->distance;
    rec->point     = contact->point;
    rec->response  = contact->response;
}

s32 Gp_PairHandler1(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind)
{
    u8*                           head;
    _WorldCollisionSphereScratch* block;
    s32                           ret;

    // Reserve the scratch and place both centres in world space.
    head                                               = SCRATCH_STACK_CURSOR(u8);
    block                                              = (_WorldCollisionSphereScratch*)(head - sizeof(_WorldCollisionSphereScratch));
    SCRATCH_STACK_CURSOR(_WorldCollisionSphereScratch) = block;
    worldCollisionGetBodyComposedPosition(arg0, &block->centre0);
    worldCollisionGetBodyComposedPosition(arg1, &block->centre1);

    ret                   = 0;
    block->centreDelta.vx = block->centre0.vx - block->centre1.vx;
    block->centreDelta.vy = block->centre0.vy - block->centre1.vy;
    block->centreDelta.vz = block->centre0.vz - block->centre1.vz;
    // Reject an X or Z separation beyond a signed halfword.
    if ((ABS(block->centreDelta.vx) > 0x7FFF) || (ABS(block->centreDelta.vz) > 0x7FFF)) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionSphereScratch);
        return 0;
    }

    block->radiusSum = arg0->radius + arg1->radius;
    if (block->centreDelta.vx * block->centreDelta.vx + block->centreDelta.vy * block->centreDelta.vy + block->centreDelta.vz * block->centreDelta.vz < block->radiusSum * block->radiusSum) {
        // Each body records the other centre, a zero response and the radius sum.
        block->contact.point.vx              = block->centre1.vx;
        block->contact.point.vy              = block->centre1.vy;
        block->contact.point.vz              = block->centre1.vz;
        block->contact.response.direction.vx = 0;
        block->contact.response.direction.vy = 0;
        block->contact.response.direction.vz = 0;
        block->contact.distance              = block->radiusSum;
        _worldCollisionRecordPairContact(arg0, arg1, &block->contact);
        ret = 1;

        block->contact.point.vx              = block->centre0.vx;
        block->contact.point.vy              = block->centre0.vy;
        block->contact.point.vz              = block->centre0.vz;
        block->contact.response.direction.vx = 0;
        block->contact.response.direction.vy = 0;
        block->contact.response.direction.vz = 0;
        block->contact.distance              = block->radiusSum;
        _worldCollisionRecordPairContact(arg1, arg0, &block->contact);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionSphereScratch);
    return ret;
}

s32 Gp_PairHandler3(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind)
{
    _WorldCollisionBodyAddress     sourceAddress;
    u8*                            head;
    _WorldCollisionCapsuleScratch* block;
    WorldCollisionCapsule*         rec;
    s32                            proj;
    s32                            ret;
    s32                            tapered;
    VECTOR*                        sphereCenter;
    s32                            radiusSquared;
    s32                            dx0;
    s32                            dy0;
    s32                            dz0;
    s32                            dx1;
    s32                            dy1;
    s32                            dz1;
    s32                            dx2;
    s32                            dy2;
    s32                            dz2;
    s32                            dx3;
    s32                            dy3;
    s32                            dz3;
    s32                            dx4;
    s32                            dy4;
    s32                            dz4;
    s32                            len;
    s32                            plen;
    s32                            r0;
    s32                            r1;
    s32                            tmp; // combined radius on a straight capsule, taper ratio less 1.0 on a tapered one

    // Address the centre from the cursor before the reservation is stored.
    // Taking &block->sphereCenter after that store does not keep this order.
    head                       = SCRATCH_STACK_CURSOR(u8);
    sphereCenter               = &((_WorldCollisionCapsuleScratch*)(head - sizeof(_WorldCollisionCapsuleScratch)))->sphereCenter;
    SCRATCH_STACK_CURSOR(void) = head - sizeof(_WorldCollisionCapsuleScratch);
    rec                        = arg1->context.capsule;
    block                      = (_WorldCollisionCapsuleScratch*)(head - sizeof(_WorldCollisionCapsuleScratch));
    // Place the sphere centre and the capsule segment in world space.
    worldCollisionGetBodyComposedPosition(arg0, sphereCenter);
    func_800DEC80(arg1, block->ends, &block->segmentDirection, 0);

    block->work.radiusAlongSegment.vx = (block->segmentDirection.vx * arg0->radius) >> 12;
    block->work.radiusAlongSegment.vy = (block->segmentDirection.vy * arg0->radius) >> 12;
    block->work.radiusAlongSegment.vz = (block->segmentDirection.vz * arg0->radius) >> 12;

    block->extendedEnd0.vx = block->ends[0].vx + block->work.radiusAlongSegment.vx;
    block->extendedEnd0.vy = block->ends[0].vy + block->work.radiusAlongSegment.vy;
    block->extendedEnd0.vz = block->ends[0].vz + block->work.radiusAlongSegment.vz;
    block->extendedEnd1.vx = block->ends[1].vx - block->work.radiusAlongSegment.vx;
    block->extendedEnd1.vy = block->ends[1].vy - block->work.radiusAlongSegment.vy;
    block->extendedEnd1.vz = block->ends[1].vz - block->work.radiusAlongSegment.vz;

    // Reject a centre outside the segment extended by the sphere radius.
    dx0 = (block->sphereCenter.vx - block->extendedEnd0.vx) * block->segmentDirection.vx;
    dy0 = (block->sphereCenter.vy - block->extendedEnd0.vy) * block->segmentDirection.vy;
    dz0 = (block->sphereCenter.vz - block->extendedEnd0.vz) * block->segmentDirection.vz;
    ret = 0;
    if (dx0 + dy0 + dz0 > 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
        return 0;
    }

    dx1  = (block->sphereCenter.vx - block->extendedEnd1.vx) * block->segmentDirection.vx;
    dy1  = (block->sphereCenter.vy - block->extendedEnd1.vy) * block->segmentDirection.vy;
    dz1  = (block->sphereCenter.vz - block->extendedEnd1.vz) * block->segmentDirection.vz;
    proj = (dx1 + dy1 + dz1) >> 12;
    if (proj <= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
        return 0;
    }

    r1      = rec->end0Radius;
    tapered = r1 != rec->end1Radius;
    // Equal radii share one combined radius. A taper restores geometric
    // endpoint 0 when a contact replaced it, then interpolates the radius.
    if (!tapered) {
        tmp                 = arg0->radius + r1;
        block->axisPoint.vx = (u16)block->extendedEnd1.vx + ((block->segmentDirection.vx * proj) >> 12);
        block->axisPoint.vy = (u16)block->extendedEnd1.vy + ((block->segmentDirection.vy * proj) >> 12);
        block->axisPoint.vz = (u16)block->extendedEnd1.vz + ((block->segmentDirection.vz * proj) >> 12);
        proj                = tmp;
    } else {
        if (arg1->flags & (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT)) {
            gte_SetRotMatrix(&arg1->coord->workm);
            block->work.localEndpoint.vx = (u16)rec->ends[0].vx + (u16)arg1->pos.vx;
            block->work.localEndpoint.vy = (u16)rec->ends[0].vy + (u16)arg1->pos.vy;
            block->work.localEndpoint.vz = (u16)rec->ends[0].vz + (u16)arg1->pos.vz;
            gte_ldv0(&block->work.localEndpoint);
            gte_rtv0();
            gte_stlvnl(&block->ends[0]);
            block->ends[0].vx += (arg1->coord)->workm.t[0];
            block->ends[0].vy += (arg1->coord)->workm.t[1];
            block->ends[0].vz += (arg1->coord)->workm.t[2];
        }

        block->segmentDelta.vx = block->ends[0].vx - block->ends[1].vx;
        block->segmentDelta.vy = block->ends[0].vy - block->ends[1].vy;
        block->segmentDelta.vz = block->ends[0].vz - block->ends[1].vz;
        dx2                    = block->segmentDelta.vx * block->segmentDelta.vx;
        dy2                    = block->segmentDelta.vy * block->segmentDelta.vy;
        dz2                    = block->segmentDelta.vz * block->segmentDelta.vz;
        len                    = SquareRoot0(dx2 + dy2 + dz2);

        block->work.projectionOffset.vx = (block->segmentDirection.vx * proj) >> 12;
        block->work.projectionOffset.vy = (block->segmentDirection.vy * proj) >> 12;
        block->work.projectionOffset.vz = (block->segmentDirection.vz * proj) >> 12;
        dx3                             = block->work.projectionOffset.vx * block->work.projectionOffset.vx;
        dy3                             = block->work.projectionOffset.vy * block->work.projectionOffset.vy;
        dz3                             = block->work.projectionOffset.vz * block->work.projectionOffset.vz;
        proj                            = len;
        plen                            = SquareRoot0(dx3 + dy3 + dz3);

        r0                  = (rec->end0Radius << 12) / rec->end1Radius;
        proj                = (plen << 12) / proj;
        tmp                 = r0 - 0x1000;
        r1                  = arg0->radius;
        proj                = r1 + ((((tmp * proj) >> 12) * rec->end1Radius >> 12) + rec->end1Radius);
        block->axisPoint.vx = (u16)block->work.projectionOffset.vx + (u16)block->extendedEnd1.vx;
        block->axisPoint.vy = (u16)block->work.projectionOffset.vy + (u16)block->extendedEnd1.vy;
        block->axisPoint.vz = (u16)block->work.projectionOffset.vz + (u16)block->extendedEnd1.vz;
    }

    block->work.centreToAxis.vx = (u16)block->axisPoint.vx - (u16)block->sphereCenter.vx;
    block->work.centreToAxis.vy = (u16)block->axisPoint.vy - (u16)block->sphereCenter.vy;
    block->work.centreToAxis.vz = (u16)block->axisPoint.vz - (u16)block->sphereCenter.vz;
    dx4                         = block->work.centreToAxis.vx * block->work.centreToAxis.vx;
    dy4                         = block->work.centreToAxis.vy * block->work.centreToAxis.vy;
    dz4                         = block->work.centreToAxis.vz * block->work.centreToAxis.vz;
    radiusSquared               = proj * proj;
    if (dx4 + dy4 + dz4 < radiusSquared) {
        // The sphere records endpoint 1 and the segment direction. The capsule
        // records the axis point, or the sphere centre when the capsule tapers.
        block->contact.distance              = 0;
        block->contact.point.vx              = (u16)block->ends[1].vx;
        block->contact.point.vy              = (u16)block->ends[1].vy;
        block->contact.point.vz              = (u16)block->ends[1].vz;
        block->contact.response.direction.vx = (u16)block->segmentDirection.vx;
        block->contact.response.direction.vy = (u16)block->segmentDirection.vy;
        block->contact.response.direction.vz = (u16)block->segmentDirection.vz;
        _worldCollisionRecordPairContact(arg0, arg1, &block->contact);
        if (!tapered) {
            block->contact.point = block->axisPoint;
        } else {
            block->contact.point.vx = (u16)block->sphereCenter.vx;
            block->contact.point.vy = (u16)block->sphereCenter.vy;
            block->contact.point.vz = (u16)block->sphereCenter.vz;
        }
        if (arg1->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {
            sourceAddress.object                     = arg0;
            block->contact.response.bodyAddress.low  = sourceAddress.address;
            block->contact.response.bodyAddress.high = sourceAddress.address >> 16;
        } else {
            block->contact.response.direction.vx = 0;
            block->contact.response.direction.vy = 0;
        }
        block->contact.response.direction.vz = 0;
        block->contact.distance              = 0;
        _worldCollisionRecordPairContact(arg1, arg0, &block->contact);
        ret = 1;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
    return ret;
}

void Gp_CollideObjGrid(WorldCollisionBody* arg0)
{
    u8*                               head;
    _WorldCollisionGridSphereScratch* scratch;
    WorldCollisionGridFace*           face;
    WorldCollisionContact*            slot;
    s16*                              cell;
    s32                               id;
    s32                               i;
    s32                               n;
    s32                               outside;
    s32                               val;
    s32                               faceDot;
    s32                               edgeDot;
    u16                               dist;
    u16                               flags;

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - sizeof(_WorldCollisionGridSphereScratch);
    scratch                    = (_WorldCollisionGridSphereScratch*)(head - sizeof(_WorldCollisionGridSphereScratch));
    worldCollisionGetBodyComposedPosition(arg0, &scratch->centre);
    Gp_LocalToGrid((VECTOR3*)&scratch->centre, (SVECTOR3*)&scratch->gridCell);

    if ((u16)scratch->gridCell.vx < Gp_GridParams->cellCountX && (u16)scratch->gridCell.vz < Gp_GridParams->cellCountZ) {
        cell = Gp_GridParams->cellFaceIds[scratch->gridCell.vx * Gp_GridParams->cellCountZ + scratch->gridCell.vz];
        if (cell != NULL) {
            for (;;) {
                id = *cell;
                if (id == WORLD_COLLISION_GRID_CELL_END) {
                    goto done;
                }
                face = &Gp_GridParams->faces[id];
                if (face->vertexIndices[0] == 0 && face->vertexIndices[1] == 0) {
                    cell++;
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

                faceDot = (scratch->geometry.faceNormal.vx * scratch->geometry.corners[0].vx + scratch->geometry.faceNormal.vy * scratch->geometry.corners[0].vy +
                           scratch->geometry.faceNormal.vz * scratch->geometry.corners[0].vz) >>
                          12;
                dist = ((scratch->geometry.faceNormal.vx * scratch->centre.vx + scratch->geometry.faceNormal.vy * scratch->centre.vy +
                         scratch->geometry.faceNormal.vz * scratch->centre.vz) >>
                        12) -
                       faceDot;
                if (arg0->radius >= ABS((s16)dist)) {
                    goto edges;
                }
                goto next_face;

            mark_outside:
                outside = 1;
                goto edges_done;

            fill:
                slot->flags              = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                slot->distance           = arg0->radius - dist;
                slot->key.value          = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                slot->point.vx           = 0;
                slot->point.vy           = 0;
                slot->point.vz           = 0;
                slot->response.direction = Gp_GridParams->normals[face->normalIndex];
                goto next_face;

            edges:
                n = (face->vertexIndices[3] != WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[i]]);
                    gte_rtv0();
                    gte_stlvnl(&scratch->geometry.corners[i]);
                    scratch->geometry.corners[i].vx += Gp_GridParams->viewCoord->workm.t[0];
                    scratch->geometry.corners[i].vy += Gp_GridParams->viewCoord->workm.t[1];
                    scratch->geometry.corners[i].vz += Gp_GridParams->viewCoord->workm.t[2];
                }

                outside = 0;
                // Reuse the displacement slot for each edge's outward Q12 plane normal.
                for (i = n - 3; i < n * 2 - 3; i++) {
                    scratch->geometry.edgeWork.vx =
                        scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vx - scratch->geometry.corners[Gp_FaceEdgePairs[i].startCornerIndex].vx;
                    scratch->geometry.edgeWork.vy =
                        scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vy - scratch->geometry.corners[Gp_FaceEdgePairs[i].startCornerIndex].vy;
                    scratch->geometry.edgeWork.vz =
                        scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vz - scratch->geometry.corners[Gp_FaceEdgePairs[i].startCornerIndex].vz;
                    VectorNormal(&scratch->geometry.edgeWork, &scratch->geometry.edgeDirection);
                    gte_ldopv1(&scratch->geometry.faceNormal);
                    gte_ldopv2(&scratch->geometry.edgeDirection);
                    gte_op12();
                    gte_stlvnl(&scratch->geometry.edgeWork);

                    edgeDot = (scratch->geometry.edgeWork.vx * scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vx +
                               scratch->geometry.edgeWork.vy * scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vy +
                               scratch->geometry.edgeWork.vz * scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vz) >>
                              12;
                    val = (s16)(((scratch->geometry.edgeWork.vx * scratch->centre.vx + scratch->geometry.edgeWork.vy * scratch->centre.vy +
                                  scratch->geometry.edgeWork.vz * scratch->centre.vz) >>
                                 12) -
                                edgeDot);
                    if (val - 10 > 0) {
                        goto mark_outside;
                    }
                }
            edges_done:
                if (outside) {
                    cell++;
                    continue;
                }

                slot = arg0->context.contacts;
                for (;;) {
                    flags = slot->flags;
                    if (!(flags & WORLD_COLLISION_CONTACT_OCCUPIED)) {
                        goto fill;
                    }
                    if (flags & WORLD_COLLISION_CONTACT_LAST) {
                        goto done;
                    }
                    slot++;
                }

            next_face:
                cell++;
            }
        }
    }

done:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_WorldCollisionGridSphereScratch));
}

/// Tests the moving sphere of `arg0` against every face listed for its grid
/// cell and records or deepens a contact for each face it touches; stops when
/// the contact slots run out.
static inline void _worldCollisionCollideMovingSphereCell(WorldCollisionBody* arg0, _WorldCollisionGridSphereScratch* scratch,
                                                          WorldCollisionMotionContext* motionContext)
{
    WorldCollisionGridFace* face;
    WorldCollisionContact*  slot;
    s16*                    cell;
    s32                     id;
    s32                     i;
    s32                     n;
    s32                     outside;
    s32                     val;
    s32                     faceDot;
    s32                     edgeDot;
    u16                     dist;
    s32                     extra;
    s32                     faceKind;
    u16                     flags;

    if ((u16)scratch->gridCell.vx < Gp_GridParams->cellCountX && (u16)scratch->gridCell.vz < Gp_GridParams->cellCountZ) {
        cell = Gp_GridParams->cellFaceIds[scratch->gridCell.vx * Gp_GridParams->cellCountZ + scratch->gridCell.vz];
        if (cell != NULL) {
            for (;;) {
                id = *cell;
                if (id == WORLD_COLLISION_GRID_CELL_END) {
                    return;
                }
                face = &Gp_GridParams->faces[id];
                if (face->vertexIndices[0] == 0 && face->vertexIndices[1] == 0) {
                    cell++;
                    continue;
                }
                if (Gp_GridParams->normals[face->normalIndex].vy < -0xDDA) {
                    cell++;
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
                    0x280000) {
                    cell++;
                    continue;
                }

                faceDot = (scratch->geometry.faceNormal.vx * scratch->geometry.corners[0].vx + scratch->geometry.faceNormal.vy * scratch->geometry.corners[0].vy +
                           scratch->geometry.faceNormal.vz * scratch->geometry.corners[0].vz) >>
                          12;
                dist = ((scratch->geometry.faceNormal.vx * scratch->centre.vx + scratch->geometry.faceNormal.vy * scratch->centre.vy +
                         scratch->geometry.faceNormal.vz * scratch->centre.vz) >>
                        12) -
                       faceDot;
                if (arg0->radius < ABS((s16)dist)) {
                    cell++;
                    continue;
                }

                n = (face->vertexIndices[3] != WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[i]]);
                    gte_rtv0();
                    gte_stlvnl(&scratch->geometry.corners[i]);
                    scratch->geometry.corners[i].vx += Gp_GridParams->viewCoord->workm.t[0];
                    scratch->geometry.corners[i].vy += Gp_GridParams->viewCoord->workm.t[1];
                    scratch->geometry.corners[i].vz += Gp_GridParams->viewCoord->workm.t[2];
                }

                extra   = 0;
                outside = 0;
                // Reuse the displacement slot for each edge's outward Q12 plane normal.
                for (i = n - 3; i < n * 2 - 3; i++) {
                    scratch->geometry.edgeWork.vx =
                        scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vx - scratch->geometry.corners[Gp_FaceEdgePairs[i].startCornerIndex].vx;
                    scratch->geometry.edgeWork.vy =
                        scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vy - scratch->geometry.corners[Gp_FaceEdgePairs[i].startCornerIndex].vy;
                    scratch->geometry.edgeWork.vz =
                        scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vz - scratch->geometry.corners[Gp_FaceEdgePairs[i].startCornerIndex].vz;
                    VectorNormal(&scratch->geometry.edgeWork, &scratch->geometry.edgeDirection);
                    gte_ldopv1(&scratch->geometry.faceNormal);
                    gte_ldopv2(&scratch->geometry.edgeDirection);
                    gte_op12();
                    gte_stlvnl(&scratch->geometry.edgeWork);

                    edgeDot = (scratch->geometry.edgeWork.vx * scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vx +
                               scratch->geometry.edgeWork.vy * scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vy +
                               scratch->geometry.edgeWork.vz * scratch->geometry.corners[Gp_FaceEdgePairs[i].endCornerIndex].vz) >>
                              12;
                    val = (s16)(((scratch->geometry.edgeWork.vx * scratch->centre.vx + scratch->geometry.edgeWork.vy * scratch->centre.vy +
                                  scratch->geometry.edgeWork.vz * scratch->centre.vz) >>
                                 12) -
                                edgeDot);
                    if (val - arg0->radius > 0) {
                        outside = 1;
                        break;
                    }
                    if (val > 0) {
                        if ((s16)dist < 0) {
                            outside = 1;
                            break;
                        }
                        extra = WORLD_COLLISION_CONTACT_GRID_EDGE;
                    }
                }
                if (!outside) {
                    slot = arg0->context.motion->contacts;
                    for (;;) {
                        flags = slot->flags;
                        if (flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                            if ((slot->key.value & -0x100) == (extra | WORLD_COLLISION_CONTACT_GRID)) {
                                if (slot->response.direction.vx == Gp_GridParams->normals[face->normalIndex].vx &&
                                    slot->response.direction.vy == Gp_GridParams->normals[face->normalIndex].vy &&
                                    slot->response.direction.vz == Gp_GridParams->normals[face->normalIndex].vz) {
                                    if (slot->distance < (s32)arg0->radius - (s16)dist) {
                                        slot->distance = arg0->radius - dist;
                                    }
                                    break;
                                }
                            }
                        } else {
                            slot->flags              = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                            slot->distance           = arg0->radius - dist;
                            faceKind                 = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                            slot->key.value          = extra | faceKind;
                            slot->point.vx           = 0;
                            slot->point.vy           = 0;
                            slot->point.vz           = 0;
                            slot->response.direction = Gp_GridParams->normals[face->normalIndex];
                            break;
                        }
                        if (slot->flags & WORLD_COLLISION_CONTACT_LAST) {
                            return;
                        }
                        slot++;
                    }
                }
                cell++;
            }
        }
    }
}

void Gp_CollideObjGridDir(WorldCollisionBody* arg0)
{
    u8*                               head;
    _WorldCollisionGridSphereScratch* scratch;
    WorldCollisionMotionContext*      motionContext;

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - sizeof(_WorldCollisionGridSphereScratch);
    scratch                    = (_WorldCollisionGridSphereScratch*)(head - sizeof(_WorldCollisionGridSphereScratch));
    motionContext              = arg0->context.motion;
    worldCollisionGetBodyComposedPosition(arg0, &scratch->centre);
    Gp_LocalToGrid((VECTOR3*)&scratch->centre, (SVECTOR3*)&scratch->gridCell);

    _worldCollisionCollideMovingSphereCell(arg0, scratch, motionContext);

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
