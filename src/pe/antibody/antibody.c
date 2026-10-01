#include "pe/antibody.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"

/// One 14-byte row of `D_antibody_80130BD4`, indexed by `EffectWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the effect scales with the combo
/// counter). `field_6` is the draw parameter `func_antibody_8012F734` seeds
/// `EffectWork.scale` with, and `field_8` is the base it is re-rolled from
/// on later frames (doubled in state 3). The remaining fields belong to the
/// draw helpers.
typedef struct AntibodyStep {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
    /* 0xA */ s16 field_A;
    /* 0xC */ s16 field_C;
} AntibodyStep;
STATIC_ASSERT_SIZEOF(AntibodyStep, 0xE);

/// 0x28-byte scratch block `func_antibody_80130428` takes from
/// the scratch stack to draw one antibody arc. `v0` is the effect
/// coordinate's world position and `v1` the player's second part coordinate;
/// both are projected through `GsWSMATRIX` with one `RTPS` each, giving
/// `sx0`/`sy0` and `sx1`/`sy1`. `flag` is the `gte_stflg` of whichever
/// projection ran last (a negative value drops the quad) and `otz` is the
/// first projection's `gte_stszotz`, incremented by 1 before it becomes both
/// the radius divisor and the OT bucket. `dx` / `dy` hold the current
/// `(arg2 * 23 / otz) * rsin|rcos(angle) >> 12` half-extents; only their low
/// halves are read back.
typedef struct AntibodyArcScratch {
    /* 0x00 */ SVECTOR v0;
    /* 0x08 */ SVECTOR v1;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ s32     dx;
    /* 0x1C */ s32     dy;
    /* 0x20 */ s16     sx0;
    /* 0x22 */ s16     sy0;
    /* 0x24 */ s16     sx1;
    /* 0x26 */ s16     sy1;
} AntibodyArcScratch;
STATIC_ASSERT_SIZEOF(AntibodyArcScratch, 0x28);

/// Per-level tuning for the antibody motes, one row per PE level 1-3,
/// weakest first.
static AntibodyStep D_antibody_80130BD4[] = {
    { 0x0008, 0x0090, 0x0005, 0x0200, 0x0080, 0x0600, 0x0008 },
    { 0x000C, 0x00C0, 0x0006, 0x0300, 0x0100, 0x0700, 0x0006 },
    { 0x0010, 0x00F0, 0x0007, 0x0400, 0x0180, 0x0800, 0x0004 },
};

/// The `SndEvt_EnqueueType6` id for each `D_antibody_80130BD4` row, played
/// once when `func_antibody_8012EF34` seeds the cast.
static s32 D_antibody_80130C00[] = { 0xE0290001, 0xE02C0001, 0xE02F0001 };

