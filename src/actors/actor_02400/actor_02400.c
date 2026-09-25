#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Main-executable counter whose lowest bit the flicker alternates on.
extern s32 D_80070F70;

/// Work block of the main body, hung off `Task::work` by its spawn handler
/// (`memCalloc(0x154)`).
///
/// `obj40` is the body's collision object with the four contact records
/// `rec60`; `objC0` is the second body with its single record `recE0`, whose
/// `flags` bit 15 is raised while the body may be hit. `effArg` is the
/// coordinate and argument the hit sparks are spawned with. `field_100` keeps
/// the unscaled model matrix that the per-axis scale `field_128..field_12C` is
/// applied to each frame, and `field_120..field_124` the position before this
/// frame's step. `field_13C` is the behaviour mode and `field_13E` the phase
/// within it, `field_140` their frame counter; `field_130` is the task of the
/// effect the body holds while it grows. `variant` picks one of the two model
/// and parameter sets.
typedef struct Actor02400Work {
    /* 0x000 */ MATRIX   color;
    /* 0x020 */ MATRIX   light;
    /* 0x040 */ GpObj    obj40;
    /* 0x060 */ GpRec18  rec60[4];
    /* 0x0C0 */ GpObj    objC0;
    /* 0x0E0 */ GpRec18  recE0;
    /* 0x0F8 */ GpEffArg effArg;
    /* 0x100 */ MATRIX   field_100;
    /* 0x120 */ s16      field_120;
    /* 0x122 */ s16      field_122;
    /* 0x124 */ s16      field_124;
    /* 0x126 */ byte     pad_126[2];
    /* 0x128 */ s16      field_128;
    /* 0x12A */ s16      field_12A;
    /* 0x12C */ s16      field_12C;
    /* 0x12E */ byte     pad_12E[2];
    /* 0x130 */ Task**   field_130;
    /* 0x134 */ s16      field_134;
    /* 0x136 */ s16      field_136;
    /* 0x138 */ s16      field_138;
    /* 0x13A */ s16      field_13A;
    /* 0x13C */ s16      field_13C;
    /* 0x13E */ s16      field_13E;
    /* 0x140 */ s16      field_140;
    /* 0x142 */ s16      field_142;
    /* 0x144 */ s16      field_144;
    /* 0x146 */ u16      field_146;
    /* 0x148 */ s16      field_148;
    /* 0x14A */ u16      field_14A;
    /* 0x14C */ s16      field_14C;
    /* 0x14E */ s16      variant;
    /* 0x150 */ u16      field_150;
    /* 0x152 */ s16      field_152;
} Actor02400Work;
STATIC_ASSERT_SIZEOF(Actor02400Work, 0x154);

/// Work block of the projectile the main body spawns, hung off `Task::work` by
/// its spawn handler (`memCalloc(0xB4)`).
///
/// `obj_0` and `obj_20` are its two collision bodies sharing the record
/// `rec_40`, and `obj_58` the swept shape `pose_78`, whose table is `field_90`.
/// `field_A8..field_AC` is the direction it travels, taken from the parent's
/// Z axis. `field_B0` is the lifetime timer and `field_B2` the teardown phase.
typedef struct Actor02400ChildWork {
    /* 0x00 */ GpObj        obj_0;
    /* 0x20 */ GpObj        obj_20;
    /* 0x40 */ GpRec18      rec_40;
    /* 0x58 */ GpObj        obj_58;
    /* 0x78 */ GpActorD4Rec pose_78;
    /* 0x90 */ GpRec18      field_90;
    /* 0xA8 */ s16          field_A8;
    /* 0xAA */ s16          field_AA;
    /* 0xAC */ s16          field_AC;
    /* 0xAE */ byte         pad_AE[0x2];
    /* 0xB0 */ u16          field_B0;
    /* 0xB2 */ s16          field_B2;
} Actor02400ChildWork;
STATIC_ASSERT_SIZEOF(Actor02400ChildWork, 0xB4);

/// Scratchpad block the model's scale is applied through: an identity `mat`
/// scaled per axis by `scale`, and the coordinate's translation `t`, restored
/// after the multiply.
typedef struct Actor02400ScaleScratch {
    /* 0x00 */ OverlayMat mat;
    /* 0x20 */ VECTOR     scale;
    /* 0x30 */ VECTOR     t;
} Actor02400ScaleScratch;
STATIC_ASSERT_SIZEOF(Actor02400ScaleScratch, 0x40);

extern s32 D_80115728;
extern s32 D_80115734;
extern s32 D_80115754;

extern GpU16Pair  Actor02400_BodyPairs;
extern GpPairSrcE Actor02400_Params0;
extern GpPairSrcE Actor02400_Params1;
extern u16        Actor02400_D045B8;
extern u16        Actor02400_D045C8;
/// Frames the grown body waits before it spawns, indexed by `variant`.
extern s16      Actor02400_D045D4[];
extern s16      Actor02400_D045D8[];
extern s16      Actor02400_D045DC[];
extern s16      Actor02400_D0463C[];
extern TaskDesc Actor02400_D0465C[];

void Actor02400_Fn005BC(GsCOORDINATE2* arg0, s32 arg1);
void Actor02400_Fn0095C(GpEnemy* enemy, Task* task);
void Actor02400_Fn024F8(GpEnemy* enemy, Task* task);
void Actor02400_Fn02790(GpEnemy* enemy, Task* task);
void Actor02400_Fn02AF0(GpEnemy* enemy, Task* task);
void Actor02400_Fn02E0C(GpEnemy* enemy, Task* task);
void Actor02400_Fn033B4(GpEnemy* enemy, Task* task);
void Actor02400_Fn02EDC(Task* task);
void Actor02400_Fn02F94(Task* task);
void Actor02400_Fn03098(Task* task);
void Actor02400_Fn03140(Task* task);
void Actor02400_Fn031D0(Task* task);
void Actor02400_Fn03228(Task* task);
void Actor02400_Fn03278(Task* task);

