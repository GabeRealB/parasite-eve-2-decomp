/* Actor joint rotation and obstacle-contact steering/push routines.
 *
 * TurnJoint and PushContact are used by the including TU. All five functions
 * remain static, including retained helpers without callers. Each TU owns its
 * contact scratch allocation; ActorContact_GetScratchPosition supplies its
 * SVECTOR view, including when the allocation retains trailing bytes.
 *
 * Include this header in the prologue and actor_contacts.inc.c at the function
 * run. Actor 210600 retains two copies in separate TUs, each with private
 * scratch. Neither symbol-name nor linkage configuration is needed.
 */

#ifndef SRC_SHARED_ACTOR_CONTACTS_H
#define SRC_SHARED_ACTOR_CONTACTS_H

#include "types.h"

#include "main/coord.h"
#include "main/session_types.h"

/* Interface for the including source. */

static void ActorContact_TurnJoint(GfxCoord* coord, s16 yaw);

static s32 ActorContact_PushContact(GfxCoord* coord, GpRec18* rec, s16 arg2);

/* View of the including overlay's contact scratch allocation. */
static inline SVECTOR* ActorContact_GetScratchPosition(void);

#endif /* SRC_SHARED_ACTOR_CONTACTS_H */
