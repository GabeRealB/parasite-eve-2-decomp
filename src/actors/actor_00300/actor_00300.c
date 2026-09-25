#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80132074.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// Main-executable counter whose lowest bit the flicker alternates on.
extern s32 D_80070F70;

/* Byte access view; preserves struct-store ordering in GCC 2.8.1. */
typedef struct Actor00300ByteView {
    s8 value;
} Actor00300ByteView;

typedef struct Actor00300InitWork {
    /* 0x00 */ GpObj        obj0;
    /* 0x20 */ GpRec18      rec20;
    /* 0x38 */ GpObj        obj38;
    /* 0x58 */ GpActorD4Rec pose;
    /* 0x70 */ GpRec18      rec70;
    /* 0x88 */ s16          timer;
    /* 0x8A */ s16          pad8A;
} Actor00300InitWork;
STATIC_ASSERT_SIZEOF(Actor00300InitWork, 0x8C);

typedef struct Actor00300MainWork {
    /* 0x000 */ ActorAnimRig19 rig;
    /* 0x43C */ Task*          field_43C;
    /* 0x440 */ u8             field_440[32];
    /* 0x460 */ u8             field_460[32];
    /* 0x480 */ GpObj          obj480;
    /* 0x4A0 */ GpActorD4Rec   pose4A0;
    /* 0x4B8 */ GpRec18        rec4B8;
    /* 0x4D0 */ GpObj          obj4D0;
    /* 0x4F0 */ GpRec18        rec4F0[3];
    /* 0x538 */ GpObj          obj538;
    /* 0x558 */ GpRec18        rec558[4];
    /* 0x5B8 */ GpObj          obj5B8;
    /* 0x5D8 */ GpRec18        rec5D8;
    /* 0x5F0 */ GsCOORDINATE2* field_5F0;
    /* 0x5F4 */ s16            field_5F4;
    /* 0x5F6 */ s16            field_5F6;
    /* 0x5F8 */ u8             pad_5F8[0x10];
    /* 0x608 */ MATRIX         matrix608;
    /* 0x628 */ u8             pad_628[0x20];
    /* 0x648 */ s32            field_648;
    /* 0x64C */ u8             pad_64C[0x1A];
    /* 0x666 */ s16            field_666;
    /* 0x668 */ u8             pad_668[0x20];
    /* 0x688 */ s16            field_688;
    /* 0x68A */ u8             pad_68A[0x2];
    /* 0x68C */ u16            field_68C;
    /* 0x68E */ u8             pad_68E[0xA];
    /* 0x698 */ s16            field_698;
    /* 0x69A */ u8             pad_69A[0xA];
} Actor00300MainWork;
STATIC_ASSERT_SIZEOF(Actor00300MainWork, 0x6A4);
typedef struct Actor00300AreaConfig {
    /* 0x0 */ s16 id;
    /* 0x2 */ s16 area;
    /* 0x4 */ s16 room;
    /* 0x6 */ u16 value;
} Actor00300AreaConfig;
STATIC_ASSERT_SIZEOF(Actor00300AreaConfig, 8);

typedef struct Actor100300Work {
    /* 0x000 */ GpObj             obj0;
    /* 0x020 */ u16               field_20;
    /* 0x022 */ byte              pad_22[0x16];
    /* 0x038 */ GpObj             obj38;
    /* 0x058 */ byte              pad_58[0x18];
    /* 0x070 */ GpRec18           field_70;
    /* 0x088 */ u16               field_88;
    /* 0x08A */ s16               field_8A;
    /* 0x08C */ byte              pad_8C[0x3B0];
    /* 0x43C */ Task*             field_43C;
    /* 0x440 */ byte              field_440[0x20];
    /* 0x460 */ byte              field_460[0x20];
    /* 0x480 */ GpObj             obj480;
    /* 0x4A0 */ GpRec18           rec4A0[2];
    /* 0x4D0 */ GpObj             obj4D0;
    /* 0x4F0 */ GpRec18           rec4F0[3];
    /* 0x538 */ GpObj             obj538;
    /* 0x558 */ GpRec18           rec558[4];
    /* 0x5B8 */ GpObj             obj5B8;
    /* 0x5D8 */ GpRec18           rec5D8;
    /* 0x5F0 */ GpEffArg          effArg5F0;
    /* 0x5F8 */ s32               field_5F8;
    /* 0x5FC */ s32               field_5FC;
    /* 0x600 */ s32               field_600;
    /* 0x604 */ byte              pad_604[4];
    /* 0x608 */ MATRIX            field_608;
    /* 0x628 */ MATRIX            field_628;
    /* 0x648 */ s32               field_648;
    /* 0x64C */ byte              pad_64C[0x8];
    /* 0x654 */ struct GpEffWork* field_654;
    /* 0x658 */ s32               field_658;
    /* 0x65C */ SVECTOR           field_65C;
    /* 0x664 */ s16               field_664;
    /* 0x666 */ u16               field_666;
    /* 0x668 */ byte              pad_668[0x2];
    /* 0x66A */ s16               field_66A;
    /* 0x66C */ s16               field_66C;
    /* 0x66E */ s16               field_66E;
    /* 0x670 */ s16               field_670;
    /* 0x672 */ u16               field_672;
    /* 0x674 */ s16               field_674;
    /* 0x676 */ s16               field_676;
    /* 0x678 */ u16               field_678;
    /* 0x67A */ s16               field_67A;
    /* 0x67C */ s16               field_67C;
    /* 0x67E */ s16               field_67E;
    /* 0x680 */ u16               field_680;
    /* 0x682 */ s16               field_682;
    /* 0x684 */ s16               field_684;
    /* 0x686 */ s16               field_686;
    /* 0x688 */ s16               field_688;
    /* 0x68A */ s16               field_68A;
    /* 0x68C */ s16               field_68C;
    /* 0x68E */ s16               field_68E;
    /* 0x690 */ s16               field_690;
    /* 0x692 */ s16               field_692;
    /* 0x694 */ s16               field_694;
    /* 0x696 */ u16               field_696;
    /* 0x698 */ s16               field_698;
    /* 0x69A */ s16               field_69A;
    /* 0x69C */ s16               field_69C;
    /* 0x69E */ s16               field_69E;
    /* 0x6A0 */ s16               field_6A0;
    /* 0x6A2 */ s16               field_6A2;
} Actor100300Work;

s32 SndEvt_EnqueueType6(s32 sound, s32 pan, s32 depth);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s32       D_80115720;
extern s32       D_80115728;
extern s32       D_80115744;
extern s32       D_8011573C;
extern GpU16Pair Actor00300_D15FD8;

extern s16 Actor00300_D16394[];
extern u16 Actor00300_D15FEC;
extern s16 Actor00300_D16000[];
extern s16 Actor00300_D15FF8[];

void Actor00300_Fn048D4(GpEnemy* arg0, Task* arg1);
void Actor00300_Fn04958(GpEnemy* arg0, Task* arg1);

void Actor00300_Fn005D0(GsCOORDINATE2* arg0, s32 arg1);
void Actor00300_Fn00970(GpEnemy* enemy, Task* task);
void Actor00300_Fn04528(Task* arg0);
void Actor00300_Fn00E54(Task* arg0);
void Actor00300_Fn01678(Task* arg0);
void Actor00300_Fn019C0(Task* arg0);
void Actor00300_Fn01D60(Task* arg0);
void Actor00300_Fn01F9C(Task* arg0);
void Actor00300_Fn02620(Task* arg0);
void Actor00300_Fn028D0(Task* arg0);
void Actor00300_Fn02CE8(Task* arg0);
void Actor00300_Fn030B8(Task* arg0);
void Actor00300_Fn032BC(Task* arg0);
void Actor00300_Fn0340C(Task* arg0);
void Actor00300_Fn03A1C(Task* arg0);
void Actor00300_Fn03B70(GpEnemy* arg0, Task* arg1);
void Actor00300_Fn047CC(GpEnemy* arg0, Task* arg1);
void Actor00300_Fn04A2C(Task* arg0);
s32  Actor00300_Fn04B14(SVECTOR* arg0, SVECTOR* arg1);
void Actor00300_Fn04C20(Task* arg0);
void Actor00300_Fn04D28(Task* arg0);
void Actor00300_Fn04E30(Task* arg0);
void Actor00300_Fn04ED4(Task* arg0);
void Actor00300_Fn04FB0(Task* arg0);
void Actor00300_Fn05008(Task* arg0);
void Actor00300_Fn0505C(Task* arg0, MATRIX* arg1, s16 arg2);
void Actor00300_Fn05194(GpEnemy* arg0, Task* arg1);
void Actor00300_Fn05278(GpEnemy* arg0, Task* arg1);

extern GpPairSrcE           Actor00300_D15FE8;
extern Actor00300AreaConfig Actor00300_D16020[];
extern s32                  Actor00300_D16278[][2];
extern TaskDesc             Actor00300_D162F0;
extern u32                  Actor00300_D16314;
extern u32                  Actor00300_D1633C;

/// State handlers of the task `Actor00300_Fn04770` dispatches, indexed by
/// `Task::state`. The first sets the task up and moves it to state 1.
const GpEnemyTaskFuncTable3 Actor00300_D00004 = {
    {
        Actor00300_Fn00970,
        Actor00300_Fn047CC,
        Actor00300_Fn03B70,
    },
};

/// Lights `Gp_RoomCoords[2]` at `coord` with a randomly flickering
/// intensity, projects `coord` and draws two `POLY_FT4` glow billboards around
/// it, the outer one half again as large as `size`; when
/// `Gp_State1C->groundTrace` is set, traces the ground below and draws the
/// ground quad there at twice the outer size.
void Actor00300_Fn00078(GsCOORDINATE2* coord, s16 size)
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
    scratch                     = SCRATCH_HEAD_ADDR;
    block                       = (GpRingScratch*)SCRATCH_HEAD_AT(scratch, void) - 1;
    block->vec.vx               = *(u16*)&coord->workm.t[0];
    alias                       = block;
    vy                          = *(u16*)&coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc                             = alias;
    sc->vec.vy                     = vy;
    sc->vec.vz                     = *(u16*)&coord->workm.t[2];
    Gp_LcgState                    = random;
    SCRATCH_HEAD_AT(scratch, void) = sc;
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
                Actor00300_Fn005D0(&ground, (s32)(s16)(outerSize * 2));
            }
        }
    }
    SCRATCH_HEAD(void) =
        (u8*)SCRATCH_HEAD(void) + sizeof(GpRingScratch);
}