/// Draws the glow around `coord`: lights `Gp_RoomCoords[2]` there
/// with a randomly flickering intensity, then projects `coord` and queues two
/// `POLY_FT4` billboards around it, the outer one half again as large as
/// `size`. With `Gp_State1C->groundTrace` set it traces the ground below and
/// draws the ground quad there at twice the outer size.
void Actor02400_Fn00064(GsCOORDINATE2* coord, s16 size)
{
    GsCOORDINATE2  ground;
    POLY_FT4*      prim;
    s16            intensity;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = (s16)(shifted >> 0x11);
    light->head.b               = (s16)(shifted >> 0x12);
    light->head.u.at.local.t[0] = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1] = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    scratch                     = (void**)G_SCRATCH_HEAD;
    block                       = (GpRingScratch*)*scratch - 1;
    block->vec.vx               = *(u16*)&coord->workm.t[0];
    alias                       = block;
    vy                          = *(u16*)&coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = *(u16*)&coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        sc->otz              = (s32)(sc->otz + 1);
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2EU;
        *(u16*)&prim->tpage  = 0x29;
        if (D_80070F70 & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2F;
        prim->tpage          = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_State1C->groundTrace != 0) {
            if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                Actor02400_Fn005BC(&ground, (s32)(s16)(outerSize * 2));
            }
        }
    }
    SCRATCH_HEAD(void) =
        (u8*)SCRATCH_HEAD(void) + sizeof(GpRingScratch);
}

/// Draws a flat textured quad on the ground under `arg0`: the corners of the
/// unit quad `D_80111E38`, scaled by `arg1` and turned into view orientation,
/// are placed around the coordinate's world translation and projected. When
/// all four project, a semi-transparent `POLY_FT4` is queued one step behind
/// their depth, its texture alternating between two frames with the display's
/// animation frame.
void Actor02400_Fn005BC(GsCOORDINATE2* arg0, s32 arg1)
{
    void**                scratch;
    u8*                   head;
    OverlayGroundScratch* sc;
    POLY_FT4*             prim;
    GpQuadCorner*         tbl;
    SVECTOR*              v;
    s32                   i;
    s32                   otz;
    s32                   flag;
    s32                   u;
    s32                   prod;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = (u8*)*scratch - sizeof(OverlayGroundScratch);

    SOFT_TOUCH_REG(head);
    *scratch = head;
    sc       = (OverlayGroundScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = sc->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy0);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
        gte_rtpt();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);

            prim->r0    = 0x30;
            prim->g0    = 0x20;
            prim->b0    = 0x20;
            prim->tpage = 0x28;
            prim->clut  = 0x428C;
            setSemiTrans(prim, 1);
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v0 = 0x38;
            prim->u0 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v1 = 0x38;
            prim->u1 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v2 = 0x57;
            prim->u2 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v3 = 0x57;
            prim->u3 = u;
            prim->x0 = *(u16*)&sc->sxy0.vx;
            prim->y0 = *(u16*)&sc->sxy0.vy;
            prim->x1 = *(u16*)&sc->sxy1.vx;
            prim->y1 = *(u16*)&sc->sxy1.vy;
            prim->x2 = *(u16*)&sc->sxy2.vx;
            prim->y2 = *(u16*)&sc->sxy2.vy;
            prim->x3 = *(u16*)&sc->sxy3.vx;
            prim->y3 = *(u16*)&sc->sxy3.vy;
            addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(sizeof(OverlayGroundScratch));
}

/// The main body's state handlers, run by `Actor02400_Fn02DB0` for the task's
/// state: spawn, per-frame tick and death.
const GpEnemyTaskFuncTable3 Actor02400_D00004 = {
    { Actor02400_Fn0095C, Actor02400_Fn02E0C, Actor02400_Fn024F8 },
};

