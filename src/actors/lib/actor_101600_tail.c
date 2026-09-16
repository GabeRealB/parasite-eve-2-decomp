#include "common.h"
#include "actors/actor_101600.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include <psyq/abs.h>

void       Actor01600_Fn00480(Actor01600* arg0);
s32        Actor01600_Fn05558(Actor01600* arg0);
void       Actor01600_Fn06810(Actor01600Ctx* arg0, Actor01600* arg1);
void       Actor01600_Fn06A84(Actor01600* arg0);
void       Actor01600_Fn06F10(Actor01600* arg0);
void       Actor01600_Fn06FDC(Actor01600* arg0, s32 arg1);
void       Actor01600_Fn06744(Actor01600* arg0);
extern s32 Gp_LcgState;
void       Gp_UnlinkNode(void* node);
void       Gp_UnlinkObj(void* node);
void       Gp_EnemyTaskExit(Actor01600* arg0);
void       Gp_SetLightMode(void* arg0, s32 arg1);
void       Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void       Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void*      Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
void       Gp_DispatchMsg(void* arg0, s32 arg1, void* arg2, s32 arg3);
void       func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, void* arg3);
void       Actor01600_Fn06880(Actor01600* arg0);
MATRIX*    ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX*    MulMatrix(MATRIX* m0, MATRIX* m1);
void       Actor01600_Fn06EA4(Actor01600* arg0);
u8         Actor01600_Fn06F78(Actor01600* arg0);

s32 Actor01600_Fn052C4(Actor01600* arg0)
{
    Actor01600CoordPos* coord;
    Actor01600CoordPos* other;
    SVECTOR             d;
    s32                 dist;

    coord = (Actor01600CoordPos*)arg0->field_2C->field_8;
    if (Gp_ActorSlots[0] != NULL) {
        other = (Actor01600CoordPos*)Gp_ActorSlots[0]->field_2C->field_8;
        d.vx  = other->x - coord->x;
        d.vy  = other->y - coord->y;
        d.vz  = other->z - coord->z;
        dist  = SquareRoot0((d.vx * d.vx) + (d.vz * d.vz));
        if (Gp_ActorSlots[1] != NULL) {
            other = (Actor01600CoordPos*)Gp_ActorSlots[1]->field_2C->field_8;
            d.vx  = other->x - coord->x;
            d.vy  = other->y - coord->y;
            d.vz  = other->z - coord->z;
            return SquareRoot0((d.vx * d.vx) + (d.vz * d.vz)) < dist;
        }
    }
    return 0;
}

