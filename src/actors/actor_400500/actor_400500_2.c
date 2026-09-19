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

void func_actor_400500_80135414(Task* arg0)
{
    TmdObject*             extra;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    Actor400500Work*       work5;
    Actor400500Work*       work6;
    Actor400500Work*       work7;
    Actor400500AnimStride* stride;
    GsCOORDINATE2*         player;
    GsCOORDINATE2*         coord2;
    TmdObject*             extra2;
    s32                    i;
    s32                    flag;
    s32                    val;
    u8                     mode;

    extra      = arg0->extra;
    enemy      = arg0->spawnArg2;
    coord      = extra->coords;
    arg0->work = memCalloc(0xA50, 0);
    work       = (Actor400500Work*)arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    extra->lightMtx    = &work->lightMtx;
    extra->colorMtx    = &work->colorMtx;
    extra->flags       = 0;
    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)arg0->extra)->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags = 1;
    enemy->field_54   = (s32)work->rec0;
    enemy->field_50   = &D_actor_400500_80153C90;
    enemy->field_40 = enemy->field_42 = D_actor_400500_80153C90.field_4;
    func_800B3F84(&work->anim, D_actor_400500_80153CC0, (GpAnimObj*)extra, work->pad_2E4,
                  work->slots);
    coord->sub       = &gGfxViewCoord;
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x18;
    work2->field_9FE = 2;
    work2->field_9FA = 2;
    work3            = (Actor400500Work*)arg0->work;
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
    i      = 1;
    stride = (Actor400500AnimStride*)work3 + 1;
    do {
        stride->field_1D = (u8)work3->field_9F8;
        Gp_AnimTickIndex(&work3->anim, i);
        i++;
        stride++;
    } while (i < 0x12);
    arg0->msgTable = D_actor_400500_80153CA0;
    func_actor_400500_80132C54(arg0);
    work4 = (Actor400500Work*)arg0->work;
    if (Gp_ActorSlots[0] != NULL) {
        player              = Gp_ActorSlots[0]->extra->coords;
        work4->field_9D0.vx = (u16)player->coord.t[0];
        work4->field_9D0.vy = (u16)player->coord.t[1];
        work4->field_9D0.vz = (u16)player->coord.t[2];
    }
    work5  = (Actor400500Work*)arg0->work;
    mode   = gGameSession->at4.loc.room;
    extra2 = arg0->extra;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        val              = 0xFF;
        work5->field_A20 = val;
        val              = 0x10;
        work5->field_A24 = 0;
        work5->field_A28 = 0;
    } else {
        val              = 0x1000;
        work5->field_A24 = val;
        val              = 0xFF;
        work5->field_A28 = val;
        val              = 0x2000;
        work5->field_A20 = 0;
    }
    work5->field_A2C = val;
    SOFT_BARRIER();
    func_8009EA50(work5->field_A20);
    extra2->lightLevel = work5->field_A24;
    func_actor_400500_80132000(arg0);
    func_actor_400500_8013226C(arg0);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    coord2                = ((TmdObject*)arg0->extra)->coords;
    work->eff_940.field_4 = 0x100;
    work->eff_940.field_6 = 3;
    work->eff_940.field_0 = &coord2[3];
    work6                 = (Actor400500Work*)arg0->work;
    work6->field_A06      = 6;
    work6->field_A08      = 0;
    work7                 = (Actor400500Work*)arg0->work;
    D_80062735            = 2;
    if (((work7->field_A46 >= 0) || ((u8)work7->field_A46 & 0x7F)) && (work7->field_A30 == 0)) {
        flag             = 0x80;
        work7->field_A46 = flag;
        work7->field_A47 = 0;
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", ActorsShared801328ccTable);

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

void func_actor_400500_80132438(Task* arg0);
void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2);
void func_actor_400500_80132E94(Task* arg0);
void func_actor_400500_8013456C(Task* arg0);
void func_actor_400500_80135EBC(Task* arg0);
void func_actor_400500_801385D0(Task* arg0);
void func_actor_400500_8013899C(Task* arg0);
void func_actor_400500_80138EA0(Task* arg0);
void func_actor_400500_8013905C(Task* arg0);
void func_actor_400500_801392D8(Task* arg0);
void func_actor_400500_801395D0(Task* arg0);
void func_actor_400500_80139C1C(Task* arg0);
void func_actor_400500_80139F6C(Task* arg0);
void func_actor_400500_8013A484(Task* arg0);
void func_actor_400500_8013AD60(Task* arg0);
void func_actor_400500_8013B5E0(Task* arg0);
void func_actor_400500_8013BA24(Task* arg0);

static __inline__ s32 lookup_zone(Task* task)
{
    Actor400500Zone* zone;
    u16              id_u;
    s16              zone_id;
    GsCOORDINATE2*   root;
    u16              px_u, pz_u;
    s16              px, pz;

    zone    = D_actor_400500_80153D6C;
    id_u    = (u16)zone->id;
    root    = ((TmdObject*)task->extra)->coords;
    zone_id = zone->id;
    px_u    = (u16)root->coord.t[0];
    pz_u    = (u16)root->coord.t[2];
    if (zone_id != -1) {
        px = (s16)px_u;
        pz = (s16)pz_u;
        do {
            if ((px >= zone->x) && ((zone->x + zone->w) >= px) &&
                (pz >= zone->z) && ((zone->z + zone->h) >= pz)) {
                return (s16)id_u;
            }
            zone++;
            id_u = (u16)zone->id;
        } while (zone->id != -1);
    }
    return 0;
}

static __inline__ VECTOR* push_color(GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    ((VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10))->vx = coord->workm.t[0];
    block->vy                                     = coord->workm.t[1];
    block->vz                                     = coord->workm.t[2];
    *(VECTOR**)G_SCRATCH_HEAD                     = block;
    return block;
}

static __inline__ void pop_scratch(s32 n)
{
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + n;
}

static __inline__ u8* push_proj(void)
{
    u8*                     head  = *(u8**)G_SCRATCH_HEAD;
    Actor400500ProjScratch* block = (Actor400500ProjScratch*)(head - 0x18);

    *(Actor400500ProjScratch**)G_SCRATCH_HEAD        = block;
    ((Actor400500ProjScratch*)(head - 0x18))->vec.vx = 0;
    block->vec.vy                                    = 0;
    block->vec.vz                                    = 0;
    return head;
}

const Actor400500TaskFuncTable13 D_actor_400500_80131E5C = { {
    func_actor_400500_80135EBC,
    func_actor_400500_8013BA24,
    func_actor_400500_801385D0,
    func_actor_400500_8013899C,
    func_actor_400500_80138EA0,
    func_actor_400500_8013905C,
    func_actor_400500_801392D8,
    func_actor_400500_801395D0,
    func_actor_400500_80139C1C,
    func_actor_400500_80139F6C,
    func_actor_400500_8013AD60,
    func_actor_400500_8013B5E0,
    func_actor_400500_8013A484,
} };