/// Spawn handler of the main body: allocates the work block, picks the model
/// and parameter variant from the placement, links the enemy node and both
/// collision bodies, starts the scale at 0x600 with a random idle countdown,
/// and moves the task to state 1.
void Actor02400_Fn0095C(GpEnemy* enemy, Task* task)
{
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    Actor02400Work* work;

    obj   = task->extra;
    coord = obj->coords;
    work  = memCalloc(0x154, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work    = work;
    obj->flags    = 0;
    coord->flg    = 0;
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    work->variant = enemy->place->mode & 1;
    if (work->variant != 0) {
        obj->clut += 1;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
    }
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->bodyPos.vy         = -0x96;
    enemy->coord              = coord;
    enemy->node.state.b.flags = 0;
    enemy->bodyPos.vx         = 0;
    enemy->bodyPos.vz         = 0;
    enemy->recs               = work->rec60;
    enemy->param              = work->variant == 0 ? &Actor02400_Params0 : &Actor02400_Params1;
    enemy->hp                 = enemy->param->hpMax;
    SCHED_BARRIER();
    work->effArg.coord      = &((TmdObject*)task->extra)->coords[1];
    work->effArg.spawnArgLo = 0x200;
    work->effArg.spawnArgHi = 1;
    Gp_IncStateF0Ref(0);
    work->field_128      = 0x600;
    work->field_12A      = 0x600;
    work->field_12C      = 0x600;
    work->field_100      = coord->coord;
    Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
    work->field_140      = (Gp_LcgState >> 16) & 0xF;
    work->obj40.key      = 0x30018;
    work->obj40.coord    = coord;
    work->obj40.ctx.recs = work->rec60;
    work->obj40.pos.vy   = -0xC8;
    work->obj40.pos.vx   = 0;
    work->obj40.pos.vz   = 0;
    work->obj40.radius   = 0xC8;
    work->obj40.flags    = 1;
    Gp_LinkObj(2, &work->obj40);
    Gp_InitRec18Table(work->rec60, 4, 0);
    work->obj40.flags   |= 0xC200;
    work->objC0.coord    = &((TmdObject*)task->extra)->coords[3];
    work->objC0.ctx.recs = &work->recE0;
    work->objC0.pos.vx   = 0;
    work->objC0.pos.vy   = 0;
    work->objC0.pos.vz   = 0x1F4;
    work->objC0.key      = Gp_PackPair(&Actor02400_BodyPairs, work->variant * 2);
    work->objC0.radius   = 0x64;
    work->objC0.flags    = 1;
    Gp_LinkObj(3, &work->objC0);
    Gp_InitRec18Table(&work->recE0, 1, 0);
    work->objC0.flags |= 0x8000;
    task->state        = 1;
}

/// Resolves this frame's contacts. The push-back from `func_800E0C10` moves
/// the body (or restores its saved position), then each of the four records is
/// handled by kind: a hit (kind 2) outside the post-hit cooldown `field_136`
/// is classified by the attacker's parameters, deals damage, may knock the
/// body back into mode 4 or 5, spawns sparks and plays the hurt sound, and a
/// kill sets the task to state 2; a solid contact (kind 3) pushes the body
/// out along the deepest overlap. A hit on the second body's record costs the
/// player the variant's MP.
void Actor02400_Fn00C08(Task* task)
{
    ActorPushFrame* scratch;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  src;
    Actor02400Work* work;
    GpEnemy*        enemy;
    s32             push;
    s32             reach;
    s32             val;
    s32             res;
    s32             i;
    s32             z;
    s32             kind;
    s32             param;
    s32             damage;
    s32             lastId;
    s16             dmg;
    s32             sndId;
    s32             pan;
    s32             stun;

    push    = 0;
    lastId  = 0;
    work    = task->work;
    scratch = (ActorPushFrame*)SCRATCH_PUSH_BYTES(0x58);
    coord   = ((TmdObject*)task->extra)->coords;
    enemy   = task->spawnArg2;
    res     = func_800E0C10(work->rec60, &scratch->delta, 4, NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.vx.h.hi;
    coord->coord.t[1] += scratch->delta.vy.h.hi;
    z                  = coord->coord.t[2] + scratch->delta.vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_120;
    coord->coord.t[1] = work->field_122;
    z                 = work->field_124;
move_z:
    coord->coord.t[2] = z;
move_done:
    if (work->field_136 != 0) {
        work->field_136--;
        if (work->field_136 <= 0) {
            work->field_136 = 0;
        }
    }
    i = 0;
    do {
        switch ((u32)work->rec60[i].key >> 16) {
            case 2:
                if (work->field_136 != 0) {
                    break;
                }
                kind   = 0;
                damage = 0;
                if (!(work->rec60[i].key & 0x8000)) {
                    kind = Actor02400_D045DC[work->rec60[i].key & 0x7F];
                }
                switch (Gp_GetIdParam0(work->rec60[i].key) & 0xFFFF) {
                    case 3:
                    case 4:
                        kind = 3;
                        break;
                    case 1:
                    case 7:
                        kind = 1;
                        break;
                    case 0:
                    case 2:
                    case 5:
                    case 6:
                    case 8:
                    case 9:
                        break;
                }
                switch (kind) {
                    case 0:
                        src                 = ((TmdObject*)Gp_ActorSlots[(work->rec60[i].key >> 7) & 1]->extra)->coords;
                        scratch->delta.vx.w = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vy.w = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vz.w = src->coord.t[2] - coord->coord.t[2];
                        work->field_150    += Gp_ComputeDamage(work->rec60[i].key, SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w), 0, 0);
                        if ((s16)work->field_150 >= 20 || work->field_13C == 5) {
                            work->field_13C = 5;
                            work->field_13E = 0;
                            work->field_152 = 0;
                            work->field_150 = 0;
                        } else {
                            work->field_13C = 4;
                            work->field_13E = 0;
                            work->field_152 = 60;
                        }
                        work->field_140 = 0;
                        work->field_138 = 0;
                        work->field_13A = 0;
                        work->field_134 = 0;
                        break;
                    case 1:
                        src                 = ((TmdObject*)Gp_ActorSlots[(work->rec60[i].key >> 7) & 1]->extra)->coords;
                        scratch->delta.vx.w = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vy.w = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vz.w = src->coord.t[2] - coord->coord.t[2];
                        damage              = Gp_ComputeDamage(work->rec60[i].key, SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w), 0, 0);
                        work->field_13C     = 4;
                        work->field_13E     = 0;
                        work->field_140     = 0;
                        work->field_138     = 0;
                        work->field_13A     = 0;
                        work->field_134     = 0;
                        param               = Gp_GetIdParam1(work->rec60[i].key) & 0xFFFF;
                        if (Actor02400_D0463C[param] == 0 && lastId != work->rec60[i].key) {
                            lastId = work->rec60[i].key;
                            func_800FDB18(param, coord, NULL, &work->effArg);
                        }
                        break;
                    case 2:
                        src                 = ((TmdObject*)Gp_ActorSlots[(work->rec60[i].key >> 7) & 1]->extra)->coords;
                        scratch->delta.vx.w = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vy.w = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vz.w = src->coord.t[2] - coord->coord.t[2];
                        damage              = (s16)Gp_ComputeDamage(work->rec60[i].key, SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w), 0, 0) * 5;
                        work->field_13C     = 4;
                        work->field_13E     = 0;
                        work->field_140     = 0;
                        work->field_138     = 0;
                        work->field_13A     = 0;
                        work->field_134     = 0;
                        Gp_SpawnEff(0x6009C, coord, 2, NULL);
                        break;
                    case 3:
                        work->field_13C    = 4;
                        work->field_13E    = 0;
                        work->objC0.flags &= 0x7FFF;
                        if (work->variant == 0) {
                            damage = Actor02400_D045B8;
                        } else {
                            damage = Actor02400_D045C8;
                        }
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        damage     += (s16)(((Gp_LcgState >> 16) & 0x7F) + 200);
                        Gp_SpawnEff(0x6009C, coord, 2, NULL);
                        break;
                }
                dmg = damage;
                func_800E2C78(task->spawnArg2, work->rec60[i].key, dmg, 0);
                func_800DA6E8(&((GpEnemy*)task->spawnArg2)->node, dmg, 0);
                if ((enemy->hp -= damage) <= 0) {
                    task->state = 2;
                }
                stun = Gp_GetIdParam2(work->rec60[i].key);
                if (stun > 0) {
                    work->field_136 = stun;
                }
                sndId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40180003;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sndId, pan, (s8)gpGetObjDepth(coord));
                break;
            case 0:
            case 1:
                break;
            case 3:
                scratch->delta.vx.w = coord->workm.t[0] - work->rec60[i].point.vx;
                scratch->delta.vy.w = coord->workm.t[1] - work->rec60[i].point.vy;
                scratch->delta.vz.w = coord->workm.t[2] - work->rec60[i].point.vz;
                reach               = work->rec60[i].depth - SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w);
                val                 = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &scratch->normal, &scratch->dir);
                }
                break;
        }
    } while (++i < 4);
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->dir.vx) >> 12;
        coord->coord.t[2] += (push * scratch->dir.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->rec60);
    if (Gp_FindRec18(&work->recE0, 0) != 0) {
        work->objC0.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(&work->recE0);
        work->field_14C = 1;
        Gp_SpendMp(Actor02400_D045D8[work->variant]);
    }
    if (work->field_152 != 0) {
        work->field_152--;
        if (work->field_152 <= 0) {
            work->field_150 = 0;
        }
    }
    SCRATCH_POP_BYTES(0x58);
}

