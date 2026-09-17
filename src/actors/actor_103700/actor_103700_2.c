#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/sound.h"

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_2", func_actor_103700_801328DC);

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_2", func_actor_103700_80132B7C);

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_2", func_actor_103700_80132DA8);

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_2", func_actor_103700_80132FD4);

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_2", func_actor_103700_80133370);

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_2", func_actor_103700_801336E8);

void func_actor_103700_80133AB4(Task* task)
{
    Actor103700Work* work;
    GpObj38*         obj;
    Task*            player;
    void*            head;
    GpAnimArg*       arg;
    s32              sound;
    s32              pan;

    work                    = (Actor103700Work*)task->idMap;
    obj                     = (GpObj38*)((TmdObject*)task->extra)->field_8;
    player                  = Game_GetPtrSlot(3);
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - sizeof(GpAnimArg);
    arg                     = (GpAnimArg*)*(void**)G_SCRATCH_HEAD;

    switch (work->field_250) {
        case 0:
            arg->field_0  = D_actor_103700_80139F1C;
            arg->field_4  = 2;
            arg->field_8  = 0;
            arg->field_C  = 0;
            arg->field_10 = 1;
            Gp_DispatchMsg(player, 0x3F4, (s32)arg, 0);
            sound = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 6;
            pan   = (s8)Gp_GetObjPan(obj);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(obj));
            work->field_250 = 1;
            break;
        case 1:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3F1, 0, 0);
                work->field_262 = 0;
                work->field_24E = 5;
                work->field_250 = 0;
            }
            break;
    }
    *(u32*)G_SCRATCH_HEAD += sizeof(GpAnimArg);
}
