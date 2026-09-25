#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/light.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/mine_cavern.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017dcb8.h"
#include "rooms/rooms_shared_8017ff88.h"

extern u8  D_8007216D;
extern s32 D_80070F70;
extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011573C;
extern s32 D_80115744;

extern SVECTOR D_mine_cavern_80188F64[];
extern SVECTOR D_mine_cavern_80188F7C[];
extern SVECTOR D_mine_cavern_80188F84[];
extern SVECTOR D_mine_cavern_80188F8C[];
extern SVECTOR D_mine_cavern_80188F94[];
extern SVECTOR D_mine_cavern_80188F9C[];
extern SVECTOR D_mine_cavern_80188FB4[];
extern SVECTOR D_mine_cavern_80188FBC;
extern SVECTOR D_mine_cavern_80188FC4[];

extern RoomHaloShade D_mine_cavern_80188FCC[];

void func_mine_cavern_8017F50C(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, u16 arg3);
void func_mine_cavern_801804CC(GsCOORDINATE2* coord, s16 size);
void func_mine_cavern_80181864(void);
void func_mine_cavern_80182184(void);
void func_mine_cavern_80182454(void);
void func_mine_cavern_801825C8(s16 arg0);
void func_mine_cavern_80182CEC(Task* arg0);
void func_mine_cavern_80182DA8(Task* task);
void func_mine_cavern_801838F4(GpEnemy* arg0, Task* arg1);
void func_mine_cavern_80183AD4(GpEnemy* enemy, Task* task);

/// Current screen id at 0x8007218B.
extern s8 D_8007218B;

/// Sound emitter positions for the cavern's four ambient loops, indexed by the
/// emitter id `func_mine_cavern_801825C8` and its siblings are called with.
extern SVECTOR D_mine_cavern_8018E39C[4];

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

/// Tick counter `func_mine_cavern_80182184` advances while `Gp_StateF0.field_4` is
/// clear; the emitters spawn on every ninth tick.
extern u16 D_mine_cavern_8018EB5C;

/// Mode byte the cavern enemy's hit check switches on: 1 skips the check and 2
/// hides the model and skips it. Its wider role is unproven.

/// Damage the cavern enemy takes from a contact, indexed by the low seven bits
/// of the contact's key.
extern u8 D_mine_cavern_8018EAF4[];

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

void func_mine_cavern_8017E330(void)
{
    D_8007216D                  = 2;
    gGameSession->at4.loc.room  = 2;
    gGameSession->roomObjsDirty = 1;
}

void func_mine_cavern_8017E358(void)
{
}

void func_mine_cavern_8017E360(void)
{
    gGameSession->at4.loc.place = 4;
    Gp_StateF0.field_0          = 0;
    Gp_StateF0.field_5          = 0;
    Gp_StateF0.field_6          = 0;
    Gp_StateF0.field_8          = 0;
    Gp_StateF0.field_C          = 0;
    Gp_StateF0.field_10         = 0;
}

void func_mine_cavern_8017E394(void)
{
    D_mine_cavern_8018EB54 = 0;
}

void func_mine_cavern_8017E3A0(s32 arg0)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    s32        v;

    sess = &gGameSession->at4.loc;
    rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    v    = arg0 & 0xFF;

    if (v == 1) {
        rec[3].field_4[5].field_4  = v;
        rec[4].field_4[6].field_4  = v;
        rec[21].field_4[5].field_4 = v;
        rec[22].field_4[3].field_4 = v;
        rec[23].field_4[4].field_4 = v;
        return;
    }
    if (v == 0) {
        rec[3].field_4[5].field_4  = 0;
        rec[4].field_4[6].field_4  = 0;
        rec[21].field_4[5].field_4 = 0;
        rec[22].field_4[3].field_4 = 0;
        rec[23].field_4[4].field_4 = 0;
    }
}

