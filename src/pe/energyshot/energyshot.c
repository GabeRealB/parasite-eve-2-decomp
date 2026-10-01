#include "pe/energyshot.h"

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
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"

/// One 8-byte row of `D_energyshot_801300E4`, indexed by `EffectWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the wedge count. `field_2` is
/// the brightness cap state 1 grows `EffectWork.scale` toward (and the ring
/// radius in state 2). `field_4` is the per-frame brightness step. `field_6` is
/// the beam depth / spawn height.
typedef struct EnergyShotScale {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ s16 field_6;
} EnergyShotScale;
STATIC_ASSERT_SIZEOF(EnergyShotScale, 8);

/// Per-level tuning for the energy shot: rows are PE levels 1-3.
static EnergyShotScale D_energyshot_801300E4[] = {
    { 0x0008, 0x0090, 0x0005, 0x0400 },
    { 0x000C, 0x00C0, 0x0006, 0x0500 },
    { 0x0010, 0x00F0, 0x0007, 0x0600 },
};

/// The `SndEvt_EnqueueType6` id for each `D_energyshot_801300E4` row.
static s32 D_energyshot_801300FC[] = { 0xE02A0001, 0xE02D0001, 0xE0300001 };

static void func_energyshot_8012FA50(GfxCoord* arg0, s16 arg1, s16 arg2, u8* arg3);

/// Sixteen per-vertex texture-frame offsets, refilled once per cast by
/// `func_energyshot_8012EF34` and consumed by the GTE pass in
/// `func_energyshot_8012FA50`, where each is added to `gDisplayState.animFrame`
/// and reduced mod 6 to pick one of the six 0x28-wide frames of the beam
/// texture.
static s16 D_energyshot_80130108[16];
/// Sixteen wedge yaws, refilled once per cast by `func_energyshot_8012EF34`
/// from `gRandomLcgState`. Entry `i` is `i * (0x1000 / field_0)` plus a 9-bit LCG
/// draw. States 1 and 2 pass one yaw per frame to `glowDrawWedge`.
static s16 D_energyshot_80130128[16];

/// Energy shot PE. `Task::spawnArg2` is the `EffectWork` block; `Task::extra`
/// reaches the coordinate. Cancel (`Gp_StateC08.field_3 == -2` or
/// `gRoomEffectState->peEffectControl >= 4`) releases the work block.
///
/// State 0 parents the coordinate, seeds 16 texture-frame offsets and 16 wedge
/// yaws from `gRandomLcgState`, and plays the combo-indexed cue. State 1 grows
/// brightness / radius, draws three rings plus `field_0` wedges and the beam,
/// and parents a `0x600F4` spark; once brightness exceeds the row cap it
/// advances to state 2, which shrinks brightness until it drops below 0x11.
void func_energyshot_8012EF34(Task* arg0)
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
                RoomEffectState*  effectState;
                s16               count;
                u16               level;

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
                state->field_6           |= 8;
                effectState               = gRoomEffectState;
                effectState->burstRequest = false;
                effectState->peFxFlags   &= (u16)~ROOM_EFFECT_PE_ENERGY_SHOT_AURA;
                arg0->state               = 1;
                mem->index                = (Gp_StateC08.field_0 % 10) - 1;
                i                         = 0;
                {
                    s16* frames;

                    frames = D_energyshot_80130108;
                    do {
                        s32 rng;

                        i              += 1;
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        *frames         = ((u32)rng >> 16) & 0xFF;
                        frames         += 1;
                        gRandomLcgState = rng;
                    } while (i < 0x10);
                }
                i = 0;
                {
                    EnergyShotScale* tbl;

                    tbl   = D_energyshot_801300E4;
                    count = tbl[mem->index].field_0;
                    level = mem->index;
                    if (count > 0) {
                        do {
                            s32 lo;
                            s32 rng;

                            lo                       = i * (0x1000 / D_energyshot_801300E4[(s16)level].field_0);
                            rng                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            D_energyshot_80130128[i] = lo + (((u32)rng >> 16) & 0x1FF);
                            i                       += 1;
                            gRandomLcgState          = rng;
                            count                    = D_energyshot_801300E4[mem->index].field_0;
                            level                    = mem->index;
                        } while (i < count);
                    }
                }
                {
                    s32 pan;

                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(D_energyshot_801300FC[mem->index], pan,
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                }
                return;
            }
            case 1: {
                EnergyShotScale* table;
                EnergyShotScale* t2;
                s32              rng;
                s16              ang;
                s16*             p;
                s16              count;

                table               = D_energyshot_801300E4;
                mem->scale          = mem->scale + table[mem->index].field_4;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = mem->scale >> 1;
                rgb[2]              = (u8)mem->scale;
                coord->coord.t[1]   = -table[mem->index].field_6;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(mem->scale * 4), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 8), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 0xC), rgb);
                i     = 0;
                count = table[mem->index].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        glowDrawWedge(coord, (s16)(mem->scale * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].field_0);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        func_energyshot_8012FA50(coord, (s16)(mem->scale * 8),
                                                 (s16)(u16)D_energyshot_801300E4[2].field_6 >> 1, rgb);
                    }
                    func_energyshot_8012FA50(
                        coord, (s16)(mem->scale * 4),
                        (u16)D_energyshot_801300E4[mem->index].field_6 * 2, rgb);
                }
                func_energyshot_8012FA50(
                    coord, (s16)(mem->scale * 6),
                    (u16)D_energyshot_801300E4[mem->index].field_6 - 0x100, rgb);
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                ang             = ((u32)rng >> 16) & 0xFFF;
                gRandomLcgState = rng;
                mem->angle      = ang;
                mem->move.vx    = (u32)(rsin(ang) * mem->scale * 3) >> 11;
                mem->move.vz    = (u32)(rcos(mem->angle) * mem->scale * 3) >> 11;
                Gp_SpawnEff(0x600F4, coord,
                            D_energyshot_801300E4[mem->index].field_6 | 0x8000,
                            &mem->move);
                if (D_energyshot_801300E4[mem->index].field_2 < mem->scale) {
                    Gp_SpawnEff(0x800600F3, coord, 0, 0);
                    mem->period = mem->scale;
                    arg0->state = 2;
                }
                return;
            }
            case 2: {
                EnergyShotScale* table;
                EnergyShotScale* t2;
                s16*             p;
                s16              count;

                if (mem->scale < 0x11) {
                    effectKillTask(mem, arg0);
                    return;
                }
                mem->scale          = mem->scale - 0x10;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = mem->scale >> 1;
                rgb[2]              = (u8)mem->scale;
                table               = D_energyshot_801300E4;
                coord->coord.t[1]   = -table[mem->index].field_6;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 4), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 8), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 0xC), rgb);
                i     = 0;
                count = table[mem->index].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        glowDrawWedge(coord, (s16)(mem->period * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].field_0);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        mem->period =
                            mem->period + D_energyshot_801300E4[2].field_4;
                        func_energyshot_8012FA50(
                            coord, (s16)(mem->period * 8),
                            (s16)(u16)D_energyshot_801300E4[mem->index].field_6 >> 1,
                            rgb);
                    }
                    func_energyshot_8012FA50(
                        coord, (s16)(mem->period * 4),
                        (u16)D_energyshot_801300E4[mem->index].field_6 * 2, rgb);
                }
                func_energyshot_8012FA50(
                    coord, (s16)(mem->period * 6),
                    (u16)D_energyshot_801300E4[mem->index].field_6 - 0x100, rgb);
                return;
            }
        }
        return;
    }
    effectKillTask(mem, arg0);
}

