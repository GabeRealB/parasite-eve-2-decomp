/* Actor contact selection, joint rotation and obstacle-contact steering/push routines.
 *
 * All five fragment functions are static: every file that uses them carries its own
 * copies, including retained helpers without callers. Each file owns its
 * last contact-push correction; `_actorContactGetLastPushStep` supplies its
 * SVECTOR view, including when the allocation retains trailing bytes.
 *
 * Include this header in the prologue, then either actor_contacts.inc.c for
 * the whole run or each actor_contacts_<name>.inc.c a file carries, at its
 * position. Define `ACTOR_CONTACT_STEER_RESULT` first when that copy's return
 * type is not `s32`. Actor 210600 retains two copies of the run in separate
 * files, each with private scratch.
 */

#ifndef SRC_SHARED_ACTOR_CONTACTS_H
#define SRC_SHARED_ACTOR_CONTACTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/abs.h>
#include <psyq/memory.h>

#include "gte.h"
#include "overlay.h"
#include "types.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"

/* Scratch blocks private to the library. */

/// Values an `ActorContactBearingPushScratch::bearing` slot holds in place of
/// a bearing. A bearing is wrapped into [-0x800, 0x800], so neither can be
/// one.
enum {
    ACTOR_CONTACT_BEARING_PUSH_END  = 0x7FFE, // Slot of the keyless record that ends the table; no later slot is written
    ACTOR_CONTACT_BEARING_PUSH_SKIP = 0x7FFF  // Slot of a record that is not an obstacle
};

/// Scratch-stack block of the push that moves a coordinate away from the
/// obstacles among its contact records, along their bearings.
///
/// The push walks the contact table up to the caller's record count or the
/// first record with no key, and takes the bearing of each obstacle record
/// (key kind 0x10000 or 0x30000) from `origin`, relative to the direction the
/// coordinate faces. It then moves the coordinate a caller-chosen distance
/// directly away from each obstacle whose bearing lies within 0x400 of every
/// other obstacle's. Angles are 4096 to a turn.
///
/// `bearing` has one slot per contact record and the walk does not clamp the
/// count, so a caller must pass no more than its 16 slots. Nothing clears the
/// block when it is reserved, and `pushed` is read back after the release,
/// while the bytes are still intact.
typedef struct {
    MATRIX  rot;              // Yaw rotation built for a push, whose Z axis gives its direction
    byte    unknown_20[0x80]; // Reserved with the block and never accessed; role unproven
    SVECTOR delta;            // Offset whose bearing is being taken: from `origin` to an obstacle, then to `forward`. Later the step added to the coordinate
    SVECTOR origin;           // The coordinate's position carried up its parent chain, the point bearings are taken from
    SVECTOR forward;          // Point 0x1000 along the coordinate's Z axis, carried up the same chain; its bearing is the facing. Later the normalised direction of a push
    s32     kind;             // Kind bits of the current record's key
    s16     bearing[16];      // Per contact record: its bearing from `origin` less the facing, wrapped into [-0x800, 0x800], or an `ACTOR_CONTACT_BEARING_PUSH_` marker
    s16     i;                // Cursor over the records, then over the bearing being tested
    s16     j;                // Cursor over the bearings compared with `i`'s
    s16     diff;             // Wrapped difference between two bearings
    s16     pushed;           // 1 once a push was applied, 0 otherwise
} ActorContactBearingPushScratch;
STATIC_ASSERT_SIZEOF(ActorContactBearingPushScratch, 0xE4);

/* Interface for the including source. */