/// Draws a semi-transparent ground quad under `arg0`: the four corners of
/// `D_80111E38` scaled by `arg1` and rotated into view space, offset by the
/// coordinate's world position, with the texture alternating each frame.
void Actor00300_Fn005D0(GsCOORDINATE2* arg0, s32 arg1)
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

    scratch = SCRATCH_HEAD_ADDR;
    head    = (u8*)SCRATCH_HEAD_AT(scratch, void) - sizeof(OverlayGroundScratch);

    SOFT_TOUCH_REG(head);
    SCRATCH_HEAD_AT(scratch, void) = head;
    sc                             = (OverlayGroundScratch*)head;
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

void Actor00300_Fn00970(GpEnemy* enemy, Task* task)
{
    GpAreaKey           key;
    GpEnemy*            child;
    GpRec18*            rec4B8;
    GpRec18*            rec4F0;
    GpRec18*            rec558;
    GpRec18*            rec5D8;
    Actor00300MainWork* work;
    TmdObject*          model;
    s32                 areaIndex;
    Task*               childTask;
    s32                 slot;
    u32                 index;
    u32                 rawId;
    u8                  areaByte0;
    GpAreaKey*          sessionKey;
    GpAreaPlace*        entry;
    TmdObject*          obj;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      parts;

    obj   = task->extra;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor00300MainWork), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work      = work;
    work->field_648 = 0;
    work->field_68C = 0U;
    for (areaIndex = 0; Actor00300_D16020[areaIndex].id != 0; areaIndex++) {
        if ((gGameSession->at4.loc.stage == Actor00300_D16020[areaIndex].area) &&
            (gGameSession->at4.loc.area == Actor00300_D16020[areaIndex].room)) {
            work->field_648 =
                Actor00300_D16278[Actor00300_D16020[areaIndex].id]
                                 [enemy->place->mode];
            work->field_68C = (u16)Actor00300_D16020[areaIndex].value;
        }
    }
    work->field_698 = (s16)enemy->place->variant;
    obj->flags      = 0;
    coord->flg      = 0;
    obj->lightMtx   = (MATRIX*)work->field_460;
    obj->colorMtx   = (MATRIX*)work->field_440;
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    slot              = 1;
    parts             = ((TmdObject*)task->extra)->coords;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->param      = &Actor00300_D15FE8;
    enemy->recs       = (work->rec4F0);
    enemy->coord      = parts + 3;
    enemy->hp         = (s16)Actor00300_D15FE8.hpMax;
    work->field_5F0   = (void*)(((TmdObject*)task->extra)->coords + 3);
    work->field_5F4   = 0x300;
    work->field_5F6   = 2;
    func_800B3F84(&work->rig.anim, &Actor00300_D1633C, obj,
                  work->rig.poses, work->rig.slots);
    do {
        Gp_AnimResetSlot(&work->rig.anim, slot, 1);
        slot += 1;
    } while (slot < 0x13);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->matrix608 = coord->coord;
    work->field_688 = 0xA;
    work->field_666 = 0x28;
    child           = Gp_SpawnEnemyFromTable(&Actor00300_D162F0, 1, 0, enemy);
    rawId           = enemy->placeKey;
    model           = child->task->extra;
    sessionKey      = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage       = sessionKey->stage;
    key.area        = sessionKey->area;
    key.room        = sessionKey->room;
    areaByte0       = gGameSession->at4.loc.view;
    index           = rawId >> 12;
    key.view        = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    entry =
        (GpAreaPlace*)((index * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    childTask                = child->task;
    work->pose4A0.end0.vz    = 0x2328;
    work->pose4A0.end0Radius = 0xFA0;
    rec4B8                   = &work->rec4B8;
    work->pose4A0.end0.vx    = 0;
    work->pose4A0.end0.vy    = 0;
    work->pose4A0.end1.vx    = 0;
    work->pose4A0.end1.vy    = 0;
    work->pose4A0.end1.vz    = 0;
    work->pose4A0.end1Radius = 0x3E8;
    work->pose4A0.recs       = rec4B8;
    work->field_43C          = childTask;
    work->obj480.coord =
        (void*)(((TmdObject*)task->extra)->coords + 2);
    work->obj480.ctx.recs = &work->pose4A0;
    work->obj480.pos.vx   = 0;
    work->obj480.pos.vy   = 0;
    work->obj480.pos.vz   = 0;
    work->obj480.key      = 0;
    work->obj480.radius   = 0;
    work->obj480.flags    = 3U;
    Gp_LinkObj(3, &work->obj480);
    Gp_InitRec18Table(rec4B8, 1, 0);
    rec4F0             = work->rec4F0;
    work->obj480.flags = (u16)(work->obj480.flags | 0xCC00);
    work->obj4D0.coord =
        (void*)(((TmdObject*)task->extra)->coords + 3);
    work->obj4D0.ctx.recs = rec4F0;
    work->obj4D0.pos.vx   = 0;
    work->obj4D0.pos.vy   = 0;
    work->obj4D0.pos.vz   = 0;
    work->obj4D0.key      = 0x30003;
    work->obj4D0.radius   = 0x15E;
    work->obj4D0.flags    = 1U;
    Gp_LinkObj(2, &work->obj4D0);
    Gp_InitRec18Table(rec4F0, 3, 0);
    work->obj4D0.flags    = (u16)(work->obj4D0.flags | 0x8000);
    work->obj538.coord    = (void*)((TmdObject*)task->extra)->coords;
    rec558                = work->rec558;
    work->obj538.key      = 0x30003;
    work->obj538.ctx.recs = rec558;
    work->obj538.pos.vx   = 0;
    work->obj538.pos.vy   = -0x1F4;
    work->obj538.pos.vz   = 0;
    work->obj538.radius   = 0x1F4;
    work->obj538.flags    = 1U;
    Gp_LinkObj(2, &work->obj538);
    Gp_InitRec18Table(rec558, 4, 0);
    work->obj538.flags    = (u16)(work->obj538.flags | 0x4200);
    work->obj5B8.coord    = ((TmdObject*)child->task->extra)->coords;
    rec5D8                = &work->rec5D8;
    work->obj5B8.ctx.recs = rec5D8;
    work->obj5B8.pos.vx   = -0x1F4;
    work->obj5B8.pos.vy   = 0x1F4;
    work->obj5B8.pos.vz   = 0;
    work->obj5B8.key      = Gp_PackPair(&Actor00300_D15FD8, 0);
    work->obj5B8.radius   = 0x2BC;
    work->obj5B8.flags    = 1U;
    Gp_LinkObj(3, &work->obj5B8);
    Gp_InitRec18Table(rec5D8, 1, 0);
    D_80062735         = 0xA;
    work->obj5B8.flags = (u16)(work->obj5B8.flags & 0x7FFF);
    task->msgTable     = &Actor00300_D16314;
    task->state        = 1;
}

void Actor00300_Fn00E54(Task* arg0)
{
    TmdObject*       obj;
    s32              clamped;
    s32              one;
    s32              critical;
    u32              lastId;
    Actor100300Work* work;
    Actor100300Work* rec;
    GpDeltaScratch*  scratchPrev;
    GpDeltaScratch*  scratch;
    GpEnemy*         enemy;
    GsCOORDINATE2*   srcCoord;
    GsCOORDINATE2*   coord;
    s16              damage;
    s16              hpLeft;
    s16              cooldown;
    s32              rngX;
    s32              rngY;
    s32              relZ;
    s32              relZ2;
    s32              rngHi;
    s32              idParam2;
    s32              hitKind;
    s32              toX;
    s32              toY;
    s32              toZ;
    s32              relX;
    s32              relY;
    s32              dist;
    s32              bestDist;
    u32              recId;
    u32              rng;
    u32              rngBit;
    u32              curId;
    u32              tmp;
    u32              param0;
    u32              rngState;

    bestDist    = 0;
    critical    = 0;
    lastId      = 0;
    work        = arg0->work;
    tmp         = (u32)SCRATCH_HEAD_ADDR;
    scratchPrev = SCRATCH_HEAD_AT(tmp, GpDeltaScratch);
    srcCoord    = (GsCOORDINATE2*)work->rec558;
    clamped     = (s32)(scratchPrev - 4);
    SOFT_TOUCH_REG_USE(clamped, srcCoord);
    scratch                              = (GpDeltaScratch*)clamped;
    obj                                  = arg0->extra;
    SCRATCH_HEAD_AT(tmp, GpDeltaScratch) = scratch;
    enemy                                = arg0->spawnArg2;
    coord                                = obj->coords;
    hitKind                              = func_800E0C10((GpRec18*)srcCoord, scratch, 4, NULL);
    switch (hitKind) {
        case 1:
            coord->coord.t[0] += scratchPrev[-4].vx.h.hi;
            coord->coord.t[1] += scratch->vy.h.hi;
            coord->coord.t[2] += scratch->vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_5F8;
            coord->coord.t[1] = work->field_5FC;
            coord->coord.t[2] = work->field_600;
            break;
        case 0:
            break;
    }
    Gp_ClearRec18Occupied(work->rec558);
    if (work->obj4D0.flags & 0x4000) {
        hitKind = func_800E0C10(work->rec4F0, scratch, 3, NULL);
        switch (hitKind) {
            case 1:
                coord->coord.t[0] += scratch->vx.h.hi;
                coord->coord.t[2] += scratch->vz.h.hi;
                break;
            case 2:
                coord->coord.t[0] = work->field_5F8;
                coord->coord.t[2] = work->field_600;
                break;
            case 0:
                break;
        }
    }
    if (work->field_66C != 0) {
        cooldown        = (u16)work->field_66C - 1;
        work->field_66C = cooldown;
        if ((cooldown << 16) <= 0 && enemy->hp > 0) {
            work->field_66C = 0;
        }
    }
    one = 1;
    rec = work;
    do {
        SOFT_TOUCH_REG_USE(one, scratch);
        recId = rec->rec4F0[0].key;
        tmp   = recId >> 0x10;
        switch (tmp) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_66C == 0) {
                    param0 = Gp_GetIdParam0((s32)recId) & 0xFFFF;
                    switch (param0) {
                        case 1:
                            if (work->field_694 == 0) {
                                work->field_694 = one;
                            }
                            break;
                        case 2:
                            Gp_SetObjFlag2(enemy, (s32)rec->rec4F0[0].key, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(enemy, (s32)rec->rec4F0[0].key, 0);
                            break;
                        case 4:
                        case 6:
                            work->field_682 = one;
                        case 0:
                        case 5:
                        case 8:
                        case 9:
                        default:
                            break;
                        case 7:
                            critical = 1;
                            break;
                    }
                    srcCoord        = ((TmdObject*)Gp_ActorSlots[(u8)rec->rec4F0[0].key >> 7]->extra)->coords;
                    scratch->vx.w   = (s32)(srcCoord->coord.t[0] - coord->coord.t[0]);
                    scratch->vy.w   = srcCoord->coord.t[1] - coord->coord.t[1];
                    relZ            = srcCoord->coord.t[2] - coord->coord.t[2];
                    scratch->vz.w   = relZ;
                    dist            = (scratch->vx.w * coord->coord.m[0][2]) + (scratch->vy.w * coord->coord.m[1][2]) + (relZ * coord->coord.m[2][2]);
                    work->field_692 = (s16)((u32)~dist >> 31);
                    relX            = scratch->vx.w;
                    relY            = scratch->vy.w;
                    relZ2           = scratch->vz.w;
                    damage          = Gp_ComputeDamage(rec->rec4F0[0].key, SquareRoot0((relX * relX) + (relY * relY) + (relZ2 * relZ2)), 0, 0);
                    work->field_690 = damage;
                    if (critical != 0) {
                        work->field_690 = (s16)((s32)(damage << 0x10) >> 0x11);
                    } else if (Gp_RollEnemyChance(enemy, rec->rec4F0[0].key, 0) != 0) {
                        work->field_690 *= 4;
                        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords + 3, 0, NULL);
                    }
                    func_800E2C78(enemy, (s32)rec->rec4F0[0].key, (s32)work->field_690, 0);
                    func_800DA6E8(&enemy->node, (s32)work->field_690, 0);
                    hpLeft    = (u16)enemy->hp - (u16)work->field_690;
                    enemy->hp = hpLeft;
                    if ((hpLeft << 0x10) <= 0) {
                        if (work->field_682 == 0) {
                            work->field_684 = 5;
                            if ((u32)((u16)work->field_66E - 0xB) < 2U) {
                                work->field_686 = 2;
                            } else {
                                goto block_48;
                            }
                        } else {
                            work->field_684 = 8;
                            work->field_686 = 0;
                            arg0->state     = 2;
                        }
                    } else {
                        work->field_682 = 0;
                        if (((work->field_690 >= 0x50) || (work->field_694 != 0)) && (work->field_684 != 5)) {
                            work->field_684 = 5;
                        block_48:
                            work->field_686 = 0;
                        } else {
                            rngState = (Gp_LcgState * 5) + 0x71357911;
                            rng      = rngState >> 0x10;
                            rngX     = (rng & 0x7F) + 0x40;
                            TOUCH_MEM(Gp_LcgState);
                            rngBit = rng & 1;
                            SOFT_BARRIER();
                            Gp_LcgState = (s32)rngState;
                            if (!rngBit) {
                                rngX = -rngX;
                            }
                            work->field_65C.vx = rngX;
                            rngHi              = (s32)(rng << 0x10) >> 0x18;
                            rngY               = (rngHi & 0x7F) + 0x40;
                            if (!(rngHi & 1)) {
                                rngY = -rngY;
                            }
                            work->field_65C.vy = rngY;
                            work->field_664    = one;
                        }
                    }
                    curId = rec->rec4F0[0].key;
                    if (lastId != curId) {
                        lastId = curId;
                        SCHED_BARRIER();
                        func_800FDB18(Gp_GetIdParam1((s32)curId) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 3, NULL, &work->effArg5F0);
                    }
                    idParam2 = Gp_GetIdParam2((s32)rec->rec4F0[0].key);
                    if ((idParam2 << 0x10) > 0) {
                        work->field_66C = (s16)idParam2;
                    }
                }
                break;
            case 3:
                srcCoord      = ((TmdObject*)arg0->extra)->coords + 3;
                toX           = srcCoord->workm.t[0] - rec->rec4F0[0].point.vx;
                scratch->vx.w = toX;
                toY           = srcCoord->workm.t[1] - rec->rec4F0[0].point.vy;
                scratch->vy.w = toY;
                toZ           = srcCoord->workm.t[2] - rec->rec4F0[0].point.vz;
                scratch->vz.w = toZ;
                dist          = rec->rec4F0[0].depth - SquareRoot0((toX * toX) + (toY * toY) + (toZ * toZ));
                clamped       = dist;
                if (dist <= 0)
                    clamped = 0;
                dist        = clamped;
                scratchPrev = scratch + 1;
                if (bestDist < dist) {
                    bestDist = dist;
                    SOFT_TOUCH_REG2(scratchPrev, scratch);
                    VectorNormal((VECTOR*)scratch, (VECTOR*)scratchPrev);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)scratchPrev, (VECTOR*)(scratch + 3));
                }
                break;
        }
        rec = (Actor100300Work*)((u8*)rec + 0x18);
        SOFT_TOUCH_REG3(rec, bestDist, scratch);
    } while ((s32)rec < (s32)&work->obj38.pos);
    if (bestDist > 0) {
        coord->coord.t[0] += (s32)(bestDist * scratch[3].vx.w) >> 0xC;
        coord->coord.t[2] += (s32)(bestDist * scratch[3].vz.w) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->rec4F0);
    if (work->rec5D8.flags & 1) {
        work->obj5B8.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(&work->rec5D8);
        Gp_SpendMp(0x14);
        work->field_666                += 0x14;
        ((SVECTOR*)(scratch + 2))[0].vx = -0x1F4U;
        ((SVECTOR*)(scratch + 2))[0].vy = 0x1F4U;
        ((SVECTOR*)(scratch + 2))[0].vz = 0U;
        Gp_SpawnEff(D_80115744, ((TmdObject*)work->field_43C->extra)->coords, 0x20001, (SVECTOR*)(scratch + 2));
    }
    work->field_6A2 = 0;
    if ((work->rec4A0[1].key & 0xFFFF0000) == 0x10000) {
        srcCoord                        = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords + 3;
        ((SVECTOR*)(scratch + 2))[0].vx = (u16)srcCoord->workm.t[0];
        ((SVECTOR*)(scratch + 2))[0].vy = (u16)srcCoord->workm.t[1];
        ((SVECTOR*)(scratch + 2))[0].vz = (u16)srcCoord->workm.t[2];
        ((SVECTOR*)(scratch + 2))[1].vx = (u16)coord->workm.t[0];
        ((SVECTOR*)(scratch + 2))[1].vy = (u16)coord->workm.t[1];
        ((SVECTOR*)(scratch + 2))[1].vz = (u16)coord->workm.t[2];
        if (Actor00300_Fn04B14((SVECTOR*)(scratch + 2), (SVECTOR*)(scratch + 2) + 1) == 0) {
            work->field_6A0 = 0x1C2;
            work->field_6A2 = 1;
        } else {
            work->field_6A0 = 0;
        }
    } else if (work->field_6A0 > 0) {
        work->field_6A0 = (u16)work->field_6A0 - 1;
    }
    Gp_ClearRec18Occupied(&work->rec4A0[1]);
    SCRATCH_POP_BYTES(0x40);
}

