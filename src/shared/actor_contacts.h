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

static void                       ActorContact_TurnJoint(GfxCoord* coord, s16 yaw);
static s32                        ActorContact_FindPush(GfxCoord* coord, WorldCollisionContact* recs, s16 count);
static ACTOR_CONTACT_STEER_RESULT ActorContact_Steer(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static s32                        ActorContact_PushContact(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2);
static s32                        ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);

/* View of the including overlay's contact scratch allocation. */
static inline SVECTOR* ActorContact_GetScratchPosition(void);

#endif /* SRC_SHARED_ACTOR_CONTACTS_H */
