#include "common.h"

#include "actors/actor_101900.h"
#include "actors/actor_101900_facing.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"

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

s32 Actor01900_Fn01A7C(Actor01900Work* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->field_5A & 0x3FF;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 7:
            id = work->field_5A & 0x3FF;
            if (id == 0xF) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x14) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
        case 3:
            id = work->field_5A & 0x3FF;
            if (id == 0x24) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x2C) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        case 4:
            id = work->field_5A & 0x3FF;
            if (id == 0xC && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000C;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        case 11:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        default:
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}

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

void Actor01900_Fn06904(Actor01900* arg0)
{
    Actor01900Work*       work;
    GpEnemy*              enemy;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Actor01900RotScratch* blk;
    u8*                   head;
    u8*                   tail;
    void*                 scratch_base;
    s16                   temp_v0;
    s16                   ang;
    s16                   cur;
    s32                   k;
    s32                   sy;
    u16                   temp_v1;
    u16                   m22;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.field_4   = 1;
        work->field_6         = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0x13);
                break;
            case 5:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->field_2C->field_8 + 2, 3, NULL);
                break;
            case 23:
                arg0->field_2C->field_C = 2;
                break;
            case 17:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 39:
                arg0->field_2C->field_C = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                       = 0x1194;
            head                                    = scratch_base;
            head                                    = *(u8**)(head + 0x3FC);
            coord                                   = arg0->field_2C->field_8;
            blk                                     = (Actor01900RotScratch*)(head - 0x34);
            sy                                      = k - (cur - 0x14) * 0xB;
            *(Actor01900RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                     = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                              = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor01900RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor01900RotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06B4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06F40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn07810);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn07BA8);

void Actor01900_Fn080A8(Actor01900* arg0)
{
    Actor01900Work*       work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Actor01900AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 0x10;
        work->field_89E          = 9;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   &= 0xBFFF;
        Actor01900_Fn01C94(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(Actor01900AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                      = *(Actor01900AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg             = 0;
    if (work->field_68 & 0x100) {
        work->field_0 = 7;
    }
    aim->angle      = Actor01900_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    work->field_8AE = aim->angle;
    if (aim->angle > 0) {
        aim->angle = 0;
    }
    if (aim->angle < 0) {
        aim->angle = 0;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    Actor01900_Fn01C94(arg0);
    *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
}

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
