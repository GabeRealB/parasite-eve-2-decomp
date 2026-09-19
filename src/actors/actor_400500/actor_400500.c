#include "common.h"

#include "actors/actors_shared_80131fc8.h"
#include "psyq/inline_c.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400500.h"
#include "actors/coord_to_view.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `func_actor_400500_80134B88` scores 87.27%
 * (12 register and 8 reorder penalties); as a one-element array it is exact,
 * the same remedy `actor_400600` needed for the same global. */
extern void* D_800678F0[1];
extern u8    D_80072170;

/* Model streams in the overlay's own `.data`, selected through `D_800678F0`. */
extern u8 D_actor_400500_8014393C[];
extern u8 D_actor_400500_80143F40[];
extern u8 D_actor_400500_80144624[];

extern GpPairSrcE D_actor_400500_80153C90;
extern u8         D_actor_400500_80153CA0[];
extern u8         D_actor_400500_80153CC0[];
extern TaskDesc   D_actor_400500_80153D48;
extern u16        D_actor_400500_80153DB4[];
extern u8         D_actor_400500_80153DD4[];
extern s32        Gp_LcgState;

void func_8009EA50(s32 arg0);
s32  func_actor_400500_80132D74(Task* arg0);
s32  func_actor_400500_80133160(Task* arg0);
s32  func_actor_400500_80133358(Task* arg0);
s32  func_actor_400500_80133460(Task* arg0);
void func_actor_400500_801335E8(Task* arg0);
void func_actor_400500_8013403C(Task* arg0);
void func_actor_400500_80139448(Task* arg0);
void func_actor_400500_8013A0B8(Task* arg0);
void func_actor_400500_8013B4A4(Task* arg0);
void func_actor_400500_8013C7A4(Task* arg0);
void func_actor_400500_8013CA38(Task* arg0);
void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DB78(Task* arg0);
void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2);
void func_actor_400500_8013DC4C(Task* arg0);
void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400500_8013DCD4(Task* arg0);
s32  func_actor_400500_8013DD8C(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DDEC(Task* arg0);
void func_actor_400500_8013DF50(Task* arg0);

void func_actor_400500_80132000(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->work;

    work->obj0.coord    = &((TmdObject*)arg0->extra)->coords[3];
    work->obj0.ctx.recs = work->rec0;
    work->obj0.pos.vz   = 0x110;
    work->obj0.pos.vx   = 0;
    work->obj0.pos.vy   = 0;
    work->obj0.key      = 0x30005;
    work->obj0.radius   = 0x260;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(work->rec0, 3, 0);
    work->obj0.flags |= 0x8000;

    work->obj1.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj1.coord    = &((TmdObject*)arg0->extra)->coords[7];
    work->obj1.ctx.recs = work->rec1;
    work->obj1.pos.vx   = -0x460;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.radius   = 0x290;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj1.flags &= 0x7FFF;

    work->obj2.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj2.coord    = &((TmdObject*)arg0->extra)->coords[7];
    work->obj2.ctx.recs = work->rec1;
    work->obj2.pos.vx   = -0x200;
    work->obj2.pos.vy   = 0;
    work->obj2.pos.vz   = 0;
    work->obj2.radius   = 0x250;
    work->obj2.flags    = 1;
    Gp_LinkObj(3, &work->obj2);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj2.flags &= 0x7FFF;

    work->obj3.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj3.coord    = &((TmdObject*)arg0->extra)->coords[10];
    work->obj3.ctx.recs = work->rec2;
    work->obj3.pos.vx   = 0x460;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj3.radius   = 0x290;
    work->obj3.flags    = 1;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj3.flags &= 0x7FFF;

    work->obj4.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj4.coord    = &((TmdObject*)arg0->extra)->coords[10];
    work->obj4.ctx.recs = work->rec2;
    work->obj4.pos.vx   = 0x200;
    work->obj4.pos.vy   = 0;
    work->obj4.pos.vz   = 0;
    work->obj4.radius   = 0x250;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj4.flags &= 0x7FFF;
}

void func_actor_400500_8013226C(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    GsCOORDINATE2*     parts;
    GsCOORDINATE2*     part7;
    GsCOORDINATE2*     part10;
    GsCOORDINATE2*     coord;
    Task*              child;
    TmdObject*         extra;
    TmdObject*         tmd;
    TmdObject*         parentTmd;

    parts              = ((TmdObject*)arg0->extra)->coords;
    work               = (Actor400500Work*)arg0->work;
    part7              = &parts[7];
    part10             = &parts[10];
    child              = Task_SpawnFromTable(&D_actor_400500_80153D48, 0, 0, 0);
    work->field_9F0[0] = child;
    extra              = (TmdObject*)child->extra;
    coord              = extra->coords;
    extra->flags       = 0x80;
    coord->sub         = part10;
    coord->coord.t[0]  = 0x400;
    coord->coord.t[1]  = 0;
    coord->coord.t[2]  = 0;
    src                = &rot;
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    func_8004BFF8(-0x180, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    parentTmd  = (TmdObject*)arg0->extra;
    tmd        = (TmdObject*)child->extra;
    tmd->tpage = parentTmd->tpage;
    tmd->clut  = parentTmd->clut;
    if (tmd->buffer != NULL) {
        tmdProcessStream(tmd);
        tmdProcessStream(tmd);
    }
    child              = Task_SpawnFromTable(&D_actor_400500_80153D48, 1, 0, 0);
    work->field_9F0[1] = child;
    extra              = (TmdObject*)child->extra;
    coord              = extra->coords;
    extra->flags       = 0x80;
    coord->sub         = part7;
    coord->coord.t[0]  = -0x400;
    coord->coord.t[1]  = 0;
    coord->coord.t[2]  = 0;
    parentTmd          = (TmdObject*)arg0->extra;
    tmd                = (TmdObject*)child->extra;
    tmd->tpage         = parentTmd->tpage;
    tmd->clut          = parentTmd->clut;
    if (tmd->buffer != NULL) {
        tmdProcessStream(tmd);
        tmdProcessStream(tmd);
    }
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    func_8004BFF8(0x180, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
}

void func_actor_400500_80132438(Task* arg0)
{
    SVECTOR            dir;
    SVECTOR*           dirp;
    SVECTOR            delta;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    Actor400500Work*   work;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     other;
    s16                dist;
    s16                heading;
    s16                vz;
    s32                y;
    s32                z;
    s32                one;
    u16                counter;

    work  = (Actor400500Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_ActorSlots[0] != NULL) {
        other              = Gp_ActorSlots[0]->extra->coords;
        work->field_9C0.vx = coord->coord.t[0];
        work->field_9C0.vy = coord->coord.t[1];
        work->field_9C0.vz = coord->coord.t[2];
        if ((s16)work->field_A1C != 5) {
            dir.vx = (u16)other->coord.t[0] - (u16)coord->coord.t[0];
            dir.vy = (u16)other->coord.t[1] - (u16)coord->coord.t[1];
            dir.vz = (u16)other->coord.t[2] - (u16)coord->coord.t[2];
        } else {
            work->field_A32 = 2;
            counter         = work->field_A22 + 1;
            work->field_A22 = counter;
            if (!(counter & 0x100)) {
                dir.vx = 0x2710 - (u16)coord->coord.t[0];
            } else {
                dir.vx = 0x3E8 - (u16)coord->coord.t[0];
            }
            y      = -0x3E8;
            dir.vy = y - (u16)coord->coord.t[1];
            z      = -0x20D0;
            dir.vz = z - (u16)coord->coord.t[2];
        }
        dist = SquareRoot0((dir.vx * dir.vx) + (dir.vz * dir.vz));
        do {
            work->field_9E0 = (u16)dir.vx;
            dirp            = &dir;
            work->field_9E2 = (u16)dir.vy;
        } while (0);
        vz              = (u16)dir.vz;
        work->field_A16 = dist;
        work->field_9E4 = vz;
        VectorNormalSS(dirp, dirp);
        work->field_A36    = (ratan2(dir.vx, dir.vz) - (u16)work->field_94A) & 0xFFF;
        delta.vx           = (u16)other->coord.t[0] - (u16)work->field_9D0.vx;
        delta.vy           = (u16)other->coord.t[1] - (u16)work->field_9D0.vy;
        one                = 0x1000;
        delta.vz           = (u16)other->coord.t[2] - (u16)work->field_9D0.vz;
        src                = &rot;
        rot.ident.m00_m01  = one;
        rot.ident.m02_m10  = 0;
        src->ident.m11_m12 = one;
        rot.ident.m20_m21  = 0;
        src->ident.m22     = one;
        heading            = work->field_94A;
        ((void (*)(s32, MATRIX*))func_8004BFF8)(-heading, &src->mat);
        ApplyMatrixSV(&src->mat, &delta, &work->field_9D8);
    }
}

void func_actor_400500_80132628(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, s32 shade)
{
    MATRIX         firstMatrix;
    MATRIX         secondMatrix;
    SVECTOR        first;
    SVECTOR        second;
    SVECTOR        corner0;
    SVECTOR        corner1;
    SVECTOR        corner2;
    SVECTOR        corner3;
    s32            screen0;
    s32            screen1;
    s32            screen2;
    s32            screen3;
    s32            perspective;
    s32            texU1;
    s32            flags;
    s16            angle;
    GsCOORDINATE2* secondCoord;
    GsCOORDINATE2* firstCoord;
    s32            offset0;
    s32            offset1;
    s32            offset2;
    s32            offset3;
    s32            halfX;
    s32            halfZ;
    s32            depth;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* viewCoord;
    POLY_FT4*      poly;
    u8             room;
    u8             col;
    u8             texU0;
    u8             texV0;
    s32            x3val;

    col         = shade;
    coords      = ((TmdObject*)task->extra)->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &firstCoord->workm, &firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &secondCoord->workm, &secondMatrix);
        first.vy       = (s16)height;
        second.vy      = (s16)height;
        first.vx       = firstMatrix.t[0];
        first.vz       = firstMatrix.t[2];
        second.vx      = secondMatrix.t[0];
        second.vz      = secondMatrix.t[2];
        angle          = ratan2((s16)secondMatrix.t[0] - (s16)firstMatrix.t[0], (s16)secondMatrix.t[2] - (s16)firstMatrix.t[2]);
        halfX          = (first.vx - second.vx) / 2;
        halfZ          = (first.vz - second.vz) / 2;
        offset0        = rcos(angle) * width;
        corner0.vy     = (s16)height;
        corner0.vx     = halfX + (first.vx - (offset0 >> 0xC));
        corner0.vz     = halfZ + (first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1        = rcos(angle) * width;
        corner1.vy     = (s16)height;
        corner1.vx     = halfX + (first.vx + (offset1 >> 0xC));
        corner1.vz     = halfZ + (first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2        = rcos(angle) * width;
        corner2.vy     = (s16)height;
        corner2.vx     = (second.vx - (offset2 >> 0xC)) - halfX;
        corner2.vz     = (second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3        = rcos(angle) * width;
        corner3.vy     = (s16)height;
        corner3.vx     = (second.vx + (offset3 >> 0xC)) - halfX;
        corner3.vz     = (second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        viewCoord      = (GsCOORDINATE2*)((u8*)&Gfx_ViewWorldMtx - OFFSET_OF(GsCOORDINATE2, workm));
        viewCoord->flg = 0;
        Gp_UpdateCoord(viewCoord);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3,
                              &perspective, &flags);
        if (flags >= 0) {
            poly           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)((u8*)poly + sizeof(POLY_FT4));
            setlen(poly, 9);
            poly->code       = 0x2E;
            texU0            = 0xC0;
            texV0            = 0x98;
            *(s32*)&poly->x0 = screen0;
            *(s32*)&poly->x1 = screen1;
            poly->tpage      = 0x48;
            *(s32*)&poly->x2 = screen2;
            x3val            = screen3;
            SOFT_COMPILER_BARRIER();
            texU1 = 0xF7;
            SOFT_COMPILER_BARRIER();
            poly->v0         = texV0;
            poly->v1         = texV0;
            poly->v2         = 0xCF;
            poly->v3         = 0xCF;
            poly->u0         = texU0;
            poly->u1         = texU1;
            poly->u2         = texU0;
            poly->u3         = texU1;
            *(s32*)&poly->x3 = x3val;
            poly->clut       = 0x4283;
            room             = gGameSession->at4.loc.room;
            if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
                poly->r0 = col;
                poly->g0 = col;
                poly->b0 = col;
            } else {
                poly->r0 = shade;
                poly->g0 = col >> 1;
                poly->b0 = shade;
            }
            addPrim((u32*)((((u32)(depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
        }
    }
}

void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    func_actor_400500_80132628(arg0, 3, 9, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 9, 0xA, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xA, 0xB, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 3, 6, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 6, 7, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 7, 8, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 5, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xC, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xC, 0xD, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xD, 0xE, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xF, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xF, 0x10, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0x10, 0x11, 0x100, (s32)arg1, temp_s2);
}

void func_actor_400500_80132C54(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    GsCOORDINATE2*     coord;
    s32                tx;

    work  = (Actor400500Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (D_80072170) {
        case 1:
            tx                = 0x800;
            work->field_94A   = tx;
            work->field_94C   = tx;
            tx                = 0x14A0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 2:
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0x4074;
            work->field_94A   = 0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 3:
            tx                = 0xC00;
            work->field_94A   = tx;
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0xFA0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
    }
    tx                 = 0x1000;
    src                = &rot;
    rot.ident.m00_m01  = tx;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = tx;
    src->ident.m20_m21 = 0;
    src->ident.m22     = tx;
    RotMatrixZ(work->field_94C, &src->mat);
    func_8004BFF8(work->field_94A, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    func_actor_400500_8013DBCC(arg0, 0xB, &work->field_9A0);
}

s32 func_actor_400500_80132D74(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->work;
    if ((s16)work->field_A1C != 5) {
        if ((work->field_A16 < (0x500 - (work->field_9D8.vz * 8))) &&
            ((u32)(work->field_A36 - 0x2E0) >= 0xA41U)) {
            if ((((u16)work->field_A32 >> 3) == 0) && !(work->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 1);
                return 1;
            }
            return 0;
        }
        if ((work->field_A16 < (0x640 - (work->field_9D8.vz * 8))) &&
            ((u32)(work->field_A36 - 0x300) >= 0xA01U) &&
            (work->field_A32 == 0)) {
            if (!(((Actor400500Work*)arg0->work)->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 2);
            } else {
                func_actor_400500_8013DB64(arg0, 3);
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

void func_actor_400500_80132E94(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    TmdObject*       extra;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    extra = (TmdObject*)arg0->extra;
    if (work->field_A46 < 0) {
        if (!((u8)work->field_A46 & 1)) {
            switch (work->field_A47) {
                case 0:
                    work->field_A20 = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 2);
                    if (work->field_A20 >= 0xF8) {
                        work->field_A20 = 0xFF;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    func_8009EA50(work->field_A20);
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2C < work->field_A2A) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A24 = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 2);
                    work->field_A28 = (u16)work->field_A28 + (-work->field_A28 >> 2);
                    if (work->field_A24 == 0) {
                        enemy->node.flags = 1;
                        if ((u8)work->field_A4C == 0) {
                            enemy->node.flags = 5;
                        }
                        work->field_A28 = 0;
                        work->field_A46 = 0;
                        extra->flags   |= 0x80;
                    }
                    extra->lightLevel = work->field_A24;
                    break;
            }
        } else {
            switch (work->field_A47) {
                case 0:
                    enemy->node.flags = 0;
                    if ((u8)work->field_A4C == 0) {
                        enemy->node.flags = 4;
                    }
                    extra->flags   &= ~0x80;
                    work->field_A24 = (u16)work->field_A24 + ((s16)(0x1000 - (u16)work->field_A24) >> 2);
                    work->field_A28 = (u16)work->field_A28 + ((0xFF - work->field_A28) >> 2);
                    if (work->field_A24 >= 0xFF0) {
                        work->field_A28 = 0xFF;
                        work->field_A24 = 0x1000;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    extra->lightLevel = work->field_A24;
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2A >= 0x11) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A20 = (u16)work->field_A20 + ((s16) - (u16)work->field_A20 >> 2);
                    if (work->field_A20 < 9) {
                        work->field_A20 = 0;
                        work->field_A46 = 0;
                        func_actor_400500_8013B4A4(arg0);
                        if (work->field_A30 == 0) {
                            work->field_A30 = (u16)work->field_A2E;
                        }
                    }
                    func_8009EA50(work->field_A20);
                    break;
            }
        }
    }
    if (work->field_A30 > 0) {
        work->field_A30 = (u16)work->field_A30 - 1;
    }
}

s32 func_actor_400500_80133160(Task* arg0)
{
    Actor400500Work* work;
    s32              soundId;
    s32              pan;
    u16              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A38 == 1) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x17, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050001;
            pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
        heading         = (u16)work->field_94A - D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    if (work->field_A38 == 2) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x18, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050002;
            pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
        heading         = (u16)work->field_94A + D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    return 0;
}

s32 func_actor_400500_80133358(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;
    s32                 ret;

    work = (Actor400500Work*)arg0->work;
    mode = work->field_A3C;
    if (mode == 1) {
        ret = 0;
        sub = work->field_A3E;
        if (sub == mode) {
            goto zero_both;
        }
        if ((sub == 2) || (sub == 4)) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A0E = 4;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xA;
            work2->field_9FA = 1;
            work->field_A3E  = 0;
            goto check_hit;
        }
        if (sub != 3) {
            goto check_hit;
        }
        func_actor_400500_8013DB64(arg0, 0xC);
        ret = 1;
    zero_both:
        work->field_A3C = 0;
        work->field_A3E = 0;
        return ret;
    check_hit:
        hit = (Actor400500HitView*)arg0->work;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

s32 func_actor_400500_80133460(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;

    work = (Actor400500Work*)arg0->work;
    mode = work->field_A3C;
    if (mode == 1) {
        sub = work->field_A3E;
        if (sub == mode) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x1C;
            work2->field_9FE = 0xB;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 2) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 4) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 3) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xE;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        }
        hit = (Actor400500HitView*)arg0->work;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

void func_actor_400500_801335E8(Task* arg0)
{
    MATRIX                 local;
    MATRIX                 local2;
    MATRIX                 world;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    Actor400500HitView*    hit;
    Actor400500ViewPos*    posA;
    Actor400500ViewPos*    posB;
    Actor400500ViewPos*    posC;
    Actor400500ViewPos*    posD;
    Actor400500ViewPos*    saveA;
    Actor400500ViewPos*    saveB;
    Actor400500ViewPos*    saveC;
    Actor400500ViewPos*    saveD;
    GsCOORDINATE2*         root;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         soundCoords;
    GsCOORDINATE2*         soundCoords2;
    s32                    i;
    s32                    cond;
    s32                    z;
    s32                    soundId;
    s32                    pan;
    s32                    pan2;
    s32                    sc0;
    s32                    sc1;
    s32                    cur;
    s32                    q0;
    s32                    q1;
    s32                    q2;
    s32                    dx;
    s32                    dz;
    s32                    posZ;
    s32                    dx2;
    s32                    dz2;
    s32                    posZ2;

    work = (Actor400500Work*)arg0->work;
    root = ((TmdObject*)arg0->extra)->coords;
    if (work->field_9FE != 2) {
        work->field_9F8 = 0x18;
        work->field_9FE = 2;
        work->field_9FA = 2;
        work2           = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i      = 1;
        stride = (Actor400500AnimStride*)work2 + 1;
        do {
            stride->field_1D = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
            stride++;
        } while (i < 0x12);
    }

    sc0 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc0 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xB00 / sc0) >> 4;
    }
    q0 = cur;

    sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc1 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xC00 / sc1) >> 4;
        COMPILER_BARRIER();
        sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    }
    q1 = cur;

    if (sc1 == 0) {
        sc0 = 0;
    } else {
        sc0 = (u32)(0x1500 / sc1) >> 4;
    }
    q2 = sc0;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A00 = 0;
    }

    SOFT_MOVE_ZERO(z);
    if (work->field_A00 == z) {
        saveA       = &work->field_9A0;
        soundCoords = ((TmdObject*)arg0->extra)->coords;
        Gp_UpdateCoord(&soundCoords[0xB]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords[0xB].workm, &local);
        posA                 = saveA;
        posA->x              = local.t[0];
        posA->z              = local.t[2];
        soundCoords[0xB].flg = 0;
        soundId              = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050001;
        pan                  = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_A00 == (s32)(q1 & 0xFF)) {
        saveB        = &work->field_9A0;
        soundCoords2 = ((TmdObject*)arg0->extra)->coords;
        Gp_UpdateCoord(&soundCoords2[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords2[8].workm, &local);
        posB                = saveB;
        posB->x             = local.t[0];
        posB->z             = local.t[2];
        soundCoords2[8].flg = 0;
        soundId             = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050002;
        pan2                = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((work->field_A00 >= z) && (work->field_A00 <= (s32)(q0 & 0xFF))) {
        coords = ((TmdObject*)arg0->extra)->coords;
        coord  = &coords[0xB];
        Gp_UpdateCoord(coord);
        saveC = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local2);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
        dx                 = world.t[0] - local2.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        posC               = saveC;
        posZ               = posC->z;
        dz                 = world.t[2] - local2.t[2];
        coords->flg        = 0;
        coords[0xB].flg    = 0;
        coords->coord.t[2] = posZ - dz;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    if ((work->field_A00 >= (s32)(q1 & 0xFF)) && (work->field_A00 <= (s32)(q2 & 0xFF))) {
        coords = ((TmdObject*)arg0->extra)->coords;
        coord  = &coords[8];
        Gp_UpdateCoord(coord);
        saveD = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local2);
        dx2                = local2.t[0] - local.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx2;
        posD               = saveD;
        posZ2              = posD->z;
        dz2                = local2.t[2] - local.t[2];
        coords->flg        = 0;
        coords[8].flg      = 0;
        coords->coord.t[2] = posZ2 - dz2;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    root->flg = 0;
}

void func_actor_400500_80133B14(Task* arg0)
{
    MATRIX                 local;
    MATRIX                 local2;
    MATRIX                 world;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    Actor400500HitView*    hit;
    Actor400500ViewPos*    posA;
    Actor400500ViewPos*    posB;
    Actor400500ViewPos*    posC;
    Actor400500ViewPos*    posD;
    Actor400500ViewPos*    saveA;
    Actor400500ViewPos*    saveB;
    Actor400500ViewPos*    saveC;
    Actor400500ViewPos*    saveD;
    GsCOORDINATE2*         root;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         soundCoords;
    GsCOORDINATE2*         soundCoords2;
    s32                    i;
    s32                    cond;
    s32                    z;
    s32                    soundId;
    s32                    pan;
    s32                    soundId2;
    s32                    pan2;
    s32                    sc0;
    s32                    sc1;
    s32                    cur;
    s32                    q0;
    s32                    q1;
    s32                    q2;
    s32                    dx;
    s32                    dz;
    s32                    posZ;
    s32                    dx2;
    s32                    dz2;
    s32                    posZ2;

    work = (Actor400500Work*)arg0->work;
    root = ((TmdObject*)arg0->extra)->coords;
    if (work->field_9FE != 2) {
        work->field_9FE = 2;
        work->field_9FA = 2;
        work2           = (Actor400500Work*)arg0->work;
        (void)*(volatile u16*)&work->field_9F8;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i      = 1;
        stride = (Actor400500AnimStride*)work2 + 1;
        do {
            stride->field_1D = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
            stride++;
        } while (i < 0x12);
    }

    sc0 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc0 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xB00 / sc0) >> 4;
    }
    q0 = cur;

    sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc1 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xC00 / sc1) >> 4;
        COMPILER_BARRIER();
        sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    }
    q1 = cur;

    if (sc1 == 0) {
        sc0 = 0;
    } else {
        sc0 = (u32)(0x1500 / sc1) >> 4;
    }
    q2 = sc0;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A00 = 0;
    }

    SOFT_MOVE_ZERO(z);
    if (work->field_A00 == z) {
        saveA       = &work->field_9A0;
        soundCoords = ((TmdObject*)arg0->extra)->coords;
        Gp_UpdateCoord(&soundCoords[0xB]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords[0xB].workm, &local);
        posA                 = saveA;
        posA->x              = local.t[0];
        posA->z              = local.t[2];
        soundCoords[0xB].flg = 0;
        soundId              = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050001;
        pan                  = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_A00 == (s32)(q1 & 0xFF)) {
        saveB        = &work->field_9A0;
        soundCoords2 = ((TmdObject*)arg0->extra)->coords;
        Gp_UpdateCoord(&soundCoords2[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords2[8].workm, &local);
        posB                = saveB;
        posB->x             = local.t[0];
        posB->z             = local.t[2];
        soundCoords2[8].flg = 0;
        soundId2            = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050002;
        pan2                = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((work->field_A00 >= z) && (work->field_A00 <= (s32)(q0 & 0xFF))) {
        coords = ((TmdObject*)arg0->extra)->coords;
        coord  = &coords[0xB];
        Gp_UpdateCoord(coord);
        saveC = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local2);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
        dx                 = world.t[0] - local2.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        posC               = saveC;
        posZ               = posC->z;
        dz                 = world.t[2] - local2.t[2];
        coords->flg        = 0;
        coords[0xB].flg    = 0;
        coords->coord.t[2] = posZ - dz;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    if ((work->field_A00 >= (s32)(q1 & 0xFF)) && (work->field_A00 <= (s32)(q2 & 0xFF))) {
        coords = ((TmdObject*)arg0->extra)->coords;
        coord  = &coords[8];
        Gp_UpdateCoord(coord);
        saveD = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local2);
        dx2                = local2.t[0] - local.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx2;
        posD               = saveD;
        posZ2              = posD->z;
        dz2                = local2.t[2] - local.t[2];
        coords->flg        = 0;
        coords[8].flg      = 0;
        coords->coord.t[2] = posZ2 - dz2;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    root->flg = 0;
}

void func_actor_400500_8013403C(Task* arg0)
{
    MATRIX                 local;
    MATRIX                 local2;
    MATRIX                 world;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    Actor400500HitView*    hit;
    Actor400500ViewPos*    posA;
    Actor400500ViewPos*    posB;
    Actor400500ViewPos*    posC;
    Actor400500ViewPos*    posD;
    Actor400500ViewPos*    saveA;
    Actor400500ViewPos*    saveB;
    Actor400500ViewPos*    saveC;
    Actor400500ViewPos*    saveD;
    GsCOORDINATE2*         root;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         soundCoords;
    GsCOORDINATE2*         soundCoords2;
    s32                    i;
    s32                    cond;
    s32                    z;
    s32                    soundId;
    s32                    pan;
    s32                    pan2;
    s32                    sc0;
    s32                    sc1;
    s32                    cur;
    s32                    q0;
    s32                    q1;
    s32                    q2;
    s32                    dx;
    s32                    dz;
    s32                    posZ;
    s32                    dx2;
    s32                    dz2;
    s32                    posZ2;

    work = (Actor400500Work*)arg0->work;
    root = ((TmdObject*)arg0->extra)->coords;
    if (work->field_9FE != 4) {
        work->field_9F8 = 0x10;
        work->field_9FE = 4;
        work->field_9FA = 2;
        work2           = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i      = 1;
        stride = (Actor400500AnimStride*)work2 + 1;
        do {
            stride->field_1D = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
            stride++;
        } while (i < 0x12);
    }

    sc0 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc0 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xD00 / sc0) >> 4;
    }
    q0 = cur;

    sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc1 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xE00 / sc1) >> 4;
        COMPILER_BARRIER();
        sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    }
    q1 = cur;

    if (sc1 == 0) {
        sc0 = 0;
    } else {
        sc0 = (u32)(0x1B00 / sc1) >> 4;
    }
    q2 = sc0;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A00 = 0;
    }

    SOFT_MOVE_ZERO(z);
    if (work->field_A00 == z) {
        saveA       = &work->field_9A0;
        soundCoords = ((TmdObject*)arg0->extra)->coords;
        Gp_UpdateCoord(&soundCoords[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords[8].workm, &local);
        posA               = saveA;
        posA->x            = local.t[0];
        posA->z            = local.t[2];
        soundCoords[8].flg = 0;
        soundId            = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050001;
        pan                = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_A00 == (s32)(q1 & 0xFF)) {
        saveB        = &work->field_9A0;
        soundCoords2 = ((TmdObject*)arg0->extra)->coords;
        Gp_UpdateCoord(&soundCoords2[0xB]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords2[0xB].workm, &local);
        posB                  = saveB;
        posB->x               = local.t[0];
        posB->z               = local.t[2];
        soundCoords2[0xB].flg = 0;
        soundId               = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050002;
        pan2                  = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((work->field_A00 >= z) && (work->field_A00 <= (s32)(q0 & 0xFF))) {
        coords = ((TmdObject*)arg0->extra)->coords;
        coord  = &coords[8];
        Gp_UpdateCoord(coord);
        saveC = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local2);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
        dx                 = world.t[0] - local2.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        posC               = saveC;
        posZ               = posC->z;
        dz                 = world.t[2] - local2.t[2];
        coords->flg        = 0;
        coords[8].flg      = 0;
        coords->coord.t[2] = posZ - dz;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    if ((work->field_A00 >= (s32)(q1 & 0xFF)) && (work->field_A00 <= (s32)(q2 & 0xFF))) {
        coords = ((TmdObject*)arg0->extra)->coords;
        coord  = &coords[0xB];
        Gp_UpdateCoord(coord);
        saveD = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local2);
        dx2                = local2.t[0] - local.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx2;
        posD               = saveD;
        posZ2              = posD->z;
        dz2                = local2.t[2] - local.t[2];
        coords->flg        = 0;
        coords[0xB].flg    = 0;
        coords->coord.t[2] = posZ2 - dz2;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    root->flg = 0;
}

void func_actor_400500_8013456C(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    u32              dmg;
    u32              amount;
    s16              amount16;
    s16              hp;
    u8               flags;
    s32              tmp;
    s16              tick;
    s32              i;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    for (i = 0; i < 3; i++) {
        if ((work->rec0[i].key & 0xFFFF0000) == 0x20000) {
            if (work->field_A44 == 0) {
                work->field_A3C = 1;
                dmg             = Gp_ComputeDamage(work->rec0[i].key, work->field_A16, 0, 0);
                amount          = dmg;
                work->field_A44 = Gp_GetIdParam2(work->rec0[i].key);
                if (Gp_RollEnemyChance(enemy, work->rec0[i].key, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[3], 0, NULL);
                }
                amount16 = amount;
                func_800E2C78((GpObj40*)enemy, work->rec0[i].key, amount16, 0);
                func_800DA6E8(&enemy->node, amount16, 0);
                hp              = (u16)enemy->field_40 - amount;
                enemy->field_40 = hp;
                if ((hp << 16) <= 0) {
                    enemy->field_40 = 0;
                    work->field_A42 = 1;
                }
                func_800FDB18(
                    Gp_GetIdParam1(work->rec0[i].key) & 0xFFFF,
                    &((TmdObject*)arg0->extra)->coords[3],
                    NULL,
                    &work->eff_940);
                if (amount16 >= 0x32) {
                    work->field_A3E = 2;
                    amount          = dmg;
                    work->field_A40 = 2;
                } else {
                    work->field_A3E = 1;
                    work->field_A40 = 1;
                }
            } else if ((Gp_GetIdParam1(work->rec0[i].key) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &((TmdObject*)arg0->extra)->coords[1], NULL, &work->eff_940);
            }
            switch (Gp_GetIdParam0(work->rec0[i].key) & 0xFFFF) {
                case 0:
                    break;
                case 1:
                    Gp_SetObjFlag1((GpObj4C*)enemy);
                    break;
                case 2:
                    Gp_SetObjFlag2((GpObj5D*)enemy, work->rec0[i].key, 0);
                    break;
                case 3:
                    Gp_SetObjFlag4((GpObj5C*)enemy, work->rec0[i].key, 0);
                    break;
                case 4:
                    work->field_A3E = 4;
                    work->field_A40 = 4;
                    break;
                case 5:
                    work->field_A3E = 2;
                    work->field_A40 = 2;
                    break;
                case 6:
                    work->field_A3E = 4;
                    work->field_A40 = 4;
                    break;
                case 7:
                    work->field_A3E = 2;
                    work->field_A40 = 2;
                    break;
                case 8:
                case 9:
                    work->field_A4A = 1;
                    break;
            }
        }
    }

    flags = enemy->field_4C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
        work->field_A3E = 2;
        work->field_A40 = 2;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_A3E  = 3;
        work->field_A40  = 3;
    }
    if (enemy->field_4C & 0xC) {
        tmp  = Gp_TickObjFlag4((GpObj5C*)enemy);
        tick = tmp;
        if (tick != 0) {
            enemy->field_40 = (u16)enemy->field_40 - tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->field_40 < 0) {
                enemy->field_40 = 0;
            }
            work->field_A3C = 1;
            work->field_A3E = 2;
            work->field_A40 = 2;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }
    Gp_ClearRec18Occupied(work->rec0);
    if (work->field_A44 > 0) {
        work->field_A44 = (u16)work->field_A44 - 1;
        return;
    }
    work->field_A44 = 0;
}

