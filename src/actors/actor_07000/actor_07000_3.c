#include "actors/actors_shared_80134810.h"
#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actor_107000_anim.h"
#include "actors/actors_shared_8013454c.h"
#include "actors/actors_shared_8014ca28.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void Actor07000_Fn00654(Task* arg0);
void Actor07000_Fn016A8(Task* arg0, u8 arg1);
void Actor07000_Fn0107C(Task* arg0);

/// Node 3's pair table, packed by `Gp_PackPair` into `obj1B4`, and the enemy
/// record whose `pairTable` points at it; its `hpMax` seeds the enemy's
/// `field_40`.
extern GpU16Pair  Actor07000_D06924;
extern GpPairSrcE Actor07000_D06928;

/// Message dispatch table the spawn parks in `Task::msgTable`.
extern u8 Actor07000_D08030[];

/// The animation data `func_800B3F84` seeds the work block's slots from.
extern u8 Actor07000_D08058[];

/// Offset the collapse arms spawn the 0x60080 effect at.
extern SVECTOR Actor07000_D08070;

extern u32     Actor07000_D06938[];
extern u32     Actor07000_D06944[];
extern SVECTOR Actor07000_D08068;

// actor_104600 (func_actor_104600_80131E68), actor_204600
// (func_actor_204600_80149E68) and actor_207000 (func_actor_207000_80149F0C)
// carry the same body, refused promotion for the reason its sibling below is:
// the pair table, the animation bank and the node-3 record it names -
// Actor07000_D06924, Actor07000_D06928 and
// Actor07000_D08058 - are this overlay's own data, so one shared object
// could not link into the other three.

// actor_104600 (func_actor_104600_801325D0), actor_204600
// (func_actor_204600_8014A5D0) and actor_207000 (func_actor_207000_8014A674)
// carry the same body, refused promotion because both of its remaining calls -
// Actor07000_Fn0107C and Actor07000_Fn016A8 - are named in this
// overlay only, so one shared object could not link into the other three.

void Actor07000_Fn011B4(GpEnemy* enemy, Task* task)
{
    TmdObject*       model;
    Actor107000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj   = task->extra;
    work  = (Actor107000Work*)task->work;
    coord = obj->coords;
    model = obj;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            model->flags     |= 0x80;
            enemy->node.flags = 1;
            break;
        case 0:
        default:
            switch (work->field_2B4) {
                case 0:
                    work->field_20A &= 0x7FFF;
                    work->field_2AC -= 0x12C;
                    task->killCountdown--;
                    if ((u32)((u16)work->field_2B2 - 5) >= 2 && task->killCountdown == 3) {
                        model->flags = 0x80;
                    }
                    if (work->field_2B2 == 6) {
                        work->field_2B8 = 1;
                        Actor107000_TickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->field_2D6 != 0) {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x4046000D;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        } else {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x402E0005;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        }
                        task->killCountdown = 0;
                        Gp_ReleaseStateF0Add(task, 0x2E);
                        if (work->field_2DA != 0) {
                            Gp_SpawnEff(0x6009E, ((TmdObject*)task->extra)->coords, 0, NULL);
                        }
                        work->field_2B4 = 1;
                        work->field_2B6 = 0;
                        work->field_2CA = 0x1000;
                        work->field_28C = coord->coord;
                        enemy->recs     = NULL;
                        Gp_UnlinkNode(&enemy->node);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->objFC);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj134);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj1B4);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj1EC);
                    }
                    break;
                case 1:
                    if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                        work->field_2B4 = 2;
                    }
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        work->field_2B4 = 2;
                    }
                    ActorsShared801349d8(task);
                    if (work->field_2B6 == 0xA) {
                        ((TmdObject*)task->extra)->flags = 2;
                    }
                    break;
                case 2:
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    return;
            }
            if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                Actor107000_TickAnim(task);
                ActorsShared80134810(task, &((TmdObject*)task->extra)->coords[1]);
                ((TmdObject*)task->extra)->coords[0].flg = 0;
                ((TmdObject*)task->extra)->coords[1].flg = 0;
                Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
                Actor107000_UpdateColor(enemy, &((TmdObject*)task->extra)->coords[1]);
            }
            break;
    }
}