void Actor00300_Fn01678(Task* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    s16              timer;
    s16              nextPoint;
    s32              state;
    s16              nextState;
    s32              dx;
    s32              dz;
    u16              angle0;
    u16              angle1;
    VECTOR*          vec;
    VECTOR*          scratchEnd;

    scratchEnd         = SCRATCH_HEAD(void);
    vec                = scratchEnd - 1;
    SCRATCH_HEAD(void) = vec;
    work               = arg0->work;
    state              = work->field_686;
    coord              = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            work->field_67C = 0;
            work->field_67A = 0;
            work->field_66E = 1;
            timer           = (u16)work->field_688 - 1;
            work->field_688 = timer;
            if ((timer << 0x10) <= 0) {
                scratchEnd[-1].vx = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vx - coord->coord.t[0]);
                vec->vy           = 0;
                vec->vz           = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vz - coord->coord.t[2]);
                angle0            = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
                nextState         = 1;
                work->field_680   = angle0;
                if (work->field_67E == angle0) {
                    nextState = 2;
                }
                work->field_686 = nextState;
                work->field_688 = 0;
            }
            break;
        case 1:
            work->field_67C   = 0x28;
            work->field_66E   = 3;
            work->field_67A   = 0;
            scratchEnd[-1].vx = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vx - coord->coord.t[0]);
            vec->vy           = 0;
            vec->vz           = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vz - coord->coord.t[2]);
            angle1            = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
            work->field_680   = angle1;
            if (work->field_67E == angle1) {
                work->field_686 = 2;
            }
            break;
        case 2:
            work->field_67C   = 0x28;
            work->field_67A   = 0x19;
            work->field_66E   = state;
            scratchEnd[-1].vx = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vx - coord->coord.t[0]);
            vec->vy           = 0;
            vec->vz           = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vz - coord->coord.t[2]);
            work->field_680   = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
            dx                = scratchEnd[-1].vx;
            dz                = vec->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) <= work->field_67A) {
                coord->coord.t[0] = (s32)((SVECTOR*)work->field_648)[work->field_68E].vx;
                coord->coord.t[2] = (s32)((SVECTOR*)work->field_648)[work->field_68E].vz;
                work->field_67A   = 0;
                nextPoint         = (u16)work->field_68E + 1;
                work->field_68E   = nextPoint;
                if (nextPoint >= work->field_68C) {
                    work->field_68E = 0;
                }
                work->field_686 = 1;
            }
            break;
    }
    if ((work->field_6A0 != 0) || (Gp_StateF0.field_28 != 0) || (work->field_690 != 0)) {
        work->field_684 = 1;
        work->field_686 = 0;
        work->field_6A0 = 0x1C2;
        Gp_ArmStateF0(1);
        Gp_StateF0.field_28 = 0;
    }
    SCRATCH_POP_BYTES(0x10);
}

