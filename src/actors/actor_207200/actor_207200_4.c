#include "common.h"

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "actors/actor_207200.h"
#include "actors/actors_shared_80134700.h"
#include "actors/actors_shared_8013851c.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E30;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014B278);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014B628);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014B87C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014BEF4);

/// Ticks the shatter timers the enemy runs while it dies. Every time a timer
/// runs out the work is armed with a fresh sound effect - one per stage of the
/// death animation - and the frame it is handed plays.
void func_actor_207200_8014C870(Actor207200* arg0, s32 arg1)
{
    Actor207200Work* work;
    Actor207200Ctx*  ctx;
    GsCOORDINATE2*   coord;
    GpEffArg*        effArg;
    s32              snd;

    work  = arg0->field_1C;
    ctx   = arg0->field_20;
    coord = (*(TmdObject**)&arg0->field_2C)->field_8;

    ctx->field_40 = (s16)((u16)ctx->field_40 - arg1);
    func_800DA6E8(&ctx->node, arg1, 0);
    if ((s16)ctx->field_40 <= 0) {
        if (work->field_4A6 == 0) {
            ctx->field_40 = 1;
            snd           = ((arg0->field_20->field_8 >> 12) << 8) | 0x40480003;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            effArg = &work->field_3F4;
            func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->field_8 + 3, &D_actor_207200_80153F18, effArg);
            func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->field_8 + 3, &D_actor_207200_80153F18, effArg);
            work->field_374.obj.flags &= 0x7FFF;
            work->field_3AC.obj.flags &= 0x7FFF;
            Gp_UnlinkObj(&work->field_2C4.obj);
            work->field_4A6 = 1;
            ctx->field_54   = (s32)&work->field_214.field_20[0];
            work->field_488 = 0;
            arg0->field_2A  = 0x14;
        }
    } else {
        if (work->field_48C == 1) {
            snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40480006;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            work->field_48C = 5;
            work->field_490 = 0;
            work->field_486 = 4;
            work->field_492 = 0;
            work->field_4A2 = 0;
            return;
        }
        snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40480001;
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014CA84);