/// The projectile's state handlers, run by `Actor02400_Fn03358` for the task's
/// state: spawn, flight and teardown.
const GpEnemyTaskFuncTable3 Actor02400_D0003C = {
    { Actor02400_Fn02790, Actor02400_Fn02AF0, Actor02400_Fn033B4 },
};

/// Mode 0, dormant: counts `field_140` down, re-arming it at random and
/// checking the global wake flag each time it runs out, and wakes the body
/// (mode 1, phase 2, unit scale) when the player comes within 1500 units on
/// the ground plane or once a projectile has been spawned.
void Actor02400_Fn01420(Task* task)
{
    Actor02400Work* work;
    GsCOORDINATE2*  coord;
    s32             flag;
    s32             dx;
    s32             dz;
    u32             random;
    VECTOR*         delta;
    VECTOR*         scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = task->work;
    coord                              = ((TmdObject*)task->extra)->coords;
    flag                               = 0;
    if (--work->field_140 < 0) {
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_140 = (random >> 0x10) & 0xF;
        Gp_LcgState     = random;
        if (Gp_StateF0.field_2 & 2) {
            flag = 1;
        }
    }
    scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy         = 0;
    dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    delta->vz         = dz;
    dx                = scratchEnd[-1].vx;
    if (SquareRoot0((dx * dx) + (dz * dz)) < 0x5DC) {
        flag = 1;
    }
    if (Gp_StateF0.field_1B != 0) {
        flag = 1;
    }
    if (flag != 0) {
        work->field_13C = 1;
        work->field_13E = 2;
        work->field_128 = 0x1000;
        work->field_12A = 0x1000;
        work->field_12C = 0x1000;
        work->field_140 = 0;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) += 1;
}

