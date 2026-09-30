/* An enemy that hides behind a see-through cloak. It fades its model's
 * translucency in and out to appear and vanish, and queues a frame-capture
 * pass at its depth so the cloaked body refracts the scene. It reappears
 * behind the player (0x5AA back along the player's heading) or when the player
 * enters one of the room's trigger boxes. It strikes, or grabs the player in a
 * hold that can end in an instant kill. While it aims, it draws a red beam
 * from its fourth part. Hits of kinds 1/2 make the cloak flicker and spark.
 * Low HP drops it to its knees, where it writhes. Death either collapses it or
 * shrinks it away. A dispatcher runs one of twelve sequences chosen by
 * field_6CC. A step-forward helper, per-animation sound cues and a hold cue
 * timer complete the frame. The dead state uses inlined copies of the reseed,
 * tint and shadow, which move into the shared header.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_CLOAKED_STALKER_H
#define SRC_SHARED_CLOAKED_STALKER_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

void stalkerPickHitReaction(Task* arg0, s32 arg1);
void stalkerBoxScanSeq(Task* arg0);
s32  stalkerPlayerInBox(Task* arg0);
void stalkerPlaceTarget(Task* arg0);
void stalkerStrikeSeq(Task* arg0);
void stalkerCloakFade(Task* arg0);
void stalkerLightFlinchSeq(Task* arg0);
void stalkerHeavyFlinchSeq(Task* arg0);
void stalkerKneelSeq(Task* arg0);
void stalkerKneelHitSeq(Task* arg0);
void stalkerCollapseDeathSeq(Task* arg0);
void stalkerPlayAnimCues(Task* arg0);
void stalkerDrawAimBeam(Task* arg0);
void stalkerDeadState(Enemy* arg0, Task* arg1);
void stalkerFrameState(Enemy* arg0, Task* arg1);
void stalkerRunSequence(Task* arg0);
void stalkerApplyScale(Task* arg0);
void stalkerKneelDeathSeq(Task* arg0);
void stalkerStepForward(Task* arg0);
void stalkerDrawShadow(Task* arg0);
void stalkerHoldCueTimer(Task* arg0);
void stalkerQueueFrameCapture(GfxCoord* arg0, s32 arg1);

/* Defined by each package. */
void stalkerTakeHits(Task* arg0);
void stalkerUpdateTint(Task* arg0);
void stalkerTickAnim(Task* arg0);
void stalkerIdleSeq(Task* arg0);
void stalkerGrabSeq(Task* arg0);
void stalkerBoxApproachSeq(Task* arg0);
void stalkerRecoverSeq(Task* arg0);

static inline void stalkerTickAnimInline(Task* arg0);
static inline void stalkerDrawShadowInline(Task* arg0);

#endif /* SRC_SHARED_CLOAKED_STALKER_H */
