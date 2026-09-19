#include "common.h"

#include "actors/actor_400100.h"
#include "actors/actor_400100_damage.h"
#include "actors/actor_400100_motion.h"
#include "actors/actor_400100_update.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"
#include <psyq/inline_c.h>

void                         Actor00100_Fn001FC(GsCOORDINATE2*, s16);
void                         Actor00100_Fn01D74(Actor00100*);
s32                          Actor00100_Fn01EEC(Actor00100*, Actor00100Work*);
void                         Gp_UpdateCoord(GsCOORDINATE2*);
void                         func_800B4114(s8*, s32, s16, s32, s32);
extern s8                    Actor00100_D1B6D0;
extern GpEnemyTaskFuncTable4 Actor00100_D001A0;

#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

extern s8 D_80114C12;
extern u8 D_80071075;

struct _GpObj20E;
void Gp_ReleaseStateF0Add(struct _GpObj20E* arg0, s32 arg1);
s32  Gp_DispatchMsg(void* arg0, s32 arg1, s32 arg2, s32 arg3);

void Actor00100_Fn0B134(void)
{
}

s16 Actor00100_Fn0B13C(Actor00100* arg0)
{
    Actor00100RecordWork* work  = (Actor00100RecordWork*)arg0->field_1C;
    s16                   found = 0;
    s16                   i;
    s32                   value;

    for (i = 0; i < 5; i++) {
        value = work->records[i].field_0;
        if (value == 0) {
            break;
        }
        if ((value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

s32 Actor00100_Fn0B1A4(Actor00100* arg0, s32 arg1, s32 arg2)
{
    TmdObject*      obj  = arg0->field_2C;
    Actor00100Work* work = arg0->field_1C;

    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

s32 Actor00100_Fn0B264(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

s32 Actor00100_Fn0B2B4(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*            coord;
    s32                       mx;
    s32                       mz;
    ActorsShared80169f74Work* work;

    work                                          = (ActorsShared80169f74Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)task->extra)->coords;
    mx                                     = coord->coord.m[2][0];
    mz                                     = coord->coord.m[2][2];
    work->yaw                              = ratan2(-mx, mz);
    return 1;
}

void Actor00100_Fn0B3B4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

void Actor00100_Fn0B3DC(Actor00100* arg0, s16 arg1, s16 arg2)
{
    Actor00100Work* work = arg0->field_1C;
    s32             spawn;

    switch (arg1) {
        case 0:
        case 1:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0;
            break;
        case 9:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x2BC;
            break;
        case 7:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (Gp_State1C->field_A == 2 && spawn == 1) {
        Gp_SpawnEff(0x60054, &arg0->field_2C->coords[arg1], arg2 | 0x80000000, &work->field_898);
    }
}

void Actor00100_Fn0B4D8(Actor00100* arg0)
{
    TmdObject*      obj;
    Actor00100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 1;
        obj->flags              |= 0x80;
        work->objs[2].flags     &= 0xBFFF;
    }
}

void Actor00100_Fn0B52C(Actor00100* arg0)
{
    Actor00100Work* work;
    Actor00100Ctx*  ctx;
    TmdObject*      obj;
    s32             value;
    u32             magnitude;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj           = arg0->field_2C;
        ctx->field_14 = 0;
        obj->flags    = 0;
        Tmd_AllocBuffers(obj);
        work->field_82E      = 0x15;
        work->field_828      = 2;
        work->field_832      = 0x10;
        work->objs[2].flags |= 0x4000;
        do {
            Actor00100_Fn02788(arg0);
        } while ((work->field_5A & 0x3FF) != 0xC);
        work->field_832 = 0x20;
        return;
    }
    arg0->field_2C->coords->flg = 0;
    value                       = (s16)work->field_832 / 2;
    work->field_832             = (u16)value;
    magnitude                   = 0x10U;
    if (value == 1) {
        work->field_832 = -magnitude;
    }
    if ((s16)work->field_832 == -1) {
        work->field_832 = 0x10;
    }
    Actor00100_Fn02788(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)ctx) == 1) {
        ctx->field_4C &= 0xFD;
        work->field_0  = 0x24;
    }
}

void Actor00100_Fn0B658(Actor00100* arg0)
{
    TmdObject*      obj;
    Actor00100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 0;
        obj->flags               = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 8;
        work->field_82A        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        Gp_ArmStateF0(1);
    }
    Actor00100_Fn00A54(arg0->field_2C->coords, &work->objs[2].field_20, 5);
    arg0->field_2C->coords->flg = 0;
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
}

void Actor00100_Fn0B730(Actor00100* arg0)
{
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 0;
        obj->flags               = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 0xC;
        work->field_82A        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x26;
    }
}

void Actor00100_Fn0B7DC(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->flags             = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 1;
        work->field_82E        = 0xA;
        work->field_844        = 0;
        work->field_840        = 0;
        work->field_83E        = 0;
        work->field_832        = work->field_834;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if ((work->field_68 & 0x100) && ((s16)work->field_82E == 0xA)) {
        if ((s16)ctx->field_40 > 0) {
            if (ctx->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0B8D8(Actor00100* arg0)
{
    Actor00100Work* work;
    u16             timer;
    u32             random;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        random        = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState   = random;
        work->field_6 = work->field_C20 + ((random >> 0x10) & 0xF);
    }
    Actor00100_Fn02788(arg0);
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if ((s16)timer < 0) {
        if ((s16)arg0->field_20->field_40 > 0) {
            work->field_0 = 0x24;
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0B98C(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->flags             = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 1;
        work->field_82E        = 0x10;
        work->field_832        = 0x10;
        work->field_840        = 0;
        work->field_83E        = 0;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        if ((s16)ctx->field_40 > 0) {
            if (ctx->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0BA70(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    TmdObject*      obj;
    Actor00100Work* work;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->flags             = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 1;
        work->field_828        = 1;
        work->field_82E        = 0x13;
        work->field_832        = 0x10;
        work->field_840        = 0;
        work->field_83E        = 0;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x15;
    }
}

void Actor00100_Fn0BB2C(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->flags             = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 2;
        work->field_82E        = 0x14;
        work->field_832        = 0x10;
        work->field_840        = 0;
        work->field_83E        = 0;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        if ((s16)ctx->field_40 > 0) {
            if (ctx->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0BC14(void)
{
}

void Actor00100_Fn0BC1C(Actor00100* arg0)
{
    GsCOORDINATE2*  coord;
    s32             y;
    s32             distance;
    register s32    slotY asm("v0");
    Actor00100Work* work;
    Task*           task;

    work = arg0->field_1C;
    task = Game_GetPtrSlot(3);
    if ((task != NULL) && (work->field_8E8 == 7)) {
        coord    = ((TmdObject*)task->extra)->coords;
        slotY    = coord->coord.t[1];
        y        = arg0->field_2C->coords->coord.t[1];
        distance = slotY - y;
        if (distance < 0) {
            distance = -distance;
        }
        if (distance >= 0x321) {
            coord->coord.t[1]                      = y;
            ((TmdObject*)task->extra)->coords->flg = 0;
        }
    }
}

void Actor00100_Fn0BCBC(s32 arg0, Task* task)
{
    Actor00100Work* work;

    work = (Actor00100Work*)task->work;
    if (work->field_C2A == 1) {
        work->field_C2A = 0;
        Gp_ReleaseStateF0Add((struct _GpObj20E*)task, 1);
    }
    if (work->field_C2A == 0) {
        task->state++;
    }
}

void Actor00100_Fn0BD28(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor00100_D001A0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
