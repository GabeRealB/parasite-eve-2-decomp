#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_100300.h"
#include "actors/actor_105100.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"

#include "main/task.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105100_80131E24;

void func_8017FC40(GsCOORDINATE2* arg0, s32 arg1, u16 arg2);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8018294C(Actor105100* arg0);
void func_actor_105100_80132C2C(Actor105100* arg0);
void func_actor_105100_80133134(Actor105100* arg0);
void func_actor_105100_8013329C(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_8013345C(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_801336B8(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_80133A14(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_80133CE4(Actor105100* arg0);
void func_actor_105100_80134130(Actor105100* arg0);
void func_actor_105100_80134284(Actor105100Ctx* arg0, Actor105100* arg1);
void func_actor_105100_80135674(Actor105100* arg0);
void func_actor_105100_801359B4(Actor105100* arg0);
void func_actor_105100_80135B40(Actor105100* arg0);
void func_actor_105100_80135E54(Actor105100* arg0);
void func_actor_105100_80135F50(Actor105100* arg0);
void func_actor_105100_80135FCC(Actor105100* arg0);
void func_actor_105100_801360AC(Actor105100* arg0);
void func_actor_105100_801361C4(Actor105100* arg0);
void func_actor_105100_80136318(Actor105100* arg0);
void func_actor_105100_80136408(Actor105100* arg0);
void func_actor_105100_80136524(Actor105100* arg0);

void func_800B4114(Actor105100Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

/// Main-executable globals with no module header yet: a `D_80114C12` of 1 or a
/// live `D_80071075` means a cutscene is already up, so the death handler skips
/// message 0x13F4.
extern u8 D_80071075;
extern s8 D_80114C12;

/// Main-executable global with no module header yet: bit 2 asks the per-frame
/// handler for the post-hit reaction, which is why `func_actor_105100_80133134`
/// runs `func_actor_105100_80135FCC` off it on every frame it is set.
extern u8 D_8011540D;

/// Main-executable global with no module header yet: the remaining-enemy count
/// `func_actor_105100_80136318` tests to decide whether the fight is over.
extern s16 D_80073BA0;

/// The run of HP caps at 0x8014139C; `func_actor_105100_80135FCC` reads the
/// first entry. Declared as an aggregate on purpose: a bare `extern u16` makes
/// `true_dependence` (`sched.c:846`) drop the dependence between the store to
/// `Actor105100Ctx::field_40` and this load -- the store is in-struct with a
/// varying address, this load a scalar MEM at a fixed one -- and sched2 then
/// hoists this load above the store, ahead of the `sll`.
extern u16 D_actor_105100_8014139C[1];

/// The s16 animation-id run at 0x801414C8, one entry per work state at
/// `Actor105100Work::field_58E`; `func_actor_105100_80136408` reads the entry
/// the new state selects before it re-queues every slot.
extern s16 D_actor_105100_801414C8[];

/// The spawn's pair tables. `Gp_PackPair` packs the `GpU16Pair` at 0x80141380
/// into the work's third list node (`Actor105100Work::obj4E4.key`), and the
/// `GpPairSrcE` at 0x80141398 is the pair source the context points at with
/// `Actor105100Ctx::field_50` -- its `hpMax` seeds the enemy's HP.
extern GpU16Pair  D_actor_105100_80141380;
extern GpPairSrcE D_actor_105100_80141398;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

/// Animation block the attack body hands the player with message 0x3F4.
extern void* D_actor_105100_801414B4;

/// The approach points the `field_40 == 1` reaction walks the model through,
/// indexed by `Actor105100Rec::field_44`. Only the x and z halves are read: the
/// reaction subtracts the model's current position and walks the resulting
/// planar delta.
extern SVECTOR D_actor_105100_80141418[6];

extern SVECTOR D_actor_105100_801413E8[];
extern s16     D_actor_105100_80141448[];
extern s16     D_actor_105100_80141450[];

/// Projects `coord` onto two `POLY_FT4` billboards, seeds `D_80114FF8` as a
/// point light at that position, and traces the ground for the ground-quad
/// helper when `Gp_State1C->groundTrace` is set.
void func_actor_105100_80131EBC(GsCOORDINATE2* coord, s16 size)
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
    GpObj44*       light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    D_80114FF8.mode           = 2;
    light                     = &D_80114FF8.data.light;
    light->field_58           = 0x300;
    light->field_5C           = 0x3000;
    random                    = (Gp_LcgState * 5) + 0x71357911;
    intensity                 = ((random >> 0x10) & 0x700) + 0x800;
    light->field_50           = intensity;
    shifted                   = intensity << 0x10;
    light->field_52           = (s16)(shifted >> 0x11);
    light->field_54           = (s16)(shifted >> 0x12);
    light->field_18.vx        = (s32)coord->coord.t[0];
    light->field_18.vy        = (s32)coord->coord.t[1];
    light->field_18.vz        = coord->coord.t[2];
    D_80114FF8.data.coord.flg = 0;
    scratch                   = (void**)G_SCRATCH_HEAD;
    block                     = (GpRingScratch*)*scratch - 1;
    block->vec.vx             = *(u16*)&coord->workm.t[0];
    alias                     = block;
    vy                        = *(u16*)&coord->workm.t[1];
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
                func_actor_105100_80132414(&ground, (s32)(s16)(outerSize * 2));
            }
        }
    }
    *(void**)G_SCRATCH_HEAD =
        (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GpRingScratch);
}