void func_actor_400500_80135770(Task* arg0)
{
    Actor400500Work*           work;
    GpEnemy*                   enemy;
    TmdObject*                 extra0;
    TmdObject*                 obj;
    GpActorWork*               slot;
    GsCOORDINATE2*             part2;
    PlayerStatus*              cfg;
    Actor400500Msg3FF          msg;
    Actor400500TaskFuncTable13 sp;
    Actor400500Matrix          rot;
    s8                         handshake;
    Actor400500Work*           work_pos;
    Actor400500Work*           work_dead;
    Actor400500Work*           work_rot;
    Actor400500Matrix*         src;
    s32                        one;
    s16                        ang;
    s16                        ang_z;
    s16                        ang_y;
    GsCOORDINATE2*             rot_root;
    GsCOORDINATE2*             player;
    TmdObject*                 extra;
    TmdObject*                 extra2;
    TmdObject*                 trans_obj;
    VECTOR*                    color;
    u8*                        head;
    GsCOORDINATE2*             color_part;
    u8                         mode;
    s16                        trans;
    s16                        trans_y;
    Actor400500ProjScratch*    proj;
    SVECTOR*                   vecp;
    MATRIX*                    workm;

    cfg    = &Player_Status;
    work   = (Actor400500Work*)arg0->work;
    enemy  = (GpEnemy*)arg0->spawnArg2;
    extra0 = arg0->extra;
    part2  = extra0->coords + 2;
    obj    = extra0;
    slot   = *Gp_ActorSlots;
    sp     = D_actor_400500_80131E5C;

    handshake = work->field_A48;
    switch (handshake) {
        case 1:
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                work->field_A48 = 2;
            }
            break;
        case 2:
            msg.field_0  = &D_actor_400500_80153CB0;
            msg.field_8  = 0;
            msg.field_C  = 0;
            msg.field_10 = 0;
            if (work->field_A4D != 0) {
                msg.field_4     = 3;
                work->field_A48 = 4;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&msg, 0);
            } else {
                msg.field_4     = handshake;
                work->field_A48 = 3;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            }
            break;
        case 3:
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
                work->field_A48 = 0;
            }
            break;
    }

    switch (D_801153F4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            func_actor_400500_80132438(arg0);
            work->field_A1A = lookup_zone(arg0);
            if (slot == NULL) {
                work->field_A1C = 0;
            } else {
                work->field_A1C = lookup_zone((Task*)slot);
            }
            sp.funcs[(s16)work->field_A06](arg0);
            work_pos = (Actor400500Work*)arg0->work;
            if (*Gp_ActorSlots != NULL) {
                player                 = (*Gp_ActorSlots)->extra->coords;
                work_pos->field_9D0.vx = (u16)player->coord.t[0];
                work_pos->field_9D0.vy = (u16)player->coord.t[1];
                work_pos->field_9D0.vz = (u16)player->coord.t[2];
            }
            src                 = &rot;
            work_rot            = (Actor400500Work*)arg0->work;
            rot_root            = ((TmdObject*)arg0->extra)->coords;
            ang                 = work_rot->field_948;
            ang_y               = work_rot->field_94A;
            work_rot->field_948 = ang & 0xFFF;
            ang_z               = work_rot->field_94C;
            work_rot->field_94A = ang_y & 0xFFF;
            work_rot->field_94C = ang_z & 0xFFF;
            one                 = 0x1000;
            rot.ident.m00_m01   = one;
            rot.ident.m02_m10   = 0;
            src->ident.m11_m12  = one;
            rot.ident.m20_m21   = 0;
            src->ident.m22      = one;
            RotMatrixZ(work_rot->field_94C, &src->mat);
            RotMatrixX(work_rot->field_948, &src->mat);
            func_8004BFF8(work_rot->field_94A, &src->mat);
            ActorsShared80132c4c(&src->mat, &rot_root->coord);
            func_actor_400500_80132E94(arg0);
            if (work->field_A32 > 0) {
                work->field_A32 = (u16)work->field_A32 - 1;
                work->field_A34 = 1;
            } else {
                work->field_A34 = 0;
            }
            obj->flags &= 0xFF7F;
            if ((enemy->field_40 > 0) || (cfg->hp <= 0)) {
                func_actor_400500_8013456C(arg0);
            } else if ((work->field_A42 == 0) && (work->field_A48 == 0)) {
                work_dead            = (Actor400500Work*)arg0->work;
                arg0->state          = 2;
                work_dead->field_A06 = 0;
                work_dead->field_A08 = 0;
            }
        case 1:
            extra      = arg0->extra;
            extra2     = extra;
            color_part = extra->coords + 1;
            color      = push_color(color_part);
            Gp_UpdateActorColor(arg0->spawnArg2, color, 0, 0);
            mode = gGameSession->at4.loc.room;
            if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
                trans_obj = extra2;
                trans     = 0x200;
                trans_y   = trans;
            } else {
                trans_obj = extra;
                trans     = 0x400;
                trans_y   = 0x1000;
            }
            Gp_SetObjTrans((GpObj20*)trans_obj, trans, trans_y, trans);
            pop_scratch(0x10);
            if (gGameSession->field_65 != 0) {
                func_actor_400500_80132AB0(arg0, -0xFA0, (u8)work->field_A28);
                return;
            }
            func_actor_400500_80132AB0(arg0, -0xFA0, ((u16)work->field_A28 >> 2) & 0xFF);
            func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->field_A28);
            head = push_proj();
            proj = (Actor400500ProjScratch*)(head - 0x18);
            Gp_UpdateCoord(part2);
            vecp  = &proj->vec;
            workm = &part2->workm;
            gte_SetRotMatrix(workm);
            gte_SetTransMatrix(workm);
            gte_ldv0(vecp);
            gte_rtps_real();
            gte_stsxy(&((Actor400500ProjScratch*)(head - 0x18))->sxy);
            gte_stdp(&((Actor400500ProjScratch*)(head - 0x18))->dp);
            gte_stflg(&((Actor400500ProjScratch*)(head - 0x18))->flag);
            gte_stszotz(&((Actor400500ProjScratch*)(head - 0x18))->otz);
            if (proj->flag < 0) {
                proj->otz = 0;
            }
            proj->otz = (proj->otz >> 4) + 0x1E;
            ActorsShared80131fc8(proj->otz);
            pop_scratch(0x18);
            return;
    }
}

extern TaskFuncTable11 D_actor_400500_80131E90;
extern TaskFuncTable3  D_actor_400500_80131EBC;

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_400500_80131E90\n"
        "dlabel D_actor_400500_80131E90\n"
        "    .word func_actor_400500_801361EC\n"
        "    .word func_actor_400500_8013662C\n"
        "    .word func_actor_400500_80136864\n"
        "    .word func_actor_400500_801369A4\n"
        "    .word func_actor_400500_80136B94\n"
        "    .word func_actor_400500_80136D00\n"
        "    .word func_actor_400500_80136EB8\n"
        "    .word func_actor_400500_80137034\n"
        "    .word func_actor_400500_801371A0\n"
        "    .word func_actor_400500_80137338\n"
        "    .word func_actor_400500_80137478\n"
        "enddlabel D_actor_400500_80131E90\n"
        "nonmatching D_actor_400500_80131EBC\n"
        "dlabel D_actor_400500_80131EBC\n"
        "    .word func_actor_400500_8013BAA4\n"
        "    .word func_actor_400500_8013BB18\n"
        "    .word func_actor_400500_8013BBB0\n"
        "    .word 0x00000000\n"
        "enddlabel D_actor_400500_80131EBC\n"
        ".section .text");
#endif