void Actor00300_Fn019C0(Task* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    s16              delta;
    s16              angle;
    s16              timer;
    s16              wrapped;
    s32              magnitude;
    s32              moveMagnitude;
    s32              distance;
    s32              random;

    scratchEnd           = SCRATCH_HEAD(VECTOR);
    vec                  = scratchEnd - 1;
    SCRATCH_HEAD(VECTOR) = vec;
    work                 = arg0->work;
    coord                = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_686) {
        case 0:
            work->field_67C = 0;
            work->field_67A = 0;
            work->field_66E = 1;
            timer           = (u16)work->field_688 - 1;
            work->field_688 = timer;
            if (timer <= 0) {
                scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                vec->vy           = 0;
                vec->vz           = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                distance          = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
                angle             = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
                work->field_680   = angle;
                delta             = (u16)angle - (u16)work->field_67E;
                magnitude         = abs(delta);
                if (magnitude < 0x800) {
                    angle = magnitude;
                } else {
                    if (delta > 0) {
                        wrapped = 0x1000 - delta;
                    } else {
                        wrapped = delta + 0x1000;
                    }
                    angle = wrapped;
                }
                if (distance >= 0xBB8 || angle >= 0x100) {
                    work->field_686 = 1;
                }
                random          = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = random;
                work->field_688 = (((u32)random >> 16) & 31) + 30;
            }
            break;
        case 1:
            work->field_67C   = 0x3C;
            work->field_67A   = 0x19;
            work->field_66E   = 2;
            scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy           = 0;
            vec->vz           = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            distance          = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
            angle             = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
            work->field_680   = angle;
            delta             = (u16)angle - (u16)work->field_67E;
            moveMagnitude     = abs(delta);
            if (moveMagnitude < 0x800) {
                angle = moveMagnitude;
            } else {
                if (delta > 0) {
                    wrapped = 0x1000 - delta;
                } else {
                    wrapped = delta + 0x1000;
                }
                angle = wrapped;
            }
            timer           = (u16)work->field_688 - 1;
            work->field_688 = timer;
            if (timer <= 0 || (distance < 0xBB8 && angle < 0x100)) {
                work->field_686 = 0;
                random          = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = random;
                work->field_688 = ((u32)random >> 16) & 31;
            }
            break;
    }
    if (work->field_6A0 == 0) {
        work->field_684                                    = 0;
        work->field_686                                    = 0;
        work->field_690                                    = 0;
        ((Actor00300ByteView*)&Gp_StateF0.field_28)->value = 0;
        work->field_688                                    = 10;
    } else {
        timer           = (u16)work->field_68A - 1;
        work->field_68A = timer;
        if (timer <= 0) {
            random          = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = random;
            work->field_68A = (((u32)random >> 16) & 63) + 60;
            if (work->field_6A2 == 1) {
                Actor00300_Fn01D60(arg0);
            }
        }
    }
    SCRATCH_POP_BYTES(0x10);
}

void Actor00300_Fn01D60(Task* arg0)
{
    Actor100300Work*  work;
    GsCOORDINATE2*    coord;
    ActorFaceScratch* sc;
    s32               random;

    sc    = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (((GpEnemy*)arg0->spawnArg2)->hp * 100 / (s32)Actor00300_D15FEC < 50 &&
        (s16)work->field_666 >= 20) {
        work->field_666 -= 20;
        work->field_66E  = 4;
        work->field_684  = 4;
        work->field_686  = 0;
    } else if ((s16)work->field_666 >= 5) {
        work->field_666 -= 5;
        work->field_66A  = Actor00300_D16000[((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 15];
        if (work->field_66A == 0) {
            work->field_684 = 1;
            work->field_686 = 0;
            work->field_688 = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 31);
        } else {
            work->field_684 = 2;
            work->field_686 = 0;
            work->field_688 = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 31) + 60;
        }
    } else {
        sc->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
        sc->delta.vy = 0;
        sc->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        if (SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vz * sc->delta.vz) < 3000) {
            work->field_684 = 3;
            work->field_686 = 0;
            random          = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = random;
            work->field_688 = (((u32)random >> 16) & 31) + 60;
        } else {
            work->field_684 = 7;
            work->field_686 = 0;
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

void Actor00300_Fn01F9C(Task* arg0)
{
    SVECTOR           sp10;
    SVECTOR           sp18;
    Actor100300Work*  work;
    GpEffWork*        effect;
    GsCOORDINATE2*    coord;
    s16               turnTimer;
    s16               effectTimer2;
    s16               effectTimer1;
    s16               state;
    s16               delta;
    s32               magnitude;
    s16               angle;
    s16               delay;
    s32               random2;
    s32               effectAngle2;
    s32               random1;
    s32               effectAngle1;
    s32               sound;
    s32               delayRandom0;
    s32               delayRandom3;
    s32               pan2;
    s32               pan1;
    s32               effectPan;
    u16               yaw;
    u32               effectRandom2;
    u32               effectRandom1;
    ActorFaceScratch* scratchEnd;
    ActorFaceScratch* scratch;

    scratchEnd = SCRATCH_HEAD(ActorFaceScratch);
    scratch =
        (ActorFaceScratch*)(SCRATCH_HEAD(u8) = (u8*)scratchEnd - 0x18);
    work  = arg0->work;
    state = work->field_686;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            work->field_67C = 0x3C;
            work->field_67A = 0;
            work->field_66E = 3;
            scratchEnd[-1].delta.vx =
                (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy = 0;
            scratch->delta.vz = (s32)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
            yaw               = ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)scratch->delta.vz) &
                  0xFFF;
            work->field_680 = yaw;
            delta           = yaw - (u16)work->field_67E;
            magnitude       = abs(delta);
            angle           = magnitude >= 0x800 ? (delta > 0 ? 0x1000 - delta : delta + 0x1000)
                                                 : magnitude;
            if (angle < 0x100) {
                work->field_686                                    = 1;
                work->field_66E                                    = 4;
                ((Actor00300ByteView*)&Gp_StateF0.field_28)->value = 1;
            } else {
                turnTimer       = (u16)work->field_688 - 1;
                work->field_688 = turnTimer;
                if ((turnTimer << 0x10) <= 0) {
                    work->field_684 = 1;
                    work->field_686 = 0;
                    delayRandom0    = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState     = delayRandom0;
                    delay           = ((u32)delayRandom0 >> 0x10) & 0xF;
                    work->field_688 = delay;
                }
            }
            break;
        case 1:
            ((Actor00300ByteView*)&Gp_StateF0.field_28)->value = 0;
            work->field_67C                                    = 0xF;
            scratchEnd[-1].delta.vx =
                (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy = 0;
            scratch->delta.vz = (s32)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
            work->field_680 =
                ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)scratch->delta.vz) &
                0xFFF;
            if (Gp_State1C->eventState == 0) {
                effectRandom1 = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState   = (s32)effectRandom1;
                if (!((effectRandom1 >> 0x10) & 3)) {
                    random1      = (effectRandom1 * 5) + 0x71357911;
                    Gp_LcgState  = random1;
                    effectAngle1 = ((u32)random1 >> 0x10) & 0xF80;
                    memset(&sp18, 0, 8);
                    sp18.vx = (s16)((u32)(rcos(effectAngle1) * 5) >> 5);
                    sp18.vz = (s16)((u32)(rsin(effectAngle1) * 5) >> 5);
                    sp10    = sp18;
                    Gp_SpawnEff(D_80115728, coord, 0x20101200, &sp10);
                }
            }
            if ((s16)work->field_672 == 0x33) {
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40030006;
                pan1  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan1,
                                    (s32)(s8)gpGetObjDepth(coord));
                scratch->rot.vy = -0x5DC;
                scratch->rot.vx = 0;
                scratch->rot.vz = 0x320;
                effect =
                    Gp_SpawnEff(D_80115744, coord,
                                Actor00300_D15FF8[work->field_66A] - 0x32, &scratch->rot);
                work->field_654 = effect;
                if (effect != NULL) {
                    Task_Reparent(arg0, effect->task);
                    work->field_69C = Actor00300_D15FF8[work->field_66A] - 0x32;
                }
                work->field_658 =
                    ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40030009;
                effectPan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_658, (s32)effectPan,
                                    (s32)(s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_672 >= Actor00300_D15FF8[work->field_66A]) {
                work->field_686 = 2;
                work->field_66E = 5;
                work->field_67C = 0;
            }
            if (work->field_69C > 0) {
                effectTimer1    = (u16)work->field_69C - 1;
                work->field_69C = effectTimer1;
                if ((effectTimer1 << 0x10) <= 0) {
                    work->field_654 = NULL;
                }
            }
            break;
        case 2:
            if (work->field_69C > 0) {
                effectTimer2    = (u16)work->field_69C - 1;
                work->field_69C = effectTimer2;
                if ((effectTimer2 << 0x10) <= 0) {
                    work->field_654 = NULL;
                }
            }
            if (((s16)work->field_672 < 0xE) && (Gp_State1C->eventState == 0)) {
                effectRandom2 = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState   = (s32)effectRandom2;
                if (!((effectRandom2 >> 0x10) & 3)) {
                    random2      = (effectRandom2 * 5) + 0x71357911;
                    Gp_LcgState  = random2;
                    effectAngle2 = ((u32)random2 >> 0x10) & 0xF80;
                    memset(&sp18, 0, 8);
                    sp18.vx = (s16)((u32)(rcos(effectAngle2) * 5) >> 5);
                    sp18.vz = (s16)((u32)(rsin(effectAngle2) * 5) >> 5);
                    sp10    = sp18;
                    Gp_SpawnEff(D_80115728, coord, 0x20101200, &sp10);
                }
            }
            if ((s16)work->field_672 == 0xE) {
                Gp_SpawnEnemyFromTable(&Actor00300_D162F0, 2, 0, arg0->spawnArg2);
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40030005;
                pan2  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan2,
                                    (s32)(s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_672 >= 0x13) {
                work->field_686 = 3;
                work->field_66E = 6;
            }
            break;
        case 3:
            if ((s16)work->field_672 >= 0x14) {
                work->field_684 = 1;
                work->field_686 = 0;
                delayRandom3    = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = delayRandom3;
                delay           = ((u32)delayRandom3 >> 0x10) & 0x1F;
                work->field_688 = delay;
            }
            break;
    }
    SCRATCH_POP_BYTES(0x18);
}

