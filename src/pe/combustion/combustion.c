#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/combustion.h"

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.
const u32 D_combustion_8012EF30 = 49;

/// Per-level tuning for the combustion flame: rows are PE levels 1-3.
CombustionStep D_combustion_80130980[] = {
    { 0x0060, 0x0120, 0x0007, 0x0015 },
    { 0x0055, 0x0187, 0x0008, 0x0017 },
    { 0x004C, 0x01F3, 0x0009, 0x0019 },
};

/// The `SndEvt_EnqueueType6` id for each `D_combustion_80130980` row.
s32 D_combustion_80130998[] = { 0xE00C0002, 0xE00F0002, 0xE0120002 };

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

void PeShared8012fb14(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

/// `rtps`: project V0 through the loaded rotation and translation matrices.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Burns the player: parents an effect coordinate to the player model, plays
/// the ignition sound and fades the screen, then spawns a flame every frame
/// while drifting the flame overlay by the `D_combustion_80130980` row for the
/// current intensity. State 1 spawns, state 2 (past `field_4`) only unwinds
/// the yaw the ignition applied, and either state ends as soon as the player
/// is dying (`Gp_StateC08.field_3`), the room is fading (`Gp_State1C`) or the
/// row's `field_6` tick is reached.
/// Live flame handle for the combustion effect.
/// lists an object in the linker script at its first subsegment, and this has
s32 D_combustion_801309A4 = 0;

void func_combustion_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    GpEffWork*     spawned;
    s32            pan;
    u8             rgb[3];

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            if (arg0->spawnArg1 == 0) {
                arg0->spawnArg1 = 1;
            }
            D_combustion_801309A4 = coord->workm.t[1];
            rot                   = (GpMtxWords*)&coord->coord;
            coord->sub            = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            rot->w0               = 0x1000;
            rot->w2               = 0x1000;
            rot->h4               = 0x1000;
            rot->w1               = 0;
            rot->w3               = 0;
            coord->coord.t[0]     = 0;
            coord->coord.t[1]     = -0x400;
            coord->coord.t[2]     = 0;
            Gfx_RotMatrixY(&coord->coord, arg0->spawnArg1 << 9, 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->field_14 = 0x200;
            pan           = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_combustion_80130998[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            arg0->state   = 1;
            mem->field_20 = Gp_StateC08.field_0 % 10 - 1;
            Gp_SpawnPadLerp(D_combustion_80130980[mem->field_20].field_6, 0xFF, 8);
            /* fallthrough */
        case 1:
            Gp_UpdateCoord(coord);
            if ((Gp_StateC08.field_3 == -2) || (Gp_State1C->field_E >= 4)) {
                goto release;
            }
            mem->field_12 = (u16)mem->field_12 + D_combustion_80130980[mem->field_20].field_0;
            mem->field_14 = (u16)mem->field_14 + D_combustion_80130980[mem->field_20].field_2;
            spawned       = Gp_SpawnEff(0x8006001C, coord, mem->field_22, (SVECTOR*)&mem->field_10);
            if (spawned != NULL) {
                Task_Reparent(arg0, spawned->field_0);
            }
            if (D_combustion_80130980[mem->field_20].field_4 < mem->field_22) {
                Gp_StateC08.field_6 |= 8;
                arg0->state          = 2;
                return;
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4) ||
                (mem->field_22 > D_combustion_80130980[mem->field_20].field_6)) {
            release:
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            Gfx_RotMatrixY(&coord->coord, -(arg0->spawnArg1 * 80), 0);
            coord->flg = 0;
            break;
    }
}

/// One flame of the combustion burn. State 0 re-bases the effect coordinate on
/// the `GpEffWork.field_8` parent with an identity rotation and the work
/// block's `field_18`..`field_1C` offset, seeds the phase `field_22` from
/// `Gp_LcgState`, the radius `field_24` from `spawnArg1` and the intensity
/// `field_20` from `Gp_StateC08.field_0 % 10 - 1`, then splits: `spawnArg1`
/// past the `D_combustion_80130980` row's `field_4` runs the wide state 2,
/// anything smaller the narrow state 1. Both states redraw every frame -
/// `field_20 < 2` picks the small draw helper, otherwise the large one - and
/// one frame in four spawn a trailing ember that adopts this task as its
/// parent. Either state releases the effect once the player is dying
/// (`D_80114C0B`), the room is fading (`Gp_State1C`) or the flame has lived
/// 0x21 frames.
void func_combustion_8012F2BC(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    GpEffWork*     spawned;
    s32            rng;
    s32            spawnRng1;
    s32            spawnRng1b;
    s32            spawnRng2;
    s32            spawnRng2b;
    s32            last;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            rot        = (GpMtxWords*)&coord->coord;
            coord->sub = mem->field_8;
            rot->w0    = 0x1000;
            rot->w1    = 0;
            rot->w2    = 0x1000;
            rot->w3    = 0;
            rot->h4    = 0x1000;

            coord->coord.t[0] = mem->field_18;
            coord->coord.t[1] = mem->field_1A;
            coord->coord.t[2] = mem->field_1C;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);

            rng           = Gp_LcgState * 5 + 0x71357911;
            mem->field_22 = ((u32)rng >> 16) & 0xF;
            mem->field_24 = arg0->spawnArg1 * 32 + 512;
            mem->field_20 = Gp_StateC08.field_0 % 10 - 1;
            last          = D_combustion_80130980[mem->field_20].field_4;
            Gp_LcgState   = rng;
            if (last < arg0->spawnArg1) {
                arg0->state = 2;
                return;
            }
            arg0->state = 1;
            return;
        case 1:
            Gp_UpdateCoord(coord);
            if (mem->field_20 < 2) {
                func_combustion_8012F5EC(coord, mem->field_22, mem->field_24);
            } else {
                func_combustion_801305F8(coord, mem->field_22, mem->field_24);
            }
            if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4) || (mem->field_22 >= 0x21)) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            spawnRng1   = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = spawnRng1;
            if ((((u32)spawnRng1 >> 16) & 3) == 0) {
                spawnRng1b  = spawnRng1 * 5 + 0x71357911;
                Gp_LcgState = spawnRng1b;
                spawned     = Gp_SpawnEff(0x600A9, coord, ((u32)spawnRng1b >> 16) & 1, 0);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->field_20 < 2) {
                func_combustion_80130184(coord, mem->field_22, mem->field_24 * 3 / 2, 0);
            } else {
                func_combustion_80130184(coord, mem->field_22, mem->field_24 * 4, 0);
            }
            if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4) || (mem->field_22 >= 0x21)) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            spawnRng2   = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = spawnRng2;
            if ((((u32)spawnRng2 >> 16) & 3) == 0) {
                spawnRng2b  = spawnRng2 * 5 + 0x71357911;
                Gp_LcgState = spawnRng2b;
                spawned     = Gp_SpawnEff(0x600A9, coord, ((u32)spawnRng2b >> 16) & 1, 0);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
            }
            return;
    }
}

