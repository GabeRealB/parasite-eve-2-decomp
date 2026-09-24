#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/wipsys.h"
#include "rooms/mine_cavern.h"
#include "rooms/room_common.h"

extern void func_mine_cavern_80181864(void);
extern void func_mine_cavern_80182184(void);
void        func_mine_cavern_801825C8(s16 arg0);
extern void func_mine_cavern_80182454(void);

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern u32 Gp_LcgState;

/// Current screen id at 0x8007218B.
extern s8 D_8007218B;

/// The mine task's three state handlers, dispatched through by state. Copied
/// onto the stack by `func_mine_cavern_80182DC8` before the call, the way every
/// other room drives its own task family.
extern const TaskFuncTable3 D_mine_cavern_8017D65C;

/// Sound emitter positions for the cavern's four ambient loops, indexed by the
/// emitter id `func_mine_cavern_801825C8` and its siblings are called with.
extern SVECTOR D_mine_cavern_8018E39C[4];

/// The shared light records in main BSS; the cavern owns one per emitter point.
extern AhlpLight D_801150C0[];

/// Parameters `func_mine_cavern_80181CAC` writes into a cavern light record.
/// `D_mine_cavern_8018E368` is the base the LCG draw is added to.
extern u16 D_mine_cavern_8018E360;
extern u16 D_mine_cavern_8018E362;
extern u16 D_mine_cavern_8018E364;
extern u16 D_mine_cavern_8018E366;
extern u16 D_mine_cavern_8018E368;

/// The four spots the cavern's enemy can be parked at, indexed by the low half
/// of `Task::spawnArg1` (the spawn table `D_mine_cavern_8018EB38` packs the
/// slot there, so a 32-bit read would index past the end).
extern SVECTOR D_mine_cavern_8018EB18[4];

/// The cavern enemy's five state handlers, dispatched through by state.
extern GpEnemyTaskFuncTable5 D_mine_cavern_8017D7F8;

/// Enemy spawn table the cavern's ambush draws from, on the `GameFlag_GetNibble(0xE2)`
/// bits.
extern TaskDesc D_mine_cavern_8018EB38;

/// Parameter record of the cavern enemy's kind.
extern GpPairSrcE D_mine_cavern_8018EAE4;

void func_mine_cavern_80183860(Task* arg0);

/// Colour of the glow fan's centre vertex, one channel per symbol.
///
/// Each channel is its own symbol, reloaded on every use, and is declared as an
/// array because the fan's position stores are only ordered against loads from
/// aggregate memory: the scheduler treats a halfword store into the primitive
/// and a load from a plain scalar global as independent, but not a load from an
/// array element.
extern u8 D_mine_cavern_8018E358[];
extern u8 D_mine_cavern_8018E359[];
extern u8 D_mine_cavern_8018E35A[];

/// Colour of the glow fan's two rim vertices, declared as the centre colour is.
extern u8 D_mine_cavern_8018E35B[];
extern u8 D_mine_cavern_8018E35C[];
extern u8 D_mine_cavern_8018E35D[];

/// Colour of the point glow fans' centre vertex, one channel per symbol and
/// declared as arrays for the same reason as the cavern glow's colours.
extern u8 D_mine_cavern_8018E350[];
extern u8 D_mine_cavern_8018E351[];
extern u8 D_mine_cavern_8018E352[];

/// Colour of the point glow fans' two rim vertices.
extern u8 D_mine_cavern_8018E353[];
extern u8 D_mine_cavern_8018E354[];
extern u8 D_mine_cavern_8018E355[];

/// The six points `func_mine_cavern_80181864` draws a glow at.
extern SVECTOR D_mine_cavern_8018E36C[6];

/// For each of the four emitter points, the views it spawns its effect in: up
/// to eight view indices, ended early by a zero.
extern u8 D_mine_cavern_8018E3BC[4][8];

/// The view index `func_mine_cavern_80182184` saw on its previous run.
extern s16 D_mine_cavern_8018E3DC;

/// The `GameFlag_GetNibble(0xE2)` emitter set `func_mine_cavern_80182184` saw
/// on its previous run.
extern s32 D_mine_cavern_8018EB58;

/// Tick counter `func_mine_cavern_80182184` advances while `D_801153F4` is
/// clear; the emitters spawn on every ninth tick.
extern u16 D_mine_cavern_8018EB5C;

