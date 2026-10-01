/* The animation driver of the Odd Stranger (actor_401000, actor_401800): a
 * 19-slot rig plus a second blend context. State handlers request a clip change (cross-fade through a 45x45
 * transition table, or a hard restart) and can overlay a secondary clip on
 * slots 1-10, mixed by weight. Each frame the driver also eases a head yaw
 * toward its target by up to 0x100 and turns joints 5 and 2 by 2/3 and 1/2 of
 * it, then plays the state's animation sound event. A 0x7D3 message puts it
 * into a scripted pose.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ODD_STRANGER_H
#define SRC_SHARED_ODD_STRANGER_H

#include "common.h"

#include "actors/actor.h"

/// Animation-state view shared by actor_401000 and actor_401800. Each actor
/// owns a larger work block; these are the fields their animation driver uses.
typedef struct OddStrangerRigWork {
    /* 0x000 */ s16            field_0; // Actor state
    /* 0x002 */ s16            field_2; // State step, -1 on entry
    /* 0x004 */ byte           pad_4[0x18];
    /* 0x01C */ ActorAnimRig19 rig;
    /* 0x458 */ ActorAnimRig19 blend;
    /* 0x894 */ byte           pad_894[4];
    /* 0x898 */ s16            field_898; // Pending clip change: cross-fade, reset, running
    /* 0x89A */ s16            field_89A; // Nonzero while pose blending is active
    /* 0x89C */ s16            field_89C; // Previous clip
    /* 0x89E */ s16            field_89E; // Requested clip
    /* 0x8A0 */ u16            field_8A0; // Frames since the clip change
    /* 0x8A2 */ s16            field_8A2; // Body slot rate
    /* 0x8A4 */ byte           pad_8A4[2];
    /* 0x8A6 */ s16            field_8A6; // Blend clip change request
    /* 0x8A8 */ s16            field_8A8; // Blend clip
    /* 0x8AA */ u16            field_8AA; // Blend slot rate
    /* 0x8AC */ s16            field_8AC; // Blend weight
    /* 0x8AE */ s16            field_8AE; // Target head yaw
    /* 0x8B0 */ s16            field_8B0; // Current head yaw
    /* 0x8B2 */ byte           pad_8B2[2];
    /* 0x8B4 */ s32            field_8B4; // Last animation event index
} OddStrangerRigWork;
STATIC_ASSERT_SIZEOF(OddStrangerRigWork, 0x8B8);

/// Animation view of the same task work block: the pose context at 0x1C and
/// its slot array, then the blend context the actor keeps beside it. The
/// arrays cover the slot indices the blended tick `oddStrangerTickBlended`
/// walks; the offsets all match `Actor01900AnimWork`, and the tail overlays
/// the work block's `field_8A2` / `field_8A4` (the state the slot writes step
/// down by 3).
typedef struct OddStrangerAnimWork {
    /* 0x000 */ byte           pad_0[0x1C];
    /* 0x01C */ ActorAnimRig19 rig;
    /* 0x458 */ ActorAnimRig19 blend;
    /* 0x894 */ byte           pad_894[0xE];
    /* 0x8A2 */ s16            field_8A2;
    /* 0x8A4 */ s16            field_8A4;
    /* 0x8A6 */ byte           pad_8A6[4];
    /* 0x8AA */ s16            field_8AA;
    /* 0x8AC */ s16            field_8AC;
} OddStrangerAnimWork;

#include "main/task_types.h"

void oddStrangerTickBlended(Task* arg0);
void oddStrangerDrive(Task* arg0);
s32  oddStrangerPlayMessage(Task* arg0, s32 arg1, AnimationPlayRequest* arg2);

s32 oddStrangerAnimEvent(OddStrangerRigWork* work);

/* The two packages animate the first footstep clip (2) with its cues on
 * different frames. Each defines them before including this header:
 *
 *   ODD_STRANGER_CLIP2_STEP_A   frame of the first step cue (0x400A0002)
 *   ODD_STRANGER_CLIP2_STEP_B   frame of the second step cue (0x400A0001)
 */

#endif /* SRC_SHARED_ODD_STRANGER_H */