/// Mode 1, awake: phase 0 turns to face the player and phase 1 rests, each
/// for a random time before handing to the other; phase 2 pulses the scale up
/// until Y passes 0x1C00. Outside phase 2 X and Z relax to unit scale while Y
/// bobs around a ceiling, and the forward speed `field_138` eases toward
/// `field_148`. Once `field_14C` reports a hit on the second body, the body
/// resets into mode 2, rolls its second part at random, plays the sound and
/// takes hold of the `D_80115734` effect it spawns.
void Actor02400_Fn01590(Task* task)
{
    Actor02400Work*   work;
    GsCOORDINATE2*    coord;
    s16               limit;
    s32               diff;
    s32               step;
    s32               sound;
    s32               pan;
    Task**            eff;
    ActorFaceScratch* scratch;
    ActorFaceScratch* scratchEnd;

    scratchEnd                                   = *(ActorFaceScratch**)PSX_SCRATCH_ADDR(0x3FC);
    *(ActorFaceScratch**)PSX_SCRATCH_ADDR(0x3FC) = scratchEnd - 1;
    scratch                                      = scratchEnd - 1;
    work                                         = task->work;
    coord                                        = ((TmdObject*)task->extra)->coords;
    limit                                        = 0x1000;
    switch (work->field_13E) {
        case 0:
            scratchEnd[-1].delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            scratch->delta.vy       = 0;
            scratch->delta.vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_146         = ratan2((s16)scratchEnd[-1].delta.vx, (s16)scratch->delta.vz) & 0xFFF;
            work->field_13A         = 0x19;
            work->field_148         = 0xA;
            work->field_140--;
            if (work->field_140 < 0) {
                work->field_13E = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_140 = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            }
            break;
        case 1:
            work->field_13A = 0;
            work->field_148 = 0x14;
            work->field_140--;
            limit = 0x600;
            if (work->field_140 < 0) {
                work->field_13E = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_140 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
        case 2:
            work->field_13A = 0;
            if (work->field_142 == 0) {
                work->field_140 += 0x40;
                if (work->field_140 >= 0x100) {
                    work->field_142 = 1;
                }
            } else {
                work->field_140 -= 0x40;
                if (work->field_140 < -0xFF) {
                    work->field_142 = 0;
                }
            }
            work->field_128 += (u16)work->field_140 + 0x80;
            work->field_12A += (u16)work->field_140 + 0x80;
            work->field_12C += (u16)work->field_140 + 0x80;
            if (work->field_12A > 0x1C00) {
                work->field_13E = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_140 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }
    if (work->field_13E < 2) {
        work->field_128 -= 0x80;
        if (work->field_128 <= 0x1000) {
            work->field_128 = 0x1000;
        }
        if (work->field_142 == 0) {
            work->field_12A += 0x80;
            if (work->field_12A > limit + 0x100) {
                work->field_142 = 1;
            }
        } else {
            work->field_12A -= 0x80;
            if (work->field_12A < limit - 0x100) {
                work->field_142 = 0;
            }
        }
        work->field_12C -= 0x80;
        if (work->field_12C <= 0x1000) {
            work->field_12C = 0x1000;
        }
    }
    diff = work->field_148 - work->field_138;
    step = -3;
    if (diff > 0) {
        step = 2;
    }
    if ((diff >= 0 ? diff : -diff) < (step >= 0 ? step : -step)) {
        work->field_138 = work->field_148;
    } else {
        work->field_138 += step;
    }
    if (work->field_14C != 0) {
        work->field_13C = 2;
        work->field_13E = 0;
        work->field_140 = 0;
        work->field_13A = 0;
        work->field_138 = 0;
        work->field_128 = 0x1000;
        work->field_12A = 0x1000;
        work->field_12C = 0x1000;
        scratch->rot.vy = 0;
        scratch->rot.vz = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        scratch->rot.vx = ((Gp_LcgState >> 16) & 0xFF) + 0x100;
        RotMatrix(&scratch->rot, &((TmdObject*)task->extra)->coords[2].coord);
        sound = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40180002;
        pan   = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        eff             = (Task**)Gp_SpawnEff(D_80115734, coord, work->variant, NULL);
        work->field_130 = eff;
        if (eff != NULL) {
            Task_Reparent(task, *eff);
        }
    }
    *(ActorFaceScratch**)PSX_SCRATCH_ADDR(0x3FC) += 1;
}

/// Mode 2, recoiling: phase 0 raises `field_134` for up to 7 frames (cut short
/// by a further hit), phase 1 lowers it and after 7 frames either moves on to
/// mode 3 when `field_14C` is set or returns to mode 1 with a random wait.
/// Every frame the Y scale swings between 0xF00 and 0x1100.
void Actor02400_Fn01A10(Task* task)
{
    Actor02400Work* work;
    s32             state;

    work  = task->work;
    state = work->field_13E;
    switch (state) {
        case 0:
            work->field_134 = 1;
            work->field_140++;
            if (work->field_140 >= 7) {
                work->field_13E = 1;
                work->field_140 = 0;
            }
            if (work->field_14C != 0) {
                work->field_13E = 1;
                work->field_140 = 0;
            }
            break;
        case 1:
            work->field_134 = 0;
            work->field_140++;
            if (work->field_140 >= 7) {
                if (work->field_14C != 0) {
                    work->field_14C = 0;
                    work->field_13C = 3;
                    work->field_13E = 0;
                    work->field_140 = 0;
                    work->field_142 = 0;
                    work->field_128 = work->field_12A;
                    work->field_12C = work->field_12A;
                } else {
                    work->field_13C    = state;
                    work->field_13E    = 0;
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_140    = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
                    work->objC0.flags |= 0x8000;
                }
            }
            break;
    }
    if (work->field_142 == 0) {
        work->field_12A += 0x80;
        if (work->field_12A > 0x1100) {
            work->field_142 = 1;
        }
    } else {
        work->field_12A -= 0x80;
        if (work->field_12A < 0xF00) {
            work->field_142 = 0;
        }
    }
}

/// Mode 3, spawning: phase 0 pulses the scale up until Y passes 0x1C00 and
/// sets the held effect to state 2, phase 1 faces the player while Y wobbles
/// around 0x1C00 for the variant's wait, phase 2 spawns the projectile from
/// `Actor02400_D0465C`, raises the wake flag, releases the effect and plays
/// the sound, and phase 3 shrinks the scale back to its floor before returning
/// to mode 1 with a random wait.
void Actor02400_Fn01B90(Task* task)
{
    Actor02400Work* work;
    GsCOORDINATE2*  coord;
    s32             flags;
    s32             sound;
    s32             pan;
    VECTOR*         delta;
    VECTOR*         scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = task->work;
    coord                              = ((TmdObject*)task->extra)->coords;
    flags                              = 0;
    switch (work->field_13E) {
        case 0:
            work->field_13A = 0;
            work->field_138 = 0;
            if (work->field_142 == 0) {
                work->field_140 += 0x40;
                if (work->field_140 >= 0x100) {
                    work->field_142 = 1;
                }
            } else {
                work->field_140 -= 0x40;
                if (work->field_140 < -0xFF) {
                    work->field_142 = 0;
                }
            }
            work->field_128 += (u16)work->field_140 + 0x40;
            work->field_12A += (u16)work->field_140 + 0x40;
            work->field_12C += (u16)work->field_140 + 0x40;
            if (work->field_12A > 0x1C00) {
                work->field_13E = 1;
                work->field_140 = 0;
                if (work->field_130 != NULL) {
                    (*work->field_130)->state = 2;
                }
            }
            break;
        case 1:
            work->field_13A   = 0x19;
            scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vy         = 0;
            delta->vz         = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_146   = ratan2((s16)scratchEnd[-1].vx, (s16)delta->vz) & 0xFFF;
            if (work->field_142 == 0) {
                work->field_12A += 0x80;
                if (work->field_12A > 0x1D00) {
                    work->field_142 = 1;
                }
            } else {
                work->field_12A -= 0x80;
                if (work->field_12A < 0x1B00) {
                    work->field_142 = 0;
                }
            }
            work->field_140++;
            if (work->field_140 > Actor02400_D045D4[work->variant]) {
                work->field_13E = 2;
                work->field_140 = 0;
                work->field_13A = 0;
            }
            break;
        case 2:
            Gp_SpawnEnemyFromTable(Actor02400_D0465C, 1, 0, task->spawnArg2);
            Gp_StateF0.field_1B = 1;
            work->field_13E     = 3;
            if (work->field_130 != NULL) {
                (*work->field_130)->state = 3;
            }
            work->field_130 = NULL;
            sound           = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40180004;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            break;
        case 3:
            work->field_128 -= 0x80;
            if (work->field_128 <= 0x1000) {
                work->field_128 = 0x1000;
                flags           = 1;
            }
            work->field_12A -= 0x80;
            if (work->field_12A <= 0x600) {
                work->field_12A = 0x600;
                flags          |= 2;
            }
            work->field_12C -= 0x80;
            if (work->field_12C <= 0x1000) {
                work->field_12C = 0x1000;
                flags          |= 4;
            }
            if (flags == 7) {
                work->field_13C    = 1;
                work->field_13E    = 0;
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_140    = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
                work->objC0.flags |= 0x8000;
            }
            break;
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) += 1;
}

/// Mode 5, stunned: shrinks the scale toward its floor, sets the held effect
/// to state 4 and lets it go, and keeps the second body unhittable; after 360
/// frames returns to mode 1 with a random wait.
void Actor02400_Fn01F74(Task* task)
{
    Actor02400Work* work = task->work;

    work->field_128 -= 0x40;
    if (work->field_128 < 0x1000) {
        work->field_128 = 0x1000;
    }
    work->field_12A -= 0x80;
    if (work->field_12A < 0x600) {
        work->field_12A = 0x600;
    }
    work->field_12C -= 0x40;
    if (work->field_12C < 0x1000) {
        work->field_12C = 0x1000;
    }
    if (work->field_130 != NULL) {
        (*work->field_130)->state = 4;
        work->field_130           = NULL;
    }
    work->objC0.flags &= 0x7FFF;
    work->field_140++;
    if (work->field_140 > 0x168) {
        work->field_13C    = 1;
        work->field_13E    = 0;
        Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
        work->field_140    = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
        work->objC0.flags |= 0x8000;
    }
}

/// Rebuilds the model coordinate from the saved matrix `field_100`, scaled per
/// axis by `field_128..field_12C`, keeping the coordinate's translation. The
/// first body's height and the second body's reach follow the Y and Z scale.
void Actor02400_Fn0208C(Task* task)
{
    GsCOORDINATE2*          coord;
    Actor02400Work*         work;
    Actor02400ScaleScratch* scratch;
    MATRIX*                 head;

    coord                      = ((TmdObject*)task->extra)->coords;
    work                       = task->work;
    work->field_100            = coord->coord;
    head                       = SCRATCH_HEAD(MATRIX);
    scratch                    = (Actor02400ScaleScratch*)((u8*)head - 0x40);
    SCRATCH_HEAD(void)         = scratch;
    work->obj40.pos.vy         = -0xC8000 / work->field_12A;
    work->objC0.pos.vz         = (work->field_12C * 250) / 4096;
    scratch->scale.vx          = work->field_128;
    scratch->scale.vy          = work->field_12A;
    scratch->scale.vz          = work->field_12C;
    scratch->t.vx              = coord->coord.t[0];
    scratch->t.vy              = coord->coord.t[1];
    scratch->t.vz              = coord->coord.t[2];
    coord->coord               = work->field_100;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->coord.t[0] = scratch->t.vx;
    coord->coord.t[1] = scratch->t.vy;
    SCRATCH_POP_BYTES(0x40);
    coord->coord.t[2] = scratch->t.vz;
    coord->flg        = 0;
}

/// Turns the body towards `field_146` by at most `field_13A` per frame and
/// rebuilds the root coordinate's rotation from the result. The yaw wraps at
/// 0x1000: when the remaining turn would overshoot through the wrap the body
/// snaps to the target instead.
void Actor02400_Fn02264(Task* task)
{
    Actor02400Work*   work;
    GsCOORDINATE2*    coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    coord = ((TmdObject*)task->extra)->coords;
    work  = task->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_146;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_144 = ang;
    if (adiff < 0x800) {
        step = work->field_13A;
        if (step >= adiff) {
            work->field_144 = want;
        } else {
            next = work->field_144;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_144 = next;
        }
    } else {
        step = work->field_13A;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_144 = work->field_146;
        goto done;
    turn:
        wrapStep = work->field_13A;
        cur      = work->field_144;
        if (diff > 0) {
            work->field_144 = cur - wrapStep;
        } else {
            work->field_144 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_144;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_POP_BYTES(0x18);
}

/// Every 25 frames plays the body's idle sound, panned and placed from the
/// root coordinate and made louder the taller the body has grown.
void Actor02400_Fn023B4(Task* task)
{
    GsCOORDINATE2*  object;
    s16             scale;
    s16             ramp;
    s32             soundId;
    s32             volume;
    s8              depth;
    u16             counter;
    Actor02400Work* work;

    work            = task->work;
    object          = ((TmdObject*)task->extra)->coords;
    counter         = work->field_14A + 1;
    work->field_14A = counter;
    if ((s16)counter >= 0x19) {
        work->field_14A = 0;
        scale           = work->field_12A;
        soundId         = (((u16)((GpEnemy*)task->spawnArg2)->placeKey >> 0xC) << 8) | 0x40180001;
        if (scale >= 0x1D01) {
            ramp = 0x1700;
        } else {
            ramp = (u16)work->field_12A - 0x600;
            if (scale < 0x600) {
                ramp = 0;
            }
        }
        volume = (ramp * 0x32) / 5888 + 0x32;
        depth  = 0x7F - (((0x7F - gpGetObjDepth(object)) * (s16)volume) / 100);
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(object), depth);
    }
}

/// Death handler of the main body. In global mode 1 it only refreshes the
/// colour and in mode 2 hides the model. Otherwise phase 0 saves the model
/// matrix, unlinks the enemy node and both bodies, starts the light fade and
/// releases the held effect; phase 1 squashes the body flat, turning it
/// semi-transparent at frame 10, spawning the death effect at frame 15 and
/// hiding it at frame 60; phase 2 destroys the enemy.
void Actor02400_Fn024F8(GpEnemy* arg0, Task* arg1)
{
    VECTOR          pos;
    Actor02400Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  cur;

    obj   = (TmdObject*)arg1->extra;
    work  = arg1->work;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 1:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
            return;
        case 2:
            obj->flags = 0x80;
            return;
        case 0:
        default:
            switch (work->field_13E) {
                case 0:
                    work->field_140 = 0;
                    work->field_12A = 0x1000;
                    work->field_100 = coord->coord;
                    arg0->recs      = 0;
                    Gp_UnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->obj40);
                    Gp_UnlinkObj(&work->objC0);
                    Gp_SetLightMode(arg0, 1);
                    Gp_ReleaseStateF0Add(arg1, 0x18);
                    work->field_13E = 1;
                    cur             = ((TmdObject*)arg1->extra)->coords;
                    pos.vx          = cur->workm.t[0];
                    pos.vy          = cur->workm.t[1];
                    pos.vz          = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
                    if (work->field_130 != NULL) {
                        (*work->field_130)->state = 4;
                    }
                    break;
                case 1:
                    work->field_140++;
                    if ((s16)work->field_140 == 10) {
                        obj->flags = 2;
                    }
                    if ((s16)work->field_140 == 15) {
                        Gp_SpawnEff(0x600A5, coord, 3, NULL);
                    }
                    if ((s16)work->field_140 >= 60) {
                        work->field_13E = 2;
                        obj->flags      = 0x80;
                    }
                    if (work->field_12A > 0x200) {
                        work->field_12A -= 0x50;
                    }
                    Actor02400_Fn03278(arg1);
                    cur    = ((TmdObject*)arg1->extra)->coords;
                    pos.vx = cur->workm.t[0];
                    pos.vy = cur->workm.t[1];
                    pos.vz = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
                    break;
                case 2:
                    Gp_DestroyEnemy(arg0, arg1);
                    break;
            }
            break;
    }
}

/// Spawn handler of the projectile: allocates its work block, places its model
/// 0x15E units along the parent's Y axis with the parent's rotation, takes the
/// parent's Z axis as its direction, links its three collision bodies (keys
/// picked by the parent's variant), arms the 90-frame lifetime, detaches from
/// the parent and moves the task to state 1.
void Actor02400_Fn02790(GpEnemy* arg0, Task* arg1)
{
    Actor02400Work*      parentWork;
    Task*                parent;
    Actor02400ChildWork* work;
    ActorOffsetScratch*  scratch;
    void*                head;
    GsCOORDINATE2*       objCoord;
    GsCOORDINATE2*       objCoord2;
    GsCOORDINATE2*       objCoord3;
    SVECTOR*             offset;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       parentCoord;

    head               = SCRATCH_HEAD(void);
    scratch            = (ActorOffsetScratch*)((u8*)head - 0x18);
    SCRATCH_HEAD(void) = scratch;
    offset             = &scratch->offset;
    parent             = arg1->parent;
    coord              = ((TmdObject*)arg1->extra)->coords;
    parentCoord        = ((TmdObject*)parent->extra)->coords;
    parentWork         = parent->work;
    work               = memCalloc(0xB4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work         = work;
    scratch->offset.vx = 0;
    scratch->offset.vy = -0x15E;
    scratch->offset.vz = 0;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&scratch->result);
    coord->sub        = &gGfxViewCoord;
    coord->coord      = parentCoord->coord;
    coord->coord.t[0] = parentCoord->coord.t[0] + scratch->result.vx;
    coord->coord.t[1] = parentCoord->coord.t[1] + scratch->result.vy;
    coord->coord.t[2] = parentCoord->coord.t[2] + scratch->result.vz;
    coord->flg        = 0;
    work->field_A8    = parentCoord->coord.m[0][2];
    work->field_AA    = parentCoord->coord.m[1][2];
    work->field_AC    = parentCoord->coord.m[2][2];

    objCoord             = ((TmdObject*)arg1->extra)->coords;
    work->obj_0.ctx.recs = &work->rec_40;
    work->obj_0.pos.vx   = 0;
    work->obj_0.pos.vy   = 0;
    work->obj_0.pos.vz   = 0;
    work->obj_0.coord    = objCoord;
    work->obj_0.key      = Gp_PackPair(&Actor02400_BodyPairs, (parentWork->variant * 2) | 1);
    work->obj_0.radius   = 0xC8;
    work->obj_0.flags    = 1;
    Gp_LinkObj(3, &work->obj_0);
    Gp_InitRec18Table(&work->rec_40, 1, 0);
    work->obj_0.flags    |= 0x8000;
    objCoord2             = ((TmdObject*)arg1->extra)->coords;
    work->obj_20.ctx.recs = &work->rec_40;
    work->obj_20.pos.vx   = 0;
    work->obj_20.pos.vy   = 0;
    work->obj_20.pos.vz   = 0;
    work->obj_20.coord    = objCoord2;
    if (parentWork->variant == 0) {
        work->obj_20.key = 0x22D2D;
    } else {
        work->obj_20.key = 0x22E2E;
    }
    work->obj_20.radius = 0xC8;
    work->obj_20.flags  = 1;
    Gp_LinkObj(1, &work->obj_20);

    work->pose_78.end1.vz    = -0xD2;
    work->pose_78.end0Radius = 1;
    work->pose_78.end1Radius = 1;
    work->pose_78.end0.vx    = 0;
    work->pose_78.end0.vy    = 0;
    work->pose_78.end0.vz    = 0;
    work->pose_78.end1.vx    = 0;
    work->pose_78.end1.vy    = 0;
    work->pose_78.recs       = &work->field_90;
    work->obj_20.flags      |= 0x8000;
    objCoord3                = ((TmdObject*)arg1->extra)->coords;
    work->obj_58.ctx.d4rec   = &work->pose_78;
    work->obj_58.pos.vx      = 0;
    work->obj_58.pos.vy      = 0;
    work->obj_58.pos.vz      = 0;
    work->obj_58.key         = 0;
    work->obj_58.radius      = 0;
    work->obj_58.flags       = 3;
    work->obj_58.coord       = objCoord3;
    Gp_LinkObj(3, &work->obj_58);
    Gp_InitRec18Table(&work->field_90, 1, 0);
    work->field_B0      = 0x5A;
    work->obj_58.flags |= 0x4400;
    Task_DetachFromParent(arg1);
    arg1->state = 1;
    SCRATCH_POP_BYTES(0x18);
}

/// Flight handler of the projectile: moves it along `field_A8` / `field_AC`
/// and draws its glow. Once the lifetime `field_B0` runs out, its record is
/// hit, or its swept shape touches a surface whose room parameter blocks it,
/// it spawns the burst effect and moves the task to state 2.
void Actor02400_Fn02AF0(GpEnemy* arg0, Task* arg1)
{
    Actor02400ChildWork* work;
    GsCOORDINATE2*       coord;
    GpRoomParamRec*      param;
    s32                  rec;
    s32                  spawn;
    u16                  timer;

    coord = ((TmdObject*)arg1->extra)->coords;
    work  = arg1->work;
    spawn = 0;
    switch (Gp_StateF0.field_4) {
        case 1:
            Actor02400_Fn00064(coord, 0x100);
            return;
        case 2:
            return;
        case 0:
        default:
            coord->coord.t[0] += (work->field_A8 * 0x19) >> 9;
            coord->coord.t[2] += (work->field_AC * 0x19) >> 9;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            Actor02400_Fn00064(coord, 0x100);
            rec = work->field_90.key;
            if ((rec != 0) &&
                (Gp_RoomParamTables[gGameSession->at4.loc.stage - 1][gGameSession->at4.loc.area - 1]
                                   [func_800E1B24(rec)]
                                       ->field_1 == 0)) {
                spawn = 1;
            }
            Gp_ClearRec18Occupied(&work->field_90);
            timer          = work->field_B0 - 1;
            work->field_B0 = timer;
            if (((timer << 0x10) <= 0) || (work->rec_40.flags & 1) || (spawn != 0)) {
                Gp_SpawnEff(D_80115754, coord, 0, NULL);
                arg1->state    = 2;
                work->field_B2 = 0;
            }
            break;
    }
}

/// While no event is running, one frame in four on average spawns the
/// `D_80115728` effect at `arg0` with the flags in `arg1`, drifting outward at
/// a random angle on the ground plane. Nothing in the package calls it.
void Actor02400_Fn02CA4(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->eventState == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}

/// Task callback of the main body: runs the `Actor02400_D00004` handler for
/// the task's state.
void Actor02400_Fn02DB0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02400_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame handler of the main body. In global mode 2 it only hides the
/// model; in mode 1 it only refreshes the colour and the ground mark. Otherwise
/// it resolves contacts, runs the current mode, turns, steps forward, moves
/// the two front parts, rescales the model and updates its coordinate first.
void Actor02400_Fn02E0C(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2* coord;
    TmdObject*     obj;

    obj   = task->extra;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 1:
            goto case1;
        case 0:
            obj->flags                = 0;
            enemy->node.state.b.flags = 0;
            break;
        case 2:
            obj->flags                = 0x80;
            enemy->node.state.b.flags = 1;
            return;
    }
    Actor02400_Fn00C08(task);
    Actor02400_Fn02EDC(task);
    Actor02400_Fn02264(task);
    Actor02400_Fn03140(task);
    Actor02400_Fn03098(task);
    Actor02400_Fn0208C(task);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor02400_Fn031D0(task);
    Actor02400_Fn03228(task);
}