/// Mode byte the cavern enemy's hit check switches on: 1 skips the check and 2
/// hides the model and skips it. Its wider role is unproven.
extern u8 D_801153F4;

/// Damage the cavern enemy takes from a contact, indexed by the low seven bits
/// of the contact's key.
extern u8 D_mine_cavern_8018EAF4[];

/// Scratch block the radius test squares its operands in.
typedef struct _MineCavernRangeScratch {
    s32 dx;
    s32 dz;
    s32 r;
} _MineCavernRangeScratch;

/// Scratch block the enemy's hit check works in: the model's world position
/// (then the offset to the player's model), the offset to the player or to a
/// contact, and the values derived from that contact.
typedef struct _MineCavernHitScratch {
    VECTOR3 pos;
    s32     pad_C;
    SVECTOR d;
    s32     dist;
    u32     key;
    s32     bits;
    s16     angle;
    s16     damage;
} _MineCavernHitScratch;

/// Non-zero when the XZ offset `d` lies outside radius `r`; squares in a
/// scratch block.
static __inline__ s32 _mineCavernOutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    _MineCavernRangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((_MineCavernRangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (_MineCavernRangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((_MineCavernRangeScratch*)(head - 0xC))->dx *= ((_MineCavernRangeScratch*)(head - 0xC))->dx;
    *(_MineCavernRangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((_MineCavernRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Draws a glow at each of the six points of `D_mine_cavern_8018E36C`, the
/// fourth skipped while view 4 is active: per point, a fan of eight
/// semi-transparent Gouraud triangles around its projected position, each
/// followed by a drawing-mode packet, both linked at the point's depth. The
/// radius is scaled by depth and jittered by the shared LCG, and its base
/// shrinks as more `GameFlag_GetNibble(0xE2)` bits are set. A point whose
/// projection flags an error is skipped.
void func_mine_cavern_80181864(void)
{
    s32       sxy;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    s32       radius;
    s32       i;
    s32       flags;
    u8        count;
    s32       j;
    s32       base;
    u16       view;
    s32       size;
    s32       shift;
    u16       x;
    u16       y;

    flags = GameFlag_GetNibble(0xE2);
    view  = Gp_GetViewIndex() & 0xFF;
    count = 0;
    for (j = 0; j < 4; j++) {
        if ((flags >> j) & 1) {
            count++;
        }
    }
    switch (count) {
        case 0:
        case 1:
            base = 0x428;
            break;
        case 2:
            base = 0x3C0;
            break;
        case 3:
            base = 0xC8;
            break;
        case 4:
        default:
            base = 0x80;
            break;
    }
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    size = base;
    for (j = 0; j < 6; j++) {
        shift = 12; // fraction bits of rsin/rcos
        if (j == 3 && view == 4) {
            continue;
        }
        gte_ldv0(&D_mine_cavern_8018E36C[j]);
        gte_rtps_real();
        gte_stsxy(&sxy);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        if (flag < 0) {
            continue;
        }
        x           = sxy;
        y           = sxy >> 16;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        radius      = (s32)(size + ((Gp_LcgState >> 16) & 0xF)) * 0x160 / (otz * 4);
        for (i = 0; i < 8; i++) {
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = (POLY_GT3*)prim + 1;
            setPolyG3(prim);
            prim->r0 = D_mine_cavern_8018E350[0];
            prim->g0 = D_mine_cavern_8018E351[0];
            prim->b0 = D_mine_cavern_8018E352[0];
            prim->x0 = x;
            prim->y0 = y;
            prim->r1 = D_mine_cavern_8018E353[0];
            prim->g1 = D_mine_cavern_8018E354[0];
            prim->b1 = D_mine_cavern_8018E355[0];
            prim->r2 = D_mine_cavern_8018E353[0];
            prim->g2 = D_mine_cavern_8018E354[0];
            prim->b2 = D_mine_cavern_8018E355[0];
            setSemiTrans(prim, 1);
            prim->x1 = x + ((rsin(i << 9) * radius) >> shift);
            prim->y1 = y + ((rcos(i << 9) * radius) >> shift);
            prim->x2 = x + ((rsin(i * 0x200 + 0x200) * radius) >> shift);
            prim->y2 = y + ((rcos(i * 0x200 + 0x200) * radius) >> shift);
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = (DR_MODE*)dr + 1;
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[otz >> 4], dr);
        }
    }
}

/// Switches on the light record for cavern point `point`: fills it from the
/// cavern's light parameters and the point's position in
/// `D_mine_cavern_8018E39C`, with `field_5C` jittered by a draw from the shared
/// LCG.
void func_mine_cavern_80181CAC(s16 point)
{
    AhlpLight*     light = &D_801150C0[point];
    AhlpLightWork* work  = &light->work;

    light->state        = 2;
    work->field_58      = D_mine_cavern_8018E366;
    work->field_5C      = D_mine_cavern_8018E368 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x7FF);
    work->field_50      = D_mine_cavern_8018E360;
    work->field_52      = D_mine_cavern_8018E362;
    work->field_54      = D_mine_cavern_8018E364;
    work->x             = D_mine_cavern_8018E39C[point].vx;
    work->y             = D_mine_cavern_8018E39C[point].vy;
    work->z             = D_mine_cavern_8018E39C[point].vz;
    light->work.field_0 = 0;
}

/// Draws a glow at cavern point `point` of `D_mine_cavern_8018E39C`: a fan of
/// eight semi-transparent Gouraud triangles around the point's projected
/// position, each followed by a drawing-mode packet, both linked at the point's
/// depth. The radius is scaled by depth and jittered by the shared LCG, and its
/// base shrinks as more `GameFlag_GetNibble(0xE2)` bits are set. Nothing is
/// drawn when the projection flags an error.
void func_mine_cavern_80181D80(s16 point)
{
    s32       sxy;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    s32       radius;
    s32       i;
    s32       flags;
    u8        count;
    s32       j;
    s32       base;
    u16       x;
    u16       y;

    flags = GameFlag_GetNibble(0xE2);
    count = 0;
    for (j = 0; j < 4; j++) {
        if ((flags >> j) & 1) {
            count++;
        }
    }
    switch (count) {
        case 0:
        case 1:
            base = 0x780;
            break;
        case 2:
            base = 0x500;
            break;
        case 3:
            base = 0x280;
            break;
        case 4:
        default:
            base = 0x200;
            break;
    }
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&D_mine_cavern_8018E39C[point]);
    gte_rtps_real();
    gte_stsxy(&sxy);
    gte_stflg(&flag);
    gte_stszotz(&otz);
    if (flag >= 0) {
        x           = sxy;
        y           = sxy >> 16;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        radius      = (s32)(base | ((Gp_LcgState >> 16) & 0x7F)) * 0x160 / (otz * 4);
        for (i = 0; i < 8; i++) {
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = (POLY_GT3*)prim + 1;
            setPolyG3(prim);
            prim->r0 = D_mine_cavern_8018E358[0];
            prim->g0 = D_mine_cavern_8018E359[0];
            prim->b0 = D_mine_cavern_8018E35A[0];
            prim->x0 = x;
            prim->y0 = y;
            prim->r1 = D_mine_cavern_8018E35B[0];
            prim->g1 = D_mine_cavern_8018E35C[0];
            prim->b1 = D_mine_cavern_8018E35D[0];
            prim->r2 = D_mine_cavern_8018E35B[0];
            prim->g2 = D_mine_cavern_8018E35C[0];
            prim->b2 = D_mine_cavern_8018E35D[0];
            setSemiTrans(prim, 1);
            prim->x1 = x + ((rsin(i << 9) * radius) >> 12);
            prim->y1 = y + ((rcos(i << 9) * radius) >> 12);
            prim->x2 = x + ((rsin(i * 0x200 + 0x200) * radius) >> 12);
            prim->y2 = y + ((rcos(i * 0x200 + 0x200) * radius) >> 12);
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = (DR_MODE*)dr + 1;
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[otz >> 4], dr);
        }
    }
}

/// Runs the cavern's four emitter points while `GameFlag_GetNibble(0x7A)` is
/// below 5. Each point whose bit is set in `GameFlag_GetNibble(0xE2)` has its
/// light refreshed, and its sound restarted when the view has just been set up
/// or the enabled set changed since the last run. A point listed for the
/// current view in `D_mine_cavern_8018E3BC` also runs
/// `func_mine_cavern_80181D80`, and on every ninth tick or on entering the view
/// spawns effect `0x60080` within 64 units of the point on each axis, unless
/// `D_801153F4` is set.
void func_mine_cavern_80182184(void)
{
    VECTOR        unused;
    GsCOORDINATE2 coord;
    MATRIX*       m;
    SVECTOR*      pos;
    s32           view;
    s32           flags;
    s16           i;
    s16           j;
    s16           k;

    view  = Gp_GetViewIndex() & 0xFF;
    flags = GameFlag_GetNibble(0xE2);
    for (i = 0; i < 4 && GameFlag_GetNibble(0x7A) < 5; i++) {
        if (!((flags >> i) & 1)) {
            continue;
        }
        func_mine_cavern_80181CAC(i);
        if (gGameSession->viewReady == 1 || D_mine_cavern_8018EB58 != flags) {
            func_mine_cavern_801825C8(i);
        }
        for (j = 0; j < 8 && D_mine_cavern_8018E3BC[i][j] != 0; j++) {
            k = D_mine_cavern_8018E3BC[i][j];
            if (k != (u8)view) {
                continue;
            }
            func_mine_cavern_80181D80(i);
            if ((s16)((s16)D_mine_cavern_8018EB5C % 9) != 0 && D_mine_cavern_8018E3DC == k) {
                continue;
            }
            if (D_801153F4 != 0) {
                continue;
            }
            m                           = &coord.coord;
            *(s32*)&coord.coord.m[0][0] = 0x1000;
            *(s32*)&coord.coord.m[0][2] = 0;
            *(s32*)&m->m[1][1]          = 0x1000;
            *(s32*)&coord.coord.m[2][0] = 0;
            m->m[2][2]                  = 0x1000;
            coord.sub                   = &gGfxViewCoord;
            pos                         = &D_mine_cavern_8018E39C[i];
            coord.coord.t[0]            = pos->vx + ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 0x7F) - 0x40;
            coord.coord.t[1]            = pos->vy + ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 0x7F) - 0x40;
            coord.coord.t[2]            = pos->vz + ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 0x7F) - 0x40;
            coord.flg                   = 0;
            Gp_SpawnEff(0x60080, &coord, 0x800004FF, NULL);
        }
    }
    if (D_801153F4 == 0) {
        D_mine_cavern_8018EB5C++;
    }
    D_mine_cavern_8018E3DC = view;
    D_mine_cavern_8018EB58 = flags;
}

