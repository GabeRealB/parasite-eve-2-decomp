#include "common.h"
#include "actors/actor_104400.h"
#include "actors/actors_shared_80163354.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_8016a538.h"
#include "actors/actors_shared_8016945c.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "psyq/abs.h"
#include "psyq/inline_c.h"

void Actor04400_Fn00220(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4, u8 arg5);
void Actor04400_Fn006A8(Task* arg0);
void Actor04400_Fn022A8(Task* arg0, s16 arg1);
/* Reads the caller's Task* from $a0; the call passes no argument. */
void Actor04400_Fn02B8C();
void Actor04400_Fn02D18(Task* arg0);
void Actor04400_Fn031B8(Task* arg0);
void Actor04400_Fn061B4(void);
void Actor04400_Fn06520(Task* arg0, s16 arg1, u16* arg2);
void Actor04400_Fn07360(Task* arg0);
void func_8004BFF8(s32 angle, MATRIX* matrix);
/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
s16  Actor04400_Fn06328(Task* arg0);
void Actor04400_Fn06374(Task* arg0, s32 arg1);
s32  Actor04400_Fn063E4(Task* arg0);
s16  Actor04400_Fn065F4(Task* arg0, s32 arg1);
s16  Actor04400_Fn06618(Task* arg0);
void Actor04400_Fn067A0(Task* arg0, s32 step);
void Actor04400_Fn08208(Task* arg0);
void Actor04400_Fn0823C(Task* arg0);

extern u32 Gp_LcgState;

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. Declared as a one-element array so GCC 2.8.1
 * cannot treat the store as a non-aliasing scalar and sink it past the
 * `TmdObject` loads. */
extern void*           D_800678F0[1];
extern TaskFuncTable11 Actor04400_D00044;
extern TaskFuncTable9  Actor04400_D000EC;
extern u8              Actor04400_D098FC[];
extern u8              Actor04400_D09FA0[];
extern u8              Actor04400_D0A510[];

/// Copy of `ActorsShared80163354`: draws a semi-transparent textured quad
/// between model parts `firstJoint` and `secondJoint`.
void Actor04400_Fn00220(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade)
{
    ActorsShared80163354Scratch* s;
    s16                          angle;
    GsCOORDINATE2*               secondCoord;
    GsCOORDINATE2*               firstCoord;
    s32                          offset0;
    s32                          offset1;
    s32                          offset2;
    s32                          offset3;
    GsCOORDINATE2*               coords;
    POLY_FT4*                    poly;

    coords      = ((TmdObject*)task->extra)->field_8;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorsShared80163354Scratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(ActorsShared80163354Scratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy       = (s16)height;
        s->second.vy      = (s16)height;
        s->first.vx       = s->firstMatrix.t[0];
        s->first.vz       = s->firstMatrix.t[2];
        s->second.vx      = s->secondMatrix.t[0];
        s->second.vz      = s->secondMatrix.t[2];
        angle             = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        s->halfX          = (s->first.vx - s->second.vx) / 2;
        s->halfZ          = (s->first.vz - s->second.vz) / 2;
        offset0           = rcos(angle) * width;
        s->corner0.vy     = (s16)height;
        s->corner0.vx     = s->halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz     = s->halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1           = rcos(angle) * width;
        s->corner1.vy     = (s16)height;
        s->corner1.vx     = s->halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz     = s->halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2           = rcos(angle) * width;
        s->corner2.vy     = (s16)height;
        s->corner2.vx     = (s->second.vx - (offset2 >> 0xC)) - s->halfX;
        s->corner2.vz     = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        offset3           = rcos(angle) * width;
        s->corner3.vy     = (s16)height;
        s->corner3.vx     = (s->second.vx + (offset3 >> 0xC)) - s->halfX;
        s->corner3.vz     = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        Gfx_ViewCoord.flg = 0;
        Gp_UpdateCoord(&Gfx_ViewCoord);
        gte_SetRotMatrix(&Gfx_ViewCoord.workm);
        gte_SetTransMatrix(&Gfx_ViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = Gpu_PrimCursor;
            Gpu_PrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(ActorsShared80163354Scratch);
    }
}

void Actor04400_Fn006A8(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* src;
    TmdObject* src2;

    D_800678F0[0] = Actor04400_D098FC;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[6], 0x200, NULL);
    if (eff != NULL) {
        src           = (TmdObject*)arg0->extra;
        dst           = (TmdObject*)eff->field_0->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = Actor04400_D09FA0;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[8], 0x200, NULL);
    } else {
        D_800678F0[0] = Actor04400_D0A510;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
    }
    if (eff2 != NULL) {
        src2           = (TmdObject*)arg0->extra;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[4], 0x200, NULL);
}