/// Runs the handler of the body's current mode `field_13C`: dormant, awake,
/// recoiling, spawning, hurt or stunned. The awake, recoiling and spawning
/// modes also play the idle sound.
void Actor02400_Fn02EDC(Task* task)
{
    switch (((Actor02400Work*)task->work)->field_13C) {
        case 0:
            Actor02400_Fn01420(task);
            break;
        case 1:
            Actor02400_Fn01590(task);
            Actor02400_Fn023B4(task);
            break;
        case 2:
            Actor02400_Fn01A10(task);
            Actor02400_Fn023B4(task);
            break;
        case 3:
            Actor02400_Fn01B90(task);
            Actor02400_Fn023B4(task);
            break;
        case 4:
            Actor02400_Fn02F94(task);
            break;
        case 5:
            Actor02400_Fn01F74(task);
            break;
    }
}

/// Mode 4, hurt: swings the Y scale between 0x1400 and 0x1800 in steps of
/// 0x200, releases the held effect (state 4) and keeps the second body
/// unhittable; after 16 frames returns to mode 1 with a random wait.
void Actor02400_Fn02F94(Task* task)
{
    Actor02400Work* work;
    Task**          held;
    u16             sweep;
    u16             counter;
    u32             state;

    work = task->work;
    if (work->field_142 == 0) {
        sweep           = work->field_12A + 0x200;
        work->field_12A = sweep;
        if ((s16)sweep >= 0x1801) {
            work->field_142 = 1;
        }
    } else {
        sweep           = work->field_12A - 0x200;
        work->field_12A = sweep;
        if ((s16)sweep < 0x1400) {
            work->field_142 = 0;
        }
    }
    held = work->field_130;
    if (held != NULL) {
        (*held)->state  = 4;
        work->field_130 = NULL;
    }
    work->objC0.flags = work->objC0.flags & 0x7FFF;
    counter           = work->field_140 + 1;
    work->field_140   = counter;
    if ((s16)counter >= 0x10) {
        state             = (Gp_LcgState * 5) + 0x71357911;
        work->field_13C   = 1;
        work->field_13E   = 0;
        Gp_LcgState       = state;
        work->field_140   = (u16)(((state >> 0x10) & 0x1F) + 0x1E);
        work->objC0.flags = work->objC0.flags | 0x8000;
    }
}