void func_actor_400500_801348D8(Task* arg0, s32 arg1)
{
    SVECTOR                pos;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         joint;
    GsCOORDINATE2*         player;
    GpActorWork*           slot;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500AnimStride* stride;
    Actor400500AnimStride* stride2;
    s32                    i;
    s32                    cur;
    s32                    sample;

    coords = ((TmdObject*)arg0->extra)->coords;
    slot   = *Gp_ActorSlots;
    joint  = coords + 8;
    work   = (Actor400500Work*)arg0->work;
    if (slot != NULL) {
        player = slot->extra->coords;
        work2  = work;
        if (work->field_9FA == 1) {
            if ((s16)work->field_9FC != work->field_9FE) {
                work->field_A00 = 0;
            } else {
                work->field_A00 = func_actor_400500_8013DD8C(arg0, work->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work->field_9FA = 3;
            work->field_A00 = 0;
        } else if (work->field_9FA == 3) {
            work->field_A00 = (u16)work->field_A00 + 1;
        }
        i      = 1;
        stride = (Actor400500AnimStride*)work2 + 1;
        do {
            stride->field_1D = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
            stride++;
        } while (i < 0x12);
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        joint->flg = 0;
        Gp_UpdateCoord(joint);
        pos.vx = 0x160;
        pos.vy = 0x148;
        pos.vz = 0x2C0;
        ActorCoordToView(joint, &pos);
        if ((arg1 << 0x10) == 0) {
            player->coord.t[0] = pos.vx;
            player->coord.t[2] = pos.vz;
        } else {
            sample             = pos.vx;
            cur                = player->coord.t[0];
            cur               += (sample - cur) >> 2;
            player->coord.t[0] = cur;
            sample             = pos.vz;
            cur                = player->coord.t[2];
            cur               += (sample - cur) >> 2;
            player->coord.t[2] = cur;
        }
        player->flg = 0;
        Gp_UpdateCoord(player);
        work->field_9F8 = -0x10;
        work3           = (Actor400500Work*)arg0->work;
        if (work3->field_9FA == 1) {
            if ((s16)work3->field_9FC != work3->field_9FE) {
                work3->field_A00 = 0;
            } else {
                work3->field_A00 = func_actor_400500_8013DD8C(arg0, work3->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work3->field_9FA = 3;
        } else if (work3->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work3->field_9FA = 3;
            work3->field_A00 = 0;
        } else if (work3->field_9FA == 3) {
            work3->field_A00 = (u16)work3->field_A00 + 1;
        }
        i       = 1;
        stride2 = (Actor400500AnimStride*)work3 + 1;
        do {
            stride2->field_1D = (u8)work3->field_9F8;
            Gp_AnimTickIndex(&work3->anim, i);
            i++;
            stride2++;
        } while (i < 0x12);
        work->field_9F8 = 0x10;
    }
}

void func_actor_400500_80134B88(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;

    D_800678F0[0] = D_actor_400500_8014393C;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[3], 0x200, NULL);
    if (eff != NULL) {
        src        = (TmdObject*)arg0->extra;
        dst        = (TmdObject*)eff->task->extra;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_400500_80143F40;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[1], 0x200, NULL);
    if (eff2 != NULL) {
        src2        = (TmdObject*)arg0->extra;
        dst2        = (TmdObject*)eff2->task->extra;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_400500_80144624;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[1], 0x200, NULL);
    if (eff3 != NULL) {
        src3        = (TmdObject*)arg0->extra;
        dst3        = (TmdObject*)eff3->task->extra;
        dst3->tpage = src3->tpage;
        dst3->clut  = src3->clut;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[3], 0x200, NULL);
}
