#include "common.h"

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_102300.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102300_80131E80;

/// First frame of each animation, indexed by `Actor102300Work::field_694`;
/// the state handlers offset it to get the frames their cues fire on.
extern s16 D_actor_102300_80135D64[];
/// The `Gp_PackPair` entry the lunge parks in the work block's 0x5E4 node.
extern GpU16Pair D_actor_102300_801477E4;
/// Base sound id of the lunge cue, ORed with the enemy's id nibble.
extern s32 D_actor_102300_80147918;

INCLUDE_RODATA("actors/nonmatchings/actor_102300/actor_102300", D_actor_102300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80131EA4);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_8013290C);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80132AF0);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80132C2C);

INCLUDE_RODATA("actors/nonmatchings/actor_102300/actor_102300", D_actor_102300_80131E80);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133008);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133100);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_8013332C);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_801334B8);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_801336C4);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133840);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133C10);

/// Per-frame tick for the enemy's lunge cycle, sharing the `field_6A8` state
/// with the rest of the overlay. State 0 measures the offset to the player
/// through a 0x10-byte `G_SCRATCH_HEAD` block: over the window from frame 0x22
/// to 0x26 of the current animation the enemy commits to the lunge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, aims
/// `field_6A4` at them every frame, raises the 0x5E4 node's 0x8000 flag on
/// frame 0x20 and queues the cue on frame 0x21, then hands over to state 1 on
/// animation 9 once the animation is past frame 0x27. State 1 waits for frame
/// 0x5E and moves on to state 2 on animation 4.
void func_actor_102300_801340B0(Actor102300* arg0)
{
    s16              startFrame;
    s16              state;
    s16              frame;
    s32*             scratch;
    s32              dz;
    s32              sound;
    s32              dx;
    s32              pan;
    u8*              head;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;
    delta                 = (VECTOR*)(head - 0x10);
    work                  = arg0->field_1C;
    state                 = work->field_6A8;
    self                  = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            ((VECTOR*)(head - 0x10))->vx = (s32)(Wip_SysConfig.field_4->t[0] - self->coord.t[0]);
            dz                           = Wip_SysConfig.field_4->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = D_actor_102300_80135D64[work->field_694];
            frame                        = work->field_698;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0x14;
            work->field_6A4 = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->field_698 == (D_actor_102300_80135D64[work->field_694] + 0x20)) {
                work->field_5E4.flags    = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.field_18 = Gp_PackPair(&D_actor_102300_801477E4, 0);
            }
            if (work->field_698 == (D_actor_102300_80135D64[work->field_694] + 0x21)) {
                sound = D_actor_102300_80147918 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan   = (s8)Gp_GetObjPan((GpObj38*)self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)self));
            }
            if (work->field_698 >= (D_actor_102300_80135D64[work->field_694] + 0x27)) {
                work->field_6A8       = 1;
                work->field_694       = 9;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x5E) {
                work->field_6A6 = 2;
                work->field_6A8 = 2;
                work->field_694 = 4;
            }
            break;
    }
    scratch   = (s32*)G_SCRATCH_HEAD;
    *scratch += 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80134338);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_801346CC);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80134CC0);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_8013509C);
