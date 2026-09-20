#include "common.h"

#include "actors/actor_101100.h"
#include "actors/actors_shared_80137fb8.h"
#include "actors/actors_shared_801511c8.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern Actor101100StateFuncTable3 D_actor_101100_80131E24;

extern s32 D_8007216C;

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
    sound      = soundBase | (ActorsShared80137fb8ActorId << 8);
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
    obj->key            = Gp_PackPair(&D_actor_101100_80139318, 5);
    obj->flags          = 1;
    Gp_InitRec18Table(rec, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags        |= 0xC000;
    task->exitCallback = ActorsShared801511c8;
    task->state++;
    ActorsShared8013845cSub1(task);
}

/// Runs the actor's current state handler, copying the table onto the stack
/// before the call. The handler is handed a 0x68-byte scratchpad buffer,
/// borrowed from the scratchpad stack for the duration of the call, with its
/// last byte cleared.
void func_actor_101100_80138374(Task* task)
{
    Actor101100StateFuncTable3 sp;
    GpEnemy*                   enemy;
    void*                      work;
    u8*                        scratch;

    sp      = D_actor_101100_80131E24;
    enemy   = task->spawnArg2;
    work    = task->work;
    scratch = (u8*)(SCRATCH_SP -= 0x68);

    scratch[0x64] = 0;
    sp.funcs[task->state](enemy, task, work, scratch);
    SCRATCH_SP += 0x68;
}