/// Queues the cavern's darkness overlay: a semi-transparent flat quad filling
/// the screen with the tint `D_mine_cavern_8018E3E0` holds for the number of
/// `GameFlag_GetNibble(0xE2)` bits set, followed by the drawing-mode packet
/// that restores the room's texture page (`0xE100004A`). Both go into the head
/// of the current OT, and the cavern's own two passes are run afterwards.
void func_mine_cavern_80182454(void)
{
    POLY_F4* poly;
    DR_MODE* dr;
    s32      flags;
    s16      i;
    s16      count;

    flags = GameFlag_GetNibble(0xE2);
    count = 0;

    poly           = (POLY_F4*)gGpuPrimCursor;
    gGpuPrimCursor = poly + 1;
    setlen(poly, 5);
    setcode(poly, 0x2A);

    for (i = 0; i < 4; i++) {
        if ((flags >> i) & 1) {
            count++;
        }
    }

    poly->r0 = D_mine_cavern_8018E3E0[count].r;
    poly->g0 = D_mine_cavern_8018E3E0[count].g;
    poly->b0 = D_mine_cavern_8018E3E0[count].b;

    poly->x0 = -0xA0;
    poly->y0 = -0x78;
    poly->x1 = 0xA0;
    poly->y1 = -0x78;
    poly->x2 = -0xA0;
    poly->y2 = 0x78;
    poly->x3 = 0xA0;
    poly->y3 = 0x78;
    addPrim(gGpuCurrentOt, poly);

    dr             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100004A;
    addPrim(gGpuCurrentOt, dr);

    func_mine_cavern_80181864();
    func_mine_cavern_80182184();
}

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", D_mine_cavern_8017D65C);