void func_mine_cavern_8017E474(Task* arg0)
{
    u32 rnd;

    if (arg0->state == 0) {
        D_80115728                 = 0x60244;
        D_80115744                 = 0x60250;
        D_8011573C                 = 0x6023F;
        D_80115720                 = 0x60267;
        Gp_State1C->roomEffectMode = 2;
        arg0->state                = 1;
    }

    if (GameFlag_GetNibble(0xC4) == 1) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 7) == 0) {
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vx = ((Gp_LcgState >> 16) & 0x3F) + 0x1766;
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vy = ((Gp_LcgState >> 16) & 0x3F) - 0x5B4;
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vz = ((Gp_LcgState >> 16) & 0x3F) - 0x14A;
            Gp_SpawnEff(0x600E0, NULL, 0x300, &D_mine_cavern_80188FBC);
        }
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3:
        case 9: {
            SVECTOR* p = D_mine_cavern_80188F84;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[5], 1, 0x300);
            break;
        }
        case 5:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F8C, 1, 0x300);
        case 23: {
            SVECTOR* p = D_mine_cavern_80188F64;
            func_mine_cavern_8017E774(&p[0], 0x180, 0x222);
            func_mine_cavern_8017E774(&p[1], 0x180, 0x222);
            func_mine_cavern_8017EFB8(&p[3], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_cavern_80188F8C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[4], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_mine_cavern_80188F84;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[3], 1, 0x300);
            break;
        }
        case 8:
        case 20:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F94, 1, 0x300);
            break;
        case 10:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188FB4, 1, 0x300);
            break;
        case 11:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F8C, 1, 0x300);
        case 13: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[5], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[6], 1, 0x300);
            break;
        }
        case 14:
        case 16:
        case 21:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F8C, 1, 0x300);
            break;
        case 17:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F9C, 1, 0x300);
            break;
        case 24:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188FC4, 1, 0x300);
        case 22:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F7C, 1, 0x300);
            break;
        case 25: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            break;
        }
    }
}