void Actor01600_Fn05400(Actor01600* arg0)
{
    TmdObject*      obj;
    TmdObject*      obj2;
    TmdObject*      obj3;
    TmdObject*      obj4;
    s32             kind;
    Actor01600Ctx*  ctx;
    Actor01600Work* work;

    ctx  = arg0->field_20;
    kind = ctx->field_3C->field_2;
    work = arg0->field_1C;
    switch (kind) {
        case 0:
            Actor01600_Fn00480(arg0);
            Tmd_AllocBuffers(arg0->field_2C);
            obj             = arg0->field_2C;
            obj->field_C   &= 0xFFFB;
            obj2            = arg0->field_2C;
            obj2->field_C  &= 0xFF7F;
            work->field_52E = 1;
            return;
        case 3:
            work->field_53C    = 1;
            ctx->node.field_4  = 1;
            work->field_52E    = 0;
            work->field_530    = 1;
            work->field_4D8    = 0;
            work->field_4DA    = (u16)ctx->field_3C->field_A;
            work->field_4DC    = 0;
            Actor01600_D12874 += 1;
            return;
        case 1:
            work->field_536 = kind;

        default:
            obj3               = arg0->field_2C;
            obj3->field_C     |= 0x80;
            obj4               = arg0->field_2C;
            obj4->field_C     |= 4;
            work->field_53C    = 1;
            ctx->node.field_4  = 1;
            work->field_532    = 1;
            work->field_52E    = 0;
            work->field_530    = 1;
            Actor01600_D12874 += 1;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_tail", Actor01600_Fn05558);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_tail", Actor01600_Fn05B08);

void Actor01600_Fn05F80(Actor01600* arg0)
{
    SVECTOR         rot;
    GsCOORDINATE2*  coord;
    Actor01600Work* work;
    GsCOORDINATE2*  part1;
    u32             variant;
    s32             anim;
    u16             timer;
    s32             value;

    coord   = arg0->field_2C->field_8;
    work    = arg0->field_1C;
    variant = arg0->field_20->field_3C->field_1;
    part1   = &arg0->field_2C->field_8[1];

    if (variant == 1) {
        switch (work->field_506) {
            case 0x1A:
                work->field_54A = variant;
                if ((u32)((u16)work->field_50A - 6) < 0xEU) {
                    coord->coord.t[1] -= 0xC8;
                }
                if ((u32)((u16)work->field_50A - 0x14) < 0xFU) {
                    value             = coord->coord.t[1] + 0x96;
                    coord->coord.t[1] = value;
                    if (value >= -0x497) {
                        coord->coord.t[1] = -0x498;
                    }
                }
                if ((u32)((u16)work->field_50A - 0xB) < 0x14U) {
                    value             = coord->coord.t[0] + ((coord->coord.m[0][2] * 0x4B) >> 0xB);
                    coord->coord.t[0] = value;
                    if (value < 0x3B23) {
                        coord->coord.t[0] = 0x3B23;
                    }
                    value             = coord->coord.t[2] + ((coord->coord.m[2][2] * 0x4B) >> 0xB);
                    coord->coord.t[2] = value;
                    if (value >= -0xD11) {
                        coord->coord.t[2] = -0xD12;
                    }
                }
                if ((s16)work->field_50A >= 0x31) {
                    work->field_506 = 8;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
                break;
            case 8:
                if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                    work->field_4D8  = 0;
                    work->field_4DC  = 0;
                    work->field_4DA += 0x32;
                    RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                }
                if ((s16)work->field_50A >= 0x1C) {
                    work->field_506 = 9;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
                break;
            case 0xFF:
                anim = work->field_50A;
                if (anim == 2) {
                    work->field_404 = part1;
                    work->field_408 = 0x100;
                    work->field_40A = anim;
                    func_800FDB18(1, part1, 0, &work->field_404);
                    Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[1], 0, NULL);
                }
                if ((u32)((u16)work->field_50A - 0xF) < 8U) {
                    coord->coord.t[1] += 0x80;
                }
                if ((s16)work->field_50A >= 3) {
                    coord->coord.t[0] += 0x10E;
                    coord->coord.t[2] -= 0xC8;
                    coord              = &arg0->field_2C->field_8[6];
                    rot.vx             = -0x400;
                    rot.vy             = 0;
                    rot.vz             = 0;
                    RotMatrix(&rot, &coord->coord);
                    coord  = &arg0->field_2C->field_8[8];
                    rot.vx = -0x400;
                    rot.vy = 0;
                    rot.vz = 0;
                    RotMatrix(&rot, &coord->coord);
                    if ((s16)work->field_50A >= 3) {
                        work->field_4D8  = 0x384;
                        work->field_4DA += 0x96;
                        timer            = work->field_4DC + 0x64;
                        work->field_4DC  = timer;
                        if ((s16)timer >= 0x384) {
                            work->field_4DC = 0x384;
                        }
                        coord = &arg0->field_2C->field_8[1];
                        RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                    }
                }
                if ((s16)work->field_50A >= 0x12) {
                    Actor01600_Fn06FDC(arg0, 0);
                }
                work->field_50A += 1;
                break;
        }
    }
    if (variant == 2) {
        timer           = work->field_524 - 1;
        work->field_524 = timer;
        if ((s16)timer < 0) {
            anim            = work->field_546;
            work->field_524 = 1;
            switch (anim) {
                case 7:
                    work->field_506 = anim;
                    if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                        work->field_4D8  = 0;
                        work->field_4DC  = 0;
                        work->field_4DA += 0x32;
                        RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                    }
                    if ((s16)work->field_50A >= 0x1C) {
                        work->field_546 = 9;
                        work->field_506 = 9;
                        work->field_508 = 0;
                        work->field_50A = 0;
                    }
                    break;
                case 9:
                    work->field_506 = anim;
                    if ((s16)work->field_50A >= 0x32) {
                        work->field_50A = 0;
                        work->field_508 = 0;
                        work->field_506 = 3;
                        work->field_546 = 0;
                    }
                    break;
            }
        }
    }
    if (variant == 4) {
        timer           = work->field_524 - 1;
        work->field_524 = timer;
        if ((s16)timer < 0) {
            anim            = work->field_546;
            work->field_524 = 1;
            if (anim == 7) {
                work->field_506 = anim;
                if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                    work->field_4D8  = 0;
                    work->field_4DC  = 0;
                    work->field_4DA -= 0xFA;
                    RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                }
                if ((s16)work->field_50A >= 0x1C) {
                    work->field_546 = 9;
                    work->field_506 = 9;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
            }
        }
    }
}

