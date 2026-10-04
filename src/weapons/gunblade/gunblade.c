#include "weapons/gunblade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gunblade_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/blade_trail.h"
#include "gameplay/animation.h"
#include "types.h"

/// The near end of the gunblade beam inside the muzzle frame, `(0, 0x60, 0x80)`;
/// the task's own coordinate starts there. The far end follows it directly, and
/// state 0 reaches that as element 1 of this array.
static SVECTOR D_gunblade_8011E704[1] = { { 0, 0x0060, 0x0080, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_gunblade_8011E704[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
static SVECTOR D_gunblade_8011E70C = { 0, 0x0060, 0x0380, 0 };

void func_gunblade_8011D1E4(Task* task)
{
    GfxCoord    local;
    GfxCoord*   coord;
    GfxCoord*   dst;
    EffectWork* work;
    EffectWork* eff;
    s32         keep;
    SVECTOR*    vec;
    s32         i;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        keep = gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                coord->parent       = work->parent;
                coord->coord.t[0]   = D_gunblade_8011E704[0].vx;
                D_gunblade_8012E244 = task;
                coord->coord.t[1]   = D_gunblade_8011E704[0].vy;
                D_gunblade_8012E248 = work;
                coord->coord.t[2]   = D_gunblade_8011E704[0].vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                task->state        = 1;
                vec                = &D_gunblade_8011E704[1];
                local.parent       = work->parent;
                local.coord.t[0]   = vec->vx;
                local.coord.t[1]   = vec->vy;
                local.coord.t[2]   = vec->vz;
                local.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst         = &gBladeTrailBase[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &gBladeTrailTip[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                return;
            case 1:
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                local.parent       = work->parent;
                local.coord.t[0]   = D_gunblade_8011E70C.vx;
                local.coord.t[1]   = D_gunblade_8011E70C.vy;
                local.coord.t[2]   = D_gunblade_8011E70C.vz;
                local.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&local);
                dst         = &gBladeTrailBase[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &gBladeTrailTip[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &gBladeTrailBase[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(dst);
                    dst               = &gBladeTrailTip[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(dst);
                }
                if (work->age < 9) {
                    bladeTrailDraw(work->age & 7, 0x112);
                    return;
                }
                if (work->index == 1) {
                    work->index++;
                    eff = Gp_SpawnEff(EFFECT_GUNBLADE_CHARGE_FLASH, coord, task->spawnArg1.value, NULL);
                    if (eff != NULL) {
                        taskReparent(task, eff->task);
                    }
                }
                bladeTrailDraw(work->age & 7, 0x331);
                keep = work->age < 0xD;
                break;
            default:
                return;
        }
    }
    if (!keep) {
        D_gunblade_8012E248 = NULL;
        effectKillTask(work, task);
    }
}

#include "../../shared/blade_trail_draw.inc.c"

/// Charge-up / blast flash for the gunblade's three shot grades
/// (`Task::spawnArg1` 13, 14 and 15). Frame 0 of each grade spawns the same
/// four effects with a grade-coloured parameter plus a burst of sparks, then
/// seeds the ring size (`scale`), its spin (`angle`), the arc size
/// (`period`) and the arc angle (`step`). Every frame draws the ring at
/// twice the spin, then either the two crossing arcs and a full-screen fade
/// while the arc is still large, or shrinks the ring and releases the pool
/// block once it falls under 0x20. The three grades differ only in which RGB
/// channel gets the full brightness, so the tails are identical and the
/// compiler cross-jumps them.
void func_gunblade_8011DAA4(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];
    s32         i;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    work->age++;

    switch (task->spawnArg1.value) {
        case 13:
            if (task->state == 0) {
                Gp_SpawnEff(EFFECT_IMPACT_FLASH, coord, 0x600, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x10000, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x102AA, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x10555, NULL);
                for (i = 0; i < 8; i++) {
                    Gp_SpawnEff(EFFECT_SPARK_STREAK, coord, 0, NULL);
                }
                task->state = 1;
                work->scale = work->period = 0xE0;
                work->angle = work->step = 0x80;
            }
            rgb[0] = rgb[1] = work->scale;
            rgb[2]          = work->scale >> 2;
            work->angle    += 0x10;
            Gp_DrawRing(coord, (s16)(work->angle * 2), rgb);
            if (work->period >= 0x11) {
                rgb[0] = rgb[1] = work->period;
                rgb[2]          = work->period >> 2;
                Gp_DrawArc(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
                if (work->age & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->step * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->period -= 0x10;
                work->step   += 0x40;
                return;
            }
            work->scale -= 0x20;
            if (work->scale < 0x20) {
                effectKillTask(work, task);
            }
            return;
        case 14:
            if (task->state == 0) {
                Gp_SpawnEff(EFFECT_IMPACT_FLASH, coord, 0x600, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x20000, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x202AA, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x20555, NULL);
                for (i = 0; i < 4; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_BOUNCING_SPARK, coord, ((gRandomLcgState >> 16) & 0x3F) | 0x100, NULL);
                }
                task->state = 1;
                work->scale = work->period = 0xE0;
                work->angle = work->step = 0x80;
            }
            rgb[0]       = work->scale;
            rgb[1]       = work->scale >> 1;
            rgb[2]       = work->scale >> 2;
            work->angle += 0x10;
            Gp_DrawRing(coord, (s16)(work->angle * 2), rgb);
            if (work->period >= 0x11) {
                rgb[0] = work->period;
                rgb[1] = work->period >> 1;
                rgb[2] = work->period >> 2;
                Gp_DrawArc(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
                if (work->age & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->step * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->period -= 0x10;
                work->step   += 0x40;
                return;
            }
            work->scale -= 0x20;
            if (work->scale < 0x20) {
                effectKillTask(work, task);
            }
            return;
        case 15:
            if (task->state == 0) {
                Gp_SpawnEff(EFFECT_IMPACT_FLASH, coord, 0x600, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x30000, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x302AA, NULL);
                Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, 0x30555, NULL);
                for (i = 0; i < 8; i++) {
                    Gp_SpawnEff(EFFECT_SPARK_STREAK, coord, 1, NULL);
                }
                task->state = 1;
                work->scale = work->period = 0xE0;
                work->angle = work->step = 0x80;
            }
            rgb[0]       = work->scale >> 2;
            rgb[1]       = work->scale >> 1;
            rgb[2]       = work->scale;
            work->angle += 0x10;
            Gp_DrawRing(coord, (s16)(work->angle * 2), rgb);
            if (work->period >= 0x11) {
                rgb[0] = work->period >> 2;
                rgb[1] = work->period >> 1;
                rgb[2] = work->period;
                Gp_DrawArc(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
                if (work->age & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->step * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->period -= 0x10;
                work->step   += 0x40;
                return;
            }
            work->scale -= 0x20;
            if (work->scale < 0x20) {
                effectKillTask(work, task);
            }
            return;
    }
}

void func_gunblade_8011E008(s32 arg0)
{
    EffectWork* work = D_gunblade_8012E248;

    if (work != NULL) {
        D_gunblade_8012E244->spawnArg1.value = arg0;
        work->index++;
    }
}

static TmdBone _gGunbladeModel017FCSkeleton[1] = {
#include "assets/gunblade_model_017FC_skeleton.inc"
};

static u32 _gGunbladeModel017FCPartVerts[1] = {
#include "assets/gunblade_model_017FC_partVerts.inc"
};

static SVECTOR _gGunbladeModel017FCVerts[41] = {
#include "assets/gunblade_model_017FC_verts.inc"
};

static SVECTOR _gGunbladeModel017FCNormals[39] = {
#include "assets/gunblade_model_017FC_normals.inc"
};

static u32 _gGunbladeModel017FCStream[317] = {
#include "assets/gunblade_model_017FC_stream.inc"
};

TmdSource D_gunblade_8011EEB0 = {
    0,
    2224,
    0,
    1,
    _gGunbladeModel017FCPartVerts,
    _gGunbladeModel017FCVerts,
    _gGunbladeModel017FCNormals,
    _gGunbladeModel017FCSkeleton,
    _gGunbladeModel017FCStream,
};

static AnimationPackedPose _gGunbladeAnimation01EA4Bank1[2] = {
#include "assets/gunblade_animation_01EA4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation01EA4Bank4[8] = {
#include "assets/gunblade_animation_01EA4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation01EA4Records[76] = {
#include "assets/gunblade_animation_01EA4_records.inc"
};

static u16 _gGunbladeAnimation01EA4Indices[20] = {
#include "assets/gunblade_animation_01EA4_indices.inc"
};

static AnimationSet _gGunbladeAnimation01EA4 = {
    _gGunbladeAnimation01EA4Records,
    _gGunbladeAnimation01EA4Indices,
    { NULL, _gGunbladeAnimation01EA4Bank1, NULL, NULL, _gGunbladeAnimation01EA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation02548Bank1[12] = {
#include "assets/gunblade_animation_02548_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation02548Bank4[151] = {
#include "assets/gunblade_animation_02548_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation02548Records[218] = {
#include "assets/gunblade_animation_02548_records.inc"
};

static u16 _gGunbladeAnimation02548Indices[20] = {
#include "assets/gunblade_animation_02548_indices.inc"
};

static AnimationSet _gGunbladeAnimation02548 = {
    _gGunbladeAnimation02548Records,
    _gGunbladeAnimation02548Indices,
    { NULL, _gGunbladeAnimation02548Bank1, NULL, NULL, _gGunbladeAnimation02548Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation02DA8Bank1[19] = {
#include "assets/gunblade_animation_02DA8_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation02DA8Bank4[169] = {
#include "assets/gunblade_animation_02DA8_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation02DA8Records[290] = {
#include "assets/gunblade_animation_02DA8_records.inc"
};

static u16 _gGunbladeAnimation02DA8Indices[20] = {
#include "assets/gunblade_animation_02DA8_indices.inc"
};

static AnimationSet _gGunbladeAnimation02DA8 = {
    _gGunbladeAnimation02DA8Records,
    _gGunbladeAnimation02DA8Indices,
    { NULL, _gGunbladeAnimation02DA8Bank1, NULL, NULL, _gGunbladeAnimation02DA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0360CBank1[19] = {
#include "assets/gunblade_animation_0360C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0360CBank4[170] = {
#include "assets/gunblade_animation_0360C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0360CRecords[290] = {
#include "assets/gunblade_animation_0360C_records.inc"
};

static u16 _gGunbladeAnimation0360CIndices[20] = {
#include "assets/gunblade_animation_0360C_indices.inc"
};

static AnimationSet _gGunbladeAnimation0360C = {
    _gGunbladeAnimation0360CRecords,
    _gGunbladeAnimation0360CIndices,
    { NULL, _gGunbladeAnimation0360CBank1, NULL, NULL, _gGunbladeAnimation0360CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation03920Bank1[3] = {
#include "assets/gunblade_animation_03920_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation03920Bank4[69] = {
#include "assets/gunblade_animation_03920_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation03920Records[99] = {
#include "assets/gunblade_animation_03920_records.inc"
};

static u16 _gGunbladeAnimation03920Indices[20] = {
#include "assets/gunblade_animation_03920_indices.inc"
};

static AnimationSet _gGunbladeAnimation03920 = {
    _gGunbladeAnimation03920Records,
    _gGunbladeAnimation03920Indices,
    { NULL, _gGunbladeAnimation03920Bank1, NULL, NULL, _gGunbladeAnimation03920Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0407CBank1[14] = {
#include "assets/gunblade_animation_0407C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0407CBank4[156] = {
#include "assets/gunblade_animation_0407C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0407CRecords[253] = {
#include "assets/gunblade_animation_0407C_records.inc"
};

static u16 _gGunbladeAnimation0407CIndices[20] = {
#include "assets/gunblade_animation_0407C_indices.inc"
};

static AnimationSet _gGunbladeAnimation0407C = {
    _gGunbladeAnimation0407CRecords,
    _gGunbladeAnimation0407CIndices,
    { NULL, _gGunbladeAnimation0407CBank1, NULL, NULL, _gGunbladeAnimation0407CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation04804Bank1[16] = {
#include "assets/gunblade_animation_04804_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation04804Bank4[167] = {
#include "assets/gunblade_animation_04804_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation04804Records[247] = {
#include "assets/gunblade_animation_04804_records.inc"
};

static u16 _gGunbladeAnimation04804Indices[20] = {
#include "assets/gunblade_animation_04804_indices.inc"
};

static AnimationSet _gGunbladeAnimation04804 = {
    _gGunbladeAnimation04804Records,
    _gGunbladeAnimation04804Indices,
    { NULL, _gGunbladeAnimation04804Bank1, NULL, NULL, _gGunbladeAnimation04804Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation04AD8Bank1[6] = {
#include "assets/gunblade_animation_04AD8_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation04AD8Bank4[52] = {
#include "assets/gunblade_animation_04AD8_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation04AD8Records[91] = {
#include "assets/gunblade_animation_04AD8_records.inc"
};

static u16 _gGunbladeAnimation04AD8Indices[20] = {
#include "assets/gunblade_animation_04AD8_indices.inc"
};

static AnimationSet _gGunbladeAnimation04AD8 = {
    _gGunbladeAnimation04AD8Records,
    _gGunbladeAnimation04AD8Indices,
    { NULL, _gGunbladeAnimation04AD8Bank1, NULL, NULL, _gGunbladeAnimation04AD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation04E68Bank1[7] = {
#include "assets/gunblade_animation_04E68_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation04E68Bank4[73] = {
#include "assets/gunblade_animation_04E68_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation04E68Records[114] = {
#include "assets/gunblade_animation_04E68_records.inc"
};

static u16 _gGunbladeAnimation04E68Indices[20] = {
#include "assets/gunblade_animation_04E68_indices.inc"
};

static AnimationSet _gGunbladeAnimation04E68 = {
    _gGunbladeAnimation04E68Records,
    _gGunbladeAnimation04E68Indices,
    { NULL, _gGunbladeAnimation04E68Bank1, NULL, NULL, _gGunbladeAnimation04E68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation052F0Bank1[9] = {
#include "assets/gunblade_animation_052F0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation052F0Bank4[104] = {
#include "assets/gunblade_animation_052F0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation052F0Records[139] = {
#include "assets/gunblade_animation_052F0_records.inc"
};

static u16 _gGunbladeAnimation052F0Indices[20] = {
#include "assets/gunblade_animation_052F0_indices.inc"
};

static AnimationSet _gGunbladeAnimation052F0 = {
    _gGunbladeAnimation052F0Records,
    _gGunbladeAnimation052F0Indices,
    { NULL, _gGunbladeAnimation052F0Bank1, NULL, NULL, _gGunbladeAnimation052F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation054ECBank1[3] = {
#include "assets/gunblade_animation_054EC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation054ECBank4[22] = {
#include "assets/gunblade_animation_054EC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation054ECRecords[76] = {
#include "assets/gunblade_animation_054EC_records.inc"
};

static u16 _gGunbladeAnimation054ECIndices[20] = {
#include "assets/gunblade_animation_054EC_indices.inc"
};

static AnimationSet _gGunbladeAnimation054EC = {
    _gGunbladeAnimation054ECRecords,
    _gGunbladeAnimation054ECIndices,
    { NULL, _gGunbladeAnimation054ECBank1, NULL, NULL, _gGunbladeAnimation054ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation057C4Bank1[6] = {
#include "assets/gunblade_animation_057C4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation057C4Bank4[57] = {
#include "assets/gunblade_animation_057C4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation057C4Records[87] = {
#include "assets/gunblade_animation_057C4_records.inc"
};

static u16 _gGunbladeAnimation057C4Indices[20] = {
#include "assets/gunblade_animation_057C4_indices.inc"
};

static AnimationSet _gGunbladeAnimation057C4 = {
    _gGunbladeAnimation057C4Records,
    _gGunbladeAnimation057C4Indices,
    { NULL, _gGunbladeAnimation057C4Bank1, NULL, NULL, _gGunbladeAnimation057C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation05A68Bank1[4] = {
#include "assets/gunblade_animation_05A68_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation05A68Bank4[55] = {
#include "assets/gunblade_animation_05A68_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation05A68Records[82] = {
#include "assets/gunblade_animation_05A68_records.inc"
};

static u16 _gGunbladeAnimation05A68Indices[20] = {
#include "assets/gunblade_animation_05A68_indices.inc"
};

static AnimationSet _gGunbladeAnimation05A68 = {
    _gGunbladeAnimation05A68Records,
    _gGunbladeAnimation05A68Indices,
    { NULL, _gGunbladeAnimation05A68Bank1, NULL, NULL, _gGunbladeAnimation05A68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation05C68Bank1[3] = {
#include "assets/gunblade_animation_05C68_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation05C68Bank4[23] = {
#include "assets/gunblade_animation_05C68_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation05C68Records[76] = {
#include "assets/gunblade_animation_05C68_records.inc"
};

static u16 _gGunbladeAnimation05C68Indices[20] = {
#include "assets/gunblade_animation_05C68_indices.inc"
};

static AnimationSet _gGunbladeAnimation05C68 = {
    _gGunbladeAnimation05C68Records,
    _gGunbladeAnimation05C68Indices,
    { NULL, _gGunbladeAnimation05C68Bank1, NULL, NULL, _gGunbladeAnimation05C68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation05FBCBank1[8] = {
#include "assets/gunblade_animation_05FBC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation05FBCBank4[68] = {
#include "assets/gunblade_animation_05FBC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation05FBCRecords[101] = {
#include "assets/gunblade_animation_05FBC_records.inc"
};

static u16 _gGunbladeAnimation05FBCIndices[20] = {
#include "assets/gunblade_animation_05FBC_indices.inc"
};

static AnimationSet _gGunbladeAnimation05FBC = {
    _gGunbladeAnimation05FBCRecords,
    _gGunbladeAnimation05FBCIndices,
    { NULL, _gGunbladeAnimation05FBCBank1, NULL, NULL, _gGunbladeAnimation05FBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation06270Bank1[5] = {
#include "assets/gunblade_animation_06270_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation06270Bank4[55] = {
#include "assets/gunblade_animation_06270_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation06270Records[83] = {
#include "assets/gunblade_animation_06270_records.inc"
};

static u16 _gGunbladeAnimation06270Indices[20] = {
#include "assets/gunblade_animation_06270_indices.inc"
};

static AnimationSet _gGunbladeAnimation06270 = {
    _gGunbladeAnimation06270Records,
    _gGunbladeAnimation06270Indices,
    { NULL, _gGunbladeAnimation06270Bank1, NULL, NULL, _gGunbladeAnimation06270Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation06590Bank1[6] = {
#include "assets/gunblade_animation_06590_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation06590Bank4[66] = {
#include "assets/gunblade_animation_06590_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation06590Records[96] = {
#include "assets/gunblade_animation_06590_records.inc"
};

static u16 _gGunbladeAnimation06590Indices[20] = {
#include "assets/gunblade_animation_06590_indices.inc"
};

static AnimationSet _gGunbladeAnimation06590 = {
    _gGunbladeAnimation06590Records,
    _gGunbladeAnimation06590Indices,
    { NULL, _gGunbladeAnimation06590Bank1, NULL, NULL, _gGunbladeAnimation06590Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation06D54Bank1[18] = {
#include "assets/gunblade_animation_06D54_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation06D54Bank4[184] = {
#include "assets/gunblade_animation_06D54_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation06D54Records[239] = {
#include "assets/gunblade_animation_06D54_records.inc"
};

static u16 _gGunbladeAnimation06D54Indices[20] = {
#include "assets/gunblade_animation_06D54_indices.inc"
};

static AnimationSet _gGunbladeAnimation06D54 = {
    _gGunbladeAnimation06D54Records,
    _gGunbladeAnimation06D54Indices,
    { NULL, _gGunbladeAnimation06D54Bank1, NULL, NULL, _gGunbladeAnimation06D54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation07FECBank1[29] = {
#include "assets/gunblade_animation_07FEC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation07FECBank4[450] = {
#include "assets/gunblade_animation_07FEC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation07FECRecords[633] = {
#include "assets/gunblade_animation_07FEC_records.inc"
};

static u16 _gGunbladeAnimation07FECIndices[20] = {
#include "assets/gunblade_animation_07FEC_indices.inc"
};

static AnimationSet _gGunbladeAnimation07FEC = {
    _gGunbladeAnimation07FECRecords,
    _gGunbladeAnimation07FECIndices,
    { NULL, _gGunbladeAnimation07FECBank1, NULL, NULL, _gGunbladeAnimation07FECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation08B64Bank1[12] = {
#include "assets/gunblade_animation_08B64_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation08B64Bank4[266] = {
#include "assets/gunblade_animation_08B64_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation08B64Records[412] = {
#include "assets/gunblade_animation_08B64_records.inc"
};

static u16 _gGunbladeAnimation08B64Indices[20] = {
#include "assets/gunblade_animation_08B64_indices.inc"
};

static AnimationSet _gGunbladeAnimation08B64 = {
    _gGunbladeAnimation08B64Records,
    _gGunbladeAnimation08B64Indices,
    { NULL, _gGunbladeAnimation08B64Bank1, NULL, NULL, _gGunbladeAnimation08B64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation09280Bank1[9] = {
#include "assets/gunblade_animation_09280_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation09280Bank4[144] = {
#include "assets/gunblade_animation_09280_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation09280Records[264] = {
#include "assets/gunblade_animation_09280_records.inc"
};

static u16 _gGunbladeAnimation09280Indices[20] = {
#include "assets/gunblade_animation_09280_indices.inc"
};

static AnimationSet _gGunbladeAnimation09280 = {
    _gGunbladeAnimation09280Records,
    _gGunbladeAnimation09280Indices,
    { NULL, _gGunbladeAnimation09280Bank1, NULL, NULL, _gGunbladeAnimation09280Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation09700Bank1[6] = {
#include "assets/gunblade_animation_09700_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation09700Bank4[107] = {
#include "assets/gunblade_animation_09700_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation09700Records[143] = {
#include "assets/gunblade_animation_09700_records.inc"
};

static u16 _gGunbladeAnimation09700Indices[20] = {
#include "assets/gunblade_animation_09700_indices.inc"
};

static AnimationSet _gGunbladeAnimation09700 = {
    _gGunbladeAnimation09700Records,
    _gGunbladeAnimation09700Indices,
    { NULL, _gGunbladeAnimation09700Bank1, NULL, NULL, _gGunbladeAnimation09700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation098D8Bank1[3] = {
#include "assets/gunblade_animation_098D8_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation098D8Bank4[32] = {
#include "assets/gunblade_animation_098D8_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation098D8Records[57] = {
#include "assets/gunblade_animation_098D8_records.inc"
};

static u16 _gGunbladeAnimation098D8Indices[20] = {
#include "assets/gunblade_animation_098D8_indices.inc"
};

static AnimationSet _gGunbladeAnimation098D8 = {
    _gGunbladeAnimation098D8Records,
    _gGunbladeAnimation098D8Indices,
    { NULL, _gGunbladeAnimation098D8Bank1, NULL, NULL, _gGunbladeAnimation098D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation09E3CBank1[11] = {
#include "assets/gunblade_animation_09E3C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation09E3CBank4[125] = {
#include "assets/gunblade_animation_09E3C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation09E3CRecords[167] = {
#include "assets/gunblade_animation_09E3C_records.inc"
};

static u16 _gGunbladeAnimation09E3CIndices[20] = {
#include "assets/gunblade_animation_09E3C_indices.inc"
};

static AnimationSet _gGunbladeAnimation09E3C = {
    _gGunbladeAnimation09E3CRecords,
    _gGunbladeAnimation09E3CIndices,
    { NULL, _gGunbladeAnimation09E3CBank1, NULL, NULL, _gGunbladeAnimation09E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0A030Bank1[3] = {
#include "assets/gunblade_animation_0A030_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0A030Bank4[20] = {
#include "assets/gunblade_animation_0A030_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0A030Records[76] = {
#include "assets/gunblade_animation_0A030_records.inc"
};

static u16 _gGunbladeAnimation0A030Indices[20] = {
#include "assets/gunblade_animation_0A030_indices.inc"
};

static AnimationSet _gGunbladeAnimation0A030 = {
    _gGunbladeAnimation0A030Records,
    _gGunbladeAnimation0A030Indices,
    { NULL, _gGunbladeAnimation0A030Bank1, NULL, NULL, _gGunbladeAnimation0A030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0A4B0Bank1[8] = {
#include "assets/gunblade_animation_0A4B0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0A4B0Bank4[105] = {
#include "assets/gunblade_animation_0A4B0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0A4B0Records[139] = {
#include "assets/gunblade_animation_0A4B0_records.inc"
};

static u16 _gGunbladeAnimation0A4B0Indices[20] = {
#include "assets/gunblade_animation_0A4B0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0A4B0 = {
    _gGunbladeAnimation0A4B0Records,
    _gGunbladeAnimation0A4B0Indices,
    { NULL, _gGunbladeAnimation0A4B0Bank1, NULL, NULL, _gGunbladeAnimation0A4B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0A68CBank1[2] = {
#include "assets/gunblade_animation_0A68C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0A68CBank4[17] = {
#include "assets/gunblade_animation_0A68C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0A68CRecords[76] = {
#include "assets/gunblade_animation_0A68C_records.inc"
};

static u16 _gGunbladeAnimation0A68CIndices[20] = {
#include "assets/gunblade_animation_0A68C_indices.inc"
};

static AnimationSet _gGunbladeAnimation0A68C = {
    _gGunbladeAnimation0A68CRecords,
    _gGunbladeAnimation0A68CIndices,
    { NULL, _gGunbladeAnimation0A68CBank1, NULL, NULL, _gGunbladeAnimation0A68CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0B1B0Bank1[21] = {
#include "assets/gunblade_animation_0B1B0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0B1B0Bank4[278] = {
#include "assets/gunblade_animation_0B1B0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0B1B0Records[352] = {
#include "assets/gunblade_animation_0B1B0_records.inc"
};

static u16 _gGunbladeAnimation0B1B0Indices[20] = {
#include "assets/gunblade_animation_0B1B0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0B1B0 = {
    _gGunbladeAnimation0B1B0Records,
    _gGunbladeAnimation0B1B0Indices,
    { NULL, _gGunbladeAnimation0B1B0Bank1, NULL, NULL, _gGunbladeAnimation0B1B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0BCB4Bank1[21] = {
#include "assets/gunblade_animation_0BCB4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0BCB4Bank4[264] = {
#include "assets/gunblade_animation_0BCB4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0BCB4Records[358] = {
#include "assets/gunblade_animation_0BCB4_records.inc"
};

static u16 _gGunbladeAnimation0BCB4Indices[20] = {
#include "assets/gunblade_animation_0BCB4_indices.inc"
};

static AnimationSet _gGunbladeAnimation0BCB4 = {
    _gGunbladeAnimation0BCB4Records,
    _gGunbladeAnimation0BCB4Indices,
    { NULL, _gGunbladeAnimation0BCB4Bank1, NULL, NULL, _gGunbladeAnimation0BCB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0C9C0Bank1[24] = {
#include "assets/gunblade_animation_0C9C0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0C9C0Bank4[330] = {
#include "assets/gunblade_animation_0C9C0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0C9C0Records[413] = {
#include "assets/gunblade_animation_0C9C0_records.inc"
};

static u16 _gGunbladeAnimation0C9C0Indices[20] = {
#include "assets/gunblade_animation_0C9C0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0C9C0 = {
    _gGunbladeAnimation0C9C0Records,
    _gGunbladeAnimation0C9C0Indices,
    { NULL, _gGunbladeAnimation0C9C0Bank1, NULL, NULL, _gGunbladeAnimation0C9C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0E7C4Bank1[55] = {
#include "assets/gunblade_animation_0E7C4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0E7C4Bank4[803] = {
#include "assets/gunblade_animation_0E7C4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0E7C4Records[933] = {
#include "assets/gunblade_animation_0E7C4_records.inc"
};

static u16 _gGunbladeAnimation0E7C4Indices[20] = {
#include "assets/gunblade_animation_0E7C4_indices.inc"
};

static AnimationSet _gGunbladeAnimation0E7C4 = {
    _gGunbladeAnimation0E7C4Records,
    _gGunbladeAnimation0E7C4Indices,
    { NULL, _gGunbladeAnimation0E7C4Bank1, NULL, NULL, _gGunbladeAnimation0E7C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0EDA4Bank1[8] = {
#include "assets/gunblade_animation_0EDA4_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0EDA4Bank4[142] = {
#include "assets/gunblade_animation_0EDA4_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0EDA4Records[190] = {
#include "assets/gunblade_animation_0EDA4_records.inc"
};

static u16 _gGunbladeAnimation0EDA4Indices[20] = {
#include "assets/gunblade_animation_0EDA4_indices.inc"
};

static AnimationSet _gGunbladeAnimation0EDA4 = {
    _gGunbladeAnimation0EDA4Records,
    _gGunbladeAnimation0EDA4Indices,
    { NULL, _gGunbladeAnimation0EDA4Bank1, NULL, NULL, _gGunbladeAnimation0EDA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation0F6F0Bank1[15] = {
#include "assets/gunblade_animation_0F6F0_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation0F6F0Bank4[238] = {
#include "assets/gunblade_animation_0F6F0_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation0F6F0Records[292] = {
#include "assets/gunblade_animation_0F6F0_records.inc"
};

static u16 _gGunbladeAnimation0F6F0Indices[20] = {
#include "assets/gunblade_animation_0F6F0_indices.inc"
};

static AnimationSet _gGunbladeAnimation0F6F0 = {
    _gGunbladeAnimation0F6F0Records,
    _gGunbladeAnimation0F6F0Indices,
    { NULL, _gGunbladeAnimation0F6F0Bank1, NULL, NULL, _gGunbladeAnimation0F6F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation1004CBank1[15] = {
#include "assets/gunblade_animation_1004C_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation1004CBank4[242] = {
#include "assets/gunblade_animation_1004C_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation1004CRecords[292] = {
#include "assets/gunblade_animation_1004C_records.inc"
};

static u16 _gGunbladeAnimation1004CIndices[20] = {
#include "assets/gunblade_animation_1004C_indices.inc"
};

static AnimationSet _gGunbladeAnimation1004C = {
    _gGunbladeAnimation1004CRecords,
    _gGunbladeAnimation1004CIndices,
    { NULL, _gGunbladeAnimation1004CBank1, NULL, NULL, _gGunbladeAnimation1004CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation107ECBank1[16] = {
#include "assets/gunblade_animation_107EC_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation107ECBank4[157] = {
#include "assets/gunblade_animation_107EC_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation107ECRecords[263] = {
#include "assets/gunblade_animation_107EC_records.inc"
};

static u16 _gGunbladeAnimation107ECIndices[20] = {
#include "assets/gunblade_animation_107EC_indices.inc"
};

static AnimationSet _gGunbladeAnimation107EC = {
    _gGunbladeAnimation107ECRecords,
    _gGunbladeAnimation107ECIndices,
    { NULL, _gGunbladeAnimation107ECBank1, NULL, NULL, _gGunbladeAnimation107ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGunbladeAnimation10F20Bank1[13] = {
#include "assets/gunblade_animation_10F20_bank1.inc"
};

static AnimationPackedRotation _gGunbladeAnimation10F20Bank4[167] = {
#include "assets/gunblade_animation_10F20_bank4.inc"
};

static AnimationRecord _gGunbladeAnimation10F20Records[235] = {
#include "assets/gunblade_animation_10F20_records.inc"
};

static u16 _gGunbladeAnimation10F20Indices[20] = {
#include "assets/gunblade_animation_10F20_indices.inc"
};

static AnimationSet _gGunbladeAnimation10F20 = {
    _gGunbladeAnimation10F20Records,
    _gGunbladeAnimation10F20Indices,
    { NULL, _gGunbladeAnimation10F20Bank1, NULL, NULL, _gGunbladeAnimation10F20Bank4, NULL, NULL, NULL },
};

AnimationBank D_gunblade_8012E108 = { { {
    NULL,
    &_gGunbladeAnimation01EA4,
    &_gGunbladeAnimation02548,
    &_gGunbladeAnimation107EC,
    &_gGunbladeAnimation10F20,
    &_gGunbladeAnimation02DA8,
    &_gGunbladeAnimation0360C,
    &_gGunbladeAnimation0F6F0,
    &_gGunbladeAnimation1004C,
    &_gGunbladeAnimation0A68C,
    &_gGunbladeAnimation0E7C4,
    &_gGunbladeAnimation0EDA4,
    &_gGunbladeAnimation0BCB4,
    &_gGunbladeAnimation0B1B0,
    &_gGunbladeAnimation0C9C0,
    &_gGunbladeAnimation0C9C0,
    &_gGunbladeAnimation06270,
    &_gGunbladeAnimation06590,
    &_gGunbladeAnimation06D54,
    &_gGunbladeAnimation02548,
    &_gGunbladeAnimation0C9C0,
    &_gGunbladeAnimation01EA4,
    &_gGunbladeAnimation01EA4,
    &_gGunbladeAnimation07FEC,
    &_gGunbladeAnimation09280,
    &_gGunbladeAnimation08B64,
    &_gGunbladeAnimation052F0,
    &_gGunbladeAnimation054EC,
    &_gGunbladeAnimation057C4,
    &_gGunbladeAnimation05A68,
    &_gGunbladeAnimation05C68,
    &_gGunbladeAnimation05FBC,
    &_gGunbladeAnimation09700,
    &_gGunbladeAnimation098D8,
    &_gGunbladeAnimation09700,
    &_gGunbladeAnimation098D8,
    &_gGunbladeAnimation0407C,
    &_gGunbladeAnimation04804,
    &_gGunbladeAnimation04E68,
    &_gGunbladeAnimation04AD8,
    &_gGunbladeAnimation03920,
    &_gGunbladeAnimation01EA4,
    &_gGunbladeAnimation09E3C,
    &_gGunbladeAnimation0A030,
    &_gGunbladeAnimation0A4B0,
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

/// The running beam task and its `EffectWork`, cached on entry to state 0 so
/// `func_gunblade_8011E008` can reach them from outside the task.
Task*       D_gunblade_8012E244 = NULL;
EffectWork* D_gunblade_8012E248 = NULL;

/// Nothing reads the two words after the work pointer; they keep the offset of
/// the trails that follow.
static s32 s_unused_8012E24C[2] = { 0, 0 };

/// The eight-segment beam trails, one array per end of the blade.
GfxCoord gBladeTrailBase[8] = { 0 };
GfxCoord gBladeTrailTip[8]  = { 0 };