void func_actor_400500_80135EBC(Task* arg0)
{
    Actor400500Work*       work;
    GpEnemy*               enemy;
    TaskFuncTable11        sp10;
    TaskFuncTable3         sp40;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;
    s32                    flag;
    Actor400500Work*       work4;
    Actor400500AnimStride* stride2;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp10  = D_actor_400500_80131E90;
    sp40  = D_actor_400500_80131EBC;
    if (enemy->field_40 <= 0) {
        sp40.funcs[(s16)work->field_A0A](arg0);
        work2 = (Actor400500Work*)arg0->work;
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
        work3 = (Actor400500Work*)arg0->work;
        if ((work3->field_A46 >= 0) || (((u8)work3->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        return;
    }
    if ((s16)work->field_A08 != 0) {
        work4 = (Actor400500Work*)arg0->work;
        if (work4->field_9FA == 1) {
            if ((s16)work4->field_9FC != work4->field_9FE) {
                work4->field_A00 = 0;
            } else {
                work4->field_A00 = func_actor_400500_8013DD8C(arg0, work4->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work4->field_9FA = 3;
        } else if (work4->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work4->field_9FA = 3;
            work4->field_A00 = 0;
        } else if (work4->field_9FA == 3) {
            work4->field_A00 = (u16)work4->field_A00 + 1;
        }
        i       = 1;
        stride2 = (Actor400500AnimStride*)work4 + 1;
        do {
            stride2->field_1D = (u8)work4->field_9F8;
            Gp_AnimTickIndex(&work4->anim, i);
            i++;
            stride2++;
        } while (i < 0x12);
    }
    sp10.funcs[(s16)work->field_A08](arg0);
    work3 = (Actor400500Work*)arg0->work;
    if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
        flag             = 0x80;
        work3->field_A46 = flag;
        work3->field_A47 = 0;
    }
}

void func_actor_400500_801361EC(Task* arg0)
{
    Actor400500Matrix      rot;
    MATRIX                 local;
    Actor400500Matrix*     src;
    MATRIX*                dst;
    Actor400500Work*       work;
    Actor400500Work*       workA;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coords2;
    Actor400500AnimStride* stride;
    Actor400500ViewPos*    pos;
    Actor400500ViewPos*    pos2;
    Actor400500ViewPos*    pos3;
    Actor400500ViewPos*    pos4;
    s32                    flag;
    s32                    flag2;
    s32                    heading;
    s32                    i;
    s32                    tx;
    s32                    a1c;

    work    = (Actor400500Work*)arg0->work;
    heading = (u16)work->field_94A & 0xFFF;
    coord   = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        workA = (Actor400500Work*)arg0->work;
        if (workA->field_A49 != 0) {
            workA->field_A49 = 0;
            if (workA->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0)) {
            work3            = (Actor400500Work*)arg0->work;
            work3->field_A38 = 0;
            work3->field_A3A = 0;
            work4            = (Actor400500Work*)arg0->work;
            work4->field_9F8 = 0x18;
            work4->field_9FE = 2;
            work4->field_9FA = 2;
            work2            = (Actor400500Work*)arg0->work;
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
            switch ((s16)((u16)work->field_A1A - 1)) {
                case 3:
                    if (heading != 0x400) {
                        goto case3_ne;
                    }
                    work->field_A08 = 1;
                    break;
                case3_ne:
                    work->field_A08 = 4;
                    break;
                case 0:
                    if (work->field_9E0 >= 0) {
                        goto case0_ge;
                    }
                    work->field_A08 = 3;
                    break;
                case0_ge:
                    work->field_A08 = 1;
                    break;
                case 1:
                    a1c = (s16)work->field_A1C;
                    if ((a1c == 1) || (a1c == 4) || (a1c == 5)) {
                        SOFT_BARRIER();
                        work->field_A08 = 3;
                    } else if ((a1c == 3) && (heading == 0) && (coord->coord.t[0] >= 0x4074)) {
                        work->field_A08 = 6;
                    } else if ((s16)work->field_A1C == 2) {
                        if (coord->coord.t[2] < -0x209D) {
                            goto a1c2_lt;
                        }
                        work->field_A08 = 6;
                        break;
                    a1c2_lt:
                        work->field_A08 = 1;
                        break;
                    } else {
                        work->field_A08 = 1;
                    }
                    break;
                case 2:
                    if (work->field_9E4 >= 0) {
                        goto case2_ge;
                    }
                    work->field_A08 = 8;
                    break;
                case2_ge:
                    work->field_A08 = 6;
                    break;
                case 5:
                    if (heading != 0x800) {
                        goto case5_ne;
                    }
                    work->field_A08 = 8;
                    break;
                case5_ne:
                    work->field_A08 = 7;
                    break;
                default:
                    tx                 = -0x3E8;
                    coord->coord.t[0]  = tx;
                    tx                 = -0xFA0;
                    coord->coord.t[1]  = tx;
                    tx                 = -0x2116;
                    coord->coord.t[2]  = tx;
                    tx                 = 0x400;
                    work->field_94A    = tx;
                    tx                 = 0x800;
                    work->field_94C    = tx;
                    tx                 = 0x1000;
                    src                = &rot;
                    work->field_948    = 0;
                    work->field_A1E    = 0;
                    rot.ident.m00_m01  = tx;
                    src->ident.m02_m10 = 0;
                    src->ident.m11_m12 = tx;
                    src->ident.m20_m21 = 0;
                    src->ident.m22     = tx;
                    RotMatrixZ(work->field_94C, &src->mat);
                    func_8004BFF8(work->field_94A, &src->mat);
                    dst          = &coord->coord;
                    dst->m[0][0] = src->mat.m[0][0];
                    dst->m[0][1] = src->mat.m[0][1];
                    dst->m[0][2] = src->mat.m[0][2];
                    dst->m[1][0] = src->mat.m[1][0];
                    dst->m[1][1] = src->mat.m[1][1];
                    dst->m[1][2] = src->mat.m[1][2];
                    dst->m[2][0] = src->mat.m[2][0];
                    dst->m[2][1] = src->mat.m[2][1];
                    dst->m[2][2] = src->mat.m[2][2];
                    pos2         = &work->field_9A0;
                    coords       = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
                    Gp_UpdateCoord(&coords[11]);
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[11].workm, &local);
                    pos    = pos2;
                    pos->x = local.t[0];
                    pos->z = local.t[2];
                    SOFT_BARRIER();
                    coords[11].flg  = 0;
                    work->field_A08 = 1;
                    break;
            }
            pos4    = &work->field_9A0;
            coords2 = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
            Gp_UpdateCoord(&coords2[11]);
            Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords2[11].workm, &local);
            pos3            = pos4;
            pos3->x         = local.t[0];
            pos3->z         = local.t[2];
            coords2[11].flg = 0;
        }
    }
}

void func_actor_400500_8013662C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;
    u16              a1c;

    work  = (Actor400500Work*)arg0->work;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0x400) {
                if (((0x400 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                a1a = work->field_A1A;
                if (a1a != 1) {
                    if ((a1a == 2) && (coord->coord.t[0] >= 0x4075)) {
                        a1c = work->field_A1C;
                        if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
                            work->field_A08 = 5;
                        } else {
                            work->field_A08 = a1a;
                        }
                    } else {
                        func_actor_400500_801335E8(arg0);
                    }
                } else {
                    if (work->field_A34 == 0) {
                        if (work->field_9E0 <= 0) {
                            work->field_A08 = 2;
                        }
                    } else if (work->field_9E0 < -0xF9F) {
                        rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                        Gp_LcgState = rnd;
                        if (((rnd >> 0x10) & 0x1F) == 0) {
                            if (!(work->field_A1E & 1)) {
                                work2 = (Actor400500Work*)arg0->work;
                                val   = 7;
                            } else {
                                work2 = (Actor400500Work*)arg0->work;
                                val   = 8;
                            }
                            work2->field_A06 = val;
                            work2->field_A08 = 0;
                        }
                    }
                    func_actor_400500_801335E8(arg0);
                }
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

void func_actor_400500_80136864(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

void func_actor_400500_801369A4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;

    work  = (Actor400500Work*)arg0->work;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0xC00) {
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                a1a = work->field_A1A;
                if (a1a != 1) {
                    if (a1a == 4) {
                        work->field_A08 = a1a;
                    }
                } else if (work->field_A34 == 0) {
                    if (work->field_9E0 >= 0) {
                        work->field_A08 = 4;
                    }
                } else if (work->field_9E0 >= 0xFA0) {
                    rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState = rnd;
                    if (((rnd >> 0x10) & 0x1F) == 0) {
                        if (!(work->field_A1E & 1)) {
                            work2 = (Actor400500Work*)arg0->work;
                            val   = 7;
                        } else {
                            work2 = (Actor400500Work*)arg0->work;
                            val   = 8;
                        }
                        work2->field_A06 = val;
                        work2->field_A08 = 0;
                    }
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

void func_actor_400500_80136B94(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x400) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 1;
                return;
            }
            if ((s16)work->field_A1C == 4) {
                if (work->field_9E4 > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else if (((0x400 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_80136D00(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            if (((u32)(work->field_A1C - 2) < 2U) || ((s16)work->field_A1C == 6)) {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 6;
                    return;
                }
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0xC00) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 3;
                    return;
                }
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_80136EB8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->work;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if (heading & 0xFFF) {
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                if (work->field_A1A != 3) {
                    if (work->field_A1A == 6) {
                        work->field_A08 = 7;
                    }
                } else if ((work->field_A34 == 0) && (work->field_9E4 < 0)) {
                    work->field_A08 = 7;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

void func_actor_400500_80137034(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x800) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 8;
                return;
            }
            if ((s16)work->field_A1C == 6) {
                if (work->field_9E0 > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else if (((0x800 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_801371A0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->work;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0x800) {
                if (((0x800 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                switch (work->field_A1A) {
                    case 2:
                        if (coord->coord.t[2] < -0x209E) {
                            work->field_A08 = 9;
                        }
                        break;
                    case 3:
                        if ((work->field_A34 == 0) && (work->field_9E4 > 0)) {
                            work->field_A08 = 0xA;
                        }
                        break;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

void func_actor_400500_80137338(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

void func_actor_400500_80137478(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 6;
                return;
            }
            if (((0 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

void func_actor_400500_801375B8(Task* arg0)
{
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    Actor400500AnimStride* stride;
    s32                    flag;
    s32                    i;

    work              = (Actor400500Work*)arg0->work;
    work->obj0.radius = 0x130;
    work2             = (Actor400500Work*)arg0->work;
    if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
        flag             = 0x81;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->work;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 5;
    work3->field_9FA = 2;
    work4            = (Actor400500Work*)arg0->work;
    if (work4->field_9FA == 1) {
        if ((s16)work4->field_9FC != work4->field_9FE) {
            work4->field_A00 = 0;
        } else {
            work4->field_A00 = func_actor_400500_8013DD8C(arg0, work4->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work4->field_9FA = 3;
    } else if (work4->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work4->field_9FA = 3;
        work4->field_A00 = 0;
    } else if (work4->field_9FA == 3) {
        work4->field_A00 = (u16)work4->field_A00 + 1;
    }
    i      = 1;
    stride = (Actor400500AnimStride*)work4 + 1;
    do {
        stride->field_1D = (u8)work4->field_9F8;
        Gp_AnimTickIndex(&work4->anim, i);
        i++;
        stride++;
    } while (i < 0x12);
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_9BC = 0;
    work->field_A08 = work->field_A08 + 1;
}

void func_actor_400500_8013771C(Task* arg0)
{
    SVECTOR            in;
    SVECTOR            out;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    union {
        MATRIX    mat;
        GpAnimArg msg;
    } slot;
    MATRIX                 parent;
    MATRIX*                parentp;
    MATRIX*                tmp;
    MATRIX*                mtx;
    MATRIX*                scratch;
    MATRIX*                scratch2;
    MATRIX*                scratch3;
    u8*                    scratchBase;
    MATRIX*                viewWorld;
    TmdObject*             model2;
    GsCOORDINATE2*         coords2;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coord8;
    GsCOORDINATE2*         playerCoords;
    GsCOORDINATE2*         walker;
    GsCOORDINATE2*         viewCoord;
    GsCOORDINATE2*         viewCoord2;
    GsCOORDINATE2*         part;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    Actor400500Work*       work5;
    Actor400500AnimStride* stride;
    Actor400500HitView*    hit;
    GameActor*             player;
    void*                  spawn;
    SVECTOR                dvec;
    s16                    vz;
    s32                    dist;
    s32                    delta;
    s32                    ident;
    s32                    one;
    s32                    i;
    s32                    flag;
    s32                    cond;
    s32                    soundId;
    s32                    soundId2;
    s32                    soundId3;
    s32                    pan;
    s32                    pan2;
    s32                    pan3;
    s32                    r;
    u16                    heading;
    u16                    heading2;
    s32                    cur;

    work            = (Actor400500Work*)arg0->work;
    player          = Gp_ActorSlots[0]->actor;
    spawn           = arg0->spawnArg2;
    work->field_A04 = work->field_A04 + 1;
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
    src = &rot;
    if ((s16)work->field_A04 < 0xF) {
        in.vx              = (u16)work->field_9E0;
        in.vy              = 0;
        vz                 = (u16)work->field_9E4;
        ident              = 0x1000;
        rot.ident.m00_m01  = ident;
        rot.ident.m02_m10  = 0;
        in.vz              = vz;
        src->ident.m11_m12 = ident;
        rot.ident.m20_m21  = 0;
        src->ident.m22     = ident;
        func_8004BFF8(work->field_94A, &src->mat);
        ApplyMatrixSV(&src->mat, &in, &out);
        r     = ratan2(out.vx, work->field_9E2 - 0x6A0);
        cur   = (u16)work->field_9BC;
        delta = (-r - cur) << 20;
    } else {
        cur   = (u16)work->field_9BC;
        delta = -(cur << 20);
    }
    cur             = cur + (delta >> 23);
    work->field_9BC = cur;
    cur             = (s16)work->field_A04;
    if (cur == 7) {
        if (player->field_954 != 2) {
            coords = ((TmdObject*)arg0->extra)->coords;
            coord8 = coords + 8;
            if (Gp_ActorSlots[0] == NULL) {
                dist = 0x7FFF;
            } else {
                playerCoords = Gp_ActorSlots[0]->extra->coords;
                Gp_UpdateCoord(playerCoords + 4);
                Gp_UpdateCoord(coord8);
                viewWorld = &Gfx_ViewWorldMtx;
                Gp_WorldToLocal(viewWorld, &playerCoords[4].workm, &slot.mat);
                Gp_WorldToLocal(viewWorld, &coords[8].workm, &parent);
                dvec.vx = (u16)slot.mat.t[0] - (u16)parent.t[0];
                dvec.vz = (u16)slot.mat.t[2] - (u16)parent.t[2];
                dist    = SquareRoot0((dvec.vx * dvec.vx) + (dvec.vz * dvec.vz));
            }
            if ((s16)dist < 0x500) {
                one               = 1;
                slot.msg.field_0  = D_actor_400500_80153CB0;
                slot.msg.field_4  = one;
                slot.msg.field_8  = 0;
                slot.msg.field_C  = 0;
                slot.msg.field_10 = 0;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&slot.msg, 0);
                work->field_A48      = one;
                Gp_StateC08.field_6 |= one;
                work->field_A18      = one;
            }
        }
        cur = (s16)work->field_A04;
    }
    if ((cur == 0xE) && (work->field_A18 == 0)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A0E = 2;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 6;
        work3->field_9FA = 1;
        work->field_A04  = 0;
        work4            = (Actor400500Work*)arg0->work;
        if (((work4->field_A46 >= 0) || ((u8)work4->field_A46 & 0x7F)) && (work4->field_A30 == 0)) {
            flag             = 0x80;
            work4->field_A46 = flag;
            work4->field_A47 = 0;
        }
        work->field_A08 = work->field_A08 + 1;
    }
    viewCoord                        = &gGfxViewCoord;
    parentp                          = &parent;
    heading                          = work->field_9BC;
    part                             = ((TmdObject*)arg0->extra)->coords + 6;
    scratch                          = (*(MATRIX**)G_SCRATCH_HEAD);
    walker                           = part->sub;
    scratchBase                      = (u8*)PSX_SCRATCH;
    *(MATRIX**)(scratchBase + 0x3FC) = scratch - 1;
    scratch[-1]                      = part->coord;
    mtx                              = scratch - 1;
    while (1) {
        if (walker == NULL) {
            break;
        }
        if (walker == viewCoord) {
            break;
        }
        parent = walker->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(mtx);
        MatrixNormal(mtx, &slot.mat);
        *mtx   = slot.mat;
        walker = walker->sub;
    }
    func_8004BFF8((s16)heading, mtx);
    func_actor_400500_8013B720(part, mtx);
    __builtin_memcpy(&part->coord, mtx, 18);
    part->flg = 0;
    Gp_UpdateCoord(part);
    SOFT_BARRIER();
    viewCoord2 = &gGfxViewCoord;
    SOFT_USE_REG(work);
    parentp = &parent;
    model2  = (TmdObject*)arg0->extra;
    coords2 = model2->coords;
    __asm__("lui %0, 0x1F80" : "=r"(scratch2) : "r"(model2));
    scratch2 = *(MATRIX**)((u8*)scratch2 + 0x3FC);
    heading2 = (u16)work->field_9BC;
    part     = &coords2[9];
    walker   = part->sub;
    __asm__("move %0,%1" : "=r"(mtx) : "r"(scratch2), "r"(walker));
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch2 + 1) : "memory");
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch2) : "memory");
    *mtx = part->coord;
    while (1) {
        if (walker == NULL) {
            break;
        }
        if (walker == viewCoord2) {
            break;
        }
        parent = walker->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(mtx);
        MatrixNormal(mtx, &slot.mat);
        *mtx   = slot.mat;
        walker = walker->sub;
    }
    func_8004BFF8((s16)heading2, mtx);
    func_actor_400500_8013B720(part, mtx);
    __builtin_memcpy(&part->coord, mtx, 18);
    part->flg = 0;
    Gp_UpdateCoord(part);
    scratch3                         = (*(MATRIX**)G_SCRATCH_HEAD);
    scratchBase                      = (u8*)PSX_SCRATCH;
    *(MATRIX**)(scratchBase + 0x3FC) = scratch3 + 1;
    if (((u32)(work->field_A04 - 7) < 4U) && (work->field_A18 == 1)) {
        func_actor_400500_801348D8(arg0, 1);
    }
    if (((u32)(work->field_A04 - 0xB) < 0x14U) && (work->field_A18 == 1)) {
        func_actor_400500_801348D8(arg0, 0);
    }
    if (((s16)work->field_A04 == 0xC) && (work->field_A18 != 0)) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 6;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_A04 == 0x1E) {
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050007;
        pan2     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_A04 == 0x2A) {
        Gp_SpawnPadLerp(8, 0xC0U, 8U);
        soundId3 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050008;
        pan3     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId3, pan3, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_A04 == 0x1F) {
        Gp_SpawnPadLerp(6, 0xFFU, 0x80U);
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)spawn, 1), 0) != 0) {
            player->field_956 = 0xA;
            work->field_A4D   = 1;
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work5             = (Actor400500Work*)arg0->work;
        work5->field_A06  = 0;
        work5->field_A08  = 0;
        work->field_A32   = 0x3C;
        work->obj0.radius = 0x260;
    }
}

void func_actor_400500_80138088(Task* arg0)
{
    MATRIX                 normal;
    MATRIX                 parent;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    GsCOORDINATE2*         view;
    GsCOORDINATE2*         view2;
    MATRIX*                parentp;
    MATRIX*                tmp;
    u8*                    head;
    u8*                    head2;
    u8*                    head3;
    MATRIX*                allocated;
    MATRIX*                matrix;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coords2;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         current;
    GsCOORDINATE2*         dest;
    Actor400500HitView*    hit;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    s32                    i;
    s32                    cond;
    s32                    flag;
    u16                    angle;
    u16                    addend;
    s32                    delta;
    TmdObject*             model2;
    TmdObject*             model;

    work            = (Actor400500Work*)arg0->work;
    work->field_A04 = work->field_A04 + 1;
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

    addend = *(volatile u16*)&work->field_9BC;
    SOFT_USE_REG(work);
    __asm__("lui %0,%%hi(%1)" : "=r"(model) : "i"(&gGfxViewCoord));
    __asm__("addiu %0,%1,%%lo(%2)" : "=r"(view) : "r"(model), "i"(&gGfxViewCoord));
    SOFT_USE_REG(work);
    delta = work->field_9BC;
    __asm__("lui %0, 0x1F80" : "=r"(head) : "r"(delta));
    head   = *(u8**)(head + 0x3FC);
    addend = addend + ((s32) - (delta * 0x10) >> 7);
    SOFT_TOUCH_REG(addend);
    allocated = (MATRIX*)(head - sizeof(MATRIX));
    SOFT_TOUCH_REG(allocated);
    work->field_9BC = (s16)addend;
    model           = (TmdObject*)arg0->extra;
    __asm__("move %0,%1" : "=r"(matrix) : "r"(allocated), "r"(model));
    SOFT_TOUCH_REG_USE(model, matrix);
    coords = model->coords;
    SOFT_USE_REG(work);
    parentp                           = &parent;
    coord                             = &coords[6];
    current                           = coord->sub;
    angle                             = addend;
    *(void**)G_SCRATCH_HEAD           = matrix;
    *(MATRIX*)(head - sizeof(MATRIX)) = coords[6].coord;
    __asm__("" : "+r"(current), "=r"(head), "=r"(addend));
    while (1) {
        if (current == NULL) {
            break;
        }
        if (current == view) {
            break;
        }
        parent = current->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &normal);
        *matrix = normal;
        current = current->sub;
    }
    func_8004BFF8((s16)angle, matrix);
    func_actor_400500_8013B720(coord, matrix);
    dest = coord;
    __builtin_memcpy(&dest->coord, matrix, 18);
    SOFT_TOUCH_REG_USE(dest, work);
    dest->flg = 0;
    Gp_UpdateCoord(dest);
    SOFT_BARRIER();

    view2 = &gGfxViewCoord;
    SOFT_USE_REG(work);
    parentp = &parent;
    model2  = (TmdObject*)arg0->extra;
    coords2 = model2->coords;
    __asm__("lui %0, 0x1F80" : "=r"(head2) : "r"(model2));
    head2   = *(u8**)(head2 + 0x3FC);
    angle   = (u16)work->field_9BC;
    coord   = &coords2[9];
    current = coord->sub;
    __asm__("move %0,%1" : "=r"(matrix) : "r"(head2), "r"(current));
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2 + sizeof(MATRIX)) : "memory");
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2) : "memory");
    *matrix = coords2[9].coord;
    while (1) {
        if (current == NULL) {
            break;
        }
        if (current == view2) {
            break;
        }
        parent = current->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &normal);
        *matrix = normal;
        current = current->sub;
    }
    func_8004BFF8((s16)angle, matrix);
    func_actor_400500_8013B720(coord, matrix);
    dest = coord;
    __builtin_memcpy(&dest->coord, matrix, 18);
    SOFT_TOUCH_REG_USE(dest, work);
    dest->flg = 0;
    Gp_UpdateCoord(dest);

    hit                     = (Actor400500HitView*)arg0->work;
    head3                   = (u8*)PSX_SCRATCH;
    head3                   = *(u8**)(head3 + 0x3FC);
    *(void**)G_SCRATCH_HEAD = head3 + sizeof(MATRIX);
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj0.radius = 0x260;
        work4             = (Actor400500Work*)arg0->work;
        work4->field_A06  = 0;
        work4->field_A08  = 0;
        work3             = (Actor400500Work*)arg0->work;
        if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
            flag             = 0x80;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->field_A32 = 0x3C;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131EE4);

extern TaskFuncTable3 D_actor_400500_80131EF0;
extern TaskFuncTable3 D_actor_400500_80131EFC;

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_400500_80131EF0\n"
        "dlabel D_actor_400500_80131EF0\n"
        "    .word func_actor_400500_8013BE50\n"
        "    .word func_actor_400500_8013BEC4\n"
        "    .word func_actor_400500_801387E8\n"
        "enddlabel D_actor_400500_80131EF0\n"
        "nonmatching D_actor_400500_80131EFC\n"
        "dlabel D_actor_400500_80131EFC\n"
        "    .word func_actor_400500_8013BC9C\n"
        "    .word func_actor_400500_8013BCCC\n"
        "    .word func_actor_400500_8013BD64\n"
        "enddlabel D_actor_400500_80131EFC\n"
        ".section .text");
#endif

void func_actor_400500_801385D0(Task* arg0)
{
    Actor400500Work*       work;
    GpEnemy*               enemy;
    TaskFuncTable3         sp10;
    TaskFuncTable3         sp20;
    Actor400500Work*       workA;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    skip;
    s32                    i;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp10  = D_actor_400500_80131EF0;
    sp20  = D_actor_400500_80131EFC;
    if (enemy->field_40 <= 0) {
        if (work->field_A40 == 4) {
            work->field_A42 = 0;
        } else {
            sp20.funcs[(s16)work->field_A0A](arg0);
        }
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        goto common;
    }
    workA = (Actor400500Work*)arg0->work;
    if (workA->field_A4A != 0) {
        workA->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        skip = 1;
    } else {
        skip = 0;
    }
    if (skip == 0) {
        sp10.funcs[(s16)work->field_A08](arg0);
        ((Actor400500Work*)arg0->work)->field_A49 = 0;
        func_actor_400500_80133358(arg0);
    common:
        work2 = (Actor400500Work*)arg0->work;
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
}

void func_actor_400500_801387E8(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    Actor400500Matrix   rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    s32                 flag;
    u16                 frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xB) {
        work->obj1.flags |= 0x8000;
        work->obj2.flags |= 0x8000;
        soundId           = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050005;
        pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_A04 >= 0x12) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((TmdObject*)((Actor400500Work*)arg0->work)->field_9F0[1]->extra)->flags = 0x80;
            work->obj1.flags                                                        &= 0x7FFF;
            work->obj2.flags                                                        &= 0x7FFF;
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0;
        work2->field_A08 = 0;
        work3            = (Actor400500Work*)arg0->work;
        if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
            flag             = 0x80;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->field_A32 = 0x3C;
    }
}

extern TaskFuncTable5 D_actor_400500_80131F08;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131F08);

void func_actor_400500_8013899C(Task* arg0)
{
    Actor400500Work*       work;
    GpEnemy*               enemy;
    TaskFuncTable5         sp;
    Actor400500Work*       workA;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;
    s32                    skip;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131F08;
    if (enemy->field_40 > 0) {
        workA = (Actor400500Work*)arg0->work;
        if (workA->field_A4A != 0) {
            workA->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            skip = 1;
        } else {
            skip = 0;
        }
        if (skip == 0) {
            sp.funcs[(s16)work->field_A08](arg0);
            goto common;
        }
    } else {
        work->field_A42   = 0;
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
    common:
        func_actor_400500_80133358(arg0);
        work2 = (Actor400500Work*)arg0->work;
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
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A49 = 0;
    }
}

void func_actor_400500_80138B78(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Matrix   rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    u16                 frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xD) {
        work->obj3.flags |= 0x8000;
        work->obj4.flags |= 0x8000;
        soundId           = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050005;
        pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_A04 >= 0x10) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((TmdObject*)((Actor400500Work*)arg0->work)->field_9F0[0]->extra)->flags = 0x80;
            work->obj3.flags                                                        &= 0x7FFF;
            work->obj4.flags                                                        &= 0x7FFF;
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A08 = work->field_A08 + 1;
    }
}

void func_actor_400500_80138CE8(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    Task*              child;
    GsCOORDINATE2*     coord;
    s32                angle;

    work                              = (Actor400500Work*)arg0->work;
    src                               = &rot;
    angle                             = work->field_A26 - 0x80;
    work->field_A26                   = angle;
    child                             = ((Actor400500Work*)arg0->work)->field_9F0[0];
    ((TmdObject*)child->extra)->flags = 0;
    coord                             = ((TmdObject*)child->extra)->coords;
    rot.ident.m00_m01                 = 0x1000;
    rot.ident.m02_m10                 = 0;
    src->ident.m11_m12                = 0x1000;
    rot.ident.m20_m21                 = 0;
    src->ident.m22                    = 0x1000;
    func_8004BFF8(-angle, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    if ((s16)work->field_A26 <= 0) {
        ((TmdObject*)((Actor400500Work*)arg0->work)->field_9F0[0]->extra)->flags = 0x80;
        work->field_A08                                                          = work->field_A08 + 1;
    }
}

void func_actor_400500_80138DC4(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    s32                 cond;
    s32                 flag;
    u32                 rnd;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work = (Actor400500Work*)arg0->work;
        if (((work->field_A46 >= 0) || ((u8)work->field_A46 & 0x7F)) && (work->field_A30 == 0)) {
            flag            = 0x80;
            work->field_A46 = flag;
            work->field_A47 = 0;
        }
        ((Actor400500Work*)hit)->field_A32 = 0x3C;
        rnd                                = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState                        = rnd;
        if (!((rnd >> 0x10) & 3)) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 0;
            work2->field_A08 = 0;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 8;
        work3->field_A08 = 0;
    }
}

extern TaskFuncTable4 D_actor_400500_80131F1C;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131F1C);

void func_actor_400500_80138EA0(Task* arg0)
{
    Actor400500Work*       work;
    GpEnemy*               enemy;
    TaskFuncTable4         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131F1C;
    if ((enemy->field_40 <= 0) && (work->field_A40 == 4)) {
        work->field_A42   = 0;
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        return;
    }
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
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
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

extern TaskFuncTable7 D_actor_400500_80131F2C;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131F2C);

void func_actor_400500_8013905C(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable7         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F2C;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
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

void func_actor_400500_801391B0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((TmdObject*)((Actor400500Work*)arg0->work)->field_9F0[1]->extra)->flags = 0x80;
    work->obj1.flags                                                        &= 0x7FFF;
    work->obj2.flags                                                        &= 0x7FFF;
    ((TmdObject*)((Actor400500Work*)arg0->work)->field_9F0[0]->extra)->flags = 0x80;
    work->obj3.flags                                                        &= 0x7FFF;
    flags                                                                    = work->field_A1E;
    work->obj4.flags                                                        &= 0x7FFF;
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->work;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

void func_actor_400500_801392D8(Task* arg0)
{
    Actor400500Work*       work             = (Actor400500Work*)arg0->work;
    void                   (*fns[2])(Task*) = { func_actor_400500_8013C7A4, func_actor_400500_80139448 };
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    Actor400500AnimStride* stride;
    s32                    i;

    fns[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
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
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
    work4            = (Actor400500Work*)arg0->work;
    if (work4->field_A4A != 0) {
        work4->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
    }
}

void func_actor_400500_80139448(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    Actor400500Work* work5;
    s16              mode;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A16 < 0x9C4) {
        Gp_ArmStateF0(1);
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work2 = (Actor400500Work*)arg0->work;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 2;
        work3->field_A08 = 0;
        return;
    }
    mode = work->field_A3C;
    if (mode == 1) {
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050004;
        pan2     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work4 = (Actor400500Work*)arg0->work;
        if ((work4->field_A46 >= 0) || (((u8)work4->field_A46 & 0x7F) != mode)) {
            flag             = 0x81;
            work4->field_A46 = flag;
            work4->field_A47 = 0;
        }
        work5            = (Actor400500Work*)arg0->work;
        work5->field_A06 = 0;
        work5->field_A08 = 0;
    }
}

extern TaskFuncTable3 D_actor_400500_80131F48;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131F48);

void func_actor_400500_801395D0(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable3         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F48;
    if ((s16)work->field_A08 != 0) {
        work2 = (Actor400500Work*)arg0->work;
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
    sp.funcs[(s16)work->field_A08](arg0);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

void func_actor_400500_8013973C(Task* arg0)
{
    Actor400500Matrix      rot;
    MATRIX                 local0;
    MATRIX                 local3;
    Actor400500Matrix*     src;
    MATRIX*                view;
    Actor400500Work*       work;
    Actor400500Work*       workRot;
    Actor400500Work*       workAnim;
    Actor400500Work*       work3;
    GsCOORDINATE2*         coordsEarly;
    GsCOORDINATE2*         coordsMain;
    GsCOORDINATE2*         coordsRot;
    GsCOORDINATE2*         part3;
    GsCOORDINATE2*         root;
    Actor400500ViewPos*    pos;
    Actor400500ViewPos*    pos2;
    Actor400500ViewPos*    posMain;
    Actor400500ViewPos*    posMain2;
    Actor400500AnimStride* stride;
    s32                    i;
    s32                    three;
    s32                    curX;
    s32                    curZ;
    s32                    tgtX;
    s32                    tgtZ;
    s32                    dx;
    s32                    dz;
    u16                    step;
    u16                    accum;
    u16                    pitch;
    s32                    y;
    s32                    viewZ;

    work = (Actor400500Work*)arg0->work;
    root = ((TmdObject*)arg0->extra)->coords;
    if ((s16)++work->field_A04 < 8) {
        pos2        = &work->field_9A0;
        coordsEarly = ((TmdObject*)arg0->extra)->coords;
        Gp_UpdateCoord(&coordsEarly[3]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coordsEarly[3].workm, &rot.mat);
        pos                = pos2;
        pos->x             = rot.mat.t[0];
        pos->z             = rot.mat.t[2];
        coordsEarly[3].flg = 0;
        return;
    }
    tgtX              = (s16)work->field_950;
    curX              = work->field_9A0.x;
    tgtZ              = (s16)work->field_954;
    curZ              = work->field_9A0.z;
    work->field_9A0.x = (u16)work->field_9A0.x + ((tgtX - curX) >> 2);
    work->field_9A0.z = (u16)work->field_9A0.z + ((tgtZ - curZ) >> 2);
    posMain2          = &work->field_9A0;
    coordsMain        = ((TmdObject*)arg0->extra)->coords;
    part3             = &coordsMain[3];
    Gp_UpdateCoord(part3);
    view = &Gfx_ViewWorldMtx;
    Gp_WorldToLocal(view, &coordsMain->workm, &local0);
    Gp_WorldToLocal(view, &coordsMain[3].workm, &local3);
    posMain                = posMain2;
    dx                     = local3.t[0] - local0.t[0];
    coordsMain->coord.t[0] = posMain->x - dx;
    SCHED_BARRIER();
    viewZ                  = posMain->z;
    dz                     = local3.t[2] - local0.t[2];
    coordsMain->coord.t[2] = viewZ - dz;
    coordsMain->flg        = 0;
    coordsMain[3].flg      = 0;
    Gp_UpdateCoord(part3);
    Gp_UpdateCoord(coordsMain);
    step             = (u16)work->field_A10 + 2;
    accum            = (u16)work->field_A12 + step;
    work->field_A12  = accum;
    work->field_A10  = step;
    y                = root->coord.t[1] + (s16)accum;
    root->coord.t[1] = y;
    pitch            = work->field_948;
    three            = 3;
    if ((pitch & 0xFFF) != 0x800) {
        work->field_948 = pitch - 0x80;
    }
    if (root->coord.t[1] >= -0x3E7) {
        y                   = -0x3E8;
        root->coord.t[0]    = (s16)work->field_950;
        root->coord.t[2]    = (s16)work->field_954;
        root->coord.t[1]    = y;
        work->field_A08     = work->field_A08 + 1;
        root->coord.t[1]    = y;
        src                 = &rot;
        work->field_948     = 0;
        work->field_94C     = 0;
        work->field_94A     = (u16)work->field_94A + 0x800;
        workRot             = (Actor400500Work*)arg0->work;
        coordsRot           = ((TmdObject*)arg0->extra)->coords;
        workRot->field_948 &= 0xFFF;
        workRot->field_94A &= 0xFFF;
        workRot->field_94C &= 0xFFF;
        rot.ident.m00_m01   = 0x1000;
        rot.ident.m02_m10   = 0;
        src->ident.m11_m12  = 0x1000;
        rot.ident.m20_m21   = 0;
        src->ident.m22      = 0x1000;
        RotMatrixZ(workRot->field_94C, &src->mat);
        RotMatrixX(workRot->field_948, &src->mat);
        func_8004BFF8(workRot->field_94A, &src->mat);
        ActorsShared80132c4c(&src->mat, &coordsRot->coord);
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0x19;
        work3->field_9FA = 2;
        workAnim         = (Actor400500Work*)arg0->work;
        if (workAnim->field_9FA == 1) {
            if ((s16)workAnim->field_9FC != workAnim->field_9FE) {
                workAnim->field_A00 = 0;
            } else {
                workAnim->field_A00 = func_actor_400500_8013DD8C(arg0, workAnim->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            workAnim->field_9FA = 3;
        } else if (workAnim->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            workAnim->field_9FA = three;
            workAnim->field_A00 = 0;
        } else if (workAnim->field_9FA == three) {
            workAnim->field_A00 = (u16)workAnim->field_A00 + 1;
        }
        i      = 1;
        stride = (Actor400500AnimStride*)workAnim + 1;
        do {
            stride->field_1D = (u8)workAnim->field_9F8;
            Gp_AnimTickIndex(&workAnim->anim, i);
            i++;
            stride++;
        } while (i < 0x12);
        root->flg = 0;
        Gp_UpdateCoord(root);
        work->field_A04 = 0;
    }
}

void func_actor_400500_80139AC4(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    GpEnemy*            enemy;
    s32                 soundId;
    s32                 pan;
    s32                 flag;
    s32                 cond;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->field_40 > 0) {
        if ((s16)++work->field_A04 == 1) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050003;
            pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        }
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            flag = 1;
        } else {
            flag = 0;
        }
        if (flag == 0) {
            hit = (Actor400500HitView*)arg0->work;
            if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
                cond = 1;
            } else {
                cond = 0;
            }
            if (cond) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 0;
                work3->field_A08 = 0;
                work->field_A1E |= 1;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

extern TaskFuncTable3 D_actor_400500_80131F54;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131F54);

void func_actor_400500_80139C1C(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable3         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F54;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
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
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

void func_actor_400500_80139D70(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    GpEnemy*         enemy;
    u16              step;
    u16              accum;
    s32              y;
    s16              angle;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((s16)++work->field_A04 < 9) {
        if (enemy->field_40 <= 0) {
            work->field_A42 = 0;
            return;
        }
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
        }
    } else {
        step              = (u16)work->field_A10 - 2;
        accum             = (u16)work->field_A12 + step;
        work->field_A12   = accum;
        work->field_A10   = step;
        y                 = coord->coord.t[1] - (s16)accum;
        coord->coord.t[1] = y;
        if (work->field_948 < 0x800) {
            angle           = (u16)work->field_948 + 0x98;
            work->field_948 = angle;
            if (angle >= 0x801) {
                work->field_948 = 0x800;
            }
        }
        if (coord->coord.t[1] < -0xFA0) {
            coord->coord.t[1] = -0xFA0;
            work->field_A08   = work->field_A08 + 1;
            coord->coord.t[1] = -0xFA0;
            work->field_948   = 0;
            work->field_94C   = 0x800;
            work->field_94A   = (u16)work->field_94A + 0x800;
            work2             = (Actor400500Work*)arg0->work;
            work2->field_9F8  = 0x10;
            work2->field_9FE  = 0x19;
            work2->field_9FA  = 2;
            soundId           = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050001;
            pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
            soundId2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050002;
            pan2     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(soundId2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        }
    }
}

void func_actor_400500_80139F6C(Task* arg0)
{
    Actor400500Work*       work             = (Actor400500Work*)arg0->work;
    void                   (*fns[2])(Task*) = { func_actor_400500_8013CA38, func_actor_400500_8013A0B8 };
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500AnimStride* stride;
    s32                    i;

    fns[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
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
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

void func_actor_400500_8013A0B8(Task* arg0)
{
    Actor400500Matrix      rot;
    MATRIX                 local2;
    Actor400500Matrix*     src;
    Actor400500Work*       work;
    Actor400500Work*       ang;
    Actor400500Work*       work3;
    Actor400500Work*       nextWork;
    Actor400500Work*       anim;
    Actor400500HitView*    hit;
    Actor400500AnimStride* stride;
    Actor400500ViewPos*    pos;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord14;
    GpEnemy*               enemy;
    MATRIX*                view;
    Actor400500ViewPos*    pos2;
    s32                    z;
    s32                    cond;
    s32                    flag;
    s32                    i;
    s32                    dx;
    s32                    dz;
    s32                    delta;
    s32                    neg;
    u32                    rnd;

    neg    = -1;
    coords = ((TmdObject*)arg0->extra)->coords;
    work   = (Actor400500Work*)arg0->work;
    enemy  = (GpEnemy*)arg0->spawnArg2;
    if ((s16)work->field_A04 == neg) {
        coord14 = &coords[0xE];
        Gp_UpdateCoord(coord14);
        pos2 = &work->field_9A0;
        view = &Gfx_ViewWorldMtx;
        Gp_WorldToLocal(view, &coords->workm, &rot.mat);
        Gp_WorldToLocal(view, &coord14->workm, &local2);
        dx                 = local2.t[0] - rot.mat.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        pos                = pos2;
        z                  = pos->z;
        delta              = local2.t[2] - rot.mat.t[2];
        coords->flg        = 0;
        coord14->flg       = 0;
        dz                 = delta;
        coords->coord.t[2] = z - dz;
        Gp_UpdateCoord(coord14);
        Gp_UpdateCoord(coords);
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        src = &rot;
        if (enemy->field_40 > 0) {
            work->field_A1E   &= 0xFFFD;
            work->field_94A    = ((u16)work->field_94A + 0x800) & 0xFFF;
            ang                = (Actor400500Work*)arg0->work;
            coord              = ((TmdObject*)arg0->extra)->coords;
            ang->field_948    &= 0xFFF;
            ang->field_94A    &= 0xFFF;
            ang->field_94C    &= 0xFFF;
            rot.ident.m00_m01  = 0x1000;
            rot.ident.m02_m10  = 0;
            src->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21  = 0;
            src->ident.m22     = 0x1000;
            RotMatrixZ(ang->field_94C, &src->mat);
            RotMatrixX(ang->field_948, &src->mat);
            func_8004BFF8(ang->field_94A, &src->mat);
            ActorsShared80132c4c(&src->mat, &coord->coord);
            work3            = (Actor400500Work*)arg0->work;
            work3->field_9F8 = 0x10;
            work3->field_9FE = 1;
            work3->field_9FA = 2;
            anim             = (Actor400500Work*)arg0->work;
            if (anim->field_9FA == 1) {
                if ((s16)anim->field_9FC != anim->field_9FE) {
                    anim->field_A00 = 0;
                } else {
                    anim->field_A00 = func_actor_400500_8013DD8C(arg0, anim->field_A00);
                }
                func_actor_400500_8013DCD4(arg0);
                anim->field_9FA = 3;
            } else if (anim->field_9FA == 2) {
                func_actor_400500_8013DC4C(arg0);
                anim->field_9FA = 3;
                anim->field_A00 = 0;
            } else if (anim->field_9FA == 3) {
                anim->field_A00 = (u16)anim->field_A00 + 1;
            }
            i      = 1;
            stride = (Actor400500AnimStride*)anim + 1;
            do {
                stride->field_1D = (u8)anim->field_9F8;
                Gp_AnimTickIndex(&anim->anim, i);
                i++;
                stride++;
            } while (i < 0x12);
            coords->flg = 0;
            Gp_UpdateCoord(coords);
            work3 = (Actor400500Work*)arg0->work;
            if (work3->field_A4A != 0) {
                work3->field_A4A = 0;
                func_actor_400500_8013DB64(arg0, 5);
                flag = 1;
            } else {
                flag = 0;
            }
            if (flag == 0) {
                rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 0x10) & 1) == 0) {
                    nextWork            = (Actor400500Work*)arg0->work;
                    nextWork->field_A06 = 0;
                    nextWork->field_A08 = 0;
                    return;
                }
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 8;
                work3->field_A08 = 0;
            }
        } else {
            work->field_A42    = 0;
            work->field_94A    = ((u16)work->field_94A + 0x800) & 0xFFF;
            ang                = (Actor400500Work*)arg0->work;
            coord              = ((TmdObject*)arg0->extra)->coords;
            ang->field_948    &= 0xFFF;
            ang->field_94A    &= 0xFFF;
            ang->field_94C    &= 0xFFF;
            rot.ident.m00_m01  = 0x1000;
            rot.ident.m02_m10  = 0;
            src->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21  = 0;
            src->ident.m22     = 0x1000;
            RotMatrixZ(ang->field_94C, &src->mat);
            RotMatrixX(ang->field_948, &src->mat);
            func_8004BFF8(ang->field_94A, &src->mat);
            ActorsShared80132c4c(&src->mat, &coord->coord);
            coords->flg = 0;
            Gp_UpdateCoord(coords);
        }
    }
}

extern TaskFuncTable7 D_actor_400500_80131F60;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131F60);

void func_actor_400500_8013A484(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable7         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F60;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
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

void func_actor_400500_8013A5D8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((TmdObject*)((Actor400500Work*)arg0->work)->field_9F0[1]->extra)->flags = 0x80;
    work->obj1.flags                                                        &= 0x7FFF;
    work->obj2.flags                                                        &= 0x7FFF;
    ((TmdObject*)((Actor400500Work*)arg0->work)->field_9F0[0]->extra)->flags = 0x80;
    work->obj3.flags                                                        &= 0x7FFF;
    flags                                                                    = work->field_A1E;
    work->obj4.flags                                                        &= 0x7FFF;
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->work;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131F7C);

void func_actor_400500_8013A700(Task* arg0)
{
    TmdObject*       extra;
    Actor400500Work* work;
    TaskFuncTable10  sp;
    TmdObject*       extra2;
    TmdObject*       extraCopy;
    u8*              head;
    u8*              head2;
    VECTOR*          block;
    GsCOORDINATE2*   coord;
    u8               session;
    u16              a28;
    u32              flags;
    u32              shifted;

    extra = (TmdObject*)arg0->extra;
    work  = (Actor400500Work*)arg0->work;
    sp    = D_actor_400500_80131F7C;
    switch (D_801153F4) {
        case 2:
            extra->flags |= 0x80;
            return;
        case 0:
            sp.funcs[(s16)work->field_A06](arg0);
        case 1:
            extra2 = (TmdObject*)arg0->extra;
            SOFT_USE_REG(extra2);
            SOFT_USE_REG(extra2);
            SOFT_USE_REG(extra2);
            SOFT_USE_REG(extra2);
            coord = extra2->coords;
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            head                      = *(u8**)(head + 0x3FC);
            coord                     = coord + 1;
            ((VECTOR*)head)[-1].vx    = coord->workm.t[0];
            block                     = (VECTOR*)(head - 0x10);
            block->vy                 = coord->workm.t[1];
            block->vz                 = coord->workm.t[2];
            *(VECTOR**)G_SCRATCH_HEAD = block;
            Gp_UpdateActorColor(arg0->spawnArg2, block, 0, 0);
            extraCopy = extra2;
            session   = gGameSession->at4.loc.room;
            if (session != 1) {
                SOFT_TOUCH_REG(extraCopy);
            }
            if ((session == 1) || (session == 3) || (session == 5) || (session == 6)) {
                Gp_SetObjTrans((GpObj20*)extraCopy, 0x200, 0x200, 0x200);
            } else {
                Gp_SetObjTrans((GpObj20*)extra2, 0x400, 0x1000, 0x400);
            }
            SOFT_USE_REG(extraCopy);
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head2) : "r"(work));
            head2                 = *(u8**)(head2 + 0x3FC);
            a28                   = work->field_A28;
            head2                += 0x10;
            flags                 = (u32)a28 << 0x10;
            *(u8**)G_SCRATCH_HEAD = head2;
            if (flags != 0) {
                shifted = flags >> 0x12;
                SOFT_TOUCH_REG(shifted);
                func_actor_400500_80132AB0(arg0, -0xFA0, shifted & 0xFF);
                func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->field_A28);
            }
            return;
    }
}

void func_actor_400500_8013A8E4(Task* arg0)
{
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500AnimStride* stride;
    GpEnemy*               enemy;
    s32                    mapped;
    s32                    i;

    work            = (Actor400500Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    mapped          = D_actor_400500_80153DD4[work->field_9FE];
    work->field_9F8 = 0x10;
    work->field_9FA = 2;
    work->field_9FE = mapped;
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
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj3);
    Gp_UnlinkObj(&work->obj2);
    Gp_UnlinkObj(&work->obj4);
    GameFlag_SetNibble(0xCE, 1);
    if (work->field_A40 == 4) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 6;
        work3->field_A08 = 0;
        return;
    }
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013AA98(Task* arg0)
{
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    Actor400500HitView*    hit;
    s32                    i;
    s32                    cond;

    work  = (Actor400500Work*)arg0->work;
    work2 = work;
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
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013ABE4(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    VECTOR           scale;
    SVECTOR          pos;
    u16              frame;

    work  = (Actor400500Work*)arg0->work;
    model = (TmdObject*)arg0->extra;
    coord = model->coords;

    work->field_A20   = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 4);
    work->field_A24   = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 4);
    work->field_A28   = (u16)work->field_A28 + (-work->field_A28 >> 4);
    model->lightLevel = work->field_A24;
    func_8009EA50(work->field_A20);

    work->field_A02 = (u16)work->field_A02 - 0x30;
    scale.vx        = 0x1000;
    scale.vy        = work->field_A02;
    scale.vz        = 0x1000;
    coord->coord    = work->matrix_808;
    ScaleMatrix(&coord->coord, &scale);
    coord->flg = 0;

    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0x10) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 5, &pos);
    }
    if ((s16)work->field_A04 >= 0x41) {
        model->flags   |= 0x80;
        work->field_A06 = work->field_A06 + 1;
    }
}

extern TaskFuncTable11 D_actor_400500_80131FA4;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131FA4);

void func_actor_400500_8013AD60(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable11        sp;
    GpEnemy*               enemy;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;
    s32                    flag;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131FA4;
    if ((s16)work->field_A08 != 0) {
        work2 = (Actor400500Work*)arg0->work;
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
    if (enemy->field_40 > 0) {
        sp.funcs[(s16)work->field_A08](arg0);
    } else {
        work->field_A42 = 0;
    }
    work3 = (Actor400500Work*)arg0->work;
    if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
        flag             = 0x80;
        work3->field_A46 = flag;
        work3->field_A47 = 0;
    }
}
