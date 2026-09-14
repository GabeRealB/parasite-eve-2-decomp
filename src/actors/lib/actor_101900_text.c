#include "common.h"

#include "actors/actor_101900.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn00260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0056C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn008B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn00E00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn00FA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn016F0);

void Actor01900_Fn01950(Actor01900* arg0)
{
    GpAnimPose          pose;
    GpAnimPose          blendPose;
    GpAnimCtx*          anim;
    s16                 weight;
    s16                 i;
    Actor01900AnimWork* work;

    work   = (Actor01900AnimWork*)arg0->field_1C;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_8AA;
            work->slots[i].field_9      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn01A7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn01C94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn02018);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn02664);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn02A50);

void Actor01900_Fn03710(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;
    TmdObject*      obj;
    s32             step;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                 = arg0->field_2C;
        enemy->node.field_4 = 0;
        obj->field_C        = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x17;
        work->field_A08.flags |= 0x4000;
        do {
            Actor01900_Fn01C94(arg0);
        } while ((u32)(work->field_5A & 0x3FF) < 6U);
        work->field_8A2 = 0x20;
        return;
    }
    arg0->field_2C->field_8->flg = 0;
    step                         = (s16)work->field_8A2 / 2;
    work->field_8A2              = (u16)step;
    if (step == 1) {
        work->field_8A2 = -0x10;
    }
    if ((s16)work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    Actor01900_Fn01C94(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)enemy) == 1) {
        enemy->field_4C &= 0xFD;
        work->field_0    = 0x11;
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x11;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03854);

void Actor01900_Fn03C04(GameSessionFrom4* session, Actor01900Delta* delta)
{
    Actor01900HeightClamp* row;
    s32                    offset;
    s32                    lo;
    s16                    i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->field_3 == row->field_0 && session->field_2 == row->field_2) {
            lo     = row->lo;
            offset = delta->field_1C;
            if (offset < lo) {
                delta->field_1C = lo;
            } else if (row->hi < offset) {
                delta->field_1C = row->hi;
            }
            return;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03C98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03FF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn042BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn04D14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0551C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn05B4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn05F38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06100);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06634);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06904);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06B4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06F40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn07810);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn07BA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn080A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn083E8);

void Actor01900_Fn08724(Actor01900* arg0)
{
    SVECTOR         vec;
    GpAreaKey       key;
    GpAreaKey*      sessionKey;
    GpAreaKey*      keyPtr;
    u8              areaByte0;
    GpAreaRec*      rec;
    GpCdRec10*      entry;
    GpEffWork*      eff;
    TmdObject*      model;
    s32             idx;
    u32             raw;
    u16             next;
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0x80;
        work->field_8C8.field_1C = 0x180;
        work->field_A08.flags    = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.field_4      = 1;
        work->field_8AE          = 0;
        work->field_6            = 0U;
        vec.vx                   = 0x64;
        vec.vz                   = 0;
        vec.vy                   = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0x13);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch ((s16)next) {
        case 3:
            D_80114B78[0] = &Actor01900_D10B68;
            vec.vz        = 0x64;
            vec.vy        = 0;
            vec.vx        = 0;
            eff           = Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 9, 0x200, &vec);
            goto body;
        case 4:
            D_80114B78[0] = &Actor01900_D10B68;
            vec.vy        = 0;
            vec.vx        = 0;
            eff           = Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 12, 0x200, &vec);
        body:
            if (eff != NULL) {
                sessionKey  = (GpAreaKey*)&Game_Session->field_4;
                raw         = enemy->field_8;
                model       = (TmdObject*)eff->field_0->extra;
                key.field_3 = sessionKey->field_3;
                key.field_2 = sessionKey->field_2;
                key.field_1 = sessionKey->field_1;
                areaByte0   = Game_Session->field_4;
                idx         = raw >> 12;
                /* Both calls take `&key`. CSE of that address across the first
                   jal costs a callee-saved register; the ROM rematerializes
                   `addiu a0, sp, key` for each call. Same shape as
                   Actor02000_Fn0251C. */
                SOFT_BARRIER();
                keyPtr = &key;
                TOUCH_REG(keyPtr);
                key.field_0 = areaByte0;
                Gp_SyncAreaKeyIndex(keyPtr);
                rec             = Gp_GetNestedAreaRec(&key);
                entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
                model->field_24 = entry->field_D;
                model->field_25 = entry->field_E;
                if (model->field_18 != NULL) {
                    Tmd_ProcessStream(model);
                    Tmd_ProcessStream(model);
                }
            }
            break;
    }
    if ((s16)work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0892C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn09694);

