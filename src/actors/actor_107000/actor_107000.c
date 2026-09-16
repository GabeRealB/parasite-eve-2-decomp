#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_107000_801334C8(Task* arg0, s32 arg1);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80131F0C);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", D_actor_107000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", D_actor_107000_80131E30);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132298);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132474);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132674);

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