/// Same body as `ActorsShared801639a8`.
void Actor04400_Fn00874(Task* arg0)
{
    SVECTOR                       rot;
    ActorsShared801639a8Mat       mtx;
    ActorsShared801639a8MatWords* ident;
    Actor104400Work*              work;
    GsCOORDINATE2*                coords;
    MATRIX*                       m5;
    MATRIX*                       m4;
    MATRIX*                       m3;

    work   = (Actor104400Work*)arg0->idMap;
    ident  = &mtx.ident;
    coords = ((TmdObject*)arg0->extra)->field_8;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m5                = &coords[5].coord;
    Gp_MtxToEuler(m5, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m5->m[0][0]   = (u16)mtx.mat.m[0][0];
    m5->m[0][1]   = (u16)mtx.mat.m[0][1];
    m5->m[0][2]   = (u16)mtx.mat.m[0][2];
    m5->m[1][0]   = (u16)mtx.mat.m[1][0];
    m5->m[1][1]   = (u16)mtx.mat.m[1][1];
    m5->m[1][2]   = (u16)mtx.mat.m[1][2];
    m5->m[2][0]   = (u16)mtx.mat.m[2][0];
    m5->m[2][1]   = (u16)mtx.mat.m[2][1];
    m5->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[5].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m4                = &coords[4].coord;
    Gp_MtxToEuler(m4, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m4->m[0][0]   = (u16)mtx.mat.m[0][0];
    m4->m[0][1]   = (u16)mtx.mat.m[0][1];
    m4->m[0][2]   = (u16)mtx.mat.m[0][2];
    m4->m[1][0]   = (u16)mtx.mat.m[1][0];
    m4->m[1][1]   = (u16)mtx.mat.m[1][1];
    m4->m[1][2]   = (u16)mtx.mat.m[1][2];
    m4->m[2][0]   = (u16)mtx.mat.m[2][0];
    m4->m[2][1]   = (u16)mtx.mat.m[2][1];
    m4->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[4].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m3                = &coords[3].coord;
    Gp_MtxToEuler(m3, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m3->m[0][0]   = (u16)mtx.mat.m[0][0];
    m3->m[0][1]   = (u16)mtx.mat.m[0][1];
    m3->m[0][2]   = (u16)mtx.mat.m[0][2];
    m3->m[1][0]   = (u16)mtx.mat.m[1][0];
    m3->m[1][1]   = (u16)mtx.mat.m[1][1];
    m3->m[1][2]   = (u16)mtx.mat.m[1][2];
    m3->m[2][0]   = (u16)mtx.mat.m[2][0];
    m3->m[2][1]   = (u16)mtx.mat.m[2][1];
    m3->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[3].flg = 0;
}

/// Main enemy init. Allocates the 0x454-byte `Actor104400Work`, points the
/// model at the light / color matrices inside it, runs the animation context,
/// links the collision objects and the enemy's list node, and enters state 2
/// for spawn kind 1 (low nibble of `spawnArg1`), state 1 otherwise. The root
/// coord is lifted by 0x3C and its translation kept as the spawn position.
///
/// Same body as `func_actor_342400_80163C58`.
///
/// `one` is a separate variable set before `Gp_IncStateF0Ref`: the ROM holds
/// the constant in `$s0`, which GCC only picks for a pseudo that crosses a
/// call (sched2 then sinks the `li` below the `jal`).
void Actor04400_Fn00B24(Task* arg0)
{
    GpEnemy*         enemy;
    GsCOORDINATE2*   root;
    Actor104400Work* work;
    TmdObject*       obj;
    Actor104400Work* w;
    GpEnemy*         e;
    GsCOORDINATE2*   coord;
    Actor104400Work* w2;
    Actor104400Work* w3;
    Actor104400Work* w4;
    s32              one;

    enemy       = arg0->spawnArg2;
    root        = ((TmdObject*)arg0->extra)->field_8;
    arg0->idMap = Mem_Calloc(0x454, 0);
    work        = (Actor104400Work*)arg0->idMap;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    Actor04400_Fn061B4();
    obj                = arg0->extra;
    w                  = (Actor104400Work*)arg0->idMap;
    e                  = arg0->spawnArg2;
    coord              = obj->field_8;
    arg0->field_24     = Actor04400_D107CC;
    obj->field_1C      = &w->lightMtx;
    obj->field_20      = &w->colorMtx;
    e->field_50        = &Actor04400_D0D318;
    e->field_54        = (s32)w->rec_2EC;
    w->eff_3FC.field_0 = &((TmdObject*)arg0->extra)->field_8[1];
    w->eff_3FC.field_4 = 0x140;
    w->eff_3FC.field_6 = 2;
    e->field_40 = e->field_42 = Actor04400_D0D318.field_4;
    func_800B3F84(&w->anim, Actor04400_D10778, (GpAnimObj*)obj, w->field_21C, &w->slot_B4);
    w2            = (Actor104400Work*)arg0->idMap;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = 2;
    Actor04400_Fn02B8C(arg0);
    coord->sub = &Gfx_ViewCoord;
    Actor04400_Fn02D18(arg0);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    enemy       = arg0->spawnArg2;
    Gp_LinkNode(&enemy->node);
    enemy->field_4      = &((TmdObject*)arg0->extra)->field_8->coord;
    enemy->field_48     = 0;
    enemy->field_1C.vx  = 0;
    enemy->field_1C.vy  = 0;
    enemy->field_1C.vz  = 0;
    enemy->field_18     = &((TmdObject*)arg0->extra)->field_8[1];
    enemy->node.field_4 = 4;
    one                 = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if ((arg0->spawnArg1 & 0xF) == one) {
        w3            = (Actor104400Work*)arg0->idMap;
        arg0->state   = 2;
        w3->field_420 = 0;
        w3->field_422 = 0;
    } else {
        w4            = (Actor104400Work*)arg0->idMap;
        arg0->state   = one;
        w4->field_420 = 0;
        w4->field_422 = 0;
    }
    work->field_80    = root->coord.t[0];
    root->coord.t[1] -= 0x3C;
    work->field_82    = root->coord.t[1];
    work->field_84    = root->coord.t[2];
}

/// Variant of `Actor04400_Fn00B24`'s init: also destroys the enemy when bit 16
/// of `spawnArg1` is set, sets bit 0x80 of the model's `field_C` for spawn
/// kind 2, and enters state 6 with `field_451` set and the collision flags
/// 0x8000 / 0x4000 cleared on `obj_2AC` / `obj_2CC`.
///
/// Same body as `func_actor_342400_80163E70`.
///
/// `two` is a variable for the same reason as `one` in the sibling: the ROM
/// holds the constant in `$s5` across the calls. `kind` has to be its own
/// variable too - masking `flags` in place reuses `$v1` for the result.
void Actor04400_Fn00D3C(Task* arg0)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GsCOORDINATE2*   root;
    Actor104400Work* work;
    TmdObject*       obj;
    Actor104400Work* w;
    GpEnemy*         e;
    GsCOORDINATE2*   coord;
    Actor104400Work* w2;
    Actor104400Work* w3;
    GpEnemy*         e2;
    s32              flags;
    s32              kind;
    s32              two;

    model       = arg0->extra;
    enemy       = arg0->spawnArg2;
    root        = model->field_8;
    arg0->idMap = Mem_Calloc(0x454, 0);
    work        = (Actor104400Work*)arg0->idMap;
    if (work == NULL) {
        goto destroy;
    }
    Actor04400_Fn061B4();
    flags = arg0->spawnArg1;
    if ((flags >> 16) & 1) {
    destroy:
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    kind = flags & 0xF;
    two  = 2;
    if (kind == two) {
        model->field_C |= 0x80;
    }
    obj                = arg0->extra;
    w                  = (Actor104400Work*)arg0->idMap;
    e                  = arg0->spawnArg2;
    coord              = obj->field_8;
    arg0->field_24     = Actor04400_D107CC;
    obj->field_1C      = &w->lightMtx;
    obj->field_20      = &w->colorMtx;
    e->field_50        = &Actor04400_D0D318;
    e->field_54        = (s32)w->rec_2EC;
    w->eff_3FC.field_0 = &((TmdObject*)arg0->extra)->field_8[1];
    w->eff_3FC.field_4 = 0x140;
    w->eff_3FC.field_6 = two;
    e->field_40 = e->field_42 = Actor04400_D0D318.field_4;
    func_800B3F84(&w->anim, Actor04400_D10778, (GpAnimObj*)obj, w->field_21C, &w->slot_B4);
    w2            = (Actor104400Work*)arg0->idMap;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = two;
    Actor04400_Fn02B8C(arg0);
    coord->sub = &Gfx_ViewCoord;
    Actor04400_Fn02D18(arg0);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    e2 = arg0->spawnArg2;
    Gp_LinkNode(&e2->node);
    e2->field_4          = &((TmdObject*)arg0->extra)->field_8->coord;
    e2->field_48         = 0;
    e2->field_1C.vx      = 0;
    e2->field_1C.vy      = 0;
    e2->field_1C.vz      = 0;
    e2->field_18         = &((TmdObject*)arg0->extra)->field_8[1];
    e2->node.field_4     = 1;
    work->field_80       = root->coord.t[0];
    root->coord.t[1]    -= 0x3C;
    work->field_82       = root->coord.t[1];
    work->field_84       = root->coord.t[2];
    work->field_451      = 1;
    work->obj_2AC.flags &= 0x7FFF;
    work->obj_2CC.flags &= 0xBFFF;
    w3                   = (Actor104400Work*)arg0->idMap;
    arg0->state          = 6;
    w3->field_420        = 0;
    w3->field_422        = 0;
}

/// Moves the task to `state` with a fresh state machine.
static __inline__ void Actor04400_SetState(Task* arg0, s32 state)
{
    Actor104400Work* w = (Actor104400Work*)arg0->idMap;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Colours `enemy` from `coord`'s world position through a 0x10-byte `VECTOR`
/// taken off `G_SCRATCH_HEAD`. The same helper as `func_actor_342400_801640B0`.
static __inline__ void Actor04400_UpdateColor(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `field_422` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 Actor04400_TakeHit(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    Actor104400Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            Actor04400_SetState(arg0, 3);
            w2            = (Actor104400Work*)arg0->idMap;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        Actor04400_SetState(arg0, 7);
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off `G_SCRATCH_HEAD`.
static __inline__ void Actor04400_UpdateRotation(Task* arg0)
{
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    MATRIX*          m     = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          dst;

    work->field_78           &= 0xFFF;
    work->field_7A           &= 0xFFF;
    work->field_7C           &= 0xFFF;
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    func_8004BFF8(work->field_7A, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

/// Same body as `func_actor_342400_801640B0`. Per-frame callback for the main
/// enemy. In mode 0 it aims at the nearest actor (`Actor04400_Fn031B8`), lets
/// a pending hit (`Actor04400_TakeHit`) replace the state handler, eases
/// `field_424` toward zero, rebuilds the root rotation, and then picks the
/// next state: the `field_448` request once dead, state 4 when dead, 8 / 9 for
/// messages 4 / 5 while `field_438` is clear.
void Actor04400_Fn00F7C(Task* arg0)
{
    GpEnemy*         enemy = arg0->spawnArg2;
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable11  sp    = Actor04400_D00044;
    s32              cur;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            Actor04400_Fn031B8(arg0);
            if (Actor04400_TakeHit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            Actor04400_Fn02B8C(arg0);
            cur             = (u16)work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            Actor04400_Fn00874(arg0);
            if (work->field_432 == 1) {
                Actor04400_Fn06520(arg0, 6, (u16*)&work->field_98);
            }
            Actor04400_UpdateRotation(arg0);
            Actor04400_Fn022A8(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->field_40 <= 0) {
                Actor04400_SetState(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->field_40 <= 0) {
                Actor04400_SetState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                Actor04400_SetState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                Actor04400_SetState(arg0, 9);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
            Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
            Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}

/// Same body as `ActorsShared8016454c`.
void Actor04400_Fn01418(Task* arg0)
{
    Actor104400Work* work;
    s32              soundId;
    s32              pan;
    s16              step;

    work            = (Actor104400Work*)arg0->idMap;
    work->field_426 = 8;
    work->field_418 = 7;
    work->field_41C = 0x10;
    work->field_414 = 1;
    work->field_422++;
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0001;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_410 = (Gp_LcgState >> 0x10) & 0x7FF;
    if (work->field_43A < 1000) {
        work->field_41C = 0x10;
        work->field_436 = 0x10;
        return;
    }
    if (work->field_43A < 2000) {
        work->field_41C = 0x14;
        step            = 0x12;
    } else if (work->field_43A < 3000) {
        work->field_41C = 0x18;
        step            = 0x14;
    } else if (work->field_43A < 4000) {
        work->field_41C = 0x1C;
        step            = 0x16;
    } else if (work->field_43A < 5000) {
        work->field_41C = 0x20;
        step            = 0x18;
    } else {
        work->field_41C = 0x40;
        step            = 0x20;
    }
    work->field_436 = step;
}

void Actor04400_Fn01584(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    s16              dist;
    s16              limit;
    s16              step;
    s16              angle;
    s16              speed;
    s32              soundId;
    s32              pan;

    dist = work->field_43A;
    if (dist < 1000) {
        limit = 0x10;
        step  = 0x10;
    } else if (dist < 2000) {
        step  = 0x12;
        limit = 0x14;
    } else if (dist < 3000) {
        step  = 0x14;
        limit = 0x18;
    } else if (dist < 4000) {
        step  = 0x16;
        limit = 0x1C;
    } else if (dist < 5000) {
        limit = 0x20;
        step  = 0x18;
    } else {
        limit = 0x40;
        step  = 0x20;
    }
    if (work->field_41C < limit) {
        work->field_41C = limit;
        work->field_436 = step;
    }
    Actor04400_Fn067A0(arg0, work->field_436);
    speed                                           = Actor04400_Fn065F4(arg0, -0x10);
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0001;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (work->field_43A < work->field_410 + 2000 && (work->field_43A < 1500 || work->field_44A == 0) &&
        (u16)(((work->field_444 + 0x800) & 0xFFF) - 0x200) > 0xC00) {
        work->field_422++;
    }
}

/// Same body as `func_actor_342400_801648E4`.
void Actor04400_Fn017B0(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    GsCOORDINATE2*   root = ((TmdObject*)arg0->extra)->field_8;
    MATRIX           local;
    s16              angle;
    s32              soundId;
    s32              pan;
    s16              facing;
    s16              speed;

    if ((s16)++work->field_412 < 40) {
        if ((s16)Actor04400_Fn063E4(arg0)) {
            return;
        }
    } else {
        work->field_438 = 1;
    }
    if ((s16)work->field_412 == 43) {
        GsCOORDINATE2* coords = ((TmdObject*)arg0->extra)->field_8;
        SVECTOR*       v;

        Gfx_ViewCoord.flg = 0;
        Gp_UpdateCoord(&Gfx_ViewCoord);
        coords[6].flg = 0;
        Gp_UpdateCoord(&coords[6]);
        Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coords[6].workm, &local);
        v             = &work->field_98;
        v->vx         = local.t[0];
        v->vy         = local.t[1];
        v->vz         = local.t[2];
        coords[6].flg = 0;
    }
    if (work->field_412 >= 43 && work->field_412 <= 46) {
        work->field_432 = 1;
    } else {
        work->field_432 = 0;
    }
    if ((s16)work->field_412 == 46) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0005;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((s16)work->field_412 == 45) {
        facing = (work->field_444 + 0x800) & 0xFFF;
        if (facing < 0x300) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else if (facing >= 0xD00) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else {
            work->field_40C = work->field_7A;
        }
    }
    if (work->field_412 >= 45 && work->field_412 <= 53) {
        angle                                           = work->field_40C;
        speed                                           = -250;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
        work->obj_3AC.flags                            |= 0x8000;
    } else {
        work->obj_3AC.flags &= 0x7FFF;
    }
    if (work->field_412 >= 45 && work->field_412 <= 48 && work->field_43A < 0x171) {
        Actor104400Work* w;

        work->field_428      = 0;
        work->field_42A      = -200;
        w                    = (Actor104400Work*)arg0->idMap;
        w->field_426         = 2;
        w->field_41C         = 0x10;
        w->field_418         = 0x10;
        w->field_414         = 1;
        work->field_432      = 0;
        work->obj_3AC.flags &= 0x7FFF;
        work->field_422     += 2;
        return;
    }
    if ((s16)work->field_412 >= 47) {
        root->coord.t[1]       += work->field_42A;
        work->obj_2CC.field_12 += work->field_42A;
        work->field_428        += 30;
        work->field_42A        += work->field_428;
        if (root->coord.t[1] >= (s16)work->field_92) {
            Actor104400Work* w = (Actor104400Work*)arg0->idMap;

            w->field_426           = 2;
            w->field_41C           = 0x10;
            w->field_418           = 0x12;
            w->field_414           = 1;
            root->coord.t[1]       = (s16)work->field_92;
            work->obj_2CC.field_12 = 0;
            work->field_412        = 0;
            work->field_422++;
        }
    }
}

void Actor04400_Fn01B70(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u32              rand;

    work = (Actor104400Work*)arg0->idMap;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 0;
        rand             = Gp_LcgState * 5 + 0x71357911;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work->field_7A  += 0x800;
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 2;
        work3            = (Actor104400Work*)arg0->idMap;
        Gp_LcgState      = rand;
        arg0->state      = 1;
        work3->field_420 = 0;
        work3->field_422 = 0;
    }
}

/// Same body as `ActorsShared80164dd4`. Moves the root along `field_40C`
/// and applies the fall velocity; on landing it requests animation 0x13 and
/// advances the sub-state.
void Actor04400_Fn01CA0(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                            = (Actor104400Work*)arg0->idMap;
    angle                                           = work->field_40C;
    coord                                           = ((TmdObject*)arg0->extra)->field_8;
    dx                                              = rsin(angle) << 4;
    speed                                           = 0xC8;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    coord->coord.t[1]                              += work->field_42A;
    work->obj_2CC.field_12                         += work->field_42A;
    work->field_428                                += 0xE;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] >= (s16)work->field_92) {
        anim                   = (Actor104400Work*)arg0->idMap;
        anim->field_426        = 2;
        anim->field_41C        = 0x10;
        anim->field_418        = 0x13;
        anim->field_414        = 1;
        coord->coord.t[1]      = (s16)work->field_92;
        work->obj_2CC.field_12 = 0;
        work->field_412        = 0;
        work->field_422++;
    }
}

/// Same body as `func_actor_342400_80164F3C`. Per-frame callback with a
/// one-entry handler table. `D_801153F4` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `field_448` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `Actor04400_Fn00220` for three part pairs and unhide it. The work
/// block is reloaded through its own local for the state reset, as the
/// original does.
void Actor04400_Fn01E08(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFunc         sp[1] = { Actor04400_Fn07360 };

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp[(s16)work->field_420](arg0);
            Actor04400_Fn022A8(arg0, 1);
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->field_40 <= 0) {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;

                arg0->state  = work->field_448;
                w->field_420 = 0;
                w->field_422 = 0;
            }
            Actor04400_Fn02B8C(arg0);
            if (work->field_432 == 1) {
                Actor04400_Fn06520(arg0, 6, &work->field_80);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
            Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
            Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->field_C &= ~0x80;
            return;
    }
}

/// Same body as `ActorsShared8016513c`: ease the pitch `field_434` a quarter
/// of the way back to zero, rebuild the model root's rotation from it and the
/// heading `field_7A`, then drop the root under an accelerating fall
/// (`field_428` the acceleration, `field_42A` the speed). Once it reaches the
/// floor (Y 0) it stops there, requests animation 12 (kind 2, speed 0x20) and
/// advances `field_422`.
void Actor04400_Fn02008(Task* arg0)
{
    Actor104400Work*         work;
    GsCOORDINATE2*           coord;
    ActorsShared8016a538Mat  rot;
    ActorsShared8016a538Mat* src;
    MATRIX*                  dst;
    Actor104400Work*         anim;

    work               = (Actor104400Work*)arg0->idMap;
    coord              = ((TmdObject*)arg0->extra)->field_8;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    work->field_434   += -work->field_434 >> 2;
    RotMatrixX(work->field_434, &src->mat);
    func_8004BFF8(work->field_7A, &src->mat);
    dst                = &coord->coord;
    dst->m[0][0]       = src->mat.m[0][0];
    dst->m[0][1]       = src->mat.m[0][1];
    dst->m[0][2]       = src->mat.m[0][2];
    dst->m[1][0]       = src->mat.m[1][0];
    dst->m[1][1]       = src->mat.m[1][1];
    dst->m[1][2]       = src->mat.m[1][2];
    dst->m[2][0]       = src->mat.m[2][0];
    dst->m[2][1]       = src->mat.m[2][1];
    dst->m[2][2]       = src->mat.m[2][2];
    work->field_428   += 2;
    work->field_42A   += work->field_428;
    coord->coord.t[1] += work->field_42A;
    if (coord->coord.t[1] > 0) {
        work->field_412   = 0;
        coord->coord.t[1] = 0;
        anim              = (Actor104400Work*)arg0->idMap;
        anim->field_41C   = 0x20;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_422++;
    }
}

/// Same body as `ActorsShared801652a0`.
void Actor04400_Fn0216C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* next;
    Actor104400Work* next2;
    u32              soundId;
    s32              pan;

    work = (Actor104400Work*)arg0->idMap;
    if ((s16)++work->field_412 == 1) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->field_8;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((s16)work->field_412 == 2) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->field_8;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (Actor04400_Fn06618(arg0)) {
        next             = (Actor104400Work*)arg0->idMap;
        arg0->state      = 3;
        next->field_420  = 0;
        next->field_422  = 0;
        next2            = (Actor104400Work*)arg0->idMap;
        next2->field_420 = 3;
        next2->field_422 = 0;
    }
}

/// Picks the per-axis step: the collision `step` when there is one and the
/// push-out opposes it, otherwise whichever of the two is larger in the
/// direction of `step`.
static __inline__ s16 Actor04400_PickStep(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Push-out of the model from contact record `rec`: how far `coord` sits
/// inside the record's radius (`field_2`), along the direction from the
/// record's centre to the root part, carried into grid space.
///
/// `rec` must stay an inline argument: `integrate.c` expands it with
/// `EXPAND_SUM`, giving `(i * 0x18 + work) + 0x2EC` rather than a loop giv.
static __inline__ void Actor04400_CalcPush(Task* arg0, GsCOORDINATE2* coord, GpRec18* rec, SVECTOR* out)
{
    SVECTOR        pos;
    VECTOR         d;
    VECTOR         n;
    GsCOORDINATE2* c2;
    s32            t;
    s32            pen;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    c2     = ((TmdObject*)arg0->extra)->field_8;
    d.vx   = pos.vx - rec->field_8;
    d.vy   = 0;
    d.vz   = pos.vz - rec->field_C;
    pen    = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen    = rec->field_2 - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = c2->workm.t[0] - rec->field_8;
    d.vy = c2->workm.t[1] - rec->field_A;
    d.vz = c2->workm.t[2] - rec->field_C;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Same body as `func_actor_342400_801653DC`. Per-frame contact handling for
/// the overlay's enemy. Walks the eight contact records: kind 1 (skipped when
/// `arg1` is set) and kind 3 push the model out, kind 2 applies a hit -
/// damage, status effects and the pending state request in `field_448` -
/// unless `field_40E` is still cooling down. Then ticks the status flags,
/// applies `func_800E0C10`'s collision step (snapping back to `field_60` when
/// it reports a conflict) and moves the root by the combined step and
/// push-out.
void Actor04400_Fn022A8(Task* arg0, s16 arg1)
{
    GpDeltaScratch   delta;
    SVECTOR          push;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    u8               blocked;
    Actor104400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;

    stepZ                  = 0;
    maxX                   = 0;
    maxZ                   = 0;
    stepX                  = 0;
    blocked                = 0;
    work                   = (Actor104400Work*)arg0->idMap;
    coord                  = ((TmdObject*)arg0->extra)->field_8;
    enemy                  = arg0->spawnArg2;
    *(u8**)G_SCRATCH_HEAD -= 8;
    work->field_41E        = 0;
    for (i = 0; i < 8; i++) {
        switch (work->rec_2EC[i].field_4 & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                Actor04400_CalcPush(arg0, coord, &work->rec_2EC[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_40E == 0) {
                    work->field_41E = 1;
                    dmg             = Gp_ComputeDamage(work->rec_2EC[i].field_4, work->field_43A, 0, 0);
                    amount          = dmg;
                    work->field_40E = Gp_GetIdParam2(work->rec_2EC[i].field_4);
                    if (Gp_RollEnemyChance(enemy, work->rec_2EC[i].field_4, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->field_8[3], 0, NULL);
                    }
                    func_800E2C78((GpObj40*)enemy, work->rec_2EC[i].field_4, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->field_40 -= amount;
                    if (enemy->field_40 < 0) {
                        enemy->field_40 = 0;
                    }
                    func_800FDB18(Gp_GetIdParam1(work->rec_2EC[i].field_4) & 0xFFFF,
                                  &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
                    if (amount >= 0x28) {
                        work->field_448 = 2;
                    } else {
                        work->field_448 = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_2EC[i].field_4) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1((GpObj4C*)enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2((GpObj5D*)enemy, work->rec_2EC[i].field_4, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4((GpObj5C*)enemy, work->rec_2EC[i].field_4, 0);
                            break;
                        case 4:
                            work->field_448 = 4;
                            break;
                        case 5:
                            work->field_448 = 2;
                            break;
                        case 6:
                            work->field_448 = 4;
                            break;
                        case 7:
                            work->field_448 = 2;
                            break;
                        case 8:
                            work->field_448 = 3;
                            break;
                        case 9:
                            work->field_448 = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_2EC[i].field_4) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
                }
                break;
        }
    }

    if (enemy->field_4C & 1) {
        enemy->field_4C &= 0xFE;
        work->field_448  = 5;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_448  = 3;
    }
    if (enemy->field_4C & 0xC) {
        work->field_44E = 1;
        tmp             = Gp_TickObjFlag4((GpObj5C*)enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->field_40 -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->field_40 < 0) {
                enemy->field_40 = 0;
            }
            work->field_41E = 1;
            work->field_448 = 2;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }

    switch (func_800E0C10(work->rec_2EC, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.h.hi;
            stepX = delta.vx.w >> 16;
            if (delta.vx.w & 0xFFFF) {
                if (delta.vx.w > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.w & 0xFFFF) {
                if (delta.vz.w > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_60.vx;
            coord->coord.t[2] = work->field_60.vz;
            coord->flg        = 0;
            blocked           = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_2EC);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->field_40E > 0) {
        work->field_40E--;
    }
    if (blocked == 0) {
        work->field_80    += Actor04400_PickStep(stepX, maxX >> 3);
        work->field_84    += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->coord.t[0] += Actor04400_PickStep(stepX, maxX >> 3);
        coord->coord.t[2] += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->flg         = 0;
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Same body as `ActorsShared80165cc0`.
void Actor04400_Fn02B8C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* start;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor104400Work*)arg0->idMap;
    if (work->field_414 == 1) {
        start = work;
        if (start->field_416 == start->field_418) {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].field_9 = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
        } else {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].field_9 = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
            start->field_426 = 0;
        }
        goto advance;
    }
    if (work->field_414 == 2) {
        start = work;
        for (j = 1; j < 9; j++) {
            Gp_AnimResetSlot(&start->anim, j, start->field_418);
            (&start->slot_B4)[j].field_9 = start->field_41C;
        }
    advance:
        start->field_416 = start->field_418;
        work->field_414  = 3;
        work->field_41A  = 0;
    } else if (work->field_414 == 3) {
        work->field_41A++;
    }
    for (k = 1; k < 9; k++) {
        (&work->slot_B4)[k].field_9 = work->field_41C;
        Gp_AnimTickIndex(&work->anim, k);
    }
}

/// Same body as `ActorsShared80164b68`. Links the actor's three collision
/// objects onto `Gp_ObjLists[2]` and clears their record tables.
void Actor04400_Fn02D18(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    work->obj_2AC.field_8  = &((TmdObject*)arg0->extra)->field_8[1];
    work->obj_2AC.field_C  = work->rec_2EC;
    work->obj_2AC.field_10 = 0;
    work->obj_2AC.field_12 = 0;
    work->obj_2AC.field_14 = 0;
    work->obj_2AC.field_18 = 0x3002C;
    work->obj_2AC.field_1C = 0x170;
    work->obj_2AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2AC);
    Gp_InitRec18Table(work->rec_2EC, 8, 0);
    work->obj_2AC.flags |= 0x8000;

    work->obj_3AC.field_8  = &((TmdObject*)arg0->extra)->field_8[1];
    work->obj_3AC.field_C  = work->rec_3CC;
    work->obj_3AC.field_10 = 0;
    work->obj_3AC.field_12 = 0;
    work->obj_3AC.field_14 = 0;
    work->obj_3AC.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_3AC.field_1C = 0x170;
    work->obj_3AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_3AC);
    Gp_InitRec18Table(work->rec_3CC, 2, 0);
    work->obj_3AC.flags &= 0x7FFF;

    work->obj_2CC.field_8  = &((TmdObject*)arg0->extra)->field_8[1];
    work->obj_2CC.field_C  = work->rec_2EC;
    work->obj_2CC.field_10 = 0;
    work->obj_2CC.field_12 = 0;
    work->obj_2CC.field_14 = 0;
    work->obj_2CC.field_18 = 0x3002C;
    work->obj_2CC.field_1C = 0x224;
    work->obj_2CC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2CC);
    work->obj_2CC.flags |= 0x4000;
}

/// Same body as `func_actor_342400_80165FC0`. Per-frame callback of the main
/// enemy. `D_801153F4` 2 hides the model, 0 runs the current state handler
/// (then colours it), 1 only colours it. Unless `field_451` is set, it then
/// runs `Actor04400_Fn00220` for three part pairs.
void Actor04400_Fn02E8C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable9   sp    = Actor04400_D000EC;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Same body as `ActorsShared80166180`. Death squash: copy `matrix_0` onto the
/// model root, scale Y by the shrinking `field_430`, spawn spark 0x600A5 on
/// frame 4, switch the light mode on frame 16, and hide the model after frame
/// 32.
void Actor04400_Fn0304C(Task* arg0)
{
    Actor104400Work*              work;
    TmdObject*                    obj;
    GsCOORDINATE2*                coord;
    VECTOR                        scale;
    ActorsShared801639a8Mat       m;
    ActorsShared801639a8MatWords* ident;
    SVECTOR                       ofs;

    work             = (Actor104400Work*)arg0->idMap;
    ident            = &m.ident;
    obj              = arg0->extra;
    coord            = obj->field_8;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->matrix_0;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 4) {
        ofs.vx = 0;
        ofs.vy = 0;
        ofs.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 3, &ofs);
    }
    if ((s16)work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if ((s16)work->field_412 > 0x20) {
        obj->field_C |= 0x80;
        work->field_420++;
    }
}

/// Latch the model root position into `field_60`, then aim at the nearer of
/// the two `Gp_ActorSlots` actors (distance measured in XZ): its offset goes
/// to `field_88`..`field_8C`, the distance to `field_43A`, and its heading
/// relative to `field_7A` to `field_444`. Nothing is written when slot 0 is
/// empty. Same body as `ActorsShared801662ec`.
void Actor04400_Fn031B8(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   other;
    GpActorWork*     player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist;
    s32              dist2;

    work              = (Actor104400Work*)arg0->idMap;
    coord             = ((TmdObject*)arg0->extra)->field_8;
    player            = Gp_ActorSlots[0];
    work->field_60.vx = coord->coord.t[0];
    work->field_60.vy = coord->coord.t[1];
    work->field_60.vz = coord->coord.t[2];
    if (player != NULL) {
        other = player->extra->field_8;
        d0.vx = other->coord.t[0] - coord->coord.t[0];
        d0.vy = other->coord.t[1] - coord->coord.t[1];
        d0.vz = other->coord.t[2] - coord->coord.t[2];
        dist  = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (Gp_ActorSlots[1] != NULL) {
            other = Gp_ActorSlots[1]->extra->field_8;
            d1.vx = other->coord.t[0] - coord->coord.t[0];
            d1.vy = other->coord.t[1] - coord->coord.t[1];
            d1.vz = other->coord.t[2] - coord->coord.t[2];
            dist2 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist2 < dist) {
                dist  = dist2;
                d0.vx = d1.vx;
                d0.vy = d1.vy;
                d0.vz = d1.vz;
            }
        }
        do {
            work->field_88  = d0.vx;
            work->field_8A  = d0.vy;
            work->field_8C  = d0.vz;
            work->field_43A = dist;
        } while (0);
        VectorNormalSS(&d0, &d0);
        work->field_444 = (ratan2(d0.vx, d0.vz) - work->field_7A) & 0xFFF;
    }
}
