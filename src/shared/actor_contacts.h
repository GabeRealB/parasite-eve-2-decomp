/* Actor joint rotation and obstacle-contact steering/push routines.
 *
 * All five functions are static: every file that uses them carries its own
 * copies, including retained helpers without callers. Each file owns its
 * contact scratch allocation; ActorContact_GetScratchPosition supplies its
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

/// Return type of `ActorContact_Steer`.
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
static ACTOR_CONTACT_STEER_RESULT ActorContact_Steer(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static s32                        ActorContact_PushContact(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2);
static s32                        ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);

/* View of the including overlay's contact scratch allocation. */
static inline SVECTOR* ActorContact_GetScratchPosition(void);

#endif /* SRC_SHARED_ACTOR_CONTACTS_H */
