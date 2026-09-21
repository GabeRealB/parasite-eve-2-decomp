#include "common.h"

#include "actors/actor_101100.h"
#include "actors/actor_201100.h"
#include "actors/actors_shared_801511c8.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"

void ActorsShared8013845cSub0(Task* task)
{
    GpEffWork*       effect;
    s32              variant;
    s32              soundBase;
    GpRec18*         rec;
    Actor101100Work* work;
    s32              area;
    s32              sound;
    s32              pan;
    GpObj*           obj;
    GsCOORDINATE2*   coord;
    GpMtxWords*      rotation;

    coord   = ((TmdObject*)task->extra)->coords;
    area    = D_8007216C & 0xFFFF0000;
    variant = area == 0x03200000;
    work    = memCalloc(sizeof(Actor101100Work), 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }
    task->work = work;
    soundBase  = (variant << 22) | 0x400B000C;
    sound      = soundBase | (D_actor_201100_8015F490 << 8);
    pan        = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
    effect = Gp_SpawnEff(0x60070, coord, 0xC0031FFF, NULL);
    if (effect != NULL) {
        Task_Reparent(task, effect->task);
    }
    task->killCountdown = 0x5A;
    rotation            = (GpMtxWords*)&coord->coord;
    obj                 = &work->obj;
    rotation->w0        = 0x1000;
    rotation->w1        = 0;
    rotation->w2        = 0x1000;
    rotation->w3        = 0;
    rotation->h4        = 0x1000;
    rec                 = work->rec;
    coord->flg          = 0;
    coord->coord.t[1]  += 0x30;
    obj->coord          = coord;
    obj->ctx.recs       = rec;
    obj->pos.vx         = 0;
    obj->pos.vy         = 0;
    obj->pos.vz         = 0;
    obj->radius         = 0x2EE;
    obj->key            = Gp_PackPair(&D_actor_201100_80151318, 5);
    obj->flags          = 1;
    Gp_InitRec18Table(rec, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags        |= 0xC000;
    task->exitCallback = ActorsShared801511c8;
    task->state++;
    ActorsShared8013845cSub1(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_201100/actor_201100_4", func_actor_201100_80150374);