void Actor01600_Fn0646C(Actor01600* arg0)
{
    Actor01600** effect;
    TmdObject*   obj;
    TmdObject*   obj2;
    s32          randomState;
    s32          choice;

    if (arg0->field_1C->field_540 != 0) {
        D_800626EC[5].setupArg = (s32)&Actor01600_D0973C;
        effect                 = Gp_SpawnEff(0x80005, arg0->field_2C->field_8 + 1, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(*effect, arg0);
        }
        D_800626EC[5].setupArg = (s32)&Actor01600_D09EE0;
        effect                 = Gp_SpawnEff(0x80005, arg0->field_2C->field_8 + 2, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(*effect, arg0);
        }
        D_800626EC[5].setupArg = (s32)&Actor01600_D09EE0;
        effect                 = Gp_SpawnEff(0x80005, arg0->field_2C->field_8 + 3, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(*effect, arg0);
        }
        Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x300, &Actor01600_D12868);
        return;
    }
    randomState = (Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState = randomState;
    choice      = ((u32)randomState >> 0x10) & 3;
    switch (choice) {
        case 0:
        case 1:
            D_800626EC[5].setupArg = (s32)&Actor01600_D0973C;
            effect                 = Gp_SpawnEff(0x80005, arg0->field_2C->field_8 + 1, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(*effect, arg0);
            }
            break;
        case 2:
            D_800626EC[5].setupArg = (s32)&Actor01600_D09EE0;
            effect                 = Gp_SpawnEff(0x80005, arg0->field_2C->field_8 + 2, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(*effect, arg0);
            }
            break;
        case 3:
            D_800626EC[5].setupArg = (s32)&Actor01600_D09CFC;
            effect                 = Gp_SpawnEff(0x80005, arg0->field_2C->field_8 + 6, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(*effect, arg0);
            }
            break;
    }
    Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x50, &Actor01600_D12868);
    obj            = arg0->field_2C;
    obj->field_C  |= 0x80;
    obj2           = arg0->field_2C;
    obj2->field_C |= 4;
}

