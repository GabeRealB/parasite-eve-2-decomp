#ifndef ACTORS_SHARED_80138EFC_H
#define ACTORS_SHARED_80138EFC_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

/// The enemy's work block: allocated zeroed by the spawn handler and parked in
/// `Task::work`, then handed to every state handler and message handler of the
/// entry. It holds the model's root coordinate, two animation contexts each with
/// a slot per model part and a pose buffer, the four display nodes with a
/// three-entry contact table apiece, the model's light and colour matrices, and
/// the per-state counters and latches the handlers share.
typedef struct ActorsShared80138efcWork {
    /// Root coordinate the model's second part is parented to.
    /* 0x000 */ GsCOORDINATE2 coord;
    /// Body animation. Slot 1's `flags` report the clip's end and its control
    /// entries to the state handlers, and its `curSet` is the motion playing.
    /* 0x050 */ GpAnimCtx  anim;
    /* 0x064 */ GpAnimSlot slots[21];
    /* 0x3AC */ byte       poses[0x150];
    /// Second animation, blended into the first by `field_BA2`.
    /* 0x4FC */ GpAnimCtx  anim2;
    /* 0x510 */ GpAnimSlot slots2[21];
    /* 0x858 */ byte       poses2[0x150];
    /// Display nodes: the first on the model's root, the last on part 3, and
    /// between them the pair on parts 12 and 8 that the handlers switch on and
    /// off through the top two bits of `flags`.
    /* 0x9A8 */ GpObj objs[4];
    /// One three-entry contact table per display node. The first is resolved
    /// against the world; the last is scanned for the hits the enemy takes.
    /* 0xA28 */ GpRec18 contacts[4][3];
    /* 0xB48 */ MATRIX  lightMtx;
    /* 0xB68 */ MATRIX  colorMtx;
    /// Actor id, `placeKey >> 12`. Stored as a word; the sound calls read its
    /// low byte into bits 8-15 of their ids.
    /* 0xB88 */ u32 actorId;
    /// Countdown a state arms and decrements per frame: `func_actor_104900_80138D58`
    /// posts 0x64 into it and acts when it reaches zero, and this unit's
    /// 0x80138E34 arms 0xA.
    /* 0xB8C */ s16 field_B8C;
    /// Walking offset `func_actor_104900_80138B5C` steps 0x30 back toward zero
    /// from either end of the +-0x30 band, one frame at a time.
    /* 0xB8E */ s16 field_B8E;
    /// Yaw toward actor slot 0 in this model's frame, written by ActorsShared801357f0.
    /* 0xB90 */ s16 field_B90;
    /* 0xB92 */ s16 field_B92;
    /// Decay counters the 0x80138B5C body subtracts from - 0x400 for the axis
    /// pair and 0x100 for the next two - clamping each at zero.
    /* 0xB94 */ s16 field_B94;
    /* 0xB96 */ s16 field_B96;
    /* 0xB98 */ s16 field_B98;
    /* 0xB9A */ s16 field_B9A;
    /* 0xB9C */ s16 field_B9C;
    /// Distance-mapped pitch the 0x80136230 body writes on the first frame:
    /// 0 inside 0x384, 0x2000 past 0xA8C, otherwise `((dist - 0x384) << 9) / 100`.
    /// `field_B94` ramps toward it while the countdown sits in `[0x1E, 0x2B]`.
    /* 0xB9E */ u16 field_B9E;
    /// Visibility the 0x7D5 message last asked for; the handler acts only
    /// when the request changes it.
    /* 0xBA0 */ s8 field_BA0;
    /// The enemy link node's `state.b.flags`, saved while the model is hidden
    /// and put back when it is shown again.
    /* 0xBA1 */ u8 field_BA1;
    /* 0xBA2 */ s8 field_BA2;
    /* 0xBA3 */ s8 field_BA3;
    /// Motion id armed for the frame; every sibling writes a different pair
    /// here (0xB/0xE here, 0x15/0x16 next door, 5 in the setup handler).
    /* 0xBA4 */ s8 field_BA4;
    /// Set alongside `field_BA4` to ask for the motion to be restarted.
    /* 0xBA5 */ s8 field_BA5;
    /// Set when the trigger at `field_BA9` fires. The 0x801339B0 handler tests
    /// it with `lbu` before staging the 0xA state, so it is unsigned even
    /// though its neighbours at 0xBA4..0xBA9 are signed.
    /* 0xBA6 */ u8 field_BA6;
    /// Index into the dispatcher's 26-entry handler table, read there with
    /// `lb` and multiplied by 4.
    /* 0xBA7 */ s8 state;
    /// Run-once latch: 0 means the state has not started yet. Read signed for
    /// the test but re-read unsigned for the increment, hence the `(u8)` cast
    /// at the one place it is stepped.
    /* 0xBA8 */ s8 field_BA8;
    /// Trigger this handler consumes: nonzero sets `field_BA6`, resets the
    /// countdown at `field_B9C` and selects state 0xF.
    /* 0xBA9 */ s8 field_BA9;
    /// Byte counter the 0x801366E8 body steps by one when it leaves the 0xBA8
    /// latch at 3; the 0x801339B0 handler zeroes it on the frame it arms, and
    /// again alongside the state it stages.
    /* 0xBAA */ u8 field_BAA;
    /// Compared against 1 (`lbu`) by the 0x80138B5C body, which skips its whole
    /// decay block while it is set.
    /* 0xBAB */ u8 field_BAB;
    /// Lunge-exit gate read with `lbu` and compared against 0xB. Unsigned,
    /// unlike the signed byte that follows it.
    /* 0xBAC */ u8 field_BAC;
    /// Frame within the lunge. Armed to -1, then stepped with an unsigned
    /// read (`lbu`/`sb`) and tested signed (`lb`) against 1 and 0x2E.
    /* 0xBAD */ s8 field_BAD;
    /* 0xBAE */ u8 field_BAE;
    /// Armed alongside `state` by the 0x80138E34 body, which the dispatcher's
    /// trigger then compares against. The 0x80138B5C body gates the `field_B8E`
    /// step on it (`lbu`).
    /* 0xBAF */ u8 field_BAF;
    /// Placement of the hit sparks, on the model's part 4.
    /* 0xBB0 */ GpEffArg effArg;
    /// Sound variant bit the slot's setup body at 0x8013279C picks from the
    /// spawn record, 0 or 1. `func_actor_104900_80138D58` and the bodies at
    /// 0x80132D78 / 0x80136230 shift it into bit 22 of the id they hand
    /// `SndEvt_EnqueueType6`.
    /* 0xBB8 */ u8 field_BB8;
    /* 0xBB9 */ u8 field_BB9;
    /* 0xBBA */ u8 field_BBA;
    /// Entry id of the placement the enemy was spawned from; 0x31 selects the
    /// second parameter set and a scaled model.
    /* 0xBBB */ u8  field_BBB;
    /* 0xBBC */ s16 field_BBC;
    /// Frames the hit sparks keep being re-spawned for.
    /* 0xBBE */ s16 field_BBE;
    /// Spark effect id of the last hit, from the hit id's first parameter.
    /* 0xBC0 */ s32 field_BC0;
    /// Frames before another hit is taken, from the hit id's second parameter.
    /* 0xBC4 */ s32 field_BC4;
    /// One-shot latch for the 0x13F4 dispatch. Stays clear until the area id
    /// is 0x0518, the player is alive, and that message has been sent.
    /* 0xBC8 */ u8 field_BC8;
    /// Read as a byte and compared against 1, then against `field_BA9`: the
    /// 0x80138A2C body only runs its restart path when both are 1.
    /* 0xBC9 */ u8   field_BC9;
    /* 0xBCA */ byte pad_BCA[0x2];
} ActorsShared80138efcWork;
STATIC_ASSERT_SIZEOF(ActorsShared80138efcWork, 0xBCC);

