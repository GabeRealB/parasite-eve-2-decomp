/* The code the Knight GOLEM (actor_402200) and the Bishop GOLEM (actor_403900)
 * share. It fades its model's translucency in and out to appear and vanish,
 * and queues a frame-capture pass at its depth so the translucent body
 * refracts the scene. It reappears
 * behind the player (0x5AA back along the player's heading) or when the player
 * enters one of the room's trigger boxes. It strikes, or grabs the player in a
 * hold that can end in an instant kill. While it aims, it draws a red beam
 * from its fourth part. Hits of kinds 1/2 make the translucency flicker and spark.
 * Low HP drops it to its knees, where it writhes. Death either collapses it or
 * shrinks it away. A dispatcher runs one of twelve sequences chosen by
 * field_6CC. A step-forward helper, per-animation sound cues and a hold cue
 * timer complete the frame. The dead state uses inlined copies of the reseed,
 * tint and shadow, which move into the shared header.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GOLEM_KNIGHT_BISHOP_H
#define SRC_SHARED_GOLEM_KNIGHT_BISHOP_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

void golemKnightBishopPickHitReaction(Task* arg0, s32 arg1);
void golemKnightBishopBoxScanSeq(Task* arg0);
s32  golemKnightBishopPlayerInBox(Task* arg0);
void golemKnightBishopPlaceTarget(Task* arg0);
void golemKnightBishopStrikeSeq(Task* arg0);
void golemKnightBishopTranslucencyFade(Task* arg0);
void golemKnightBishopLightFlinchSeq(Task* arg0);
void golemKnightBishopHeavyFlinchSeq(Task* arg0);
void golemKnightBishopKneelSeq(Task* arg0);
void golemKnightBishopKneelHitSeq(Task* arg0);
void golemKnightBishopCollapseDeathSeq(Task* arg0);
void golemKnightBishopPlayAnimCues(Task* arg0);
void golemKnightBishopDrawAimBeam(Task* arg0);
void golemKnightBishopDeadState(Enemy* arg0, Task* arg1);
void golemKnightBishopFrameState(Enemy* arg0, Task* arg1);
void golemKnightBishopRunSequence(Task* arg0);
void golemKnightBishopApplyScale(Task* arg0);
void golemKnightBishopKneelDeathSeq(Task* arg0);
void golemKnightBishopStepForward(Task* arg0);
void golemKnightBishopDrawShadow(Task* arg0);
void golemKnightBishopHoldCueTimer(Task* arg0);
void golemKnightBishopQueueFrameCapture(GfxCoord* arg0, s32 arg1);

/* Defined by each package. */
void golemKnightBishopTakeHits(Task* arg0);
void golemKnightBishopUpdateTint(Task* arg0);
void golemKnightBishopTickAnim(Task* arg0);
void golemKnightBishopIdleSeq(Task* arg0);
void golemKnightBishopGrabSeq(Task* arg0);
void golemKnightBishopBoxApproachSeq(Task* arg0);
void golemKnightBishopRecoverSeq(Task* arg0);

static inline void golemKnightBishopTickAnimInline(Task* arg0);
static inline void golemKnightBishopDrawShadowInline(Task* arg0);

#endif /* SRC_SHARED_GOLEM_KNIGHT_BISHOP_H */