/// Transforms a local point through the complete coordinate chain for contact bearings.
///
/// Applies `startCoord->coord` and each ancestor's local matrix through the
/// parentless node, ending in the space above the topmost node. For ordinary
/// actors this includes the view transform and root offset, as contact points
/// do. A NULL `startCoord` leaves XYZ unchanged. The matrices use 12
/// fractional bits; point components use signed integer game-coordinate units
/// and narrow to signed halfwords after every node. `point->pad` is untouched.
/// Uses the local matrices directly without reading or refreshing `workm`.
///
/// Requires a live, acyclic parent chain, separate writable point storage,
/// and an initialized scratch-stack cursor with one free, word-aligned
/// `OverlayCoordChainScratch` below it, disjoint from the inputs. Borrows the
/// pointers only for this call and leaves the nodes unchanged. Restores the
/// scratch cursor on return; a nonempty chain changes GTE rotation/translation and working
/// registers; transform flags are stored but do not gate the result.
static __inline__ void _actorContactTransformPointToChainRoot(GfxCoord* startCoord, SVECTOR* point)
{
    OverlayCoordChainScratch* scratch;

    (SCRATCH_STACK_CURSOR(OverlayCoordChainScratch))[-1].coord = startCoord;
    SCRATCH_STACK_RESERVE_BLOCK(OverlayCoordChainScratch);
    scratch         = SCRATCH_STACK_CURSOR(OverlayCoordChainScratch);
    scratch->vec.vx = point->vx;
    scratch->vec.vy = point->vy;
    scratch->vec.vz = point->vz;

    // Include the topmost node; keep the per-node narrowing of the point.
    while (scratch->coord != NULL) {
        gte_SetTransMatrix(&scratch->coord->coord);
        gte_SetRotMatrix(&scratch->coord->coord);
        gte_ldv0(&scratch->vec);
        gte_rtv0tr();
        gte_stlvnl(&scratch->out);
        gte_stflg(&scratch->flag);
        scratch->vec.vx = scratch->out.vx;
        scratch->vec.vy = scratch->out.vy;
        scratch->vec.vz = scratch->out.vz;
        scratch->coord  = scratch->coord->parent;
    }
    point->vx = scratch->vec.vx;
    point->vy = scratch->vec.vy;
    point->vz = scratch->vec.vz;

    SCRATCH_STACK_RELEASE_BLOCK(OverlayCoordChainScratch);
}

/// Carries a scratch point into its current node's parent frame and advances the node.
///
/// Requires a live, non-NULL `scratch->coord` and writable, word-aligned
/// scratch storage. Narrows XYZ after the local transform, preserving pad;
/// stores the GTE flags without testing them and changes GTE working state.
static __inline__ void _actorContactCarryPointToParent(OverlayCoordChainScratch* scratch)
{
    gte_SetTransMatrix(&scratch->coord->coord);
    gte_SetRotMatrix(&scratch->coord->coord);
    gte_ldv0(&scratch->vec);
    gte_rtv0tr();
    gte_stlvnl(&scratch->out);
    gte_stflg(&scratch->flag);
    scratch->vec.vx = scratch->out.vx;
    scratch->vec.vy = scratch->out.vy;
    scratch->vec.vz = scratch->out.vz;
    scratch->coord  = scratch->coord->parent;
}

/// Transforms a point through the complete coordinate chain after staging its inputs.
///
/// Applies `startCoord->coord` and every ancestor's local matrix through the
/// parentless node, ending in the space above that node. Ordinary actor chains
/// include the view transform and root offset, as body-contact points do.
/// Matrices use 12 fractional bits; XYZ use signed integer game-coordinate
/// units and narrow to signed halfwords after each node. A NULL `startCoord`
/// leaves XYZ unchanged. `point->pad` is untouched; `workm` is neither read nor
/// refreshed, and transform flags are stored without gating the result.
///
/// Requires a live, acyclic parent chain, separate writable point storage,
/// and an initialized scratch-stack cursor with one free, word-aligned
/// `OverlayCoordChainScratch` below it, disjoint from the inputs. Stages the
/// starting node and XYZ there before publishing the reservation. Borrows
/// both pointers only for this call and leaves the nodes unchanged. Restores
/// the cursor on return; a nonempty chain changes GTE rotation, translation
/// and working registers.
static __inline__ void _actorContactTransformStagedPointToChainRoot(GfxCoord* startCoord, SVECTOR* point)
{
    OverlayCoordChainScratch* scratch;

    scratch         = SCRATCH_STACK_CURSOR(OverlayCoordChainScratch) - 1;
    scratch->coord  = startCoord;
    scratch->vec.vx = point->vx;
    scratch->vec.vy = point->vy;
    scratch->vec.vz = point->vz;

    SCRATCH_STACK_CURSOR(OverlayCoordChainScratch) = scratch;
    // Include the topmost node and retain the per-node halfword narrowing.
    while (scratch->coord != NULL) {
        _actorContactCarryPointToParent(scratch);
    }
    point->vx = scratch->vec.vx;
    point->vy = scratch->vec.vy;
    point->vz = scratch->vec.vz;

    SCRATCH_STACK_RELEASE_BLOCK(OverlayCoordChainScratch);
}