/// Moves the model's two front parts: coordinates 2 and 3 slide out along
/// their Z axis while `field_134` is set and back while it is clear, part 2
/// down to 0 and part 3 down to 0x1E.
void Actor02400_Fn03098(Task* task)
{
    Actor02400Work* work;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  c2;
    GsCOORDINATE2*  c3;

    work  = task->work;
    coord = ((TmdObject*)task->extra)->coords;
    c2    = coord + 2;
    c3    = coord + 3;

    c2->coord.t[0] = 0;
    c2->coord.t[1] = -0x5F;
    if (work->field_134 != 0) {
        c2->coord.t[2] += 0x14;
    } else {
        c2->coord.t[2] -= 0x28;
        if (c2->coord.t[2] < 0) {
            c2->coord.t[2] = 0;
        }
    }
    c2->flg        = 0;
    c3->coord.t[0] = 0;
    c3->coord.t[1] = 0;
    if (work->field_134 != 0) {
        c3->coord.t[2] += 0x50;
    } else {
        c3->coord.t[2] -= 0xA0;
        if (c3->coord.t[2] < 0x1E) {
            c3->coord.t[2] = 0x1E;
        }
    }
    c3->flg = 0;
}

/// Saves the root coordinate's position into `field_120..field_124`, then
/// steps it forward along its own Z axis by the speed `field_138` and drops it
/// 0x80.
void Actor02400_Fn03140(Task* task)
{
    Actor02400Work* work;
    GsCOORDINATE2*  coord;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->work;

    work->field_120    = coord->coord.t[0];
    work->field_122    = coord->coord.t[1];
    work->field_124    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_138) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_138) >> 12;
}