/// Links one frame of the small combustion flame at `arg0`'s world position.
/// The position is projected through `GsWSMATRIX` by a single `RTPS` and the
/// quad is dropped when that sets a negative `gte_stflg`. `arg1 % 6` picks one
/// of the six 0x20-wide texture frames on tpage 0x29 (CLUT 0x4282), and `arg2`
/// sizes it: the corners sit `arg2 * 31 / otz` from the projected centre.
/// Same 0x18-byte scratch and axis-aligned quad as `func_combustion_8012FF0C`.
void func_combustion_8012F5EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s16              x;
    s16              y;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpEffFt4Scratch*)(head - 0x18);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x4282;
        prim->v0    = 0x98;
        prim->v1    = 0x98;
        prim->v2    = 0xB7;
        prim->v3    = 0xB7;
        u0          = (s16)(arg1 % 6) * 0x20;
        u1          = u0 + 0x1F;
        prim->u1    = u1;
        prim->u3    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        block->size = (arg2 * 0x1F) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// One trailing ember shed by a `func_combustion_8012F2BC` flame. State 0 rolls
/// the kind from `Gp_StateC08.field_0 % 10 - 1` into `field_2A`, two
/// `Gp_LcgState` draws into the spin `field_24` and the per-frame rise
/// `field_12` (`-(rand & 0xFF) - kind * 64`, so bigger embers climb faster),
/// sizes the sprite as `kind * 0x100 + 0x300` in `field_26`, and enters
/// `spawnArg1 + 1` - or one state later on a coin flip when `kind >= 2`. Every
/// later state lifts the coordinate by `field_12` and redraws: state 1 steps
/// `field_20` every other frame and draws the shared `PeShared8012fb14` flame
/// on the odd frames, state 2 draws `func_combustion_8012FF0C` and state 3 the
/// small `func_combustion_8012F5EC`, each releasing the ember after eight (six
/// for state 3) frames.
void func_combustion_8012F888(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            rng;
    s32            rng2;
    s32            y;
    s16            step;
    s16            kind;
    s16            frame;
    s32            state;
    s32            tmp;
    s32            hi;
    s32            tmp2;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    state         = arg0->state;
    switch (state) {
        case 0:
            kind          = (Gp_StateC08.field_0 % 10U) - 1;
            rng           = Gp_LcgState * 5 + 0x71357911;
            rng2          = rng * 5 + 0x71357911;
            mem->field_24 = ((u32)rng2 >> 16) & 0xFFF;
            hi            = ((u32)rng >> 16) & 0xFF;
            mem->field_2A = kind;
            /* The global store between the `field_2A` store and its reload keeps
             * GCC from forwarding `kind` into the `lh`. */
            Gp_LcgState   = rng;
            tmp           = mem->field_2A;
            mem->field_12 = -hi - (tmp << 6);
            Gp_LcgState   = rng2;
            arg0->state   = arg0->spawnArg1 + 1;
            tmp2          = mem->field_2A;
            mem->field_26 = (tmp2 << 8) + 0x300;
            if (mem->field_2A >= 2) {
                Gp_LcgState  = rng2 * 5 + 0x71357911;
                arg0->state += ((u32)Gp_LcgState >> 16) & 1;
            }
            /* fallthrough */
        case 1:
            step              = mem->field_12;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->field_22 & 1)) {
                mem->field_20 = (u16)mem->field_20 + 1;
            }
            frame = mem->field_20;
            if (frame < 8) {
                if ((u16)mem->field_22 & 1) {
                    PeShared8012fb14(coord, frame, mem->field_26, mem->field_24);
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
        case 2:
            step              = mem->field_12;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            frame         = (u16)mem->field_20 + 1;
            mem->field_20 = frame;
            if (frame < 8) {
                func_combustion_8012FF0C(coord, frame, mem->field_26);
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        case 3:
            step              = mem->field_12;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            frame         = (u16)mem->field_20 + 1;
            mem->field_20 = frame;
            if (frame < 6) {
                func_combustion_8012F5EC(coord, frame, mem->field_26);
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}