void Actor01900_Fn09BE8(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0xB;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_Fn00E00(arg0->field_2C->field_8, work->pad_8E8, 0xC);
    Actor01900_Fn00E00(arg0->field_2C->field_8, work->pad_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn09D3C);

void Actor01900_Fn0A314(void)
{
}

s32 Actor01900_Fn0A31C(Actor01900* arg0, s32 arg1, Actor01900Msg7D3* arg2)
{
    Actor01900Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

s32 Actor01900_Fn0A38C(Actor01900* arg0, s32 arg1, s32 arg2)
{
    TmdObject*      obj  = arg0->field_2C;
    Actor01900Work* work = arg0->field_1C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->field_C  = 0;
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

s32 Actor01900_Fn0A44C(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->field_40 > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->field_C;
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

s32 Actor01900_Fn0A49C(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*            coord;
    s32                       mx;
    s32                       mz;
    ActorsShared80169f74Work* work;

    work                                           = (ActorsShared80169f74Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    coord                                   = ((TmdObject*)task->extra)->field_8;
    mx                                      = coord->coord.m[2][0];
    mz                                      = coord->coord.m[2][2];
    work->yaw                               = ratan2(-mx, mz);
    return 1;
}

s32 Actor01900_Fn0A59C(void)
{
    return 1;
}

s32 Actor01900_Fn0A5A4(Actor01900* arg0, s32 arg1, u16* arg2)
{
    u16             room;
    u16             state;
    u16             state2;
    Actor01900Work* work;

    work               = arg0->field_1C;
    work->field_C34[0] = ((u8*)arg2)[0];
    work->field_C34[1] = ((u8*)arg2)[1];
    work->field_C34[2] = ((u8*)arg2)[2];
    room               = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        switch (state) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 1:
                work->field_0 = 0x17;
                return 1;
            default:
                return 0;
        }
    } else if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                       = 0x1C;
                arg0->field_2C->field_8->coord.t[0] = -0x595;
                arg0->field_2C->field_8->coord.t[1] = 0;
                arg0->field_2C->field_8->coord.t[2] = -0x5B1;
                Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, -0x400, 1);
                arg0->field_2C->field_8->flg = 0;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

void Actor01900_Fn0A6CC(Task* task)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = (Actor01900Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C38 != NULL) {
            Task_Kill(work->field_C38);
        }
        if (work->field_C3C != NULL) {
            Task_Kill(work->field_C3C);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void Actor01900_Fn0A764(Actor01900* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = (u16)(obj->field_C | 0x80);
        work->field_B48.flags        = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags        = (u16)(work->field_A08.flags & 0xBFFF);
    }
}

void Actor01900_Fn0A7C0(Actor01900* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        Actor01900_Fn01C94(arg0);
    }
}

void Actor01900_Fn0A868(Actor01900* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        Actor01900_Fn01C94(arg0);
    }
}

void Actor01900_Fn0A914(Actor01900* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        Actor01900_Fn01C94(arg0);
    }
}

void Actor01900_Fn0A9C0(Actor01900* arg0)
{
    Actor01900Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 2;
        work->field_8A2        = 0x12;
        work->field_89E        = 0xD;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
    }
    arg0->field_2C->field_8->flg = 0;
    Actor01900_Fn01C94(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 7;
    }
}

void Actor01900_Fn0AA78(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 8;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        work->field_8A2          = work->field_8A4;
    }
    Actor01900_Fn01C94(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 7;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0AB1C);

void Actor01900_Fn0ABA0(s32 arg0, Task* task)
{
    u16             count;
    Actor01900Work* work;

    work          = (Actor01900Work*)task->idMap;
    count         = work->field_6 + 1;
    work->field_6 = count;
    if ((s16)count >= 3) {
        task->state++;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0ABE4);