void Actor07000_Fn016A8(Task* arg0, u8 arg1)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj         = arg0->extra;
    enemy       = arg0->spawnArg2;
    work        = (Actor107000Work*)arg0->work;
    coord       = obj->coords;
    enemy->hp   = 0;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((Gp_LcgState >> 0x10) & 2) || (arg1 & 0xFF)) {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000B;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0003;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_1D2 |= 0x8000;
        work->field_20A |= 0x8000;
        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 1, NULL);
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x300, &Actor07000_D08068);
        Gp_SpawnScript18((s32)&Actor07000_D06938, (s32)&Actor07000_D06944);
        work->field_2DA = 1;
    } else {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000C;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0004;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_2B2 = 6;
    }
}

// actor_104600 (func_actor_104600_801335EC), actor_204600
// (func_actor_204600_8014B5EC) and actor_207000 (func_actor_207000_8014B690)
// carry the same body, refused promotion because it reads its own overlay's
// data - the two pair records, the animation source and the message table are
// named per overlay, so one shared object could not resolve them in the other
// three.

/// Spawn/setup handler of the caged specimen, entry 0 of
/// `Actor07000_D00010`. A task already on this handler (`spawnArg1`'s
/// high halfword reads 1) is torn down instead of spawned.
///
/// Otherwise it allocates the 0x2E4-byte work block and hangs it off the task:
/// the model's coordinate array feeds `field_18` with its second element, the
/// context's `field_4` with that element's matrix, and the block's two
/// `MATRIX`es become the model's colour and light matrices. The work's own
/// collision record is re-rolled from the same coordinate, and the four list
/// nodes are linked into the global object lists with their `GpRec18` tables -
/// the first three leave the 0x8000 last-element bit clear, and the second
/// node's 0x4000 bit is cleared once the third has been built.
///
/// The animation context is then seeded from `func_800B3F84` over the three
/// slots, slots 1 and 2 are reset, and the task moves to handler 3.
void Actor07000_Fn01870(GpEnemy* arg0, Task* arg1)
{
    Actor107000SpawnWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part;
    TmdObject*            obj;
    s32                   one;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    one   = 1;
    if ((s16)(arg1->spawnArg1 >> 16) == one) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    work->field_2DC = (s16)(arg1->spawnArg1 >> 16);
    work->field_2D6 = (u16)arg1->spawnArg1;
    obj->flags      = 0x80;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_DC;
    obj->colorMtx   = &work->field_BC;
    arg0->field_4   = &coord[1].coord;
    arg0->field_48  = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = one;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &Actor07000_D06928;
    arg0->recs       = &work->rec154[0];
    arg0->hp         = Actor07000_D06928.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor07000_D08058, obj, work->field_8C,
                  (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8      = 1;
    work->field_2BA      = 1;
    work->field_2AC      = 0x1000;
    work->field_2DA      = 0;
    work->field_2CE      = 0;
    work->field_2D4      = 0;
    work->field_2D2      = 0;
    work->field_2CC      = 0;
    arg1->killCountdown  = 0;
    work->field_284      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_288      = 0x100;
    work->field_28A      = 1;
    work->objFC.coord    = coord;
    work->objFC.ctx.recs = &work->rec11C;
    work->objFC.pos.vx   = 0;
    work->objFC.pos.vy   = 0;
    work->objFC.pos.vz   = 0;
    work->objFC.key      = 0;
    work->objFC.radius   = 0xBB8;
    work->objFC.flags    = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154[0];
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = -0xC8;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x3002E;
    work->obj134.radius   = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags & 0x7FFF);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags & 0x3DFF);
    work->obj1B4.key      = Gp_PackPair(&Actor07000_D06924, 0);
    work->obj1B4.radius   = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj1EC.key      = 0x22323;
    work->obj1EC.radius   = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->field_2E2    = 0;
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    arg1->msgTable     = Actor07000_D08030;
    arg1->state        = 3;
}

INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_07000_3", Actor07000_D0003C);
INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_07000_3", ActorsShared801385d4Table);