#include "../../shared/glow_draw_wedge.inc.c"

/// Draws the energy shot's beam: an inner ring of radius `arg1 + 0x400` sunk
/// `arg2` along local Y and an outer ring of radius `arg1 / 2 + 0x100` in the
/// local XY plane are built by `rsin` / `rcos`, rotated by `arg0`'s `workm`
/// and offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one semi-transparent `POLY_FT4`. The texture cell
/// is one of six 0x28-wide frames picked per vertex by `D_energyshot_80130108`
/// plus the frame counter, the quad is tinted by the three bytes at `arg3`,
/// and a negative `gte_stflg` drops the segment.
static void func_energyshot_8012FA50(GfxCoord* arg0, s16 arg1, s16 arg2, u8* arg3)
{
    u8*            head;
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_FT4*      prim;
    s32            i;
    s32            next;
    s32            ang;
    s32            u;
    s16            idx;
    s16            r0;
    s16            r1;

    r1                         = arg1 / 2 + 0x100;
    r0                         = arg1 + 0x400;
    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - 0x118;
    block                      = (GpBandScratch*)(head - 0x118);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -arg2;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = (u16)block->inner[i].vx + (u16)arg0->workm.t[0];
        block->inner[i].vy = (u16)block->inner[i].vy + (u16)arg0->workm.t[1];
        block->inner[i].vz = (u16)block->inner[i].vz + (u16)arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = (u16)block->outer[i].vx + (u16)arg0->workm.t[0];
        op->vy             = (u16)op->vy + (u16)arg0->workm.t[1];
        op->vz             = (u16)op->vz + (u16)arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        idx = (u32)(D_energyshot_80130108[i] + gDisplayState.animFrame) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setRGB0(prim, arg3[0], arg3[1], arg3[2]);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x118);
}

void func_energyshot_8012FFB8(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         y;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        mem->move.vx    = 0;
        mem->move.vz    = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = 0xFFF0 - ((gRandomLcgState >> 16) & 0x3F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
        arg0->state     = 1;
    }

    y                   = coord->coord.t[1] + mem->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = y;
    Gp_UpdateCoord(coord);
    if ((mem->age & 3) == 0) {
        mem->index = mem->index + 1;
    }
    if (mem->index < 8) {
        Gp_DrawFxQuad(coord, mem->index, 0x400, mem->scale);
        return;
    }
    effectKillTask(mem, arg0);
}
