#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_104600.h"

void Actor04600_Fn005B0(Task* arg0);
void Actor04600_Fn007B0(Task* arg0);

/// Spawn handler of the first enemy, entry 0 of `Actor04600_D00004`. A spawn
/// arg whose high halfword is 1 destroys the enemy instead. Otherwise it
/// allocates the 0x2E4-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, then links the four bodies with their contact tables
/// and installs `Actor04600_Fn02C08` as the exit callback. The spawn arg's two
/// halves are kept in `field_2DC`/`field_2D6`; a low half of 1 matching the
/// task's `spawnType` steps the model's texture page and CLUT row and
/// re-streams it twice.
void Actor04600_Fn00048(GpEnemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    u16              v;
    s32              i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    if ((s16)(arg1->spawnArg1 >> 16) == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_DC;
    obj->colorMtx  = &work->field_BC;
    arg0->field_4  = &coord[1].coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = 0;
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
    work->objFC.flags     = (u16)(work->objFC.flags | 0x8000);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags | 0xC200);
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
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    work->field_2DC    = (s16)(arg1->spawnArg1 >> 16);
    v                  = (u16)arg1->spawnArg1;
    work->field_2D6    = v;
    if ((s16)v == 1 && arg1->spawnType == (s16)v) {
        obj->tpage = obj->tpage + 1;
        obj->clut  = obj->clut + 1;
        if (obj->buffer != 0) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    work->field_2B4    = 0;
    arg1->exitCallback = Actor04600_Fn02C08;
    arg1->state       += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_04600/actor_104600", Actor04600_D00004);

INCLUDE_RODATA("actors/nonmatchings/actor_04600/actor_104600", Actor04600_D00010);

/// Per-frame dispatch of the first enemy on its reaction state `field_2B2`:
/// 0 is the dormant arm `Actor04600_Fn005B0` and 1 the live handler
/// `Actor04600_Fn007B0`. State 3 suppresses the rebind until
/// `Gp_TickObjFlag2` reports the reaction over, then returns the enemy to the
/// live stage, and ends with a step of the root. States 4 and 5 collapse the
/// enemy: both scale its second part at the base factor, count frames and
/// spawn the 0x60080 effect every 0x10; state 5 also counts those spawns and,
/// on the third, arms the death - a five-frame countdown, the death phase
/// reset and task state 2, with the enemy's HP cleared. Both collapse states
/// end by suppressing the rebind.
void Actor04600_Fn003D4(Task* arg0)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    u16              frames;

    work = (Actor104600Work*)arg0->work;
    switch (work->field_2B2) {
        case 0:
            Actor04600_Fn005B0(arg0);
            return;
        case 1:
            Actor04600_Fn007B0(arg0);
            return;
        case 3:
            work->field_2D2 = 1;
            if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
                work->field_2D2 = 0;
                work->field_2B2 = 1;
                work->field_2C8 = 1;
                work->field_2BE = 0;
            }
            Actor04600_Fn0272C(arg0);
            return;
        case 4:
            work->field_2AC = 0x1000;
            Actor04600_Fn0294C(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor04600_D058A8);
                work->field_2BC = 0;
            }
            goto suppress_rebind;
        default:
            return;
        case 5:
            work->field_2AC = 0x1000;
            Actor04600_Fn0294C(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor04600_D058A8);
                work->field_2BC = 0;
                frames          = work->field_2D4 + 1;
                work->field_2D4 = frames;
                if ((s16)frames >= 3) {
                    enemy               = arg0->spawnArg2;
                    arg0->killCountdown = 5;
                    work->field_2B4     = 0;
                    arg0->state         = 2;
                    enemy->hp           = 0;
                }
            }
        suppress_rebind:
            work->field_2D2 = 1;
    }
}