void func_mine_cavern_801825C8(s16 arg0)
{
    GsCOORDINATE2 coord;
    s32           view;

    view             = Gp_GetViewIndex() & 0xFF;
    coord.sub        = &gGfxViewCoord;
    coord.coord.t[0] = D_mine_cavern_8018E39C[arg0].vx;
    coord.coord.t[1] = D_mine_cavern_8018E39C[arg0].vy;
    coord.coord.t[2] = D_mine_cavern_8018E39C[arg0].vz;
    coord.flg        = 0;
    Gp_UpdateCoord(&coord);

    switch (arg0) {
        case 0:
            switch (view) {
                case 2:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 20:
                    SndEvt_EnqueueType7(0x5402000F, 1);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 18:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 19:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0xD);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0xD);
                    break;
                case 21:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
            }
            break;
        case 1:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 20:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 23:
                case 24:
                case 25:
                    SndEvt_EnqueueType7(0x5402000E, 1);
                    break;
            }
            break;
        case 2:
            switch (view) {
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 6:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    break;
                case 14:
                case 15:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 16:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    break;
                case 17:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 8:
                case 21:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
                case 2:
                case 3:
                case 4:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 18:
                case 19:
                case 20:
                case 22:
                default:
                    SndEvt_EnqueueType7(0x54020010, 1);
                    break;
            }
            break;
        case 3:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 8:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 6:
                case 20:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x46);
                    break;
                case 3:
                case 4:
                case 5:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 22:
                case 23:
                case 24:
                case 25:
                default:
                    SndEvt_EnqueueType7(0x54020011, 1);
                    break;
            }
            break;
    }
}

