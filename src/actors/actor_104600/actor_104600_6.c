#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actors_shared_8014df20.h"
#include "actors/actor_104600.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_104600_80131E5C;

extern SVECTOR D_actor_104600_801382D4;
extern SVECTOR D_actor_104600_801382DC;

/// Per-frame hit handler. Applies the `func_800E0C10` push-back from the four
/// `field_1A4` records to the root coordinate (restoring `field_254` when two
/// records conflict), then walks the records: a kind-1 hit or a kind-2 hit
/// whose distance-scaled damage is nonzero plays the hit sound and sparks and
/// puts the task into its death state after 5 frames; a zero-damage kind-2 hit
/// applies the id's side effect instead.
void func_actor_104600_8013528C(Task* arg0)
{
    ActorShared8014df20Work* work;
    Actor104600HitScratch*   sc;
    Actor104600HitScratch*   head;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GpEnemy*                 enemy;
    s32                      i;
    s32                      sndHit;
    s32                      sndHit2;
    u32                      damage;
    s32                      snd;

    work                                     = (ActorShared8014df20Work*)arg0->work;
    head                                     = *(Actor104600HitScratch**)G_SCRATCH_HEAD;
    *(Actor104600HitScratch**)G_SCRATCH_HEAD = head - 1;
    sc                                       = head - 1;
    obj                                      = arg0->extra;
    coord                                    = obj->coords;
    enemy                                    = arg0->spawnArg2;

    switch (func_800E0C10(work->field_1A4, &head[-1].delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->delta.vx.h.hi;
            coord->coord.t[1] += sc->delta.vy.h.hi;
            coord->coord.t[2] += sc->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_254;
            coord->coord.t[1] = work->field_258;
            coord->coord.t[2] = work->field_25C;
            break;
    }
    i       = 0;
    sndHit  = 0x40480009;
    sndHit2 = 0x402E0008;
    do {
        switch (work->field_1A4[i].key & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2AC != 0) {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_104600_801382D4);
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_104600_801382D4);
                Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &D_actor_104600_801382DC);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = 0x80;
                work->field_2A0     = 0x500;
                work->field_28C     = 1;
                enemy->hp           = 0;
                work->field_2A6     = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage         = Gp_ComputeDamage(work->field_1A4[i].key,
                                                  SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                              sc->delta.vy.w * sc->delta.vy.w +
                                                              sc->delta.vz.w * sc->delta.vz.w),
                                                  0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2, work->field_1A4[i].key, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78((GpObj40*)enemy, work->field_1A4[i].key, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->field_2AC != 0) {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    } else {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    }
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_104600_801382D4);
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_104600_801382D4);
                    Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &D_actor_104600_801382DC);
                    obj->flags          = 0x80;
                    work->field_2A0     = 0x1000;
                    work->field_2A6     = 1;
                    work->field_28C     = 1;
                    enemy->hp           = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->field_1A4[i].key)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2((GpObj5D*)enemy, work->field_1A4[i].key, 0);
                        break;
                    case 8:
                        work->field_2A6 = 1;
                        break;
                }
                break;
        }
        i++;
    } while (i < 4);
    Gp_ClearRec18Occupied(work->field_1A4);
    *(Actor104600HitScratch**)G_SCRATCH_HEAD = *(Actor104600HitScratch**)G_SCRATCH_HEAD + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_6", func_actor_104600_80135778);

void func_actor_104600_801359A0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_104600_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_6", func_actor_104600_801359FC);
