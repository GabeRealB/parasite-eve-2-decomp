/* Actor joint rotation and obstacle-contact steering/push routines.
 *
 * All five functions are static: every file that uses them carries its own
 * copies, including retained helpers without callers. Each file owns its
 * contact scratch allocation; ActorContact_GetScratchPosition supplies its
 * SVECTOR view, including when the allocation retains trailing bytes.
 *
 * Include this header in the prologue, then either actor_contacts.inc.c for
 * the whole run or each actor_contacts_<name>.inc.c a file carries, at its
 * position. Actor 210600 retains two copies of the run in separate files,
 * each with private scratch.
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

static void ActorContact_TurnJoint(GfxCoord* coord, s16 yaw);
static s32  ActorContact_FindPush(GfxCoord* coord, WorldCollisionContact* recs, s16 count);
static s32  ActorContact_Steer(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static s32  ActorContact_PushContact(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2);
static s32  ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);

/* View of the including overlay's contact scratch allocation. */
static inline SVECTOR* ActorContact_GetScratchPosition(void);

#endif /* SRC_SHARED_ACTOR_CONTACTS_H */
