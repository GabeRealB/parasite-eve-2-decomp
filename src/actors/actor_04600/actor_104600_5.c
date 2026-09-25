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

/// Spawn handler of the second enemy, entry 0 of `Actor04600_D0003C`. It
/// allocates the 0x2B0-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, starts animation 1 with the light blend fully up and
/// rolls the first 0x64..0xA3 frame wait, then links the three bodies with
/// their contact tables. The placement's mode is kept in `field_2AC`; mode 1
/// matching the task's `spawnType` steps the model's texture page and CLUT
/// row and re-streams it twice. `Actor04600_Fn04100` becomes the exit
/// callback.
void Actor04600_Fn02D68(GpEnemy* arg0, Task* arg1)
{
    Actor104600Enemy2Work* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         part;
    u32                    seed;
    GpRec18*               records1;
    GpRec18*               records2;
    GpRec18*               records3;
    s32                    i;

    obj   = arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    work  = memCalloc(0x2B0U, false);
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
    arg0->param      = &Actor04600_D058B4;
    arg0->recs       = work->field_1A4;
    arg0->hp         = Actor04600_D058B4.hpMax;
    func_800B3F84(&work->context, Actor04600_D064A8, obj, work->field_8C, work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_28C  = 1;
    work->field_28E  = 1;
    work->field_2A6  = 1;
    work->field_2A4  = 0x12;
    arg0->node.flags = 1;
    obj->flags       = 0x80;
    seed             = Gp_LcgState * 5 + 0x71357911;
    work->field_2A8  = ((seed >> 16) & 0x3F) + 0x64;
    Gp_LcgState      = seed;
    Gp_SetLightMode((GpObj4C*)arg1->spawnArg2, 2);
    work->field_11C.end0.vz    = 0x1388;
    work->field_11C.end0Radius = 0xFA0;
    work->field_11C.end1Radius = 0x7D0;
    records1                   = work->field_134;
    work->field_11C.recs       = records1;
    work->field_FC.ctx.d4rec   = &work->field_11C;
    work->field_FC.coord       = coord;
    work->field_FC.pos.vx      = 0;
    work->field_FC.pos.vy      = 0;
    work->field_FC.pos.vz      = 0;
    work->field_FC.key         = 0;
    work->field_FC.radius      = 0;
    work->field_FC.flags       = 3;
    Gp_LinkObj(3, &work->field_FC);
    Gp_InitRec18Table(records1, 1, 0);
    work->field_14C.coord    = coord;
    records2                 = work->field_16C;
    work->field_14C.ctx.recs = records2;
    work->field_14C.pos.vx   = 0;
    work->field_14C.pos.vy   = 0;
    work->field_14C.pos.vz   = 0;
    work->field_14C.key      = 0;
    work->field_14C.radius   = 0x7D0;
    work->field_14C.flags    = 1;
    work->field_FC.flags     = work->field_FC.flags | 0x8000;
    Gp_LinkObj(3, &work->field_14C);
    Gp_InitRec18Table(records2, 1, 0);
    records3                 = work->field_1A4;
    work->field_184.coord    = coord;
    work->field_184.ctx.recs = records3;
    work->field_184.pos.vx   = 0;
    work->field_184.pos.vy   = -0xC8;
    work->field_184.pos.vz   = 0;
    work->field_184.key      = 0x3002F;
    work->field_184.radius   = 0xC8;
    work->field_184.flags    = 1;
    work->field_14C.flags    = work->field_14C.flags | 0x8000;
    Gp_LinkObj(2, &work->field_184);
    Gp_InitRec18Table(records3, 4, 0);
    work->field_184.flags = work->field_184.flags | 0xC200;
    work->field_2AC       = arg0->place->mode;
    if (work->field_2AC == 1 && arg1->spawnType == work->field_2AC) {
        obj->tpage++;
        obj->clut++;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    arg1->exitCallback = Actor04600_Fn04100;
    arg1->state++;
}
