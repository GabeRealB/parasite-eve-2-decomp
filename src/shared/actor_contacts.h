/* Actor joint rotation and obstacle-contact steering/push routines.
 *
 * All five functions are static: every file that uses them carries its own
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
static s32                        ActorContact_FindPush(GfxCoord* coord, WorldCollisionContact* recs, s16 count);
static ACTOR_CONTACT_STEER_RESULT _actorContactApplyAvoidancePushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, SVECTOR* pushDelta);
static s32                        _actorContactApplyGridPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount);
static s32                        ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);

/* Borrowed view of the including overlay's last contact-push correction. */
static inline SVECTOR* _actorContactGetLastPushStep(void);

#endif /* SRC_SHARED_ACTOR_CONTACTS_H */
