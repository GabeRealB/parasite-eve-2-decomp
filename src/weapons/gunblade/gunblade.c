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
                Gp_UpdateCoord(coord);
                task->state        = 1;
                vec                = &D_gunblade_8011E704[1];
                local.parent       = work->parent;
                local.coord.t[0]   = vec->vx;
                local.coord.t[1]   = vec->vy;
                local.coord.t[2]   = vec->vz;
                local.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&local);
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
                Gp_UpdateCoord(coord);
                local.parent       = work->parent;
                local.coord.t[0]   = D_gunblade_8011E70C.vx;
                local.coord.t[1]   = D_gunblade_8011E70C.vy;
                local.coord.t[2]   = D_gunblade_8011E70C.vz;
                local.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&local);
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
                    Gp_UpdateCoord(dst);
                    dst               = &gBladeTrailTip[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
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

    Gp_UpdateCoord(coord);
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