void func_mine_cavern_80182CEC(Task* arg0)
{
    s16 i;
    s32 flags;

    flags = GameFlag_GetNibble(0xE2);
    for (i = 0; i < 4; i++) {
        if (!((flags >> i) & 1)) {
            Gp_SpawnEnemyFromTable(&D_mine_cavern_8018EB38, 0, i, NULL);
        }
        Gp_SpawnEnemyFromTable(&D_mine_cavern_8018EB38, 1, i, NULL);
    }
    arg0->state++;
}

void func_mine_cavern_80182DA8(void)
{
    func_mine_cavern_80182454();
}

/// Mine task dispatcher: runs the state handler this task's `state` selects,
/// unless the screen id says the room is being left.
void func_mine_cavern_80182DC8(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_mine_cavern_8017D65C;
    if (D_8007218B != 3) {
        sp.funcs[arg0->state](arg0);
    }
}

/// Spawn state of the cavern enemy: allocates its work block, parks it in
/// `Task::work` and installs `func_mine_cavern_80183860` as the exit callback,
/// or destroys the enemy when the allocation fails. The model is hung under the
/// view coordinate, given the block's two matrices and seated on the spawn spot
/// `Task::spawnArg1` names. Two collision bodies are then linked through
/// `Gp_LinkObj`: a small one (kind 2) with four contact records and flag 0x8000
/// set, and a wide one (kind 1) with a single record and flag 0x8000 cleared.
/// The enemy takes its hit points and parameters from `D_mine_cavern_8018EAE4`,
/// the model is republished through `func_800D7A9C`, and the enemy's node is
/// linked with its flags set to 1.
///
/// The wide body's x and y offset are read from a structure at address 0. The
/// read has to be a structure member: the scheduler lets a load from a plain
/// scalar at a fixed address pass the stores into the body before it, and the
/// original keeps it behind them.
void func_mine_cavern_80182E34(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* mem;
    MineCavernWork* work;
    GpObj*          obj40;
    GpObj*          objC0;
    u16             temp;
    VECTOR          vec;

    mem        = (MineCavernWork*)memCalloc(0x14C, false);
    work       = mem;
    arg1->work = mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->exitCallback                            = func_mine_cavern_80183860;
    ((TmdObject*)arg1->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)arg1->extra)->flags              = 0;
    ((TmdObject*)arg1->extra)->lightMtx           = &work->light;
    ((TmdObject*)arg1->extra)->colorMtx           = &work->color;
    ((TmdObject*)arg1->extra)->coords->coord.t[0] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vx;
    ((TmdObject*)arg1->extra)->coords->coord.t[1] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vy;
    ((TmdObject*)arg1->extra)->coords->coord.t[2] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vz;
    ((TmdObject*)arg1->extra)->coords->flg        = 0;
    obj40                                         = &work->obj40;
    obj40->coord                                  = ((TmdObject*)arg1->extra)->coords;
    obj40->ctx.recs                               = work->recs;
    obj40->pos.vx                                 = 0;
    obj40->pos.vy                                 = -0x320;
    obj40->pos.vz                                 = 0;
    obj40->key                                    = 0x50000;
    obj40->radius                                 = 0x100;
    obj40->flags                                  = 1;
    Gp_LinkObj(2, obj40);
    obj40->flags |= 0x8000;
    Gp_InitRec18Table(obj40->ctx.recs, 4, 0);
    work->obj40.flags |= 0x8000;
    objC0              = &work->objC0;
    objC0->coord       = ((TmdObject*)arg1->extra)->coords;
    objC0->ctx.recs    = &work->recE0;
    temp               = ((SVECTOR*)NULL)->vy;
    objC0->pos.vz      = 0;
    objC0->radius      = 0xBB8;
    objC0->flags       = 1;
    objC0->pos.vy      = temp;
    objC0->pos.vx      = temp;
    Gp_LinkObj(1, objC0);
    Gp_InitRec18Table(objC0->ctx.recs, 1, 0);
    work->objC0.key    = 0x22121;
    work->objC0.flags &= 0x7FFF;
    arg0->hp           = D_mine_cavern_8018EAE4.hpMax;
    arg0->param        = &D_mine_cavern_8018EAE4;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    vec.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    vec.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    func_800D7A9C(arg1->extra, &vec, 0, 3);
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = -0x320;
    arg0->bodyPos.vz = 0;
    arg0->coord      = ((TmdObject*)arg1->extra)->coords;
    Gp_LinkNode(&arg0->node);
    arg0->node.flags = 1;
    arg1->state++;
}