static void func_antibody_8012FBB0(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_antibody_8012FFEC(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_antibody_80130428(GfxCoord* arg0, s16 arg1, s16 arg2);

/// Sixteen wedge yaws, refilled once per cast by `func_antibody_8012EF34`.
/// Entry `i` is `i * (0x1000 / field_0)` plus a 9-bit `gRandomLcgState` draw;
/// states 1 and 2 pass one yaw per frame to `glowDrawWedge`.
static s16 D_antibody_80130C0C[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/// Runs one frame of an antibody cast. `Task::spawnArg2` is the `EffectWork`
/// block and `Task::extra` reaches the effect coordinate. Cancel
/// (`Gp_StateC08.field_3 == -2` or `gRoomEffectState->peEffectControl >= 4`) releases the
/// work block.
///
/// State 0 parents the coordinate with an identity rotation at the origin,
/// seeds `index` from the combo counter, refills `D_antibody_80130C0C`
/// with one yaw per wedge, and plays the row's cue. State 1 grows the draw
/// parameter `scale` by the row's `field_4`, draws three rings plus the
/// `field_0` wedges (and an arc above the weakest row), and for the first
/// 0x14 ticks spawns four `0x600F5` motes on a `field_A`-radius circle every
/// `field_C` frames, reparenting each onto this task. Once `scale` passes
/// the row's `field_2` cap it spawns the `0x800600AC` burst, latches
/// `period` and moves to state 2, which shrinks `scale` by 0x10 a frame
/// and redraws at the capped radius until it drops below 0x11.

void func_antibody_8012EF34(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GpStateC08* state;
    s32         i;
    u8          rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((state->field_3 != -2) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0: {
                GfxRotationWords* rot;

                rot                 = (GfxRotationWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00M01         = ONE;
                rot->m02M10         = 0;
                rot->m11M12         = ONE;
                rot->m20M21         = 0;
                rot->m22            = ONE;
                coord->coord.t[2]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[0]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                gRoomEffectState->peFxFlags &= (u16)~ROOM_EFFECT_PE_ANTIBODY_AURA;
                state->field_6              |= 8;
                arg0->state                  = 1;
                mem->index                   = (Gp_StateC08.field_0 % 10) - 1;
                i                            = 0;
                if (D_antibody_80130BD4[mem->index].field_0 > 0) {
                    do {
                        s16* dst;
                        s32  lo;
                        s32  rng;

                        dst             = D_antibody_80130C0C;
                        lo              = i * (0x1000 / D_antibody_80130BD4[mem->index].field_0);
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        dst[i]          = lo + (((u32)rng >> 16) & 0x1FF);
                        gRandomLcgState = rng;
                    } while (++i < D_antibody_80130BD4[mem->index].field_0);
                }
                {
                    s32 pan;

                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(D_antibody_80130C00[mem->index], pan,
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                }
                return;
            }
            case 1: {
                AntibodyStep* table;
                AntibodyStep* t2;
                EffectWork*   eff;
                s32           rng;
                s16           ang;
                s16*          p;
                s16           count;

                table               = D_antibody_80130BD4;
                mem->scale          = mem->scale + table[mem->index].field_4;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = (u8)mem->scale;
                rgb[2]              = mem->scale >> 1;
                coord->coord.t[1]   = -0x400;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(mem->scale * 4), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 8), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 0xC), rgb);
                if (mem->index != 0) {
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)(mem->scale * 8), 0x80, rgb);
                }
                i     = 0;
                count = table[mem->index].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_antibody_80130C0C;
                    do {
                        glowDrawWedge(coord, (s16)(mem->scale * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].field_0);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (mem->age < 0x14) {
                    if ((mem->age % D_antibody_80130BD4[mem->index].field_C) == 1) {
                        i = 0;
                        do {
                            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            ang             = i + (((u32)rng >> 16) & 0x3FF);
                            gRandomLcgState = rng;
                            mem->angle      = ang;
                            mem->move.vx =
                                (D_antibody_80130BD4[mem->index].field_A * rsin(ang)) >> 12;
                            mem->move.vz = (D_antibody_80130BD4[mem->index].field_A *
                                            rcos(mem->angle)) >>
                                           12;
                            eff = Gp_SpawnEff(0x600F5, coord, 0, &mem->move);
                            if (eff != NULL) {
                                Task_Reparent(arg0, eff->task);
                            }
                            i += 0x400;
                        } while (i < 0x1000);
                    }
                }
                if (mem->scale > D_antibody_80130BD4[mem->index].field_2) {
                    Gp_SpawnEff(0x800600AC, coord, 0, 0);
                    mem->period = mem->scale;
                    arg0->state = 2;
                }
                return;
            }
            case 2: {
                AntibodyStep* table;
                AntibodyStep* t2;
                s16*          p;
                s16           count;

                if (mem->scale < 0x11) {
                    goto release;
                }
                mem->scale          = mem->scale - 0x10;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = (u8)mem->scale;
                rgb[2]              = mem->scale >> 1;
                coord->coord.t[1]   = -0x400;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                table = D_antibody_80130BD4;
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 4), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 8), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 0xC), rgb);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        mem->period = mem->period + table[mem->index].field_4;
                    }
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)(mem->period * 8), 0x80, rgb);
                }
                i     = 0;
                count = D_antibody_80130BD4[mem->index].field_0;
                if (count > 0) {
                    t2 = D_antibody_80130BD4;
                    p  = D_antibody_80130C0C;
                    do {
                        glowDrawWedge(coord, (s16)(mem->period * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].field_0);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                return;
            }
        }
        return;
    }
release:
    effectKillTask(mem, arg0);
}

