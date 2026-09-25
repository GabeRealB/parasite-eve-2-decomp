#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actor_104600.h"

/// Kills the first enemy: its HP is cleared and a random draw (or a non-zero
/// `arg1`) picks the death. The violent one plays its sound, sets the 0x8000
/// bit of the last two bodies, spawns the 0x6009C and 0x60030 effects and the
/// death script, and asks for a final effect through `field_2DA`; the other
/// plays a second sound and moves the reaction state to 6. `field_2D6` picks
/// the sound set either way.
void Actor04600_Fn01604(Task* arg0, u8 arg1)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj         = arg0->extra;
    enemy       = arg0->spawnArg2;
    work        = (Actor104600Work*)arg0->work;
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
        work->obj1B4.flags |= 0x8000;
        work->obj1EC.flags |= 0x8000;
        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 1, NULL);
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x300, &Actor04600_D058A0);
        Gp_SpawnScript18((s32)&Actor04600_D04170, (s32)&Actor04600_D0417C);
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

/// Spawn handler of the dropping first enemy, entry 0 of `Actor04600_D00010`.
/// A spawn arg whose high halfword is 1 destroys the enemy instead. Otherwise
/// it builds the same work block as `Actor04600_Fn00048` with the model hidden
/// and the node flag set, keeps the spawn arg's two halves, leaves the first
/// body's 0x8000 bit and the second's 0xC200 bits clear, parks
/// `Actor04600_D05868` as the task's message table and moves the task to state
/// 3, the drop.
void Actor04600_Fn017CC(GpEnemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    TmdObject*       obj;
    s32              one;
    s32              i;

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
    arg0->param      = &Actor04600_D04160;
    arg0->recs       = &work->rec154[0];
    arg0->hp         = Actor04600_D04160.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor04600_D05890, obj, work->field_8C, (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8            = 1;
    work->field_2BA            = 1;
    work->field_2AC            = 0x1000;
    work->field_2DA            = 0;
    work->field_2CE            = 0;
    work->field_2D4            = 0;
    work->field_2D2            = 0;
    work->field_2CC            = 0;
    arg1->killCountdown        = 0;
    work->field_284.coord      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_284.spawnArgLo = 0x100;
    work->field_284.spawnArgHi = 1;
    work->objFC.coord          = coord;
    work->objFC.ctx.recs       = &work->rec11C;
    work->objFC.pos.vx         = 0;
    work->objFC.pos.vy         = 0;
    work->objFC.pos.vz         = 0;
    work->objFC.key            = 0;
    work->objFC.radius         = 0xBB8;
    work->objFC.flags          = 1U;
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
    work->obj1B4.key      = Gp_PackPair(&Actor04600_D0415C, 0);
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
    arg1->msgTable     = Actor04600_D05868;
    arg1->state        = 3;
}

INCLUDE_RODATA("actors/nonmatchings/actor_04600/actor_04600_4", Actor04600_D0003C);