/// Draws a glow joining the two points `arg0[0]` and `arg0[1]`, projected
/// through `Gfx_ViewWorldMtx`; nothing is drawn unless both project. `arg1` is
/// the half-extent, scaled by each end's depth. Starting from the screen-space
/// angle between the ends, for each 0x400 step over half a turn it queues three
/// gouraud `POLY_G4`s: a wedge of each end and the band between them. The lit
/// vertices take the colour packed in `arg2`, four bits per channel (R, G, B
/// from high to low nibble), with the frame counter's low bit as a flicker.
void func_mine_cavern_8017E774(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is the frame-counter blend
/// byte `((animFrame & 1) * 16) + 0x20` on all three channels.
void func_mine_cavern_8017EFB8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Frame callback of a drifting mote. Setup reads speed, lifetime and drawing
/// flags out of `Task::spawnArg1`; the mote then rises or falls one step a
/// frame and is drawn with `func_mine_cavern_8017F50C` every other tick. State
/// 1 brightens while young, state 2 holds its brightness; both fade over their
/// last eight ticks of lifetime and release the work block once dark, or as
/// soon as the room's event state reaches 4.
void func_mine_cavern_8017F240(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    s32            lifetime;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1 & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1 & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1 & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_mine_cavern_8017F50C(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_mine_cavern_8017F50C(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2A) centred on the projected point. The texture cell's u origin is
/// `(arg2 >> 12) * 0x60 + (arg1 & 3) * 24` over rows 0..0x17; the low twelve
/// bits of `arg2` are the half-extent, giving an on-screen radius of
/// `extent * 23 / (otz + 1)`. The low byte of `arg3` is the grey level on all
/// three channels and its top nibble picks the CLUT on row 0x10B: column
/// `nibble * 16 + 0xF0`, or 0xB0 when the nibble is zero.
void func_mine_cavern_8017F50C(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    GpRingScratch* next;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;
    u16            vz;

    row                                     = arg2 >> 12;
    arg2                                   &= 0xFFF;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    pal                                     = arg3 >> 12;
    arg3                                   &= 0xFF;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    next                                    = (GpRingScratch*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(next));
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = *(u16*)&block->sx - *(u16*)&block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = *(u16*)&block->sx + *(u16*)&block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = *(u16*)&block->sy - *(u16*)&block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = *(u16*)&block->sy + *(u16*)&block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim.
void func_mine_cavern_8017F7D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw09Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    register s32       sum asm("a1");
    register s32       otz asm("v0");
    register s32       rOuter asm("a0");
    register s32       rInner asm("v1");
    register u8*       color asm("s4");
    s32                t;
    u16                vz;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw09Scratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw09Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw09Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw09Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw09Scratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner        = rInner / otz;
        ang           = 0;
        block->rOuter = rOuter;
        block->rInner = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / (otz + 1)`. The RGB triple in `rgb` lights only the
/// inner vertex so each wedge fades to black.
void func_mine_cavern_8017FBF4(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Frame callback of an expanding halo. State 0 parks the coordinate on its
/// parent at the spawn position and derives the ramp step from the spawn
/// argument; state 1 grows the halo's brightness and size, drawing the
/// `func_mine_cavern_8017FBF4` wedge ring (with a half-bright echo on odd
/// ticks) and a `func_mine_cavern_8017F7D0` ring; state 2 fades it out through
/// `func_mine_cavern_80180D70` and then releases the work block. The shade row
/// `D_mine_cavern_80188FCC[index]` tints each channel.
void func_mine_cavern_8017FF88(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s16            flag;
    s32            shift;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->parent;
                rot->m00_m01      = 0x1000;
                rot->m02_m10      = 0;
                rot->m11_m12      = 0x1000;
                rot->m20_m21      = 0;
                rot->m22          = 0x1000;
                coord->coord.t[0] = mem->pos.vx;
                coord->coord.t[1] = mem->pos.vy;
                coord->coord.t[2] = mem->pos.vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->index      = shift;
                arg0->spawnArg1 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
                arg0->state     = 1;
                mem->step       = 0x100 / arg0->spawnArg1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale      += mem->step;
                mem->angle      += mem->step;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->scale >> D_mine_cavern_80188FCC[mem->index].r;
                rgb[1]           = mem->scale >> D_mine_cavern_80188FCC[mem->index].g;
                rgb[2]           = mem->scale >> D_mine_cavern_80188FCC[mem->index].b;
                func_mine_cavern_8017FBF4(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_mine_cavern_8017FBF4(coord, (s16)(mem->angle + 0x100), rgb);
                }
                func_mine_cavern_8017F7D0(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_mine_cavern_80188FCC[mem->index].r;
                    rgb[1] = mem->scale >> D_mine_cavern_80188FCC[mem->index].g;
                    rgb[2] = mem->scale >> D_mine_cavern_80188FCC[mem->index].b;
                    func_mine_cavern_80180D70(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Frame callback for one of the cavern's expanding-ring effects. `Gp_State1C`'s
/// `field_4` gates the whole room-effect family: 1-3 park the effect for the
/// frame and 4 or more tear its work block down, so a task that sees them either
/// returns or releases. Otherwise the effect ticks its lifetime counter, stages
/// `scale` into an RGB triple, advances the coordinate, and draws the
/// eight-wedge `func_mine_cavern_8017FBF4` ring at twice `angle` plus the cavern's own
/// glow quads at half-extent `angle`. Once `period` reaches 0x19 the
/// two ramps swap roles - a `func_mine_cavern_8017F7D0` ring is drawn at `step * 3 / 2` and
/// then `period` shrinks by 0x18 and `step` grows by 0x30 - and the effect
/// otherwise fades `scale` by 0x18 a frame until it drops under 0x18 and the
/// work block is handed back with `Gp_ReleaseState1CMem`.
void func_mine_cavern_80180320(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             sp10[3];
    u16            temp;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->age    = 1;
            work->scale  = 0xE0;
            work->angle  = 0x80;
            work->period = 0xE0;
            work->step   = 0x80;
            task->state  = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]     = (u8)work->scale;
        sp10[1]     = (u8)(work->scale >> 1);
        sp10[2]     = (u8)(work->scale >> 2);
        temp        = work->angle + 0x10;
        work->angle = temp;
        func_mine_cavern_8017FBF4(coord, (s16)(temp * 2), sp10);
        func_mine_cavern_801804CC(coord, work->angle);
        if (work->period >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->period;
            sp10[1] = (u8)(work->period >> 1);
            sp10[2] = (u8)(work->period >> 2);
            temp_a1 = work->step * 3;
            func_mine_cavern_8017F7D0(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->period -= 0x18;
            work->step   += 0x30;
            return;
        }
        temp        = work->scale - 0x18;
        work->scale = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Glow at a coordinate: two camera-facing textured quads, an inner one of
/// half-extent `size` and an outer one of `size * 3 / 2`, plus a
/// `func_mine_cavern_801809F8` mark on the ground under it. Also feeds the
/// `Gp_RoomCoords[2]` light a flickering intensity at the coordinate's position.
/// Draws nothing when the point fails to project.
void func_mine_cavern_801804CC(GsCOORDINATE2* coord, s16 size)
{
    GsCOORDINATE2  ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
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
    block                       = SCRATCH_HEAD_AT(scratch, GpRingScratch) - 1;
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
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_mine_cavern_801809F8(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Scales the unit quad `D_80111E38` by `arg1`, rotates it flat into view space
/// with `Gfx_ViewWorldMtx` (no GTE translation) and adds `arg0->workm.t`, then
/// projects the four corners through `GsWSMATRIX`. When `gte_stflg` is
/// non-negative, queues one semi-transparent `POLY_FT4` (tpage 0x28, clut
/// 0x428C) coloured `(0x30, 0x20, 0x20)`. The frame counter picks between two
/// 0x1F-wide UV columns: `u` is `(animFrame & 1) * 32` plus 0xC0 / 0xDF, at
/// v = 0x38..0x57. Works in a `GpQuadScratch` block on the scratch stack.
void func_mine_cavern_801809F8(GsCOORDINATE2* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
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
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a two-ring billboard. `arg1` is a signed half-extent; on-screen radii
/// are `(s16)arg1 * 64 / (otz + 1)` (outer) and `(s16)arg1 * 8 / (otz + 1)`
/// (inner). The RGB triple tints the inner vertex of the inner ring at full
/// brightness and the outer ring at half, so each wedge fades to a black rim.
void func_mine_cavern_80180D70(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Frame callback of a rising spark: each tick walks its angle on by a random
/// 0x200..0x3FF, sets its velocity to a 3/16-scaled unit circle in X and Z and
/// an upward Y that grows with age, and spawns the `D_80115728` effect at the
/// task's coordinate. Releases the work block after 0x15 ticks, or as soon as
/// the room's event state reaches 4.
void func_mine_cavern_80181730(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            ang;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
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
        gte_rtps();
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

/// Switches on transient light slot `4 + point` for cavern point `point`: fills
/// it from the cavern's light parameters and the point's position in
/// `D_mine_cavern_8018E39C`, with the outer radius jittered by a draw from the
/// shared LCG.
void func_mine_cavern_80181CAC(s16 point)
{
    GpCoord64*    light = &Gp_RoomCoords[4 + point];
    GpPointLight* work  = &light->data.light;

    light->framesLeft          = 2;
    work->inner                = D_mine_cavern_8018E366;
    work->outer                = D_mine_cavern_8018E368 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x7FF);
    work->head.r               = D_mine_cavern_8018E360;
    work->head.g               = D_mine_cavern_8018E362;
    work->head.b               = D_mine_cavern_8018E364;
    work->head.u.at.local.t[0] = D_mine_cavern_8018E39C[point].vx;
    work->head.u.at.local.t[1] = D_mine_cavern_8018E39C[point].vy;
    work->head.u.at.local.t[2] = D_mine_cavern_8018E39C[point].vz;
    light->data.coord.flg      = 0;
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
    gte_rtps();
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
/// `Gp_StateF0.field_4` is set.
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
            if (Gp_StateF0.field_4 != 0) {
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
    if (Gp_StateF0.field_4 == 0) {
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

/// The mine task's state handlers, run by `func_mine_cavern_80182DC8`.
const TaskFuncTable3 D_mine_cavern_8017D65C = {
    { func_mine_cavern_80182CEC, func_mine_cavern_80182DA8, taskKill },
};

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

void func_mine_cavern_80182DA8(Task* task)
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
    arg0->node.state.b.flags = 1;
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
    switch (Gp_StateF0.field_4) {
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            return;
        case 0:
        default:
            break;
        case 1:
            return;
    }

    coords                              = ((TmdObject*)arg1->extra)->coords;
    head                                = SCRATCH_HEAD(u8);
    ((SVECTOR*)(head - 0x18))->vx       = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    d                                   = (SVECTOR*)(head - 0x18);
    d->vy                               = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    SCRATCH_HEAD(_MineCavernHitScratch) = (_MineCavernHitScratch*)(head - 0x28);
    d->vz                               = Player_Status.coordMtx->t[2] - coords->coord.t[2];
    blk                                 = (_MineCavernHitScratch*)(head - 0x28);

    if (overlayOutOfRange(d, 0x1770) || Gp_StateF0.field_0 != 1 ||
        (gGameSession->at4.loc.place != Gp_StateF0.field_0 && gGameSession->at4.loc.place != 4)) {
        arg0->node.state.b.flags = 1;
    } else {
        arg0->node.state.b.flags = 0;
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
    SCRATCH_POP_BYTES(0x28);
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

    work                      = (MineCavernWork*)task->work;
    work->obj40.flags        &= 0x7FFF;
    enemy->node.state.b.flags = 1;
    Gp_UnlinkObj(&work->obj40);
    work->field_148 = 0;
    task->state++;
}

/// The two cue lines `func_mine_cavern_801838F4` prints on its first two ticks.
/// The section attribute is load-bearing: at 8 bytes these fall under the
/// compiler's small-data threshold and would otherwise be emitted into
/// `.sdata`, which the linker script does not lay out.
const char D_mine_cavern_8017D7E8[8] __attribute__((section(".rodata"))) = "BOMB1\n";
const char D_mine_cavern_8017D7F0[8] __attribute__((section(".rodata"))) = "BOMB2\n";

/// The cavern enemy's state handlers, run by `func_mine_cavern_80183A68`.
const GpEnemyTaskFuncTable5 D_mine_cavern_8017D7F8 = {
    {
        func_mine_cavern_80182E34,
        func_mine_cavern_801830F0,
        func_mine_cavern_80183890,
        func_mine_cavern_801838F4,
        Gp_DestroyEnemy,
    },
};

/// The second enemy's state handlers, run by `func_mine_cavern_80183C10`.
const GpEnemyTaskFuncTable3 D_mine_cavern_8017D80C = {
    { func_mine_cavern_801836D0, func_mine_cavern_80183AD4, Gp_DestroyEnemy },
};

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

/// Runs the current state handler of one of the room's enemies from its
/// three-entry table - setup (`func_mine_cavern_801836D0`), per-frame tick
/// (`func_mine_cavern_80183AD4`) or teardown (`Gp_DestroyEnemy`) - copying the
/// table onto the stack before the call.
void func_mine_cavern_80183C10(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_mine_cavern_8017D80C;
    sp.funcs[task->state](task->spawnArg2, task);
}
