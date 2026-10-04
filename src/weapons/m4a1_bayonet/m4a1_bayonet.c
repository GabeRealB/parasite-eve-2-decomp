#include "weapons/m4a1_bayonet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "m4a1_bayonet_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/blade_trail.h"

/// The blade tip's translation inside the muzzle frame, `(0, 0x300, 0x40)`. The
/// hilt's translation follows it directly, and state 0 reaches that as element 1
/// of this array.
static SVECTOR D_m4a1_bayonet_8011DEC8[1] = { { 0, 0x0300, 0x0040, 0 } };

/// The hilt's translation inside the muzzle frame, `(0, 0x180, 0x40)`, directly
/// after the tip. State 0 reaches it as `D_m4a1_bayonet_8011DEC8[1]` and the
/// sweep state names it directly; the two compile to different address
/// arithmetic, so it is an object of its own rather than element 1.
static SVECTOR D_m4a1_bayonet_8011DED0 = { 0, 0x0180, 0x0040, 0 };

/// Per-frame task for the M4A1 bayonet's blade trail. Nothing runs while
/// `gRoomEffectState->effectControl` is not running; the task is released at
/// cancellation. State 0 places the tip frame at
/// `D_m4a1_bayonet_8011DEC8[0]` under the muzzle and the hilt frame at
/// `[1]` under it, then seeds all sixteen trail slots with that pose. State 1
/// re-poses both frames every frame, writes them into trail slot
/// `age & 7`, re-runs the whole ring so the older slots follow their
/// parents, and hands the ribbon to `bladeTrailDraw`. The task
/// lives 13 frames.
void func_m4a1_bayonet_8011D1E4(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   slot;
    GfxCoord    hilt;
    s32         phase;
    SVECTOR*    vec;
    s32         vx;
    s32         vy;
    s32         vz;
    s32         i;
    s32         alive;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    phase = gRoomEffectState->effectControl;
    if (phase == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        switch (task->state) {
            case 0:
                coord->parent       = work->parent;
                coord->coord.t[0]   = D_m4a1_bayonet_8011DEC8[0].vx;
                coord->coord.t[1]   = D_m4a1_bayonet_8011DEC8[0].vy;
                coord->coord.t[2]   = D_m4a1_bayonet_8011DEC8[0].vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                task->state = 1;

                vx                = D_m4a1_bayonet_8011DEC8[1].vx;
                vec               = &D_m4a1_bayonet_8011DEC8[1];
                vy                = vec->vy;
                vz                = vec->vz;
                hilt.parent       = coord;
                hilt.composeStamp = GRAPHICS_COORD_DIRTY;
                hilt.coord.t[0]   = vx;
                hilt.coord.t[1]   = vy;
                hilt.coord.t[2]   = vz;
                Gp_UpdateCoord(&hilt);

                for (i = 0; i < 8; i++) {
                    slot         = &gBladeTrailBase[i];
                    slot->parent = &gGfxViewCoord;
                    slot->workm  = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &slot->workm, &slot->coord);

                    slot         = &gBladeTrailTip[i];
                    slot->parent = &gGfxViewCoord;
                    slot->workm  = hilt.workm;
                    gte_SetRotMatrix(&hilt.workm);
                    gte_SetTransMatrix(&hilt.workm);
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &slot->workm, &slot->coord);
                }
                break;
            case 1:
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);

                hilt.parent       = work->parent;
                hilt.composeStamp = GRAPHICS_COORD_DIRTY;
                hilt.coord.t[0]   = D_m4a1_bayonet_8011DED0.vx;
                hilt.coord.t[1]   = D_m4a1_bayonet_8011DED0.vy;
                hilt.coord.t[2]   = D_m4a1_bayonet_8011DED0.vz;
                Gp_UpdateCoord(&hilt);

                slot         = &gBladeTrailBase[work->age & 7];
                slot->parent = &gGfxViewCoord;
                slot->workm  = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &slot->workm, &slot->coord);

                slot         = &gBladeTrailTip[work->age & 7];
                slot->parent = &gGfxViewCoord;
                slot->workm  = hilt.workm;
                gte_SetRotMatrix(&hilt.workm);
                gte_SetTransMatrix(&hilt.workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &slot->workm, &slot->coord);

                for (i = 0; i < 8; i++) {
                    slot               = &gBladeTrailBase[i];
                    slot->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(slot);
                    slot               = &gBladeTrailTip[i];
                    slot->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(slot);
                }
                bladeTrailDraw(work->age & 7, 0x112);
                break;
        }
        alive = work->age < 0xD;
    } else {
        alive = phase < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    }
    if (!alive) {
        effectKillTask(work, task);
    }
}

#include "../../shared/blade_trail_draw.inc.c"

static TmdBone _gM4a1BayonetModel01130Skeleton[1] = {
#include "assets/m4a1_bayonet_model_01130_skeleton.inc"
};

static u32 _gM4a1BayonetModel01130PartVerts[1] = {
#include "assets/m4a1_bayonet_model_01130_partVerts.inc"
};

static SVECTOR _gM4a1BayonetModel01130Verts[63] = {
#include "assets/m4a1_bayonet_model_01130_verts.inc"
};

static SVECTOR _gM4a1BayonetModel01130Normals[63] = {
#include "assets/m4a1_bayonet_model_01130_normals.inc"
};

static u32 _gM4a1BayonetModel01130Stream[451] = {
#include "assets/m4a1_bayonet_model_01130_stream.inc"
};

TmdSource D_m4a1_bayonet_8011E9FC = {
    0,
    3256,
    0,
    1,
    _gM4a1BayonetModel01130PartVerts,
    _gM4a1BayonetModel01130Verts,
    _gM4a1BayonetModel01130Normals,
    _gM4a1BayonetModel01130Skeleton,
    _gM4a1BayonetModel01130Stream,
};
