#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_8013454c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_107000_801334C8(Task* arg0, s32 arg1);
void func_actor_107000_80132E9C(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80131F0C);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", D_actor_107000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", D_actor_107000_80131E30);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132298);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132474);

// actor_104600 (func_actor_104600_801325D0), actor_204600
// (func_actor_204600_8014A5D0) and actor_207000 (func_actor_207000_8014A674)
// carry the same body, refused promotion because both of its remaining calls -
// func_actor_107000_80132E9C and func_actor_107000_801334C8 - are named in this
// overlay only, so one shared object could not link into the other three.

/// Per-frame handler of the caged specimen, dispatched on the reaction stage in
/// `field_2C8`: 1 is the live specimen, 2 the death throes. Every stage ends in
/// the shared epilogue, so the switch's default is a jump straight there.
///
/// Stage 1 ticks `field_2D0` down and, when it runs out, re-rolls it from
/// `Gp_LcgState` as `(state >> 16) % 100 + 0x50` frames - between 0x50 and 0xB3.
/// The re-roll also cues a sound event: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. As in `func_actor_107000_80132D8C`, the whole assignment *and* its
/// call are written out in both arms - the join the compiler builds from them is
/// what the original binary shows. The stage then re-arms `field_2BE`, switches
/// the animation to 2, runs the frame through `func_actor_107000_80132E9C` and
/// `ActorsShared8013454c`, and restarts `field_2BC` once it has spent 0x1D
/// frames on the id.
///
/// Stage 2 counts `field_2D4` up and advances `field_2AC` by 0xC8 a frame; on
/// the fifth frame the specimen is killed - `func_actor_107000_801334C8` runs
/// with a zero argument, the kill countdown is armed to 5, the reaction flag
/// `field_2B4` is cleared, the task state latches the stage it just ran, and the
/// enemy's `field_40` HP is zeroed.
void func_actor_107000_80132674(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor107000Work*)arg0->idMap;
    enemy = arg0->spawnArg2;
    mode  = work->field_2C8;
    switch (mode) {
        case 1:
            countdown       = work->field_2D0 - 1;
            work->field_2D0 = countdown;
            if ((countdown << 16) <= 0) {
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
                if (work->field_2D6 != 0) {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40460009;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402E0001;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            }
            work->field_2BE = 0x14;
            work->field_2B8 = 2;
            func_actor_107000_80132E9C(arg0);
            ActorsShared8013454c(arg0);
            if ((s16)work->field_2BC >= 0x1D) {
                work->field_2BC = 0;
            }
            break;
        case 2:
            work->field_2D4 = work->field_2D4 + 1;
            work->field_2AC = work->field_2AC + 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                func_actor_107000_801334C8(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = mode;
                enemy->field_40     = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_8013283C);

/// Damage reaction of the caged specimen. `arg1` is taken off the context's
/// HP, the same amount is pushed through the lock-slot updater, and a depleted
/// specimen switches the task to its death state (2) with a five-frame
/// countdown while `field_2B4` is cleared on the work.
///
/// A live one cues a sound event instead: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. The same call statement is written out in both arms - the join
/// the compiler builds from it is what the original binary shows.
///
/// `field_2CC` is then re-armed and, for the id the animation is playing
/// (`field_2B8 == 1`), latched into `field_2D8`.
void func_actor_107000_80132D8C(Task* arg0, s32 arg1)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              anim;
    s32              soundId;

    enemy            = arg0->spawnArg2;
    obj              = arg0->extra;
    coord            = obj->field_8;
    work             = (Actor107000Work*)arg0->idMap;
    enemy->field_40 -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->field_40 < 0) {
        func_actor_107000_801334C8(arg0, 0);
        arg0->state         = 2;
        arg0->killCountdown = 5;
        work->field_2B4     = 0;
        return;
    }
    if (work->field_2D6 != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x4046000A;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    } else {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402E0002;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    anim            = work->field_2B8;
    work->field_2CC = 0xF;
    if (anim == 1) {
        work->field_2D8 = anim;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132E9C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132FD4);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_801334C8);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80133690);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_801339C0);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80133CD0);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80133E18);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", D_actor_107000_80131E5C);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", ActorsShared801385d4Table);