/// Runs one frame of an antibody mote. State 0 re-bases the effect coordinate
/// on the `EffectWork.parent` parent with an identity rotation and the work
/// block's `pos` offset, then GPF-scales that offset by 0x100
/// (a sixteenth) into `move` as the per-frame step, and seeds
/// the intensity `index` from the combo counter, the draw parameter
/// `scale` from that row's `field_6` and the phase `angle` from
/// `gRandomLcgState`. State 1 walks the coordinate back down that step every frame
/// and draws with `func_antibody_8012FBB0`; past tick 0x10 it parks a `-0x80`
/// Y drift in `move.vy` and moves to state 2, and one frame in sixteen it
/// jumps straight to state 3 instead. State 2 applies that Y drift and keeps
/// drawing; state 3 draws the larger `func_antibody_8012FFEC` /
/// `func_antibody_80130428` pair. All three re-roll `scale` / `angle`
/// from the row's `field_8` one frame in eight, and states 2 and 3 release the
/// effect at tick 0x15.
void func_antibody_8012F734(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s32               rng0;
    s32               rng1a;
    s32               rng1b;
    s32               rng1c;
    s32               rng1d;
    s32               rng2a;
    s32               rng2b;
    s32               rng2c;
    s32               rng3a;
    s32               rng3b;
    s32               rng3c;
    s16               idx;

    mem                 = arg0->spawnArg2.pointer;
    coord               = arg0->extra.coordBody->coord;
    mem->age            = mem->age + 1;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (arg0->state) {
        case 0:
            rot           = (GfxRotationWords*)&coord->coord;
            coord->parent = mem->parent;
            rot->m00M01   = ONE;
            rot->m02M10   = 0;
            rot->m11M12   = ONE;
            rot->m20M21   = 0;
            rot->m22      = ONE;

            coord->coord.t[0] = mem->pos.vx;
            coord->coord.t[1] = mem->pos.vy;
            coord->coord.t[2] = mem->pos.vz;

            gte_lddp(0x100);
            gte_ldsv(&mem->pos);
            gte_gpf12();
            gte_stsv(&mem->move);

            arg0->state     = 1;
            rng0            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng0;
            idx             = Gp_StateC08.field_0 % 10 - 1;
            mem->index      = idx;
            mem->scale      = D_antibody_80130BD4[idx].field_6;
            mem->angle      = ((u32)rng0 >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            rng1a           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng1a;
            if ((((u32)rng1a >> 16) & 7) == 0) {
                rng1b           = rng1a * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng1b;
                mem->scale =
                    D_antibody_80130BD4[mem->index].field_8 + (((u32)rng1b >> 16) & 0x1FF);
                rng1c           = rng1b * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng1c;
                mem->angle      = ((u32)rng1c >> 16) & 0xFFF;
            }
            coord->coord.t[0]  -= mem->move.vx;
            coord->coord.t[1]  -= mem->move.vy;
            coord->coord.t[2]  -= mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            func_antibody_8012FBB0(coord, mem->age, mem->scale, mem->angle);
            if (mem->age >= 0x10) {
                mem->move.vy = -0x80;
                arg0->state  = 2;
                return;
            }
            rng1d           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng1d;
            if ((((u32)rng1d >> 16) & 0xF) == 0) {
                arg0->state = 3;
            }
            return;
        case 2:
            rng2a           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng2a;
            if ((((u32)rng2a >> 16) & 7) == 0) {
                rng2b           = rng2a * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng2b;
                mem->scale =
                    D_antibody_80130BD4[mem->index].field_8 + (((u32)rng2b >> 16) & 0x1FF);
                rng2c           = rng2b * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng2c;
                mem->angle      = ((u32)rng2c >> 16) & 0xFFF;
            }
            coord->coord.t[1]  += mem->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            func_antibody_8012FBB0(coord, mem->age, mem->scale, mem->angle);
            goto check;
        case 3:
            rng3a           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng3a;
            if ((((u32)rng3a >> 16) & 7) == 0) {
                rng3b           = rng3a * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng3b;
                mem->scale      = (s16)D_antibody_80130BD4[mem->index].field_8 * 2 +
                             (((u32)rng3b >> 16) & 0x1FF);
                rng3c           = rng3b * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng3c;
                mem->angle      = ((u32)rng3c >> 16) & 0xFFF;
            }
            Gp_UpdateCoord(coord);
            func_antibody_8012FFEC(coord, mem->age, mem->scale, mem->angle);
            func_antibody_80130428(coord, mem->age, mem->scale);
        check:
            if (mem->age >= 0x15) {
                effectKillTask(mem, arg0);
            }
            break;
    }
}

/// Draws one antibody mote as a semi-transparent raw-tex `POLY_FT4` (tpage
/// 0x29, clut 0x42C6) centred on `arg0`'s world translation, projected with a
/// single `RTPS`. `arg1` picks one of six 40-pixel columns, `arg2` is the
/// radius and `arg3` the spin angle. The quad's corners are that radius
/// rotated by `arg3` and by `arg3 + 0x400`; nothing is drawn if the centre
/// projects off-screen.
static void func_antibody_8012FBB0(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    u16              vz;
    s32              u;
    s32              ang2;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(GpFxQuadScratch)     = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x42C6;
        prim->v0    = 0x50;
        prim->v1    = 0x50;
        prim->v2    = 0x77;
        prim->v3    = 0x77;
        u           = (s16)(arg1 % 6) * 40;
        prim->u0    = u;
        prim->u1    = u + 0x27;
        prim->u2    = u;
        prim->u3    = u + 0x27;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        ang2        = arg3 + 0x400;
        prim->y3    = block->sy + (u16)block->dy;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(ang2)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(ang2)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Draws one antibody mote as a semi-transparent raw-tex `POLY_FT4`
/// (tpage 0x2A, clut 0x42C9). The effect coordinate's world position is
/// projected through `GsWSMATRIX` with one `RTPS`; the quad is a square laid
/// around that point, `arg3` giving the spin applied at that angle and at
/// `+ 0x400`. `arg1 % 6` selects one of six 40x40 texel tiles along the
/// sprite sheet row at v = 0x38..0x5F. `arg2` is a signed half-extent, so the
/// on-screen half-diagonal is `arg2 * 39 / otz`. Nothing is drawn if the
/// projection sets a negative `gte_stflg`.
static void func_antibody_8012FFEC(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    u16              vz;
    s16              tile;
    s32              u0;
    s32              u1;
    s32              ang2;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(GpFxQuadScratch)     = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        block->otz     = block->otz + 1;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C9;
        tile        = arg1 % 6;
        u0          = tile * 0x28;
        u1          = u0 + 0x27;
        setUV4(prim, u0, 0x38, u1, 0x38, u0, 0x5F, u1, 0x5F);
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        ang2      = arg3 + 0x400;
        prim->y3  = block->sy + (u16)block->dy;
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GpFxQuadScratch));
}