void Actor00300_Fn02620(Task* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    s16              delta;
    s16              angle;
    s16              timer;
    s32              magnitude;
    s32              sound;
    s32              pan;
    s32              random;

    scratchEnd           = SCRATCH_HEAD(VECTOR);
    vec                  = scratchEnd - 1;
    SCRATCH_HEAD(VECTOR) = vec;
    work                 = arg0->work;
    coord                = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_686) {
        case 0:
            work->field_67C   = 0x3C;
            work->field_67A   = 0;
            work->field_66E   = 3;
            scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy           = 0;
            vec->vz           = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            angle             = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
            work->field_680   = angle;
            delta             = (u16)angle - (u16)work->field_67E;
            magnitude         = abs(delta);
            angle             = magnitude >= 0x800 ? (delta > 0 ? 0x1000 - delta : delta + 0x1000) : magnitude;
            if (angle < 0x80) {
                work->field_686 = 1;
                work->field_67C = 0;
                work->field_66E = 7;
            } else {
                timer           = (u16)work->field_688 - 1;
                work->field_688 = timer;
                if (timer <= 0) {
                    work->field_684 = 1;
                    work->field_686 = 0;
                    random          = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState     = random;
                    work->field_688 = ((u32)random >> 16) & 15;
                }
            }
            break;
        case 1:
            if ((u32)((u16)work->field_672 - 0x10) < 0x14U) {
                work->field_676 += 0x100;
            } else if ((s16)work->field_672 >= 0x33) {
                work->field_676 -= 0x100;
            }
            if ((s16)work->field_672 == 0x20) {
                work->obj5B8.flags |= 0x8000;
                sound               = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x4003000A;
                pan                 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_672 >= 0x37) {
                work->field_676     = 0;
                work->field_684     = 1;
                work->field_686     = 0;
                random              = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState         = random;
                work->field_688     = ((u32)random >> 16) & 31;
                work->obj5B8.flags &= 0x7FFF;
            }
            break;
    }
    SCRATCH_POP_BYTES(0x10);
}

void Actor00300_Fn028D0(Task* arg0)
{
    SVECTOR          sp10;
    SVECTOR          sp18;
    SVECTOR          sp20;
    Actor100300Work* work;
    GpEffWork*       effect;
    GpEffWork*       burst;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GpEnemy*         currentEnemy;
    GsCOORDINATE2*   coord;
    s16              timer;
    s16              state;
    s32              random0;
    s32              angle0;
    s32              random1;
    s32              angle1;
    s32              sound;
    s32              random2;
    s32              pan0;
    s32              pan1;
    u32              effectRandom0;
    u32              effectRandom1;

    obj   = arg0->extra;
    work  = arg0->work;
    enemy = arg0->spawnArg2;
    state = work->field_686;
    coord = obj->coords;
    switch (state) {
        case 0:
            if (Gp_State1C->eventState == 0) {
                effectRandom0 = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState   = (s32)effectRandom0;
                if (!((effectRandom0 >> 0x10) & 3)) {
                    random0     = (effectRandom0 * 5) + 0x71357911;
                    Gp_LcgState = random0;
                    angle0      = ((u32)random0 >> 0x10) & 0xF80;
                    memset(&sp20, 0, sizeof(sp20));
                    sp20.vx = (s16)((u32)(rcos(angle0) * 5) >> 5);
                    sp20.vz = (s16)((u32)(rsin(angle0) * 5) >> 5);
                    sp18    = sp20;
                    Gp_SpawnEff(D_80115728, coord, 0x20100200, &sp18);
                }
            }
            work->field_67C = 0;
            work->field_67A = 0;
            if ((s16)work->field_672 >= 0x33) {
                work->field_686 = 1;
                work->field_66E = 5;
                sp10.vy         = -0x5DC;
                sp10.vx         = 0;
                sp10.vz         = 0x320;
                effect          = Gp_SpawnEff(D_80115744, coord, 0x10014, &sp10);
                work->field_654 = effect;
                if (effect != NULL) {
                    Task_Reparent(arg0, effect->task);
                    work->field_69C = 0x13;
                }
                work->field_658 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40030009;
                pan0            = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_658, pan0, (s8)gpGetObjDepth(coord));
                return;
            }
            return;
        case 1:
            if (work->field_69C > 0) {
                timer           = (u16)work->field_69C - 1;
                work->field_69C = timer;
                if (timer <= 0) {
                    work->field_654 = NULL;
                }
            }
            if (Gp_State1C->eventState == 0) {
                effectRandom1 = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState   = (s32)effectRandom1;
                if (!((effectRandom1 >> 0x10) & 3)) {
                    random1     = (effectRandom1 * 5) + 0x71357911;
                    Gp_LcgState = random1;
                    angle1      = ((u32)random1 >> 0x10) & 0xF80;
                    memset(&sp20, 0, sizeof(sp20));
                    sp20.vx = (s16)((u32)(rcos(angle1) * 5) >> 5);
                    sp20.vz = (s16)((u32)(rsin(angle1) * 5) >> 5);
                    sp18    = sp20;
                    Gp_SpawnEff(D_80115728, coord, 0x20100200, &sp18);
                }
            }
            if ((s16)work->field_672 >= 0x13) {
                work->field_686  = 2;
                work->field_654  = NULL;
                work->field_66E  = 6;
                currentEnemy     = arg0->spawnArg2;
                currentEnemy->hp = (u16)currentEnemy->hp + 0x64;
                func_800DA6E8(&enemy->node, -0x64, 0);
                burst = Gp_SpawnEff(D_80115720, coord, 0, NULL);
                if (burst != NULL) {
                    Task_Reparent(arg0, burst->task);
                }
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4003000B;
                pan1  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan1, (s8)gpGetObjDepth(coord));
            }
            break;
        case 2:
            if ((s16)work->field_672 >= 0xF) {
                work->field_684 = 1;
                work->field_686 = 0;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random2;
                work->field_688 = ((u32)random2 >> 0x10) & 0x1F;
            }
            break;
    }
}

void Actor00300_Fn02CE8(Task* arg0)
{
    Actor100300Work* work;
    GpEffWork*       effect;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s32              state;
    s16              animation;
    s16              deathEnd;
    s16              heavyEnd;
    s16              lightEnd;
    s32              sound;
    s32              random0;
    s32              random1;
    s32              soundBase;
    s32              pan0, pan1, pan2, pan3, pan4;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    state = work->field_686;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            effect              = work->field_654;
            work->field_67A     = 0;
            work->field_67C     = 0;
            work->obj5B8.flags &= 0x7FFF;
            if (effect != NULL) {
                effect->task->state = 3;
                work->field_654     = NULL;
                work->field_69C     = 0;
                SndEvt_EnqueueType7(work->field_658, 1);
            }
            if (enemy->hp <= 0) {
                if (work->field_692 == 0) {
                    work->field_66E = 0xB;
                    deathEnd        = 0x20;
                } else {
                    work->field_66E = 0xC;
                    deathEnd        = 0x22;
                }
                work->field_688      = deathEnd;
                work->field_69A      = 1;
                enemy->reactionFlags = 0;
                work->obj4D0.pos.vz  = 0x190;
                work->field_686      = 2;
                work->obj4D0.flags  |= 0x4000;
                work->obj538.flags  &= 0xBFFF;
                return;
            }
            if ((work->field_690 >= 0x78) || (work->field_694 != 0)) {
                if (work->field_692 == 0) {
                    work->field_66E = 0xB;
                    heavyEnd        = 0x20;
                } else {
                    work->field_66E = 0xC;
                    heavyEnd        = 0x22;
                }
                work->field_688 = heavyEnd;
                work->field_686 = 2;
            } else {
                if (work->field_692 == 0) {
                    work->field_66E = 9;
                    lightEnd        = 0x35;
                } else {
                    work->field_66E = 0xA;
                    lightEnd        = 0x33;
                }
                work->field_688 = lightEnd;
                work->field_686 = 1;
                work->field_69E = 1;
            }
            soundBase = 0x40030007;
            sound     = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | soundBase;
            pan0      = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, (s32)pan0, (s8)gpGetObjDepth(coord));
            return;
        case 1:
            if ((s16)work->field_672 >= work->field_688) {
                work->field_694 = 0;
                work->field_684 = 1;
                work->field_686 = 1;
                work->field_6A0 = 0x1C2;
                work->field_69E = 0;
                random0         = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random0;
                work->field_688 = ((u32)random0 >> 0x10) & 0x1F;
            }
            animation = work->field_66E;
            if (animation == 9) {
                if ((s16)work->field_672 == 0xA) {
                    soundBase = 0x40030003;
                    sound     = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | soundBase;
                    pan1      = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan1, (s8)gpGetObjDepth(coord));
                }
            } else if (animation == 10) {
                if ((s16)work->field_672 == 0xC) {
                    soundBase = 0x40030003;
                    sound     = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | soundBase;
                    pan2      = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan2, (s8)gpGetObjDepth(coord));
                }
            }
            break;
        case 2:
            if (work->field_69A == 1) {
                work->field_69A = state;
            }
            if ((s16)work->field_672 >= work->field_688) {
                if (enemy->hp <= 0) {
                    work->field_684 = 8;
                    work->field_686 = 0;
                    arg0->state     = (s32)state;
                } else {
                    work->field_686 = 3;
                    if (work->field_66E == 0xB) {
                        work->field_66E = 0xD;
                    } else {
                        work->field_66E = 0xE;
                    }
                    work->field_69E = 1;
                }
            }
            if (work->field_66E == 0xB) {
                if ((s16)work->field_672 == 0xD) {
                    soundBase = 0x40030004;
                    sound     = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | soundBase;
                    pan3      = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan3, (s8)gpGetObjDepth(coord));
                }
            } else if (work->field_66E == 0xC) {
                if ((s16)work->field_672 == 0x12) {
                    soundBase = 0x40030004;
                    sound     = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | soundBase;
                    pan4      = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan4, (s8)gpGetObjDepth(coord));
                }
            }
            break;
        case 3:
            if ((s16)work->field_672 >= 0x2B) {
                work->field_694 = 0;
                work->field_684 = 1;
                work->field_686 = 1;
                work->field_6A0 = 0x1C2;
                work->field_69E = 0;
                random1         = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random1;
                work->field_688 = ((u32)random1 >> 0x10) & 0x1F;
            }
            break;
    }
}