/// Refreshes the body's colour from where its root coordinate stands.
void Actor02400_Fn031D0(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)task->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2, &vec, 0, 0);
}

/// Draws the body's ground mark at its root coordinate's world position.
void Actor02400_Fn03228(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = ((TmdObject*)task->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x200, 0x30);
}

/// Squashes the dying body: restores the saved model matrix `field_100` into
/// the root coordinate and scales it on Y by `field_12A`.
void Actor02400_Fn03278(Task* task)
{
    void**             scratch;
    void*              head;
    ActorScaleScratch* blk;
    Actor02400Work*    work;
    GsCOORDINATE2*     coord;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorScaleScratch*)((u8*)head - 0x30);
    *scratch = blk;
    coord    = ((TmdObject*)task->extra)->coords;
    work     = task->work;

    blk->scale.vx          = 0x1000;
    blk->scale.vy          = work->field_12A;
    blk->scale.vz          = 0x1000;
    coord->coord           = work->field_100;
    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->flg = 0;
    SCRATCH_POP_BYTES_AT(scratch, 0x30);
}

/// Task callback of the projectile: runs the `Actor02400_D0003C` handler for
/// the task's state.
void Actor02400_Fn03358(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02400_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Teardown handler of the projectile: phase 0 unlinks its three collision
/// bodies and waits 60 frames, then phase 1 destroys the enemy.
void Actor02400_Fn033B4(GpEnemy* arg0, Task* arg1)
{
    Actor02400ChildWork* work;
    u16                  temp_v0;

    work = arg1->work;
    switch (work->field_B2) {
        case 0:
            Gp_UnlinkObj(&work->obj_0);
            Gp_UnlinkObj(&work->obj_20);
            Gp_UnlinkObj(&work->obj_58);
            work->field_B2 = 1;
            work->field_B0 = 0x3C;
            return;
        case 1:
            temp_v0        = work->field_B0 - 1;
            work->field_B0 = temp_v0;
            if ((temp_v0 << 0x10) <= 0) {
                Gp_DestroyEnemy(arg0, arg1);
            }
            return;
    }
}