/// Second state handler of `D_mine_cavern_8017D7F8`: the cavern enemy's
/// per-frame hit check. Unless gameplay is suspended, it marks the enemy
/// lockable only while the player is within 0x1770 on the XZ plane and in place
/// 1 or 4, republishes the model's world position, and looks through the work
/// block's contacts for one of class 2. A contact taken in place 1 or 4 without
/// key bit 0x8000 costs the enemy the damage `D_mine_cavern_8018EAF4` gives its
/// key; when that empties `GpEnemy::hp` the enemy's `Task::spawnArg1` bit is
/// set in flag nibble 0xE2, the model is hidden, a sound is played at it and
/// the task advances.
void func_mine_cavern_801830F0(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork*        work;
    Task*                  player;
    u8*                    head;
    _MineCavernHitScratch* blk;
    GsCOORDINATE2*         coords;
    GpRec18*               recs;
    SVECTOR*               d;
    SVECTOR*               dst;
    s16                    i;
    s16                    angle;
    u32                    key;
    s32                    id;
    s32                    pan;

    work   = arg1->work;
    player = gameGetPtrSlot(3);
    switch (D_801153F4) {
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            return;
        case 0:
        default:
            break;
        case 1:
            return;
    }

    coords                                   = ((TmdObject*)arg1->extra)->coords;
    head                                     = *(u8**)G_SCRATCH_HEAD;
    ((SVECTOR*)(head - 0x18))->vx            = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    d                                        = (SVECTOR*)(head - 0x18);
    d->vy                                    = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    *(_MineCavernHitScratch**)G_SCRATCH_HEAD = (_MineCavernHitScratch*)(head - 0x28);
    d->vz                                    = Player_Status.coordMtx->t[2] - coords->coord.t[2];
    blk                                      = (_MineCavernHitScratch*)(head - 0x28);

    if (_mineCavernOutOfRange(d, 0x1770) || Gp_StateF0.field_0 != 1 ||
        (gGameSession->at4.loc.place != Gp_StateF0.field_0 && gGameSession->at4.loc.place != 4)) {
        arg0->node.flags = 1;
    } else {
        arg0->node.flags = 0;
    }

    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    blk->pos.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    blk->pos.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    blk->pos.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    func_800D7A9C(arg1->extra, &blk->pos, 0, 3);
    ((TmdObject*)arg1->extra)->flags = 0;

    dst  = &blk->d;
    recs = work->recs;
    for (i = 0; i < 4; i++) {
        if (recs[i].key == 0) {
            break;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            dst->vx = recs[i].point.vx;
            dst->vy = recs[i].point.vy;
            dst->vz = recs[i].point.vz;
            key     = recs[i].key;
            goto found;
        }
    }
    key = 0;
found:
    blk->key = key;
    if (key & 0x8000) {
        blk->key = 0;
    }
    if (gGameSession->at4.loc.place != 1 && gGameSession->at4.loc.place != 4) {
        blk->key = 0;
    }

    if (blk->key != 0) {
        blk->d.vx -= ((TmdObject*)arg1->extra)->coords->workm.t[0];
        blk->d.vy -= ((TmdObject*)arg1->extra)->coords->workm.t[1];
        blk->d.vz -= ((TmdObject*)arg1->extra)->coords->workm.t[2];
        angle = blk->angle = ratan2(blk->d.vx, blk->d.vz) - ratan2(-((TmdObject*)arg1->extra)->coords->workm.m[2][0],
                                                                   ((TmdObject*)arg1->extra)->coords->workm.m[2][2]);
        if (angle < 0) {
        neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto neg;
            }
        } else {
        pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto pos;
            }
        }
        blk->angle  = angle;
        blk->pos.vx = ((TmdObject*)player->extra)->coords->coord.t[0] - ((TmdObject*)arg1->extra)->coords->coord.t[0];
        blk->pos.vy = ((TmdObject*)player->extra)->coords->coord.t[1] - ((TmdObject*)arg1->extra)->coords->coord.t[1];
        blk->pos.vz = ((TmdObject*)player->extra)->coords->coord.t[2] - ((TmdObject*)arg1->extra)->coords->coord.t[2];
        blk->dist   = SquareRoot0(blk->pos.vx * blk->pos.vx + blk->pos.vy * blk->pos.vy + blk->pos.vz * blk->pos.vz);
        blk->damage = Gp_ComputeDamage(blk->key, blk->dist, 0, 0);
        blk->damage = D_mine_cavern_8018EAF4[blk->key & 0x7F];
        arg0->hp   -= blk->damage;
        func_800DA6E8(&arg0->node, blk->damage, 0);
        if (arg0->hp <= 0) {
            blk->bits = GameFlag_GetNibble(0xE2);
            if (!((blk->bits >> (u16)arg1->spawnArg1) & 1)) {
                blk->bits |= 1 << (u16)arg1->spawnArg1;
                GameFlag_SetNibble(0xE2, blk->bits);
                ((TmdObject*)arg1->extra)->flags = 0x80;
            }
            id  = ((arg0->placeKey >> 12) << 8) | 0x54020014;
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg1->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)(gpGetObjDepth(((TmdObject*)arg1->extra)->coords) / 2));
            arg1->state++;
        }
    }
    Gp_ClearRec18Occupied(&work->recs[0]);
    Gp_ClearRec18Occupied(&work->recE0);
    *(u8**)G_SCRATCH_HEAD += 0x28;
}