void Actor00300_Fn030B8(Task* arg0)
{
    SVECTOR          sp10;
    SVECTOR          sp18;
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    s16              timer;
    s16              state;
    s32              random;
    s32              angle;
    s32              sound;
    s32              pan;
    u32              effectRandom;
    u32              nextRandom;

    work  = arg0->work;
    state = work->field_686;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            work->field_686 = 1;
            work->field_67C = 0;
            work->field_67A = 0;
            work->field_688 = 0;
            work->field_66E = 3;
            return;
        case 1:
            if (Gp_State1C->eventState == 0) {
                effectRandom = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState  = (s32)effectRandom;
                if (!((effectRandom >> 0x10) & 3)) {
                    random      = (effectRandom * 5) + 0x71357911;
                    Gp_LcgState = random;
                    angle       = ((u32)random >> 0x10) & 0xF80;
                    memset(&sp18, 0, sizeof(sp18));
                    sp18.vx = (u32)(rcos(angle) * 5) >> 5;
                    sp18.vz = (u32)(rsin(angle) * 5) >> 5;
                    sp10    = sp18;
                    Gp_SpawnEff(D_80115728, coord, 0x20103200, &sp10);
                }
            }
            timer           = (u16)work->field_688 + 1;
            work->field_688 = timer;
            if (timer >= 0x5B) {
                work->field_684 = 1;
                work->field_688 = 0;
                work->field_686 = 0;
                nextRandom      = (Gp_LcgState * 5) + 0x71357911;
                work->field_666 = (u16)(work->field_666 + 5);
                work->field_688 = (nextRandom >> 0x10) & 0x1F;
                Gp_LcgState     = (s32)nextRandom;
                sound           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4003000B;
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            }
            return;
    }
}

void Actor00300_Fn032BC(Task* arg0)
{
    Actor100300Work*  work;
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
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_680;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_67E = ang;
    if (adiff < 0x800) {
        step = work->field_67C;
        if (step >= adiff) {
            work->field_67E = want;
        } else {
            next = work->field_67E;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_67E = next;
        }
    } else {
        step = work->field_67C;
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
        work->field_67E = work->field_680;
        goto done;
    turn:
        wrapStep = work->field_67C;
        cur      = work->field_67E;
        if (diff > 0) {
            work->field_67E = cur - wrapStep;
        } else {
            work->field_67E = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_67E;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_POP_BYTES(0x18);
}

/// The scratch head is taken through `ActorScratchStack` rather than as
/// `SCRATCH_HEAD`, which does not compile the same.
void Actor00300_Fn0340C(Task* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                     = (MATRIX*)((ActorScratchStack*)G_SCRATCH_HEAD)->head - 1;
    ((ActorScratchStack*)G_SCRATCH_HEAD)->head = matrix;
    active                                     = 0;
    work                                       = arg0->work;
    coord                                      = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->field_65C, matrix);
    USE_REG(matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv((char*)matrix + 2);
    gte_rtir();
    gte_stclmv((char*)&coord[3].coord + 2);
    gte_ldclmv((char*)matrix + 4);
    gte_rtir();
    gte_stclmv((char*)&coord[3].coord + 4);
    angleX = work->field_65C.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_65C.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_65C.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_65C.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_65C.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_65C.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_664 = 0;
    }
    SCRATCH_POP_BYTES(0x20);
}

extern u8    Actor00300_D0AA18[];
extern u8    Actor00300_D0AECC[];
extern u8    Actor00300_D0B640[];
extern u8    Actor00300_D0BE44[];
extern u8    Actor00300_D0C2C4[];
extern void* D_80067704[1];

void Actor00300_Fn03618(Task* arg0)
{
    GpAreaKey    key;
    u8           areaByte0;
    u32          raw1, index1;
    GpEffWork*   effect1;
    TmdObject*   model1;
    GpAreaPlace* entry1;
    GpAreaKey*   sessionKey1;
    u32          raw2, index2;
    GpEffWork*   effect2;
    TmdObject*   model2;
    GpAreaPlace* entry2;
    GpAreaKey*   sessionKey2;
    u32          raw3, index3;
    GpEffWork*   effect3;
    TmdObject*   model3;
    GpAreaPlace* entry3;
    GpAreaKey*   sessionKey3;
    u32          raw4, index4;
    GpEffWork*   effect4;
    TmdObject*   model4;
    GpAreaPlace* entry4;
    GpAreaKey*   sessionKey4;
    u32          raw5, index5;
    GpEffWork*   effect5;
    TmdObject*   model5;
    GpAreaPlace* entry5;
    GpAreaKey*   sessionKey5;

    D_80067704[0] = Actor00300_D0AA18;
    effect1       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x200, NULL);
    if (effect1 != NULL) {
        sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
        raw1        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model1      = (TmdObject*)effect1->task->extra;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->at4.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->tpage;
        model1->clut  = entry1->clut;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }

    D_80067704[0] = Actor00300_D0AECC;
    effect2       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x200, NULL);
    if (effect2 != NULL) {
        sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
        raw2        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model2      = (TmdObject*)effect2->task->extra;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->at4.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->tpage;
        model2->clut  = entry2->clut;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    D_80067704[0] = Actor00300_D0B640;
    effect3       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x200, NULL);
    if (effect3 != NULL) {
        sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
        raw3        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model3      = (TmdObject*)effect3->task->extra;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->at4.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3        = (GpAreaPlace*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->tpage = entry3->tpage;
        model3->clut  = entry3->clut;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }

    D_80067704[0] = Actor00300_D0BE44;
    effect4       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x200, NULL);
    if (effect4 != NULL) {
        sessionKey4 = (GpAreaKey*)&gGameSession->at4.loc;
        raw4        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model4      = (TmdObject*)effect4->task->extra;
        key.stage   = sessionKey4->stage;
        key.area    = sessionKey4->area;
        key.room    = sessionKey4->room;
        areaByte0   = gGameSession->at4.loc.view;
        index4      = raw4 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry4        = (GpAreaPlace*)((index4 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model4->tpage = entry4->tpage;
        model4->clut  = entry4->clut;
        if (model4->buffer != NULL) {
            tmdProcessStream(model4);
            tmdProcessStream(model4);
        }
    }

    D_80067704[0] = Actor00300_D0C2C4;
    effect5       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x200, NULL);
    if (effect5 != NULL) {
        sessionKey5 = (GpAreaKey*)&gGameSession->at4.loc;
        raw5        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model5      = (TmdObject*)effect5->task->extra;
        key.stage   = sessionKey5->stage;
        key.area    = sessionKey5->area;
        key.room    = sessionKey5->room;
        areaByte0   = gGameSession->at4.loc.view;
        index5      = raw5 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry5        = (GpAreaPlace*)((index5 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model5->tpage = entry5->tpage;
        model5->clut  = entry5->clut;
        if (model5->buffer != NULL) {
            tmdProcessStream(model5);
            tmdProcessStream(model5);
        }
    }
}

void Actor00300_Fn03A1C(Task* arg0)
{
    Actor100300Work* work;
    GpAnimRec*       rec;
    GsCOORDINATE2*   coord;
    s32              sound;
    s32              pan;
    s32              pan2;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    rec   = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->flags & 0x20) && (work->field_696 & 0x20)) {
            sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40030001;
            pan   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        }
        if (!(rec->flags & 0x10) && (work->field_696 & 0x10)) {
            sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40030002;
            pan2  = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan2, (s8)gpGetObjDepth(coord));
        }
        work->field_696 = (u16)(rec->flags & 0x30);
    }
}

void Actor00300_Fn03B70(GpEnemy* arg0, Task* arg1)
{
    Actor100300Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           vec;
    s32              mode;
    s32              sound;
    s32              pan;
    s16              phase;

    obj   = arg1->extra;
    work  = arg1->work;
    mode  = Gp_StateF0.field_4;
    coord = obj->coords;
    if (mode == 1)
        goto case1;
    if (mode < 2)
        goto common;
    if (mode == 2)
        goto case2;
    goto common;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
    return;
case2:
    obj->flags                                  = 0x80;
    ((TmdObject*)work->field_43C->extra)->flags = 0x80;
    return;
common:
    switch (work->field_686) {
        case 0:
            work->field_674 = 0x1000;
            work->field_608 = coord->coord;
            arg0->recs      = NULL;
            Gp_UnlinkNode(&arg0->node.next);
            Gp_UnlinkObj(&work->obj480);
            Gp_UnlinkObj(&work->obj538);
            Gp_UnlinkObj(&work->obj4D0);
            Gp_UnlinkObj(&work->obj5B8);
            Gp_SetLightMode(arg0, 1);
            Gp_ReleaseStateF0Add(arg1, 3);
            work->field_688 = 0;
            work->field_686 = 1;
            if (work->field_682 != 0) {
                obj->flags      = 0x80;
                work->field_686 = 3;
            }
            c      = ((TmdObject*)arg1->extra)->coords;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
            if (work->field_654 != NULL) {
                work->field_654->task->state = 3;
                work->field_654              = NULL;
                work->field_69C              = 0;
                SndEvt_EnqueueType7(work->field_658, 1);
            }
            sound = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x40030008;
            pan   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            return;
        case 1:
            if (work->field_674 >= 0x201)
                work->field_674 -= 0x50;
            Actor00300_Fn0505C(arg1, &work->field_608, work->field_674);
            phase           = work->field_688 + 1;
            work->field_688 = phase;
            if (phase == 10)
                obj->flags |= 2;
            if (work->field_688 == 15)
                Gp_SpawnEff(0x600A5, &((TmdObject*)arg1->extra)->coords[3], 3, NULL);
            if (work->field_688 >= 0x3C)
                work->field_686 = 2;
            c      = ((TmdObject*)arg1->extra)->coords;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
            return;
        case 2:
            Gp_DestroyEnemy(arg0, arg1);
            return;
        case 3:
            if (work->field_682 != 0) {
                if (work->field_682 >= 2) {
                    work->field_682 = 0;
                    Tmd_FreeBuffers(obj);
                    obj->flags |= 4;
                    Actor00300_Fn03618(arg1);
                } else
                    work->field_682++;
            }
            phase           = work->field_688 + 1;
            work->field_688 = phase;
            if (phase >= 0x3C)
                work->field_686 = 2;
            return;
    }
}