void Actor01600_Fn066E8(Actor01600* arg0)
{
    Actor01600StateFuncTable3 sp;

    sp = Actor01600_D00004;
    sp.funcs[arg0->field_30]((Actor01600Ctx*)arg0->field_20, arg0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_tail", Actor01600_Fn06744);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters.
void Actor01600_Fn06810(Actor01600Ctx* arg0, Actor01600* arg1)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &arg1->field_2C->field_8[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Squashes the actor's attachment coordinate: the work block's `field_49C`
/// rotation is copied into the coordinate, an identity is splatted into a
/// 0x30-byte scratch block and scaled per axis by 1.0 / the decaying
/// `field_518` / 1.0, and the product replaces the coordinate's rotation.
/// `flg` is cleared so its own work matrix is rebuilt from `coord` next frame.
void Actor01600_Fn06880(Actor01600* arg0)
{
    GsCOORDINATE2*          coord;
    u8*                     head;
    Actor01600ScaleScratch* scratch;
    Actor01600Work*         work;

    head                = *(u8**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (Actor01600ScaleScratch*)(head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_518 >= 0x201) {
        work->field_518 = (u16)work->field_518 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_518;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_49C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Steps the attachment coordinate `distance` units along the model's facing:
/// `Gfx_MatrixCol2` reads that coordinate's column into `dir`, `ratan2` turns it
/// into a yaw, `func_8004BFF8` builds the rotation for the yaw and
/// `ApplyMatrixLV` rotates the step vector `(distance, 0, 0)` by it before the
/// result is added to `coord.t`.
void Actor01600_Fn06974(Actor01600* actor, s32 distance)
{
    Actor01600Matrix*      mat;
    Actor01600StepScratch* work;
    GsCOORDINATE2*         coord;
    VECTOR*                head;
    void**                 scratch;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    coord         = actor->field_2C->field_8;
    work          = (Actor01600StepScratch*)((u8*)head - 0x3C);
    work->move.vx = (s16)distance;
    work->move.vy = 0;
    work->move.vz = 0;
    *scratch      = work;
    Gfx_MatrixCol2(&actor->field_2C->field_8->coord, (SVECTOR*)((u8*)head - 0x2C));
    mat                = (Actor01600Matrix*)((u8*)head - 0x24);
    work->yaw          = ratan2(work->dir.vx, work->dir.vz);
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->yaw, &mat->mat);
    ApplyMatrixLV(&mat->mat, &work->move, &work->move);
    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    *scratch           = (u8*)*scratch + 0x3C;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_tail", Actor01600_Fn06A84);

s32 Actor01600_Fn06C1C(Actor01600* arg0)
{
    Actor01600Work* work;

    work            = arg0->field_1C;
    work->field_42A = (u16)(work->field_42A | 0xC000);
    Actor01600_Fn04EB0(arg0);
    if (work->field_4EA >= 2) {
        work->field_506 = 0x19;
        work->field_51A = 0;
        work->field_4FE = 0;
        work->field_4EA = 0;
        work->field_514 = 1;
        work->field_2BA = (u16)(work->field_2BA & 0x3FFF);
        return 1;
    }
    return 0;
}

/// Column 2 of the attachment matrix goes to `dir`; the trailing `SVECTOR`
/// is never read but owns the second half of the stack local block.
s32 Actor01600_Fn06C94(Actor01600* arg0, s32 arg1, s32 unusedDistance)
{
    SVECTOR         dir;
    SVECTOR         unused;
    Actor01600Work* work;
    s32             ang;
    s32             half;
    s32             res;

    work = arg0->field_1C;
    if (ABS(arg1) < 0x301) {
        return 0;
    }
    work->field_4E0 = arg1;
    Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, &dir);
    ratan2(dir.vx, dir.vz);
    ang  = ABS(work->field_4E0);
    half = 0x1000 - ang;
    if (half < ang) {
        work->field_516 = 3;
        res             = half;
    } else {
        work->field_516 = 2;
        res             = ang;
    }
    work->field_4F0 = res / 16;
    if (work->field_4F0 < 0x20) {
        work->field_4F0 = 0x20;
    }
    work->field_50A = 0;
    work->field_510 = 5;
    return 1;
}

s32 Actor01600_Fn06D74(Actor01600* arg0, s32 arg1, s32 arg2)
{
    Actor01600Work* work;
    s32             scaledState;
    s32             handled;
    s32             angle;
    u32             state;

    work = arg0->field_1C;
    if (arg2 < 0x7D1) {
        angle = (arg1 >= 0 ? arg1 : -arg1);
        if (angle < 0x201) {
            work->field_42A |= 0xC000;
            Actor01600_Fn04EB0(arg0);
            handled = 0;
            if (work->field_4EA >= 2) {
                work->field_506  = 0x19;
                work->field_51A  = 0;
                work->field_4FE  = 0;
                work->field_4EA  = 0;
                work->field_514  = 1;
                work->field_2BA &= 0x3FFF;
                handled          = 1;
            }
            if ((u8)handled) {
                return 1;
            } else {
                scaledState     = Gp_LcgState * 5;
                state           = scaledState + 0x71357911;
                work->field_4FA = 0;
                work->field_508 = 0;
                work->field_52A = 0;
                Gp_LcgState     = (s32)state;
                if ((u32)(((state >> 16) % 100) & 0xFFFF) < 0x14U) {
                    work->field_516 = 6;
                    work->field_506 = 0x1A;
                } else {
                    work->field_516 = 5;
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/// Tears the actor down: flags its context node dead (`field_4` = 1), clears
/// `field_54`, unlinks the node from its list and releases the four display
/// objects held in the work block, then hands the task to `Gp_EnemyTaskExit`.
void Actor01600_Fn06EA4(Actor01600* arg0)
{
    Actor01600Ctx*  ctx;
    Actor01600Work* work;

    ctx  = arg0->field_20;
    work = arg0->field_1C;

    ctx->node.field_4 = 1;
    ctx->field_54     = 0;
    Gp_UnlinkNode(&ctx->node);
    Gp_UnlinkObj(work->field_40C);
    Gp_UnlinkObj(work->field_29C);
    Gp_UnlinkObj(work->collision.field_2EC);
    Gp_UnlinkObj(work->field_3CC);
    Gp_EnemyTaskExit(arg0);
}

void Actor01600_Fn06F10(Actor01600* arg0)
{
    Actor01600Work* work;

    work = arg0->field_1C;
    if (work->field_534 != 0) {
        Gp_DispatchMsg(work->field_4D4, 0x3F1, NULL, 0);
        Actor01600_D127DC = 0;
        work->field_534   = 0;
        Actor01600_D12870 = 0;
        work->field_50E   = 0;
        work->field_516   = 0;
        work->field_50A   = 0;
        work->field_526   = 0;
    }
}

/// Walks the sibling ring of task slot 4's children and reports whether any of
/// them has already been flagged `0x80` in its `field_2C` object. Returns 0xFF
/// when the slot has no children at all, 1 on the first flagged sibling and 0
/// when the whole ring is clean. The task argument is unused.
u8 Actor01600_Fn06F78(Actor01600* arg0)
{
    Actor01600* head;
    Actor01600* iter;

    head = ((Actor01600*)Game_GetPtrSlot(4))->field_C;
    if (head == NULL) {
        return 0xFF;
    }
    iter = head;
    do {
        if (iter->field_2C->field_C & 0x80) {
            return 1;
        }
        iter = iter->field_10;
    } while (iter != head);
    return 0;
}

void Actor01600_Fn06FDC(Actor01600* arg0, s32 arg1)
{
    Actor01600Work* work;
    Actor01600Ctx*  ctx;
    TmdObject*      obj;
    TmdObject*      obj2;

    obj           = arg0->field_2C;
    ctx           = arg0->field_20;
    work          = arg0->field_1C;
    obj->field_C  = (u16)(obj->field_C | 0x80);
    obj2          = arg0->field_2C;
    obj2->field_C = (u16)(obj2->field_C | 4);
    ctx->field_54 = 0;
    Gp_UnlinkNode(&ctx->node);
    if (!(arg1 & 0xFF)) {
        Gp_UnlinkObj(work->field_40C);
        Gp_UnlinkObj(work->field_29C);
        Gp_UnlinkObj(work->collision.field_2EC);
        Gp_UnlinkObj(work->field_3CC);
    }
    Gp_SetLightMode(ctx, 1);
    Gp_ReleaseStateF0Add(arg0, 0x10);
    Gp_DestroyEnemy(ctx, arg0);
    Actor01600_D12874 -= 1;
}

/// Copies the source actor's texture page and CLUT row (`field_24` /
/// `field_25`) onto this actor's model object, then re-runs the model stream
/// twice so the new page/CLUT is baked into both of the object's primitive
/// buffers. Objects without an aux buffer (`field_18` NULL) have nothing to
/// rebuild and are left alone.
void Actor01600_Fn070AC(Actor01600* arg0, Actor01600* arg1)
{
    TmdObject* src;
    TmdObject* dst;

    src           = arg1->field_2C;
    dst           = arg0->field_2C;
    dst->field_24 = src->field_24;
    dst->field_25 = src->field_25;
    if (dst->field_18 != NULL) {
        Tmd_ProcessStream(dst);
        Tmd_ProcessStream(dst);
    }
}

s32 Actor01600_Fn07100(Actor01600* arg0)
{
    arg0->field_1C->field_554 = 6;
    return 0;
}