/// Draws the antibody arc between the effect and the player as one
/// semi-transparent raw-tex `POLY_FT4` (tpage 0x28, clut 0x42C8). The effect
/// coordinate's world position and the player's second part coordinate are
/// each projected through `GsWSMATRIX` with one `RTPS`; the quad is laid
/// along the line joining the two projected points, `ratan2` of their screen
/// delta giving the spin applied at that angle and at `+ 0x400`. `arg1`
/// selects the 128-texel UV tile: u = `(arg1 & 1) * 128`, v =
/// `((arg1 & 3) >> 1) * 24 - 0x30`. `arg2` is a signed half-extent, so the
/// on-screen half-width is `arg2 * 23 / otz`. Nothing is drawn if either
/// projection sets a negative `gte_stflg`.
static void func_antibody_80130428(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                 head;
    AntibodyArcScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    GfxCoord*           player;
    s32                 u0;
    s32                 u1;
    s32                 va;
    s32                 vb;
    s16                 ang;
    s32                 ang2;
    u16                 vz;

    player                                      = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1];
    head                                        = SCRATCH_STACK_CURSOR(u8);
    ((AntibodyArcScratch*)(head - 0x28))->v0.vx = (u16)arg0->workm.t[0];
    block                                       = (AntibodyArcScratch*)(head - 0x28);
    block->v0.vy                                = (u16)arg0->workm.t[1];
    block->v0.vz                                = (u16)arg0->workm.t[2];
    block->v1.vx                                = (u16)player->workm.t[0];
    block->v1.vy                                = (u16)player->workm.t[1];
    vz                                          = (u16)player->workm.t[2];
    SCRATCH_STACK_CURSOR(AntibodyArcScratch)    = block;
    block->v1.vz                                = vz;
    vec                                         = &block->v0;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((AntibodyArcScratch*)(head - 0x28))->sx0);
    gte_stflg(&((AntibodyArcScratch*)(head - 0x28))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((AntibodyArcScratch*)(head - 0x28))->otz);
        block->otz = block->otz + 1;
        gte_ldv0(&((AntibodyArcScratch*)(head - 0x28))->v1);
        gte_rtps();
        gte_stsxy(&((AntibodyArcScratch*)(head - 0x28))->sx1);
        gte_stflg(&((AntibodyArcScratch*)(head - 0x28))->flag);
        if (block->flag >= 0) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x42C8;
            u0          = (arg1 & 1) << 7;
            u1          = u0 + 0x7F;
            va          = ((arg1 & 3) >> 1) * 24 - 0x30;
            vb          = ((arg1 & 3) >> 1) * 24 - 0x19;
            setUV4(prim, u0, va, u1, va, u0, vb, u1, vb);
            ang       = ratan2(block->sy1 - block->sy0, block->sx1 - block->sx0);
            block->dx = (((arg2 * 0x17) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 0x17) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = (u16)block->sx0 + (u16)block->dx;
            prim->x3  = (u16)block->sx1 - (u16)block->dx;
            prim->y0  = (u16)block->sy0 - (u16)block->dy;
            ang2      = ang + 0x400;
            prim->y3  = (u16)block->sy1 + (u16)block->dy;
            block->dx = (((arg2 * 0x17) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 0x17) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = (u16)block->sx1 + (u16)block->dx;
            prim->x2  = (u16)block->sx0 - (u16)block->dx;
            prim->y1  = (u16)block->sy1 - (u16)block->dy;
            prim->y2  = (u16)block->sy0 + (u16)block->dy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(AntibodyArcScratch));
}

#include "../../shared/glow_draw_wedge.inc.c"