/// Second state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It allocates the work block, parks it at `Task::work` and
/// hands its two matrices to the model, then seats the model on the spawn spot
/// `Task::spawnArg1` names: the block's own coordinate adopts that spot with the
/// model's coordinate hung under it, and the model is republished through
/// `func_800D7A9C`.
///
/// `mem` and `work` are the same block: the original build tests and parks the
/// allocation through `mem` and reaches the block through `work` afterwards,
/// which is what keeps the two live ranges - and so `$v0` / `$a0` - apart.
void func_mine_cavern_801836D0(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* mem;
    MineCavernWork* work;
    VECTOR          vec;

    mem        = (MineCavernWork*)memCalloc(0x14C, false);
    work       = mem;
    arg1->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    ((TmdObject*)arg1->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)arg1->extra)->flags              = 0;
    ((TmdObject*)arg1->extra)->lightMtx           = &work->light;
    ((TmdObject*)arg1->extra)->colorMtx           = &work->color;
    ((TmdObject*)arg1->extra)->coords->coord.t[0] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vx;
    ((TmdObject*)arg1->extra)->coords->coord.t[1] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vy;
    ((TmdObject*)arg1->extra)->coords->coord.t[2] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vz;
    ((TmdObject*)arg1->extra)->coords->flg        = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    vec.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    vec.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    func_800D7A9C(arg1->extra, &vec, 0, 3);
    arg1->state++;
}

void func_mine_cavern_80183860(Task* arg0)
{
    MineCavernWork* work;

    work = (MineCavernWork*)arg0->work;
    if (work != NULL) {
        Gp_UnlinkObj(&work->obj40);
    }
}

void func_mine_cavern_80183890(GpEnemy* enemy, Task* task)
{
    MineCavernWork* work;

    work               = (MineCavernWork*)task->work;
    work->obj40.flags &= 0x7FFF;
    enemy->node.flags  = 1;
    Gp_UnlinkObj(&work->obj40);
    work->field_148 = 0;
    task->state++;
}