/// Returns the first attack key in a contact-table prefix and copies its point.
///
/// `contactCount` counts readable elements, from 0 to 32767. A zero key ends
/// the scan even if later entries are occupied; contact flags are not tested.
/// A miss returns zero and leaves `contactPointOut` untouched. A hit copies
/// XYZ in game units in the contact's cached-transform frame, without any
/// coordinate conversion, and leaves `pad` untouched. Requires separate
/// writable output and live input storage for the call. Borrows both pointers,
/// retains neither and does not consume or clear the contact.
static __inline__ s32 _actorContactFindAttack(SVECTOR* contactPointOut, const WorldCollisionContact* contacts, s16 contactCount)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            contactPointOut->vx = contacts[contactIndex].point.vx;
            contactPointOut->vy = contacts[contactIndex].point.vy;
            contactPointOut->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

/// Computes a horizontal room-axis push away from a body contact's centre.
///
/// `position` and `contact->point` use signed integer coordinates in the same
/// composed view frame. For a sphere pair, `contact->distance` is the sum of
/// the radii. The push depth is that distance less the SDK's X/Z length,
/// clamped to zero; its direction comes from the full XYZ separation, normalized
/// with 4096 for one unit and multiplied by the transpose of the active grid's
/// view basis. Translation is ignored; basis scale is retained. Dropping the
/// resulting Y component can make the horizontal push shorter than the depth.
///
/// Requires live inputs, writable output XYZ and an active `Gp_GridParams` with
/// an initialized `viewCoord->workm`. Coordinate differences must fit the SDK's
/// signed-halfword normalization inputs, and intermediate sums/products must fit
/// signed 32 bits; no range checks are made. X/Z products are shifted right by
/// 12 and narrowed to signed halfwords without saturation; Y is written as zero
/// and `pad` is untouched. All input reads precede output writes. Borrows the
/// pointers only for the call, reserves no scratch-stack storage and changes
/// GTE working registers even when the depth is zero.
static __inline__ void _actorContactCalcHorizontalPushback(const SVECTOR* position, const WorldCollisionContact* contact, SVECTOR* pushDelta)
{
    enum { ACTOR_CONTACT_DIRECTION_FRACTION_BITS = 12 };
    VECTOR delta;
    VECTOR normalizedDirection;
    s32    penetrationDepth;

    // Measure horizontal overlap before including height in the push direction.
    delta.vx         = position->vx - contact->point.vx;
    delta.vy         = 0;
    delta.vz         = position->vz - contact->point.vz;
    penetrationDepth = SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz);
    penetrationDepth = contact->distance - penetrationDepth;
    penetrationDepth = (penetrationDepth <= 0) ? 0 : penetrationDepth;

    // Remove the view basis from the full separation, then keep only room X/Z.
    delta.vx = position->vx - contact->point.vx;
    delta.vy = position->vy - contact->point.vy;
    delta.vz = position->vz - contact->point.vz;
    VectorNormal(&delta, &normalizedDirection);
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &normalizedDirection, &delta);
    pushDelta->vx = (penetrationDepth * delta.vx) >> ACTOR_CONTACT_DIRECTION_FRACTION_BITS;
    pushDelta->vy = 0;
    pushDelta->vz = (penetrationDepth * delta.vz) >> ACTOR_CONTACT_DIRECTION_FRACTION_BITS;
}

/// Return type of `_actorContactApplyAvoidancePushback`.
///
/// The body returns 0, or 1 when it saw a contact of kind 0x10000. Both
/// values fit either integer width; the type selects how a caller promotes
/// the return. The default is `s32`. A carrier defines another integer type
/// before including this header. `actor_01200` defines `s16`, so comparing
/// the return with 1 sign-extends. `actor_421600` shifts the `s32` return in
/// the caller and keeps the default.
#ifndef ACTOR_CONTACT_STEER_RESULT
#define ACTOR_CONTACT_STEER_RESULT s32
#endif

static void                       _actorRenderYawJointInWorld(GfxCoord* joint, s16 yawDelta);
static s32                        _actorContactFindLastObstaclePush(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount);
static ACTOR_CONTACT_STEER_RESULT _actorContactApplyAvoidancePushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, SVECTOR* pushDelta);
static s32                        _actorContactApplyGridPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount);
static s32                        ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);

/* Borrowed view of the including overlay's last contact-push correction. */
static inline SVECTOR* _actorContactGetLastPushStep(void);

#endif /* SRC_SHARED_ACTOR_CONTACTS_H */