static __inline__ void Actor00300_UpdateTransform(GpEnemy* arg0, Task* arg1)
{
    TmdObject*         obj;
    TmdObject*         original;
    GsCOORDINATE2*     saved;
    Actor100300Work*   work;
    s32                disabled;
    s16                flags;
    s16                scale;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    GsCOORDINATE2*     coord;

    original = arg1->extra;
    disabled = Gp_StateF0.field_4;
    USE_REG2(original, original);
    __asm__ volatile("" : "=r"(obj) : "0"(original));
    saved = obj->coords;
    work  = arg1->parent->work;
    if (disabled == 0) {
        if (gGameSession->eventState != 0) {
            flags      = ((work->field_678 & 1) == 0) << 7;
            obj->flags = flags;
            if (work->field_678 & 2) {
                obj->flags = flags | 4;
            }
        }
        scale = work->field_676;
        if (scale <= 0) {
            obj->flags = 0x80;
            return;
        }
        head                            = SCRATCH_HEAD(ActorScaleScratch);
        scratch                         = head - 1;
        coord                           = ((TmdObject*)arg1->extra)->coords;
        SCRATCH_HEAD(ActorScaleScratch) = scratch;
        scratch->scale.vx               = 0x1000;
        scratch->scale.vy               = scale;
        scratch->scale.vz               = 0x1000;
        coord->coord                    = work->field_628;
        scratch->mat.ident.m00_m01      = 0x1000;
        scratch->mat.ident.m02_m10      = 0;
        scratch->mat.ident.m11_m12      = 0x1000;
        scratch->mat.ident.m20_m21      = 0;
        scratch->mat.ident.m22          = 0x1000;
        ScaleMatrix(&scratch->mat.mat, &scratch->scale);
        MulMatrix(&coord->coord, &scratch->mat.mat);
        coord->flg = 0;
        saved->flg = 0;
        SCRATCH_POP(ActorScaleScratch);
    }
}

void Actor00300_Fn03F40(GpEnemy* arg0, Task* arg1)
{
    Actor00300_UpdateTransform(arg0, arg1);
}

void Actor00300_Fn040A4(GpEnemy* arg0, Task* arg1)
{
    Actor100300Work*    parentWork;
    Task*               parent;
    Actor00300InitWork* work;
    ActorOffsetScratch* scratch;
    ActorOffsetScratch* head;
    SVECTOR*            offset;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      parentCoord;
    GsCOORDINATE2*      objCoord;
    GsCOORDINATE2*      objCoord2;

    head                             = SCRATCH_HEAD(ActorOffsetScratch);
    scratch                          = head - 1;
    SCRATCH_HEAD(ActorOffsetScratch) = scratch;
    offset                           = &scratch->offset;
    parent                           = arg1->parent;
    coord                            = ((TmdObject*)arg1->extra)->coords;
    parentCoord                      = ((TmdObject*)parent->extra)->coords;
    parentWork                       = (Actor100300Work*)parent->work;
    work                             = memCalloc(0x8C, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work         = (TaskIdMap*)work;
    scratch->offset.vx = 0;
    scratch->offset.vy = -0x5DC;
    scratch->offset.vz = 0x320;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&scratch->result);
    coord->sub          = &gGfxViewCoord;
    coord->coord        = parentCoord->coord;
    coord->coord.t[0]   = parentCoord->coord.t[0] + scratch->result.vx;
    coord->coord.t[1]   = parentCoord->coord.t[1] + scratch->result.vy;
    coord->coord.t[2]   = parentCoord->coord.t[2] + scratch->result.vz;
    coord->flg          = 0;
    objCoord            = ((TmdObject*)arg1->extra)->coords;
    work->obj0.ctx.recs = &work->rec20;
    work->obj0.pos.vx   = 0;
    work->obj0.pos.vy   = 0;
    work->obj0.pos.vz   = 0;
    work->obj0.coord    = objCoord;
    work->obj0.key      = Gp_PackPair(&Actor00300_D15FD8, parentWork->field_66A);
    work->obj0.radius   = 0x1C2;
    work->obj0.flags    = 1;
    Gp_LinkObj(3, &work->obj0);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->pose.end1.vz    = -0x1A4;
    work->pose.end0Radius = 1;
    work->pose.end1Radius = 1;
    work->pose.end0.vx    = 0;
    work->pose.end0.vy    = 0;
    work->pose.end0.vz    = 0;
    work->pose.end1.vx    = 0;
    work->pose.end1.vy    = 0;
    work->pose.recs       = &work->rec70;
    work->obj0.flags     |= 0x8000;
    objCoord2             = ((TmdObject*)arg1->extra)->coords;
    work->obj38.ctx.d4rec = &work->pose;
    work->obj38.pos.vx    = 0;
    work->obj38.pos.vy    = 0;
    work->obj38.pos.vz    = 0;
    work->obj38.key       = 0;
    work->obj38.radius    = 0;
    work->obj38.flags     = 3;
    work->obj38.coord     = objCoord2;
    Gp_LinkObj(3, &work->obj38);
    Gp_InitRec18Table(&work->rec70, 1, 0);
    work->timer        = 0x1E;
    work->obj38.flags |= 0x4400;
    Task_DetachFromParent(arg1);
    arg1->state = 1;
    SCRATCH_POP(ActorOffsetScratch);
}

void Actor00300_Fn04370(GpEnemy* arg0, Task* arg1)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    s32              id;
    s32              expired;
    s16              timer;

    coord   = ((TmdObject*)arg1->extra)->coords;
    work    = arg1->work;
    expired = 0;
    switch (Gp_StateF0.field_4) {
        case 1:
            Actor00300_Fn00078(coord, 0x200);
            return;
        case 0:
        default:
            coord->flg         = 0;
            coord->coord.t[0] += (coord->coord.m[0][2] * 0x19) >> 8;
            coord->coord.t[2] += (coord->coord.m[2][2] * 0x19) >> 8;
            Gp_UpdateCoord(coord);
            Actor00300_Fn00078(coord, 0x200);
            id = work->field_70.key;
            if (id != 0 && Gp_RoomParamTables[gGameSession->at4.loc.stage - 1]
                                             [gGameSession->at4.loc.area - 1][func_800E1B24(id)]
                                                 ->field_1 == 0) {
                expired = 1;
            }
            Gp_ClearRec18Occupied(&work->field_70);
            Actor00300_Fn04528(arg1);
            timer          = work->field_88 - 1;
            work->field_88 = timer;
            if (timer <= 0 || (work->field_20 & 1) || expired != 0) {
                Gp_SpawnEff(D_8011573C, coord, 0, NULL);
                arg1->state    = 2;
                work->field_8A = 0;
            }
        case 2:
            return;
    }
}

void Actor00300_Fn04528(Task* arg0)
{
    s32               want;
    GsCOORDINATE2*    coord;
    ActorFaceScratch* sc;
    s16               cur;
    s32               ang;
    s32               current;
    s16               diff;
    s32               adiff;
    s16               turn;
    s16               wrap;

    coord        = ((TmdObject*)arg0->extra)->coords;
    sc           = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    sc->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = 0;
    sc->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    want         = ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF;
    ang          = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    cur          = ang;
    diff         = want - ang;
    adiff        = diff >= 0 ? diff : -diff;
    turn         = diff;
    if (adiff < 0xD) {
        cur = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = cur;
        cur     = current + 0xC;
        if (turn <= 0) {
            cur = current - 0xC;
        }
    }
    sc->rot.vx = 0;
    sc->rot.vy = cur;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_POP_BYTES(0x18);
}

/// Unless an event is running, draws from the gameplay LCG and on one call in
/// four spawns effect `D_80115728` on `arg0` with a random horizontal vector;
/// `arg1` is or-ed into the spawn flags.
void Actor00300_Fn04664(GsCOORDINATE2* arg0, s32 arg1)
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

void Actor00300_Fn04770(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor00300_D00004;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2), arg0);
}

void Actor00300_Fn047CC(GpEnemy* arg0, Task* arg1)
{
    Actor100300Work* work;

    work = arg1->work;
    switch (Gp_StateF0.field_4) {
        case 0:
            ((TmdObject*)arg1->extra)->flags            = 0;
            ((TmdObject*)work->field_43C->extra)->flags = 0;
            arg0->node.state.b.flags                    = work->field_698 != 0;
            break;
        case 1:
            Actor00300_Fn04FB0(arg1);
            Actor00300_Fn05008(arg1);
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags            = 0x80;
            ((TmdObject*)work->field_43C->extra)->flags = 0x80;
            arg0->node.state.b.flags                    = 1;
            return;
    }
    if (gGameSession->eventState != 0) {
        Actor00300_Fn048D4(arg0, arg1);
        return;
    }
    Actor00300_Fn04958(arg0, arg1);
}

void Actor00300_Fn048D4(GpEnemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    Actor100300Work* work;
    s16              flags;

    work = arg1->work;
    obj  = arg1->extra;
    if (gGameSession->eventState != 0) {
        flags      = ((work->field_678 & 1) == 0) << 7;
        obj->flags = flags;
        if (work->field_678 & 2) {
            obj->flags = flags | 4;
        }
    }
    Actor00300_Fn04ED4(arg1);
    Actor00300_Fn04FB0(arg1);
    Actor00300_Fn05008(arg1);
}

