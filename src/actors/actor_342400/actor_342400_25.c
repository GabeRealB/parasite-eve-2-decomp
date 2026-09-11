#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "psyq/inline_c.h"

#include "actors/actor_342400.h"

void func_actor_342400_8016B5B0(Task* arg0)
{
    Actor342400Work* work;
    GsCOORDINATE2*   coords;
    GsCOORDINATE2*   current;
    SVECTOR*         pos;
    SVECTOR          local;
    VECTOR           result;
    s32              flag;

    work   = (Actor342400Work*)arg0->idMap;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    SndEvt_EnqueueType7(0x402C0002, 1);
    work->field_90  = coords->coord.t[0];
    work->field_92  = coords->coord.t[1];
    work->field_94  = coords->coord.t[2];
    work->field_412 = 0;
    work->field_428 = 0;
    work->field_42A = 0;
    work->field_422++;
    pos     = &work->field_70;
    pos->vx = pos->vy = pos->vz = 0;
    current                     = &((GsCOORDINATE2*)((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8)[3];
    local.vx                    = pos->vx;
    local.vy                    = pos->vy;
    local.vz                    = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return;
        }
        if (current == &Gfx_ViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        __asm__ volatile("nop; nop; .word 0x4A480012");
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

void func_actor_342400_8016B744(Task* arg0)
{
    Actor342400Work* work;
    s32              soundId;
    s32              pan;

    work = (Actor342400Work*)arg0->idMap;
    if (D_actor_342400_80173A84[work->field_418 - 1] == 0) {
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 9;
        work->field_414 = 1;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0002;
        pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_422 = 4;
        return;
    }
    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 7;
    work->field_414 = 1;
    work->field_44F = (u8)work->field_41C * 4;
    work->field_422++;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_25", func_actor_342400_8016B84C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_25", func_actor_342400_8016B914);

void func_actor_342400_8016B9A4(Task* arg0)
{
    Actor342400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor342400Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

void func_actor_342400_8016BA3C(Task* arg0)
{
    Actor342400Work* work;
    s16              anim;
    s16              next;

    work = (Actor342400Work*)arg0->idMap;
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 5;
            work->field_414 = 1;
        } else {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 6;
            work->field_414 = 1;
        }
    } else {
        next            = D_actor_342400_80173A98[anim - 1];
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = next;
        work->field_414 = 1;
    }
    func_actor_342400_80165CC0(arg0);
    work->field_420++;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_25", func_actor_342400_8016BAF4);
