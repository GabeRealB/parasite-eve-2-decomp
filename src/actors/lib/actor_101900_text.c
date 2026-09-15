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
#include "main/session.h"
#include "main/sound.h"
#include "psyq/abs.h"

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn00260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0056C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn008B4);

extern SVECTOR Actor01900_D1730C;

s32 Actor01900_Fn00E00(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2)
{
    void**               scratch;
    u8*                  head;
    Actor01900DeltaFlag* s;
    register void*       p asm("v1");
    s32                  val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(rec, &s->delta, (s16)arg2, NULL) != 0) {
        coord->coord.t[0]   += ((Actor01900DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]   += s->delta.vz.h.hi;
        Actor01900_D1730C.vx = ((Actor01900DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        Actor01900_D1730C.vy = s->delta.vy.w >> 16;
        Actor01900_D1730C.vz = s->delta.vz.w >> 16;
        val                  = ((Actor01900DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                Actor01900_D1730C.vx++;
            } else {
                coord->coord.t[0]--;
                Actor01900_D1730C.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                Actor01900_D1730C.vz++;
            } else {
                coord->coord.t[2]--;
                Actor01900_D1730C.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}

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

void Actor01900_Fn03C04(GameSessionFrom4* session, GsCOORDINATE2* coord)
{
    Actor01900HeightClamp* row;
    s32                    offset;
    s32                    lo;
    s16                    i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->field_3 == row->field_0 && session->field_2 == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            return;
        }
    }
}

/// `Actor01900_Fn03C04`'s row scan without the clamp: nonzero when the
/// current room has an `Actor01900_D172CC` row.
static __inline__ s32 Actor01900_HasHeightClamp(GameSessionFrom4* session)
{
    Actor01900HeightClamp* row;
    s16                    i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->field_3 == row->field_0 && session->field_2 == row->field_2) {
            return 1;
        }
    }
    return 0;
}

s32 Actor01900_Fn03C98(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2, s16 arg3)
{
    Actor01900Delta* head;
    Actor01900Delta* s;
    Actor01900Delta* blk;
    s16              vy;
    SVECTOR*         step;

    if (D_80072729 == 1) {
        return 0;
    }
    head                               = *(Actor01900Delta**)G_SCRATCH_HEAD;
    blk                                = head - 1;
    *(Actor01900Delta**)G_SCRATCH_HEAD = blk;
    s                                  = blk;
    s->moved                           = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.w >> 16;
        s->step.vy = s->delta.vy.w >> 16;
        s->step.vz = s->delta.vz.w >> 16;
        if (Actor01900_HasHeightClamp(&Game_Session->field_4)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x180) {
                s->step.vy = (vy <= 0) ? -0x180 : 0x180;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0xC0) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xC0);
            gte_ldsv(step);
            __asm__ volatile("nop; nop; .word 0x4B98003D");
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.w & 0xFFFF) {
            if (s->delta.vx.w > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.w & 0xFFFF) {
            if (s->delta.vz.w > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor01900_HasHeightClamp(&Game_Session->field_4)) {
        Actor01900_Fn03C04(&Game_Session->field_4, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(Actor01900Delta**)G_SCRATCH_HEAD += 1;
    return s->moved;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03FF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn042BC);

void Actor01900_Fn04D14(Actor01900* arg0)
{
    Actor01900Work*         work;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    Actor01900ChaseScratch* s;
    s32                     turn;
    s32                     diffPos;
    s32                     diffNeg;
    s32                     yaw;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0xC0;
        work->field_898          = 1;
        work->field_89E          = 3;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C26   = 8;
        work->field_6     = 0;
        work->field_8     = 0;
        Actor01900_D172FC = 0;
        work->field_C40++;
        return;
    }
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD -= 1;
    s                                          = *(Actor01900ChaseScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg               = 0;
    Actor01900_Fn01C94(arg0);
    if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 0) {
        work->field_8++;
    } else {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    Actor01900_ConfigPositionDelta(&Wip_SysConfig, arg0->field_2C->field_8, &s->delta);
    if (work->field_8 >= 7) {
        s->playerYaw  = ratan2(-((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][0],
                               ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][2]);
        s->yaw        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->yaw        = Actor01900_NormalizeYaw(s->yaw);
        work->field_0 = 0x1A;
    }
    coord   = arg0->field_2C->field_8;
    s->turn = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn    = s->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->angle = s->turn - 1000;
        } else if (diffPos > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->angle = s->turn + 1000;
        } else if (diffNeg > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    }
    facing    = arg0->field_2C->field_8;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, s->angle, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    coord                        = arg0->field_2C->field_8;
    work->field_8AE              = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->field_2C->field_8->flg = 0;
    work->field_C24              = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C24 = work->field_C24 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C24 = 2;
    }
    Actor01900_MoveForward(arg0->field_2C->field_8, work->field_C24);
    Actor01900_D172FC += work->field_C24;
    if (work->field_C26 == 8 && work->field_8A2 >= 0x18) {
        work->field_C26 = -1;
    }
    if (work->field_C26 == -1 && work->field_8A2 == 0x12) {
        work->field_C26 = 0;
        work->field_6   = 0;
    }
    if (work->field_C26 == 0) {
        if (++work->field_6 == 5) {
            s->playerYaw = ratan2(-((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][0],
                                  ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][2]);
            Actor01900_ConfigPositionDelta(&Wip_SysConfig, arg0->field_2C->field_8, &s->delta);
            s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw    = Actor01900_NormalizeYaw(s->yaw);
            s->yaw = yaw;
            yaw    = yaw - s->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw <= 0x400) {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2                           += work->field_C26;
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0551C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn05B4C);

void Actor01900_Fn05F38(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;
    GsCOORDINATE2*  coord;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        work->field_6   = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7U) {
        coord = arg0->field_2C->field_8;
        Actor01900_StepForward(coord, -0x78);
        Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC);
        arg0->field_2C->field_8->flg = 0;
    }
    if (work->field_68 & 0x100) {
        if (enemy->node.field_5 == 1) {
            work->field_0 = 10;
        } else {
            work->field_0 = 6;
        }
    }
}

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

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor01900_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                     head;
    Actor01900RangeScratch* blk;
    s32                     ret;

    head                                         = *(u8**)G_SCRATCH_HEAD;
    ((Actor01900RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                          = (Actor01900RangeScratch*)(head - 0xC);
    blk->dz                                      = d->vz;
    blk->r                                       = r;
    ((Actor01900RangeScratch*)(head - 0xC))->dx *= ((Actor01900RangeScratch*)(head - 0xC))->dx;
    *(Actor01900RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                     *= blk->dz;
    blk->r                                      *= blk->r;
    *(u8**)G_SCRATCH_HEAD                        = head;
    ret                                          = ((Actor01900RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Arms `Gp_StateF0` and returns 1 when the player is within 500 units of the
/// actor's height (and not in `field_954` state 2).
static __inline__ s32 Actor01900_ArmIfPlayerLevel(Actor01900* arg0)
{
    GpActorWork* player;
    s32          dy;

    player = Game_GetPtrSlot(3);
    if (player->actor->field_954 != 2) {
        dy = arg0->field_2C->field_8->coord.t[1] - player->extra->field_8->coord.t[1];
        if (ABS(dy) < 0x1F4) {
            Gp_ArmStateF0(1);
            return 1;
        }
    }
    return 0;
}

void Actor01900_Fn06B4C(Actor01900* arg0)
{
    SVECTOR         delta;
    SVECTOR*        d;
    Actor01900Work* work;
    GpEnemy*        enemy;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    s32             sound;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj               = arg0->field_2C;
        Actor01900_D171B4 = &Actor01900_D16960;
        work->field_89E   = 0x10;
        work->field_898   = 2;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B0          = 0;
        work->field_8A2          = 0x10;
        work->field_8AE          = 0;
        work->field_6            = 0;
        work->field_894          = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((work->field_5A & 0x3FF) == 0xF && work->field_894 != (work->field_5A & 0x3FF) &&
        (*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x01090000) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sound       = 0x51090009;
        if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
            sound = 0x51090008;
        }
        switch ((u8)Gp_GetViewIndex()) {
            case 2:
                SndEvt_EnqueueType6(sound, 0x64, 0);
                break;
            case 3:
                SndEvt_EnqueueType6(sound, 0x50, 0x1F);
                break;
            case 4:
            default:
                SndEvt_EnqueueType6(sound, 0x40, 0x4C);
                break;
        }
    }
    if ((work->field_5A & 0x3FF) == 5 && work->field_894 != (work->field_5A & 0x3FF)) {
        work->field_8B8.field_0 = arg0->field_2C->field_8 + 1;
        work->field_8B8.field_4 = 0x200;
        work->field_8B8.field_6 = 2;
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x01030000 || (u8)Gp_GetViewIndex() != 0x10) {
            func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->field_8 + 5, NULL, &work->field_8B8);
        }
    }
    work->field_894 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->field_8;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor01900_OutOfRange(d, work->field_C32)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
            work->field_0 = 6;
        }
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06F40);

void Actor01900_Fn07810(Actor01900* arg0)
{
    Actor01900Work*        work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor01900TurnScratch* turn;
    u16                    next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B0          = 0;
        work->field_8A2          = 0x1E;
    }
    *(Actor01900TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                      = *(Actor01900TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                               = Actor01900_PositionYaw(arg0, &turn->delta, &Wip_SysConfig);
    work->field_8AE                           = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->field_2C->field_8;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, turn->angle, 1);
    if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    Actor01900_MoveForward(arg0->field_2C->field_8, work->field_C24);
    if (work->field_C24 > 0) {
        next            = work->field_C24 - 0xA;
        work->field_C24 = next;
        if ((s16)next < 0) {
            work->field_C24 = 0;
        }
    }
    Actor01900_Fn01C94(arg0);
    if ((work->field_68 & 0x100) || work->field_C24 == 0) {
        work->field_0 = 9;
    }
    *(Actor01900TurnScratch**)G_SCRATCH_HEAD += 1;
}

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
    Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC);
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

void Actor01900_Fn09D3C(GpEnemy* enemy, Actor01900* actor)
{
    VECTOR                 pos;
    Actor01900StateTable   states;
    Actor01900Work*        work;
    Actor01900ViewScratch* scratch;
    Actor01900ViewScratch* head;
    s32                    state;

    work   = actor->field_1C;
    states = Actor01900_D001BC;

    actor->field_2C->field_8->flg = 0;
    Gp_UpdateCoord(actor->field_2C->field_8);
    pos.vx = actor->field_2C->field_8->workm.t[0];
    pos.vy = actor->field_2C->field_8->workm.t[1];
    pos.vz = actor->field_2C->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (D_801153F4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
        case 2:
            actor->field_2C->field_C = 0x80;
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
    }

    head                                     = *(Actor01900ViewScratch**)G_SCRATCH_HEAD;
    *(Actor01900ViewScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                                  = head - 1;

    if (work->field_C10 > 0) {
        work->field_C10 = (s16)((u16)work->field_C10 - 1);
    } else {
        Actor01900_Fn02A50(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    state         = work->field_0;
    if ((state == 0x1C) || (state == 0x15) || (state == 0) || (state == 0x1D) || (state == 0x1E)) {
        work->field_8C8.flags &= 0x7FFF;
        work->field_A08.flags &= 0x7FFF;
    } else {
        work->field_8C8.flags |= 0x8000;
    }
    states.fn[work->field_0](actor);
    Gp_ClearRec18Occupied(&work->field_A28);
    Gp_ClearRec18Occupied(&work->field_8E8);
    Gp_ClearRec18Occupied(&work->field_B68);
    if ((D_801153F2[1] == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    Actor01900_TransformToView(actor->field_2C->field_8 + 2, &scratch->pos);

    work->field_C48[work->field_C98].vx = scratch->pos.vx;
    work->field_C48[work->field_C98].vy = scratch->pos.vy;
    work->field_C48[work->field_C98].vz = scratch->pos.vz;

    *(u8**)G_SCRATCH_HEAD += 0x18;
    work->field_C98        = (u16)work->field_C98 + 1;
    if (work->field_C98 == 7) {
        work->field_C98 = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        enemy->field_1C.vx = work->field_C48[work->field_C98].vx;
        enemy->field_1C.vy = work->field_C48[work->field_C98].vy;
        enemy->field_1C.vz = work->field_C48[work->field_C98].vz;
    } else {
        enemy->field_1C.vx = scratch->pos.vx;
        enemy->field_1C.vy = scratch->pos.vy;
        enemy->field_1C.vz = scratch->pos.vz;
    }
    enemy->field_18 = &Gfx_ViewCoord;
}

void Actor01900_Fn0A314(void)
{
}