void Actor00300_Fn04958(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*   coord;
    Actor100300Work* work;

    work  = arg1->work;
    coord = ((TmdObject*)arg1->extra)->coords;
    if (work->field_648 != 0) {
        if (arg0->reactionFlags != 0) {
            Actor00300_Fn04A2C(arg1);
        }
        Actor00300_Fn00E54(arg1);
        Actor00300_Fn04C20(arg1);
        if (work->field_67C != 0) {
            Actor00300_Fn032BC(arg1);
        }
        Actor00300_Fn04E30(arg1);
        Actor00300_Fn04ED4(arg1);
        if (work->field_664 != 0) {
            Actor00300_Fn0340C(arg1);
        }
        Actor00300_Fn03A1C(arg1);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        Actor00300_Fn04FB0(arg1);
        Actor00300_Fn05008(arg1);
    }
}

void Actor00300_Fn04A2C(Task* arg0)
{
    Actor100300Work* work;
    GpEnemy*         enemy;
    s16              damage;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & 1) {
        enemy->reactionFlags = flags & 0xFE;
    }
    if ((enemy->reactionFlags & 2) && (work->field_684 != 5)) {
        work->field_684 = 6;
        work->field_686 = 0;
    }
    if (enemy->reactionFlags & 0xC) {
        damage          = Gp_TickObjFlag4(enemy);
        work->field_690 = damage;
        if (damage != 0) {
            func_800DA6E8(&enemy->node, (s32)damage, 0);
            enemy->hp       = (u16)enemy->hp - (u16)work->field_690;
            work->field_684 = 5;
            work->field_686 = 0;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }
}

/// Tests the segment from `arg0` to `arg1` against every collision node in
/// `D_80115550` flagged 0x40, stopping at the first node that reports a hit
/// (1); returns the last node's result, 0 when none was tested.
s32 Actor00300_Fn04B14(SVECTOR* arg0, SVECTOR* arg1)
{
    void**   scratch;
    u8*      head;
    VECTOR*  vec;
    GpObj3A* node;
    s32      ret;

    ret                          = 0;
    scratch                      = SCRATCH_HEAD_ADDR;
    node                         = D_80115550;
    head                         = SCRATCH_HEAD_AT(scratch, void);
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    TOUCH_REG_USE(head, node);
    vec                            = (VECTOR*)head;
    vec->vy                        = arg1->vy - arg0->vy;
    SCRATCH_HEAD_AT(scratch, void) = vec;
    vec->vz                        = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    SCRATCH_POP_BYTES(0x10);
    return ret;
}

/// State handlers of the task `Actor00300_Fn05138` dispatches, indexed by
/// `Task::state`: a setup that attaches the coordinate to the parent's and
/// moves to state 1, an empty state, and `Gp_DestroyEnemy`.
const GpEnemyTaskFuncTable3 Actor00300_D0003C = {
    {
        Actor00300_Fn05194,
        Actor00300_Fn03F40,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the task `Actor00300_Fn0521C` dispatches, indexed by
/// `Task::state`. The first sets the task up and moves it to state 1, the
/// second moves it on to state 2, and the third destroys it once its timer
/// has run out.
const GpEnemyTaskFuncTable3 Actor00300_D00048 = {
    {
        Actor00300_Fn040A4,
        Actor00300_Fn04370,
        Actor00300_Fn05278,
    },
};

void Actor00300_Fn04C20(Task* arg0)
{
    Actor100300Work* work;

    work = arg0->work;
    switch (work->field_684) {
        case 0:
            Actor00300_Fn01678(arg0);
            break;
        case 1:
            Actor00300_Fn019C0(arg0);
            break;
        case 2:
            Actor00300_Fn01F9C(arg0);
            break;
        case 3:
            Actor00300_Fn02620(arg0);
            break;
        case 4:
            Actor00300_Fn028D0(arg0);
            break;
        case 5:
            Actor00300_Fn02CE8(arg0);
            break;
        case 6:
            Actor00300_Fn04D28(arg0);
            break;
        case 7:
            Actor00300_Fn030B8(arg0);
            break;
        case 8:
            break;
    }

    if (work->field_684 != 3) {
        if (work->field_676 != 0) {
            if (work->field_676 > 0) {
                work->field_676 -= 0x100;
            } else if (work->field_676 < 0) {
                work->field_676 = 0;
            }
        }
    }
}

void Actor00300_Fn04D28(Task* arg0)
{
    Actor100300Work* work;
    GpEnemy*         enemy;
    s32              state;
    s32              value;
    GpEffWork*       effect;

    work  = arg0->work;
    state = work->field_686;
    switch (state) {
        case 0:
            effect          = work->field_654;
            work->field_67A = 0;
            work->field_66E = 0xF;
            if (effect != NULL) {
                effect->task->state = 3;
                work->field_654     = NULL;
                work->field_69C     = 0;
                SndEvt_EnqueueType7(work->field_658, 1);
            }
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                enemy                 = arg0->spawnArg2;
                enemy->reactionFlags &= 0xFD;
                work->field_66E       = 0x12;
                work->field_686       = 1;
            }
            break;
        case 1:
            if ((s16)work->field_672 >= 0xB) {
                work->field_684 = state;
                work->field_686 = state;
                value           = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = value;
                work->field_688 = ((u32)value >> 16) & 0x1F;
            }
            break;
    }
}

void Actor00300_Fn04E30(Task* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;

    coord              = ((TmdObject*)arg0->extra)->coords;
    work               = arg0->work;
    work->field_5F8    = coord->coord.t[0];
    work->field_5FC    = coord->coord.t[1];
    work->field_600    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_67A) >> 12;
    if (work->field_69A < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_67A) >> 12;
}

void Actor00300_Fn04ED4(Task* arg0)
{
    Actor100300Work* work;
    s32              i;
    s32              val;

    work = arg0->work;
    i    = 1;
    if (work->field_66E != work->field_670) {
        TOUCH_REG(i);
        work->field_670 = work->field_66E;
        work->field_672 = 0;
        if (work->field_69E == 0) {
            val = Actor00300_D16394[work->field_66E];
        } else {
            val = 8;
            i   = 1;
        }
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_66E, 0, val);
            i++;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        work->field_672 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}

void Actor00300_Fn04FB0(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

void Actor00300_Fn05008(Task* arg0)
{
    VECTOR3        vec;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* part;

    coord  = ((TmdObject*)arg0->extra)->coords;
    part   = coord + 3;
    vec.vx = part->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, 0x80);
}

void Actor00300_Fn0505C(Task* arg0, MATRIX* arg1, s16 arg2)
{
    GsCOORDINATE2*     coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;

    head                            = SCRATCH_HEAD(ActorScaleScratch);
    scratch                         = head - 1;
    SCRATCH_HEAD(ActorScaleScratch) = scratch;
    coord                           = ((TmdObject*)arg0->extra)->coords;
    scratch->scale.vx               = 0x1000;
    scratch->scale.vy               = arg2;
    scratch->scale.vz               = 0x1000;
    coord->coord                    = *arg1;
    scratch->mat.ident.m00_m01      = 0x1000;
    scratch->mat.ident.m02_m10      = 0;
    scratch->mat.ident.m11_m12      = 0x1000;
    scratch->mat.ident.m20_m21      = 0;
    scratch->mat.ident.m22          = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    SCRATCH_POP(ActorScaleScratch);
}

void Actor00300_Fn05138(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor00300_D0003C;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2), arg0);
}

void Actor00300_Fn05194(GpEnemy* arg0, Task* arg1)
{
    Task*            parent;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoord;
    Actor100300Work* work;

    parent          = arg1->parent;
    obj             = arg1->extra;
    parentCoord     = ((TmdObject*)parent->extra)->coords;
    coord           = obj->coords;
    work            = parent->work;
    obj->flags      = 0;
    coord->flg      = 0;
    coord->sub      = parentCoord + 7;
    obj->lightMtx   = (MATRIX*)work->field_460;
    obj->colorMtx   = (MATRIX*)work->field_440;
    work->field_628 = coord->coord;
    work->field_676 = 0;
    arg1->state     = 1;
}

void Actor00300_Fn0521C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor00300_D00048;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2), arg0);
}

void Actor00300_Fn05278(GpEnemy* arg0, Task* arg1)
{
    Actor100300Work* work;
    u16              timer;

    work = (Actor100300Work*)arg1->work;
    switch (work->field_8A) {
        case 0:
            Gp_UnlinkObj(&work->obj0);
            Gp_UnlinkObj(&work->obj38);
            work->field_88 = 0x3C;
            work->field_8A = 1;
            return;
        case 1:
            timer          = work->field_88 - 1;
            work->field_88 = timer;
            if ((s16)timer <= 0) {
                Gp_DestroyEnemy(arg0, arg1);
            }
            return;
    }
}

s32 Actor00300_Fn05304(Task* arg0, s32 arg1, GpAnimArg* args)
{
    Actor100300Work* work;
    s32              i;
    s32              frames;
    s16              anim;

    work            = arg0->work;
    anim            = args->field_4 + 0x13;
    work->field_66E = anim;
    work->field_670 = anim;
    frames          = 0;
    if (args->field_8 != 0) {
        frames = args->field_C;
    }
    for (i = 1; i < 0x13; i++) {
        func_800B4114((GpAnimCtx*)work, i, work->field_66E, 0, frames);
    }
    return 0;
}

s32 Actor00300_Fn05388(Task* arg0, s32 arg1, GpXformArg* args)
{
    GsCOORDINATE2* coord = ((TmdObject*)arg0->extra)->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 Actor00300_Fn053EC(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor100300Work* work;

    obj  = arg0->extra;
    work = arg0->work;
    if (!(arg2 & 1)) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    work->field_678 = arg2;
    return 0;
}

s32 Actor00300_Fn05434(Task* arg0, s32 arg1, GpCmdArg* args)
{
    Actor100300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (args->command != 0) {
        enemy->recs = 0;
        Gp_UnlinkNode(&enemy->node);
        Gp_UnlinkObj(&work->obj480);
        Gp_UnlinkObj(&work->obj538);
        Gp_UnlinkObj(&work->obj4D0);
        Gp_UnlinkObj(&work->obj5B8);
        Gp_DestroyEnemy(enemy, arg0);
    }
    return 0;
}