/// Block `func_actor_104900_80134780` hands every state handler in `$a3`, one
/// frame of the actor's own stack. The dispatcher fills `pan` and `depth` from
/// the model's second per-part `GsCOORDINATE2` (0x50 into
/// `Task::field_2C->field_8`) right before the indirect call, storing each as a
/// halfword; the handlers read the low byte,
/// so the pair is laid out as bytes here. Which of them writes what is per
/// handler - the 0x80138D58 body writes only 0x64.
typedef struct ActorsShared80138efcArg {
    /// Scratch position. The dispatcher writes the root part's world
    /// translation here, lowered by 0x320, and hands it to
    /// `Gp_UpdateActorColor`.
    /* 0x00 */ VECTOR pos;
    /// Scratch vector. The dispatcher builds its effect offsets here for
    /// `Gp_SpawnEff`, rotating the splash offset through a part's world
    /// matrix and then into view space with `mtx`.
    /* 0x10 */ SVECTOR vec;
    /* 0x18 */ byte    pad_18[0x8];
    /// Transpose of `Gfx_ViewWorldMtx`, refreshed on the frames the splash
    /// check runs.
    /* 0x20 */ MATRIX mtx;
    /// Pose buffers of the two animation contexts. While `field_BA2` is
    /// nonzero the first context steps into `poses[0]`, the second into
    /// `poses[1]`, and `Gp_AnimWritePoseBlend` weights the pair by it.
    /* 0x40 */ GpAnimPose poses[2];
    /* 0x60 */ s8         pan;
    /* 0x61 */ byte       pad_61[0x1];
    /* 0x62 */ s8         depth;
    /* 0x63 */ byte       pad_63[0x1];
    /* 0x64 */ s8         field_64;
} ActorsShared80138efcArg;
STATIC_ASSERT_SIZEOF(ActorsShared80138efcArg, 0x68);

typedef void (*ActorsShared80138efcState)(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work,
                                          ActorsShared80138efcArg* arg);

typedef struct ActorsShared80138efcStateTable {
    ActorsShared80138efcState funcs[26];
} ActorsShared80138efcStateTable;

/// Arms the motion pair for the current sub-state when `field_BA8` is still
/// clear, and switches to state 0xF when `field_BA9` is set.
void ActorsShared80138efc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);

#endif // ACTORS_SHARED_80138EFC_H
