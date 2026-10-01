/* The scripted Desert Chaser of actor_323000 and actor_323400: an 18-slot rig
 * with 0 HP, not lockable, that cutscenes show and hide through message 2005. Its animation driver seeds its slots from a per-
 * transition start-frame table and cross-fades a second animation context into
 * slots 1-10. It eases a torso twist spread over joints 2-4 and a head turn on
 * joint 10, and plays the sound its per-package cue step returns.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_DESERT_CHASER_H
#define SRC_SHARED_DESERT_CHASER_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/// The 0x934-byte work block the spawn handler allocates and hangs behind
/// `Task::work`. Only the fields the handlers touch are known: `field_4` is
/// the state-change flag every state handler tests, and `field_828` onwards
/// are the animation-state slots the state handlers seed and the tick keeps.
///
/// It holds two animation contexts, each an `AnimationContext`, its 18-slot array
/// and the 0x120-byte pose buffer `func_800B3F84` is handed as its arg3: the
/// main one at 0x1C and the blend one at 0x420. The light / colour matrices
/// the spawn handler binds to `TmdObject::lightMtx` / `colorMtx` sit after
/// the animation state.
typedef struct DesertChaserWork {
    /// Animation state, the index `func_actor_323000_801645A4` dispatches on;
    /// `func_actor_323000_80164A54` picks it from a message, and the message
    /// 0x7D3 handler `func_actor_323000_80164AF0` restarts it at 1.
    s16 field_0;
    /// State `func_actor_323000_801645A4` ran last frame; `field_4` is set
    /// when `field_0` differs from it.
    s16 field_2;
    s16 field_4;
    /// Frame counter `func_actor_323000_8016420C` advances to time its
    /// effects; zeroed when that state or `func_actor_323000_80164C58`
    /// starts.
    s16  field_6;
    byte pad_8[0xE];
    /// Yaw of the root coordinate as the placement handler
    /// `func_actor_323000_80164954` leaves it, read back from the matrix.
    s16              field_16;
    byte             pad_18[4];
    AnimationContext anim;
    AnimationSlot    slots[18];
    /// Pose buffer `func_800B3F84` takes as its arg3, `AnimationContext.poseBuffer`.
    byte             poses[0x120];
    AnimationContext blendAnim;
    AnimationSlot    blendSlots[18];
    byte             blendPoses[0x120];
    byte             pad_824[4];
    /// Animation-state slots the handlers seed and the tick keeps: the seed
    /// mode the tick acts on (1 re-seeds from the per-state table, 2 resets
    /// the slots, 3 runs), whether the blend context is live, the clip the
    /// slots were last seeded with and the one to seed next, and the slot
    /// rate.
    s16 field_828;
    s16 field_82A;
    s16 field_82C;
    s16 field_82E;
    u16 field_830;
    s16 field_832;
    s16 field_834;
    s16 field_836;
    s16 field_838;
    s16 field_83A;
    s16 field_83C;
    s16 field_83E;
    s16 field_840;
    s16 field_842;
    /// Turn angle the tick eases toward `field_840` and splits over the body
    /// joints; cleared by the spawn handler.
    s16  field_844;
    byte pad_846[2];
    /// Clip id each slot was last seen playing by `func_actor_323000_80163448`,
    /// indexed like `slots`; zeroed (18 entries) when no watched clip plays.
    s32  field_848[18];
    byte pad_890[4];
    /// Light / colour matrices `func_actor_323000_80163EA0` binds to the
    /// model.
    MATRIX light;
    MATRIX color;
    byte   pad_8D4[0x48];
    /// Three bytes `func_actor_323000_80164A54` takes from a message payload
    /// one at a time; nothing else in this overlay reads them.
    u8   field_91C;
    u8   field_91D;
    u8   field_91E;
    byte pad_91F[0x15];
} DesertChaserWork;
STATIC_ASSERT_SIZEOF(DesertChaserWork, 0x934);

/// 0x1C-byte block `func_actor_323000_801645A4` pushes on the scratch stack:
/// the model root's world position for `Gp_UpdateActorColor`, and the local
/// point walked up the coordinate chain into view space.
typedef struct DesertChaserTickScratch {
    VECTOR  pos;
    SVECTOR local;
    s32     pad_18;
} DesertChaserTickScratch;
STATIC_ASSERT_SIZEOF(DesertChaserTickScratch, 0x1C);

void desertChaserBlendTick(Task* task);
void desertChaserAnimTick(Task* task);
void desertChaserSpawn(Enemy* enemy, Task* task);
s32  desertChaserSetVisibility(Task* task, s32 arg1, s32 arg2);

/* Defined by each package. */
s32 desertChaserAnimCues(Task* task, DesertChaserWork* work);

void desertChaserFrameState(Enemy* enemy, Task* task);
void desertChaserPartEffect(Task* arg0, s16 part, s16 flags);
void desertChaserTask(Task* task);
void desertChaserHideState(Enemy* arg0, Task* arg1);
s32  desertChaserMsgPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
void desertChaserExit(Task* task);

#endif /* SRC_SHARED_DESERT_CHASER_H */
