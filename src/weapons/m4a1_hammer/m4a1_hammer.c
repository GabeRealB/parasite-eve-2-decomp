#include "weapons/m4a1_hammer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "m4a1_hammer_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Signed texture-frame counter for the six-cell sprite's forward declaration.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"
#include "../../shared/beam_strip.h"

/// Charge-flare instance of `spriteQuadDraw`.
///
/// `pos` is three cached translation words. `frame` walks the eight-cell strip,
/// `size` is the unsigned perspective numerator and `angle` is the spin in
/// 4096 units per turn.
static void spriteQuadDrawCharge(const long* pos, u16 frame, u16 size, s16 angle);

/// Fixed offset from the parent coordinate that the hammer effect starts at.
static SVECTOR D_m4a1_hammer_8011EB60 = { 0, 0x280, 0x20, 0 };

/// Per-frame task for the hammer's charge flare. `Task::spawnArg2` is the
/// `EffectWork`, `Task::extra` reaches the coordinate the flare
/// hangs on, and `Task::spawnArg1` is the charge phase the firing code drives.
/// Hidden effects (`gRoomEffectState->effectControl` >=
/// `ROOM_EFFECT_CONTROL_HIDDEN`), and the player being in the state flagged by
/// `TmdObject::flags & 0x80`, freeze the task outright.
///
/// - State 0 hangs the coordinate off `EffectWork::parent` at the fixed offset
///   `D_m4a1_hammer_8011EB60` with an identity rotation, publishes the task as
///   `D_m4a1_hammer_8012D660` and moves to state 1.
/// - State 1 first republishes the flare's world position as
///   `D_m4a1_hammer_8012D668`, then dispatches on the charge phase. Phase 1
///   idles the flare: it re-rolls the spin angle every 16 frames and the radius
///   every frame, draws it on even frames and refreshes transient light slot 1 as a
///   narrow (`0x80` / `0x400`) light. Phase 2 charges: on the first frame it
///   seeds the eight sparks in `D_m4a1_hammer_8012D630`, and on every even
///   frame it walks each spark, rotates its offset through the flare's frame
///   and draws it, then widens the light to `0x400` / `0x4000`; five charge
///   frames drop back to phase 1. Phase 3 tears the flare down. A room fade
///   winds `age` back down and redraws instead of advancing.
void func_m4a1_hammer_8011D1E0(Task* task)
{
    EffectWork*                    work;
    GfxCoord*                      coord;
    GfxCoord*                      light;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    GfxRotationWords*              dstm;
    s32                            i;
    s32                            j;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[1];
    light     = &lightSlot->light.head.transform.coord;
    slot      = &lightSlot->light;

    if (((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) == 0 && gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age = work->age + 1;
        switch (task->state) {
            case 0:
                dstm                = (GfxRotationWords*)&coord->coord;
                coord->parent       = work->parent;
                dstm->m00M01        = ONE;
                dstm->m11M12        = ONE;
                dstm->m22           = ONE;
                dstm->m02M10        = 0;
                dstm->m20M21        = 0;
                coord->coord.t[0]   = D_m4a1_hammer_8011EB60.vx;
                coord->coord.t[1]   = D_m4a1_hammer_8011EB60.vy;
                coord->coord.t[2]   = D_m4a1_hammer_8011EB60.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;

                D_m4a1_hammer_8012D660 = task;
                Gp_UpdateCoord(coord);
                task->state = 1;
                return;
            case 1:
                D_m4a1_hammer_8012D668.vx = coord->workm.t[0];
                D_m4a1_hammer_8012D668.vy = coord->workm.t[1];
                D_m4a1_hammer_8012D668.vz = coord->workm.t[2];
                switch (task->spawnArg1.value) {
                    case 0:
                        break;
                    case 1:
                        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                            work->age = work->age - 1;
                            if ((work->age & 1) == 0) {
                                spriteQuadDrawCharge(coord->workm.t, work->age >> 1, work->period,
                                                     work->angle);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if ((work->age & 0xF) == 0) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            work->angle     = (gRandomLcgState >> 16) & 0xFFF;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->period    = ((gRandomLcgState >> 16) & 0xFF) + 0xC0;
                        if ((work->age & 1) == 0) {
                            spriteQuadDrawCharge(coord->workm.t, work->age >> 1, work->period,
                                                 work->angle);
                        }
                        lightSlot->framesLeft = 4;
                        slot->inner           = 0x80;
                        slot->outer           = 0x400;
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        slot->head.color.b    = ((gRandomLcgState >> 16) & 0x700) + 0x400;
                        slot->head.color.r    = (u16)slot->head.color.b >> 1;
                        slot->head.color.g    = (u16)slot->head.color.b >> 1;
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                        light->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->index         = 0;
                        return;
                    case 2:
                        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                            work->age = work->age - 1;
                            if ((work->age & 1) == 0) {
                                spriteQuadDraw(coord, work->age >> 1, work->period,
                                               work->angle);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if (work->index == 0) {
                            for (i = 0; i < 8; i++) {
                                gRandomLcgState                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                D_m4a1_hammer_8012D630[i]      = (i << 9) + ((gRandomLcgState >> 16) & 0x1FF);
                                gRandomLcgState                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                D_m4a1_hammer_8012D630[i + 8]  = ((gRandomLcgState >> 16) & 0x7FF) + 0x200;
                                gRandomLcgState                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                D_m4a1_hammer_8012D630[i + 16] = (gRandomLcgState >> 16) & 0x3FF;
                            }
                        }
                        if ((work->age & 0xF) == 0) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            work->angle     = (gRandomLcgState >> 16) & 0xFFF;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->period    = ((gRandomLcgState >> 16) & 0x3FF) + 0x400;
                        if ((work->age & 1) == 0) {
                            spriteQuadDraw(coord, work->age >> 1, work->period, work->angle);
                            for (i = 0; i < 8; i++) {
                                j                          = i + 8;
                                gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                D_m4a1_hammer_8012D630[i] -= ((gRandomLcgState >> 16) & 0x1FF) - 0x100;
                                gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                D_m4a1_hammer_8012D630[j] += (gRandomLcgState >> 16) & 0xFF;
                                work->pos.vx =
                                    (D_m4a1_hammer_8012D630[i + 16] * rsin(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->pos.vz =
                                    (D_m4a1_hammer_8012D630[i + 16] * rcos(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->pos.vy = D_m4a1_hammer_8012D630[j];
                                gte_SetRotMatrix(&coord->workm);
                                gte_ldv0(&work->pos);
                                gte_rtv0();
                                gte_stsv(&work->pos);
                                work->pos.vx = work->pos.vx + (u16)D_m4a1_hammer_8012D668.vx;
                                work->pos.vy = work->pos.vy + (u16)D_m4a1_hammer_8012D668.vy;
                                work->pos.vz = work->pos.vz + (u16)D_m4a1_hammer_8012D668.vz;
                                beamStripDraw(coord, &work->pos, work->age, 0x280);
                            }
                        }
                        lightSlot->framesLeft = 4;
                        slot->inner           = 0x400;
                        slot->outer           = 0x4000;
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        slot->head.color.b    = ((gRandomLcgState >> 16) & 0x700) + 0x800;
                        slot->head.color.r    = (u16)slot->head.color.b >> 1;
                        slot->head.color.g    = slot->head.color.b >> 1;
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                        light->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->index         = work->index + 1;
                        if (work->index >= 5) {
                            task->spawnArg1.value = 1;
                        }
                        return;
                    case 3:
                        effectKillTask(work, task);
                        return;
                }
                return;
        }
    }
}

#undef SPRITE_QUAD_POSITION_SOURCE_TYPE
#undef SPRITE_QUAD_POS
#undef SPRITE_QUAD_FRAME_T
#undef SPRITE_QUAD_SIZE_T
/// Read-only XYZ translation words from the charge sprite's `GfxCoord::workm.t`.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const long
#define SPRITE_QUAD_POS(p, i)            ((p)[i])
/// Unsigned texture-frame counter, masked to the charge sprite's eight cells.
#define SPRITE_QUAD_FRAME_T u16
/// Unsigned 16-bit perspective size for the charge flare, matching its forward declaration.
#define SPRITE_QUAD_SIZE_T u16
/// Defines the charge-flare instance. The six-cell strip below is unbound and becomes `spriteQuadDraw`.
#define SPRITE_QUAD_FUNC spriteQuadDrawCharge
/// Packed additive charge-flare page: 4-bit indexed texels at VRAM X=512 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 512, 0)
/// Charge-flare palette: VRAM X=192 words, Y=268 scanlines.
#define SPRITE_QUAD_CLUT getClut(192, 268)
/// Texel width and horizontal stride of each cell in the charge flare's eight-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 24
#define SPRITE_QUAD_CELL_MASK  7
/// Inclusive top texel row of the charge-flare strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x88
#define SPRITE_QUAD_V1    0x9F
/// Perspective-sizing multiplier for Hammer's charge flare.
///
/// Uses the cell's inclusive 23-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"
#undef SPRITE_QUAD_POSITION_SOURCE_TYPE
#undef SPRITE_QUAD_POS
#undef SPRITE_QUAD_FRAME_T
#undef SPRITE_QUAD_SIZE_T
/// Read-only coordinate source for the six-cell sprite's cached translation.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const GfxCoord
#define SPRITE_QUAD_POS(p, i)            ((p)->workm.t[i])
/// Restore the signed counter for the repeating six-cell sprite strip.
#define SPRITE_QUAD_FRAME_T s16
/// Restore signed 16-bit perspective sizing for the six-cell sprite strip.
#define SPRITE_QUAD_SIZE_T s16

void func_m4a1_hammer_8011DD08(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    mem->age++;
    switch (arg0->state) {
        case 0:
            taskReparent(D_m4a1_hammer_8012D660, arg0);
            if (arg0->spawnArg1.value != 0) {
                parent              = mem->parent;
                coord->coord.t[0]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[2]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                coord->parent       = parent;
                Gp_UpdateCoord(coord);
                arg0->state = 1;
            }
            mem->scale      = 0x80;
            gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            if (mem->age & 1) {
                spriteQuadDraw(coord, ++mem->index, 0x400, mem->angle);
                if (mem->age < 8) {
                    beamStripDraw(coord, &D_m4a1_hammer_8012D668, mem->index, 0x280);
                }
            }
            if (mem->age >= 0x19) {
                effectKillTask(mem, arg0);
            }
            break;
    }
}

/// Texel width and horizontal stride of each cell in Hammer's repeating six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in Hammer's repeating sprite row, separate from its masked charge strip.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of Hammer's repeating strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for Hammer's repeating sprite strip.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Repeating-strip palette: VRAM X=304 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(304, 266)
#include "../../shared/sprite_quad_draw.inc.c"

#include "../../shared/beam_strip_draw.inc.c"

static TmdBone _gM4a1HammerModel01E10Skeleton[1] = {
#include "assets/m4a1_hammer_model_01E10_skeleton.inc"
};

static u32 _gM4a1HammerModel01E10PartVerts[1] = {
#include "assets/m4a1_hammer_model_01E10_partVerts.inc"
};

static SVECTOR _gM4a1HammerModel01E10Verts[70] = {
#include "assets/m4a1_hammer_model_01E10_verts.inc"
};

static SVECTOR _gM4a1HammerModel01E10Normals[66] = {
#include "assets/m4a1_hammer_model_01E10_normals.inc"
};

static u32 _gM4a1HammerModel01E10Stream[490] = {
#include "assets/m4a1_hammer_model_01E10_stream.inc"
};

TmdSource D_m4a1_hammer_8011F778 = {
    0,
    3564,
    0,
    1,
    _gM4a1HammerModel01E10PartVerts,
    _gM4a1HammerModel01E10Verts,
    _gM4a1HammerModel01E10Normals,
    _gM4a1HammerModel01E10Skeleton,
    _gM4a1HammerModel01E10Stream,
};
