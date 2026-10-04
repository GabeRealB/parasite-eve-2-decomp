/* Pose helpers the Zebra Stalker (actor_400600) and the Ivory Stalker
 * (actor_405800) share. It leaps and lands with a slam that raises dust, and carries
 * two child models parented to root parts 10 and 7. The helpers turn it toward
 * a point, read a part's view-space position, light it from its body part and
 * rebuild its root rotation from pitch, yaw and roll. The shared behaviour
 * covers the animation request (play, blend or restart a clip, tick the
 * slots, scale the frame counter, report the clip done), the pending-action
 * dispatch, the state handlers that wait on a clip, right the Stalker from
 * its back or pick its range, the hold release, the wall-contact distance and
 * the footstep windows of clip 4.
 *
 * `StalkerZebraIvoryWork` is the work block the fragments see at `Task::work`.
 * The library does not define it. The two enemies' work blocks differ in size
 * and in where most of their members sit, so each package aliases its own
 * work type to that name after including this header and before its first
 * fragment, and the fragments compile against that layout. They reach only
 * the members below, which both packages declare with the same name and type:
 *
 *   pose        `pitch`, `yaw`, `roll` (s16 root angles, 4096ths of a turn);
 *               `anchorPos` (SVECTOR3, the pinned part's view-space position)
 *   animation   `rig` (ActorAnimRig18), of which they drive slots 1 to 17
 *               through `rig.anim` and `rig.slots`; the request
 *               `animRequest`, `animClip`, `animStep`, `animBlend`,
 *               `animPlaying` and its frame `animFrame` (s16)
 *   states      `state`, `subState`, `timer`, `playerDistance` (s16);
 *               `countdown` (u16)
 *   reactions   `pendingAction`, `pendingArmed` (s16); `roomCommand` (u8)
 *   flags       `holding`, `holdKilledPlayer`, `leftArmOut`, `rightArmOut`,
 *               `onCeiling`, `onBack` (u8)
 *   wall probe  `capsuleBody` (WorldCollisionBody), `capsuleContacts`
 *               (WorldCollisionContact[8]), `distanceMode` (u8)
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_STALKER_ZEBRA_IVORY_H
#define SRC_SHARED_STALKER_ZEBRA_IVORY_H

#define STALKER_ZEBRA 1
#define STALKER_IVORY 2
#ifndef STALKER_ZEBRA_IVORY_KIND
#error "define STALKER_ZEBRA_IVORY_KIND (STALKER_ZEBRA or STALKER_IVORY) before including stalker_zebra_ivory.h"
#endif

/* Per kind: the sound bank of the clip-4 footstep cues (cue | 1, cue | 2); and
 * where the two builds' shared code differs -
 *   RIGHTING_PINS_PART  righting itself first pins part 0xE to its view spot
 *   REBLEND_SAME_CLIP   a request for the clip already playing blends again
 *   PIN_UPDATES_ROOT    pinning a part recomposes the root afterwards (else it
 *                       marks the root dirty before measuring)
 *   TIMER_BASE          the type of the random timer's base
 *   TIMER_ZERO_STOPS    a zero base stops the timer instead of seeding it */
#if STALKER_ZEBRA_IVORY_KIND == STALKER_ZEBRA
#define STALKER_ZEBRA_IVORY_STEP_SOUNDS        0x40060000
#define STALKER_ZEBRA_IVORY_RIGHTING_PINS_PART 1
#define STALKER_ZEBRA_IVORY_REBLEND_SAME_CLIP  0
#define STALKER_ZEBRA_IVORY_PIN_UPDATES_ROOT   1
#define STALKER_ZEBRA_IVORY_TIMER_BASE         s32
#define STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS   0
#else
#define STALKER_ZEBRA_IVORY_STEP_SOUNDS        0x40050000
#define STALKER_ZEBRA_IVORY_RIGHTING_PINS_PART 0
#define STALKER_ZEBRA_IVORY_REBLEND_SAME_CLIP  1
#define STALKER_ZEBRA_IVORY_PIN_UPDATES_ROOT   0
#define STALKER_ZEBRA_IVORY_TIMER_BASE         s16
#define STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS   1
#endif

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"

#include "main/task_types.h"

/// Values of the work block's `animRequest`.
enum {
    STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND   = 1, // blend into `animClip` over `animBlend` frames
    STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART = 2, // cut straight to `animClip`
    STALKER_ZEBRA_IVORY_ANIM_REQUEST_PLAYING = 3  // `animClip` has been applied and is playing
};

/// Values of the work block's `pendingAction`, the reaction a hit asks for.
enum {
    STALKER_ZEBRA_IVORY_PENDING_NONE      = 0,
    STALKER_ZEBRA_IVORY_PENDING_LIGHT     = 1, // light recoil (state 3)
    STALKER_ZEBRA_IVORY_PENDING_HEAVY     = 2, // heavy recoil (state 4)
    STALKER_ZEBRA_IVORY_PENDING_STATUS    = 3, // status hold (state 5); on the ceiling, knocked off it (state 0xE)
    STALKER_ZEBRA_IVORY_PENDING_BLAST     = 4, // heavy recoil (state 4); a blast that kills bursts the body
    STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN = 5  // knockdown (state 0xF); on the ceiling, knocked off it (state 0xE)
};

void stalkerZebraIvoryTurnToward(Task* arg0, SVECTOR* target, s32 step);
void stalkerZebraIvoryReadPartViewXZ(Task* task, s16 index, SVECTOR3* out);
void stalkerZebraIvoryUpdateColor(Task* task);
void stalkerZebraIvoryApplyRotation(Task* arg0);
void stalkerZebraIvoryStepClip4(Task* arg0);
void stalkerZebraIvoryRightItself(Task* arg0);
s32  stalkerZebraIvoryTakePending(Task* arg0);
s32  stalkerZebraIvoryWallDistance(Task* arg0);
void stalkerZebraIvoryPinPartXZ(Task* arg0, s16 index, SVECTOR3* pos);
void stalkerZebraIvoryWaitClipThenRest(Task* arg0);
void stalkerZebraIvoryReleaseHold(Task* arg0);
void stalkerZebraIvoryRunSubStates(Task* arg0);
void stalkerZebraIvorySetMoveMode(Task* arg0, s32 arg1, u16* arg2, s32 arg3);
void stalkerZebraIvoryRestartClip(Task* arg0);
void stalkerZebraIvoryResumeClip(Task* arg0);
void stalkerZebraIvoryPickRange(Task* arg0);
void stalkerZebraIvoryAnimateUntilDone(Task* arg0);
void stalkerZebraIvoryWaitClip(Task* arg0);
void stalkerZebraIvoryClearQueued(Task* arg0);
void stalkerZebraIvorySeedTimer(Task* arg0, STALKER_ZEBRA_IVORY_TIMER_BASE base);
void stalkerZebraIvoryDropCapsuleGrid(Task* arg0);
void stalkerZebraIvoryTickAnim(Task* arg0);
void stalkerZebraIvoryPlayClip(Task* arg0, s16 clip, s16 step);
s32  stalkerZebraIvoryClipDone(Task* arg0);
s32  stalkerZebraIvoryTakeArmedPending(Task* arg0);
void stalkerZebraIvoryBlendClip(Task* arg0);
s16  stalkerZebraIvoryScaleFrame(Task* arg0, s16 frame);

static __inline__ void stalkerZebraIvoryTickAnimInline(Task* arg0);
static __inline__ void stalkerZebraIvoryApplyRotationInline(Task* arg0);

#endif /* SRC_SHARED_STALKER_ZEBRA_IVORY_H */
