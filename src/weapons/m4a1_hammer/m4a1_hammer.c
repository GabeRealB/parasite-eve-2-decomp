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
#include "gameplay/animation.h"
#include "types.h"
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

static AnimationPackedPose _gM4a1HammerAnimation0276CBank1[2] = {
#include "assets/m4a1_hammer_animation_0276C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0276CBank4[8] = {
#include "assets/m4a1_hammer_animation_0276C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0276CRecords[76] = {
#include "assets/m4a1_hammer_animation_0276C_records.inc"
};

static u16 _gM4a1HammerAnimation0276CIndices[20] = {
#include "assets/m4a1_hammer_animation_0276C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0276C = {
    _gM4a1HammerAnimation0276CRecords,
    _gM4a1HammerAnimation0276CIndices,
    { NULL, _gM4a1HammerAnimation0276CBank1, NULL, NULL, _gM4a1HammerAnimation0276CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation02E10Bank1[12] = {
#include "assets/m4a1_hammer_animation_02E10_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation02E10Bank4[151] = {
#include "assets/m4a1_hammer_animation_02E10_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation02E10Records[218] = {
#include "assets/m4a1_hammer_animation_02E10_records.inc"
};

static u16 _gM4a1HammerAnimation02E10Indices[20] = {
#include "assets/m4a1_hammer_animation_02E10_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation02E10 = {
    _gM4a1HammerAnimation02E10Records,
    _gM4a1HammerAnimation02E10Indices,
    { NULL, _gM4a1HammerAnimation02E10Bank1, NULL, NULL, _gM4a1HammerAnimation02E10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation03670Bank1[19] = {
#include "assets/m4a1_hammer_animation_03670_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation03670Bank4[169] = {
#include "assets/m4a1_hammer_animation_03670_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation03670Records[290] = {
#include "assets/m4a1_hammer_animation_03670_records.inc"
};

static u16 _gM4a1HammerAnimation03670Indices[20] = {
#include "assets/m4a1_hammer_animation_03670_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation03670 = {
    _gM4a1HammerAnimation03670Records,
    _gM4a1HammerAnimation03670Indices,
    { NULL, _gM4a1HammerAnimation03670Bank1, NULL, NULL, _gM4a1HammerAnimation03670Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation03ED4Bank1[19] = {
#include "assets/m4a1_hammer_animation_03ED4_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation03ED4Bank4[170] = {
#include "assets/m4a1_hammer_animation_03ED4_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation03ED4Records[290] = {
#include "assets/m4a1_hammer_animation_03ED4_records.inc"
};

static u16 _gM4a1HammerAnimation03ED4Indices[20] = {
#include "assets/m4a1_hammer_animation_03ED4_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation03ED4 = {
    _gM4a1HammerAnimation03ED4Records,
    _gM4a1HammerAnimation03ED4Indices,
    { NULL, _gM4a1HammerAnimation03ED4Bank1, NULL, NULL, _gM4a1HammerAnimation03ED4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation041E8Bank1[3] = {
#include "assets/m4a1_hammer_animation_041E8_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation041E8Bank4[69] = {
#include "assets/m4a1_hammer_animation_041E8_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation041E8Records[99] = {
#include "assets/m4a1_hammer_animation_041E8_records.inc"
};

static u16 _gM4a1HammerAnimation041E8Indices[20] = {
#include "assets/m4a1_hammer_animation_041E8_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation041E8 = {
    _gM4a1HammerAnimation041E8Records,
    _gM4a1HammerAnimation041E8Indices,
    { NULL, _gM4a1HammerAnimation041E8Bank1, NULL, NULL, _gM4a1HammerAnimation041E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation04944Bank1[14] = {
#include "assets/m4a1_hammer_animation_04944_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation04944Bank4[156] = {
#include "assets/m4a1_hammer_animation_04944_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation04944Records[253] = {
#include "assets/m4a1_hammer_animation_04944_records.inc"
};

static u16 _gM4a1HammerAnimation04944Indices[20] = {
#include "assets/m4a1_hammer_animation_04944_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation04944 = {
    _gM4a1HammerAnimation04944Records,
    _gM4a1HammerAnimation04944Indices,
    { NULL, _gM4a1HammerAnimation04944Bank1, NULL, NULL, _gM4a1HammerAnimation04944Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation050CCBank1[16] = {
#include "assets/m4a1_hammer_animation_050CC_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation050CCBank4[167] = {
#include "assets/m4a1_hammer_animation_050CC_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation050CCRecords[247] = {
#include "assets/m4a1_hammer_animation_050CC_records.inc"
};

static u16 _gM4a1HammerAnimation050CCIndices[20] = {
#include "assets/m4a1_hammer_animation_050CC_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation050CC = {
    _gM4a1HammerAnimation050CCRecords,
    _gM4a1HammerAnimation050CCIndices,
    { NULL, _gM4a1HammerAnimation050CCBank1, NULL, NULL, _gM4a1HammerAnimation050CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation053A0Bank1[6] = {
#include "assets/m4a1_hammer_animation_053A0_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation053A0Bank4[52] = {
#include "assets/m4a1_hammer_animation_053A0_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation053A0Records[91] = {
#include "assets/m4a1_hammer_animation_053A0_records.inc"
};

static u16 _gM4a1HammerAnimation053A0Indices[20] = {
#include "assets/m4a1_hammer_animation_053A0_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation053A0 = {
    _gM4a1HammerAnimation053A0Records,
    _gM4a1HammerAnimation053A0Indices,
    { NULL, _gM4a1HammerAnimation053A0Bank1, NULL, NULL, _gM4a1HammerAnimation053A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation05730Bank1[7] = {
#include "assets/m4a1_hammer_animation_05730_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation05730Bank4[73] = {
#include "assets/m4a1_hammer_animation_05730_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation05730Records[114] = {
#include "assets/m4a1_hammer_animation_05730_records.inc"
};

static u16 _gM4a1HammerAnimation05730Indices[20] = {
#include "assets/m4a1_hammer_animation_05730_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation05730 = {
    _gM4a1HammerAnimation05730Records,
    _gM4a1HammerAnimation05730Indices,
    { NULL, _gM4a1HammerAnimation05730Bank1, NULL, NULL, _gM4a1HammerAnimation05730Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation05BB8Bank1[9] = {
#include "assets/m4a1_hammer_animation_05BB8_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation05BB8Bank4[104] = {
#include "assets/m4a1_hammer_animation_05BB8_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation05BB8Records[139] = {
#include "assets/m4a1_hammer_animation_05BB8_records.inc"
};

static u16 _gM4a1HammerAnimation05BB8Indices[20] = {
#include "assets/m4a1_hammer_animation_05BB8_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation05BB8 = {
    _gM4a1HammerAnimation05BB8Records,
    _gM4a1HammerAnimation05BB8Indices,
    { NULL, _gM4a1HammerAnimation05BB8Bank1, NULL, NULL, _gM4a1HammerAnimation05BB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation05DB4Bank1[3] = {
#include "assets/m4a1_hammer_animation_05DB4_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation05DB4Bank4[22] = {
#include "assets/m4a1_hammer_animation_05DB4_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation05DB4Records[76] = {
#include "assets/m4a1_hammer_animation_05DB4_records.inc"
};

static u16 _gM4a1HammerAnimation05DB4Indices[20] = {
#include "assets/m4a1_hammer_animation_05DB4_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation05DB4 = {
    _gM4a1HammerAnimation05DB4Records,
    _gM4a1HammerAnimation05DB4Indices,
    { NULL, _gM4a1HammerAnimation05DB4Bank1, NULL, NULL, _gM4a1HammerAnimation05DB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0608CBank1[6] = {
#include "assets/m4a1_hammer_animation_0608C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0608CBank4[57] = {
#include "assets/m4a1_hammer_animation_0608C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0608CRecords[87] = {
#include "assets/m4a1_hammer_animation_0608C_records.inc"
};

static u16 _gM4a1HammerAnimation0608CIndices[20] = {
#include "assets/m4a1_hammer_animation_0608C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0608C = {
    _gM4a1HammerAnimation0608CRecords,
    _gM4a1HammerAnimation0608CIndices,
    { NULL, _gM4a1HammerAnimation0608CBank1, NULL, NULL, _gM4a1HammerAnimation0608CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06330Bank1[4] = {
#include "assets/m4a1_hammer_animation_06330_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06330Bank4[55] = {
#include "assets/m4a1_hammer_animation_06330_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06330Records[82] = {
#include "assets/m4a1_hammer_animation_06330_records.inc"
};

static u16 _gM4a1HammerAnimation06330Indices[20] = {
#include "assets/m4a1_hammer_animation_06330_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06330 = {
    _gM4a1HammerAnimation06330Records,
    _gM4a1HammerAnimation06330Indices,
    { NULL, _gM4a1HammerAnimation06330Bank1, NULL, NULL, _gM4a1HammerAnimation06330Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06530Bank1[3] = {
#include "assets/m4a1_hammer_animation_06530_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06530Bank4[23] = {
#include "assets/m4a1_hammer_animation_06530_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06530Records[76] = {
#include "assets/m4a1_hammer_animation_06530_records.inc"
};

static u16 _gM4a1HammerAnimation06530Indices[20] = {
#include "assets/m4a1_hammer_animation_06530_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06530 = {
    _gM4a1HammerAnimation06530Records,
    _gM4a1HammerAnimation06530Indices,
    { NULL, _gM4a1HammerAnimation06530Bank1, NULL, NULL, _gM4a1HammerAnimation06530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06884Bank1[8] = {
#include "assets/m4a1_hammer_animation_06884_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06884Bank4[68] = {
#include "assets/m4a1_hammer_animation_06884_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06884Records[101] = {
#include "assets/m4a1_hammer_animation_06884_records.inc"
};

static u16 _gM4a1HammerAnimation06884Indices[20] = {
#include "assets/m4a1_hammer_animation_06884_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06884 = {
    _gM4a1HammerAnimation06884Records,
    _gM4a1HammerAnimation06884Indices,
    { NULL, _gM4a1HammerAnimation06884Bank1, NULL, NULL, _gM4a1HammerAnimation06884Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06B38Bank1[5] = {
#include "assets/m4a1_hammer_animation_06B38_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06B38Bank4[55] = {
#include "assets/m4a1_hammer_animation_06B38_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06B38Records[83] = {
#include "assets/m4a1_hammer_animation_06B38_records.inc"
};

static u16 _gM4a1HammerAnimation06B38Indices[20] = {
#include "assets/m4a1_hammer_animation_06B38_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06B38 = {
    _gM4a1HammerAnimation06B38Records,
    _gM4a1HammerAnimation06B38Indices,
    { NULL, _gM4a1HammerAnimation06B38Bank1, NULL, NULL, _gM4a1HammerAnimation06B38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06E58Bank1[6] = {
#include "assets/m4a1_hammer_animation_06E58_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06E58Bank4[66] = {
#include "assets/m4a1_hammer_animation_06E58_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06E58Records[96] = {
#include "assets/m4a1_hammer_animation_06E58_records.inc"
};

static u16 _gM4a1HammerAnimation06E58Indices[20] = {
#include "assets/m4a1_hammer_animation_06E58_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06E58 = {
    _gM4a1HammerAnimation06E58Records,
    _gM4a1HammerAnimation06E58Indices,
    { NULL, _gM4a1HammerAnimation06E58Bank1, NULL, NULL, _gM4a1HammerAnimation06E58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0761CBank1[18] = {
#include "assets/m4a1_hammer_animation_0761C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0761CBank4[184] = {
#include "assets/m4a1_hammer_animation_0761C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0761CRecords[239] = {
#include "assets/m4a1_hammer_animation_0761C_records.inc"
};

static u16 _gM4a1HammerAnimation0761CIndices[20] = {
#include "assets/m4a1_hammer_animation_0761C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0761C = {
    _gM4a1HammerAnimation0761CRecords,
    _gM4a1HammerAnimation0761CIndices,
    { NULL, _gM4a1HammerAnimation0761CBank1, NULL, NULL, _gM4a1HammerAnimation0761CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation088B4Bank1[29] = {
#include "assets/m4a1_hammer_animation_088B4_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation088B4Bank4[450] = {
#include "assets/m4a1_hammer_animation_088B4_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation088B4Records[633] = {
#include "assets/m4a1_hammer_animation_088B4_records.inc"
};

static u16 _gM4a1HammerAnimation088B4Indices[20] = {
#include "assets/m4a1_hammer_animation_088B4_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation088B4 = {
    _gM4a1HammerAnimation088B4Records,
    _gM4a1HammerAnimation088B4Indices,
    { NULL, _gM4a1HammerAnimation088B4Bank1, NULL, NULL, _gM4a1HammerAnimation088B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0942CBank1[12] = {
#include "assets/m4a1_hammer_animation_0942C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0942CBank4[266] = {
#include "assets/m4a1_hammer_animation_0942C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0942CRecords[412] = {
#include "assets/m4a1_hammer_animation_0942C_records.inc"
};

static u16 _gM4a1HammerAnimation0942CIndices[20] = {
#include "assets/m4a1_hammer_animation_0942C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0942C = {
    _gM4a1HammerAnimation0942CRecords,
    _gM4a1HammerAnimation0942CIndices,
    { NULL, _gM4a1HammerAnimation0942CBank1, NULL, NULL, _gM4a1HammerAnimation0942CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation09B48Bank1[9] = {
#include "assets/m4a1_hammer_animation_09B48_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation09B48Bank4[144] = {
#include "assets/m4a1_hammer_animation_09B48_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation09B48Records[264] = {
#include "assets/m4a1_hammer_animation_09B48_records.inc"
};

static u16 _gM4a1HammerAnimation09B48Indices[20] = {
#include "assets/m4a1_hammer_animation_09B48_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation09B48 = {
    _gM4a1HammerAnimation09B48Records,
    _gM4a1HammerAnimation09B48Indices,
    { NULL, _gM4a1HammerAnimation09B48Bank1, NULL, NULL, _gM4a1HammerAnimation09B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation09FC8Bank1[6] = {
#include "assets/m4a1_hammer_animation_09FC8_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation09FC8Bank4[107] = {
#include "assets/m4a1_hammer_animation_09FC8_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation09FC8Records[143] = {
#include "assets/m4a1_hammer_animation_09FC8_records.inc"
};

static u16 _gM4a1HammerAnimation09FC8Indices[20] = {
#include "assets/m4a1_hammer_animation_09FC8_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation09FC8 = {
    _gM4a1HammerAnimation09FC8Records,
    _gM4a1HammerAnimation09FC8Indices,
    { NULL, _gM4a1HammerAnimation09FC8Bank1, NULL, NULL, _gM4a1HammerAnimation09FC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0A1A0Bank1[3] = {
#include "assets/m4a1_hammer_animation_0A1A0_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0A1A0Bank4[32] = {
#include "assets/m4a1_hammer_animation_0A1A0_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0A1A0Records[57] = {
#include "assets/m4a1_hammer_animation_0A1A0_records.inc"
};

static u16 _gM4a1HammerAnimation0A1A0Indices[20] = {
#include "assets/m4a1_hammer_animation_0A1A0_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0A1A0 = {
    _gM4a1HammerAnimation0A1A0Records,
    _gM4a1HammerAnimation0A1A0Indices,
    { NULL, _gM4a1HammerAnimation0A1A0Bank1, NULL, NULL, _gM4a1HammerAnimation0A1A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0A704Bank1[11] = {
#include "assets/m4a1_hammer_animation_0A704_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0A704Bank4[125] = {
#include "assets/m4a1_hammer_animation_0A704_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0A704Records[167] = {
#include "assets/m4a1_hammer_animation_0A704_records.inc"
};

static u16 _gM4a1HammerAnimation0A704Indices[20] = {
#include "assets/m4a1_hammer_animation_0A704_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0A704 = {
    _gM4a1HammerAnimation0A704Records,
    _gM4a1HammerAnimation0A704Indices,
    { NULL, _gM4a1HammerAnimation0A704Bank1, NULL, NULL, _gM4a1HammerAnimation0A704Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0A8F8Bank1[3] = {
#include "assets/m4a1_hammer_animation_0A8F8_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0A8F8Bank4[20] = {
#include "assets/m4a1_hammer_animation_0A8F8_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0A8F8Records[76] = {
#include "assets/m4a1_hammer_animation_0A8F8_records.inc"
};

static u16 _gM4a1HammerAnimation0A8F8Indices[20] = {
#include "assets/m4a1_hammer_animation_0A8F8_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0A8F8 = {
    _gM4a1HammerAnimation0A8F8Records,
    _gM4a1HammerAnimation0A8F8Indices,
    { NULL, _gM4a1HammerAnimation0A8F8Bank1, NULL, NULL, _gM4a1HammerAnimation0A8F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0AD78Bank1[8] = {
#include "assets/m4a1_hammer_animation_0AD78_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0AD78Bank4[105] = {
#include "assets/m4a1_hammer_animation_0AD78_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0AD78Records[139] = {
#include "assets/m4a1_hammer_animation_0AD78_records.inc"
};

static u16 _gM4a1HammerAnimation0AD78Indices[20] = {
#include "assets/m4a1_hammer_animation_0AD78_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0AD78 = {
    _gM4a1HammerAnimation0AD78Records,
    _gM4a1HammerAnimation0AD78Indices,
    { NULL, _gM4a1HammerAnimation0AD78Bank1, NULL, NULL, _gM4a1HammerAnimation0AD78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0AF50Bank1[2] = {
#include "assets/m4a1_hammer_animation_0AF50_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0AF50Bank4[16] = {
#include "assets/m4a1_hammer_animation_0AF50_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0AF50Records[76] = {
#include "assets/m4a1_hammer_animation_0AF50_records.inc"
};

static u16 _gM4a1HammerAnimation0AF50Indices[20] = {
#include "assets/m4a1_hammer_animation_0AF50_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0AF50 = {
    _gM4a1HammerAnimation0AF50Records,
    _gM4a1HammerAnimation0AF50Indices,
    { NULL, _gM4a1HammerAnimation0AF50Bank1, NULL, NULL, _gM4a1HammerAnimation0AF50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0B790Bank1[14] = {
#include "assets/m4a1_hammer_animation_0B790_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0B790Bank4[194] = {
#include "assets/m4a1_hammer_animation_0B790_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0B790Records[272] = {
#include "assets/m4a1_hammer_animation_0B790_records.inc"
};

static u16 _gM4a1HammerAnimation0B790Indices[20] = {
#include "assets/m4a1_hammer_animation_0B790_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0B790 = {
    _gM4a1HammerAnimation0B790Records,
    _gM4a1HammerAnimation0B790Indices,
    { NULL, _gM4a1HammerAnimation0B790Bank1, NULL, NULL, _gM4a1HammerAnimation0B790Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0C238Bank1[19] = {
#include "assets/m4a1_hammer_animation_0C238_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0C238Bank4[269] = {
#include "assets/m4a1_hammer_animation_0C238_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0C238Records[336] = {
#include "assets/m4a1_hammer_animation_0C238_records.inc"
};

static u16 _gM4a1HammerAnimation0C238Indices[20] = {
#include "assets/m4a1_hammer_animation_0C238_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0C238 = {
    _gM4a1HammerAnimation0C238Records,
    _gM4a1HammerAnimation0C238Indices,
    { NULL, _gM4a1HammerAnimation0C238Bank1, NULL, NULL, _gM4a1HammerAnimation0C238Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0D0D4Bank1[24] = {
#include "assets/m4a1_hammer_animation_0D0D4_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0D0D4Bank4[390] = {
#include "assets/m4a1_hammer_animation_0D0D4_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0D0D4Records[453] = {
#include "assets/m4a1_hammer_animation_0D0D4_records.inc"
};

static u16 _gM4a1HammerAnimation0D0D4Indices[20] = {
#include "assets/m4a1_hammer_animation_0D0D4_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0D0D4 = {
    _gM4a1HammerAnimation0D0D4Records,
    _gM4a1HammerAnimation0D0D4Indices,
    { NULL, _gM4a1HammerAnimation0D0D4Bank1, NULL, NULL, _gM4a1HammerAnimation0D0D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0D578Bank1[8] = {
#include "assets/m4a1_hammer_animation_0D578_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0D578Bank4[102] = {
#include "assets/m4a1_hammer_animation_0D578_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0D578Records[151] = {
#include "assets/m4a1_hammer_animation_0D578_records.inc"
};

static u16 _gM4a1HammerAnimation0D578Indices[20] = {
#include "assets/m4a1_hammer_animation_0D578_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0D578 = {
    _gM4a1HammerAnimation0D578Records,
    _gM4a1HammerAnimation0D578Indices,
    { NULL, _gM4a1HammerAnimation0D578Bank1, NULL, NULL, _gM4a1HammerAnimation0D578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0DC5CBank1[13] = {
#include "assets/m4a1_hammer_animation_0DC5C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0DC5CBank4[170] = {
#include "assets/m4a1_hammer_animation_0DC5C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0DC5CRecords[212] = {
#include "assets/m4a1_hammer_animation_0DC5C_records.inc"
};

static u16 _gM4a1HammerAnimation0DC5CIndices[20] = {
#include "assets/m4a1_hammer_animation_0DC5C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0DC5C = {
    _gM4a1HammerAnimation0DC5CRecords,
    _gM4a1HammerAnimation0DC5CIndices,
    { NULL, _gM4a1HammerAnimation0DC5CBank1, NULL, NULL, _gM4a1HammerAnimation0DC5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0E368Bank1[13] = {
#include "assets/m4a1_hammer_animation_0E368_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0E368Bank4[175] = {
#include "assets/m4a1_hammer_animation_0E368_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0E368Records[217] = {
#include "assets/m4a1_hammer_animation_0E368_records.inc"
};

static u16 _gM4a1HammerAnimation0E368Indices[20] = {
#include "assets/m4a1_hammer_animation_0E368_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0E368 = {
    _gM4a1HammerAnimation0E368Records,
    _gM4a1HammerAnimation0E368Indices,
    { NULL, _gM4a1HammerAnimation0E368Bank1, NULL, NULL, _gM4a1HammerAnimation0E368Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0E8E0Bank1[10] = {
#include "assets/m4a1_hammer_animation_0E8E0_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0E8E0Bank4[131] = {
#include "assets/m4a1_hammer_animation_0E8E0_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0E8E0Records[169] = {
#include "assets/m4a1_hammer_animation_0E8E0_records.inc"
};

static u16 _gM4a1HammerAnimation0E8E0Indices[20] = {
#include "assets/m4a1_hammer_animation_0E8E0_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0E8E0 = {
    _gM4a1HammerAnimation0E8E0Records,
    _gM4a1HammerAnimation0E8E0Indices,
    { NULL, _gM4a1HammerAnimation0E8E0Bank1, NULL, NULL, _gM4a1HammerAnimation0E8E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0F198Bank1[18] = {
#include "assets/m4a1_hammer_animation_0F198_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0F198Bank4[201] = {
#include "assets/m4a1_hammer_animation_0F198_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0F198Records[283] = {
#include "assets/m4a1_hammer_animation_0F198_records.inc"
};

static u16 _gM4a1HammerAnimation0F198Indices[20] = {
#include "assets/m4a1_hammer_animation_0F198_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0F198 = {
    _gM4a1HammerAnimation0F198Records,
    _gM4a1HammerAnimation0F198Indices,
    { NULL, _gM4a1HammerAnimation0F198Bank1, NULL, NULL, _gM4a1HammerAnimation0F198Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0F938Bank1[16] = {
#include "assets/m4a1_hammer_animation_0F938_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0F938Bank4[157] = {
#include "assets/m4a1_hammer_animation_0F938_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0F938Records[263] = {
#include "assets/m4a1_hammer_animation_0F938_records.inc"
};

static u16 _gM4a1HammerAnimation0F938Indices[20] = {
#include "assets/m4a1_hammer_animation_0F938_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0F938 = {
    _gM4a1HammerAnimation0F938Records,
    _gM4a1HammerAnimation0F938Indices,
    { NULL, _gM4a1HammerAnimation0F938Bank1, NULL, NULL, _gM4a1HammerAnimation0F938Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation1030CBank1[25] = {
#include "assets/m4a1_hammer_animation_1030C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation1030CBank4[223] = {
#include "assets/m4a1_hammer_animation_1030C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation1030CRecords[311] = {
#include "assets/m4a1_hammer_animation_1030C_records.inc"
};

static u16 _gM4a1HammerAnimation1030CIndices[20] = {
#include "assets/m4a1_hammer_animation_1030C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation1030C = {
    _gM4a1HammerAnimation1030CRecords,
    _gM4a1HammerAnimation1030CIndices,
    { NULL, _gM4a1HammerAnimation1030CBank1, NULL, NULL, _gM4a1HammerAnimation1030CBank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_hammer_8012D4F4 = { { {
    NULL,
    &_gM4a1HammerAnimation0276C,
    &_gM4a1HammerAnimation0F198,
    &_gM4a1HammerAnimation0F938,
    &_gM4a1HammerAnimation1030C,
    &_gM4a1HammerAnimation03670,
    &_gM4a1HammerAnimation03ED4,
    &_gM4a1HammerAnimation0E368,
    &_gM4a1HammerAnimation0E8E0,
    &_gM4a1HammerAnimation0AF50,
    &_gM4a1HammerAnimation0D578,
    &_gM4a1HammerAnimation0DC5C,
    &_gM4a1HammerAnimation0C238,
    &_gM4a1HammerAnimation0B790,
    &_gM4a1HammerAnimation0D0D4,
    &_gM4a1HammerAnimation0D0D4,
    &_gM4a1HammerAnimation06B38,
    &_gM4a1HammerAnimation06E58,
    &_gM4a1HammerAnimation0761C,
    &_gM4a1HammerAnimation02E10,
    &_gM4a1HammerAnimation0D0D4,
    &_gM4a1HammerAnimation0276C,
    &_gM4a1HammerAnimation0276C,
    &_gM4a1HammerAnimation088B4,
    &_gM4a1HammerAnimation09B48,
    &_gM4a1HammerAnimation0942C,
    &_gM4a1HammerAnimation05BB8,
    &_gM4a1HammerAnimation05DB4,
    &_gM4a1HammerAnimation0608C,
    &_gM4a1HammerAnimation06330,
    &_gM4a1HammerAnimation06530,
    &_gM4a1HammerAnimation06884,
    &_gM4a1HammerAnimation09FC8,
    &_gM4a1HammerAnimation0A1A0,
    &_gM4a1HammerAnimation09FC8,
    &_gM4a1HammerAnimation0A1A0,
    &_gM4a1HammerAnimation04944,
    &_gM4a1HammerAnimation050CC,
    &_gM4a1HammerAnimation05730,
    &_gM4a1HammerAnimation053A0,
    &_gM4a1HammerAnimation041E8,
    &_gM4a1HammerAnimation0276C,
    &_gM4a1HammerAnimation0A704,
    &_gM4a1HammerAnimation0A8F8,
    &_gM4a1HammerAnimation0AD78,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

/// Jitter table for the eight sparks the charged hammer throws: `[0..7]` are
/// the spin angles, `[8..15]` the heights and `[16..23]` the radii.
s16 D_m4a1_hammer_8012D630[24] = { 0 };

/// Parent task the hammer effect re-attaches itself to each time it restarts.
Task* D_m4a1_hammer_8012D660 = NULL;

/// Nothing reads the word after the task pointer; it keeps the offset of the
/// vector that follows.
static s32 s_unused_8012D664 = 0;

/// Offset vector handed to the `beamStripDraw` sprite draw.
SVECTOR D_m4a1_hammer_8012D668 = { 0, 0, 0, 0 };