/// The two cue lines the bomb prints on its first two ticks. They are the run
/// this function's own assembly file carried at the head of the unit's rodata,
/// so they are written here, ahead of the two `INCLUDE_RODATA` blobs that
/// follow them in address order. The section attribute is load-bearing: at 8
/// bytes these fall under the compiler's small-data threshold and would
/// otherwise be emitted into `.sdata`, which the linker script does not lay out.
const char D_mine_cavern_8017D7E8[8] __attribute__((section(".rodata"))) = "BOMB1\n";
const char D_mine_cavern_8017D7F0[8] __attribute__((section(".rodata"))) = "BOMB2\n";

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", D_mine_cavern_8017D7F8);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", D_mine_cavern_8017D80C);

/// Fourth state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It parks the model hidden (`field_C = 0x80`) and walks
/// `work->field_148` down its 0x3C-step countdown, one case per tick: 0 prints
/// "BOMB1", drops the model to y = -0x258 and spawns effect 0x01001200; 1
/// prints "BOMB2" and spawns 0x01000580, parking that effect's own first three
/// halfwords; 2 and 4 spawn 0x01002500; 3 and 5 clear the hidden bit on the
/// work block's second object (`objC0`); 9 hands `objC0` to `Gp_UnlinkObj`;
/// 0x3B advances `Task::state`.
void func_mine_cavern_801838F4(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* work;
    GpEffWork*      eff;
    u16             state;

    work = (MineCavernWork*)arg1->work;

    ((TmdObject*)arg1->extra)->flags = 0x80;

    state           = work->field_148;
    work->field_148 = state + 1;

    switch ((s16)state) {
        case 0:
            printf(D_mine_cavern_8017D7E8);
            ((TmdObject*)arg1->extra)->coords->coord.t[1] = -0x258;
            ((TmdObject*)arg1->extra)->coords->flg        = 0;
            Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
            Gp_SpawnEff(0x6005C, ((TmdObject*)arg1->extra)->coords, 0x01001200, NULL);
            return;

        case 1:
            printf(D_mine_cavern_8017D7F0);
            eff = Gp_SpawnEff(0x6005C, ((TmdObject*)arg1->extra)->coords, 0x01000580, NULL);
            if (eff != NULL) {
                eff->move.vx = 0;
                eff->move.vy = -0xA;
                eff->move.vz = 0;
            }
            return;

        case 2:
        case 4:
            Gp_SpawnEff(0x6005C, ((TmdObject*)arg1->extra)->coords, 0x01002500, NULL);
            return;

        case 3:
        case 5:
            work->objC0.flags &= 0x7FFF;
            return;

        case 9:
            Gp_UnlinkObj(&work->objC0);
            return;

        case 0x3B:
            arg1->state++;
            break;

        default:
            return;
    }
}

void func_mine_cavern_80183A68(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_mine_cavern_8017D7F8;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Third state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It republishes the model's world position through
/// `func_800D7A9C`, then settles the work block's own coordinate: when the
/// `GameFlag_GetNibble(0xE2)` bit selected by `Task::spawnArg1` is set the
/// coordinate is reset to an identity rotation parked at (0, -0x320, 0) under
/// the model's own coordinate, `field_148` ticks, and the model's `field_C` is
/// cleared; otherwise the model is flagged hidden with `field_C = 0x80`.
///
/// `ang` is declared and never read - the original build's frame reserved 8
/// bytes for it ahead of nothing, so dropping it shrinks the frame from 0x38 to
/// 0x30 and moves every spill.
void func_mine_cavern_80183AD4(GpEnemy* enemy, Task* task)
{
    MineCavernWork* work;
    MATRIX*         m;
    VECTOR          vec;
    SVECTOR         ang;

    work = (MineCavernWork*)task->work;

    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    vec.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    vec.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C(task->extra, &vec, 0, 3);

    if (!((GameFlag_GetNibble(0xE2) >> (u16)task->spawnArg1) & 1)) {
        ((TmdObject*)task->extra)->flags = 0x80;
    } else {
        m                         = &work->coord.coord;
        *(s32*)&work->coord.coord = 0x1000;
        *(s32*)&m->m[0][2]        = 0;
        *(s32*)&m->m[1][1]        = 0x1000;
        *(s32*)&m->m[2][0]        = 0;
        m->m[2][2]                = 0x1000;
        work->coord.sub           = ((TmdObject*)task->extra)->coords;
        work->coord.coord.t[2]    = 0;
        work->coord.coord.t[0]    = 0;
        work->coord.coord.t[1]    = -0x320;
        work->coord.flg           = 0;
        Gp_UpdateCoord(&work->coord);
        work->field_148++;
        ((TmdObject*)task->extra)->flags = 0;
    }
}
