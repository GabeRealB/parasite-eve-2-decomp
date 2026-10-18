#include "weapons/m4a1_bayonet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "m4a1_bayonet_private.h"
#include "weapons/weapon.h"

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
#include "../../shared/effects/blade_trail.h"

/// Far ribbon endpoint in integer weapon-part coordinates, `(0, 0x300, 0x40)`.
///
/// The trail's persistent endpoint uses this translation relative to the held weapon.
static SVECTOR _gM4a1BayonetTrailTipOffset[1] = { { 0, 0x0300, 0x0040, 0 } };

/// Near ribbon endpoint translation, `(0, 0x180, 0x40)`, in integer coordinate units.
///
/// Recording applies it in the weapon-part frame; seeding applies the same offset
/// relative to the far endpoint, preserving the trail's initial pose.
static const SVECTOR _gM4a1BayonetTrailNearOffset __attribute__((section(".data"))) = { 0, 0x0180, 0x0040, 0 };

/// Stores a composed endpoint as a world-space trail pose independent of the weapon.
///
/// `composedEndpoint->workm.t` must be a current view-space position, and
/// `gGfxViewCoord.workm` the current world-to-view transform. Copies the
/// complete endpoint cache into `historyFrame->workm` and removes the view
/// transform into `historyFrame->coord`, with the persistent view node as
/// parent. Later composition follows camera movement without following the
/// weapon. Rotation is copied and rebased too; the ribbon uses only translation.
/// Rotation elements use ONE (4096) for 1.0; translations are signed 32-bit
/// game coordinates.
///
/// The view rotation must be orthonormal. The caller-owned nodes must be
/// word-aligned and disjoint; the destination must also be disjoint from the
/// view node. Reads only the source's composed cache and retains no pointer to
/// it. Leaves the destination's composition stamp and parameters untouched;
/// the caller must mark it dirty before recomposing. Requires an initialized
/// scratch stack with 48 free bytes disjoint from both arguments, released
/// before return. Changes GTE rotation, translation and arithmetic state.
static inline void _m4a1BayonetStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* composedEndpoint)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = composedEndpoint->workm;
    gte_SetRotMatrix(&composedEndpoint->workm);
    gte_SetTransMatrix(&composedEndpoint->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

void m4a1BayonetTrailTask(Task* task)
{
    enum {
        M4A1_BAYONET_TRAIL_SEED           = 0,
        M4A1_BAYONET_TRAIL_RECORD         = 1,
        M4A1_BAYONET_TRAIL_LIFETIME_TICKS = 13
    };
    EffectWork*    effectWork;
    GfxCoord*      tipCoord;
    GfxCoord*      historyFrame;
    GfxCoord       nearEndpoint;
    s32            effectControl;
    const SVECTOR* nearOffset;
    s32            offsetX;
    s32            offsetY;
    s32            offsetZ;
    s32            historyIndex;
    s32            keepTask;

    effectWork    = task->spawnArg2.pointer;
    tipCoord      = task->extra.coordBody->coord;
    effectControl = gRoomEffectState->effectControl;
    if (effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        effectWork->age++;
        switch (task->state) {
            case M4A1_BAYONET_TRAIL_SEED:
                tipCoord->parent       = effectWork->parent;
                tipCoord->coord.t[0]   = _gM4a1BayonetTrailTipOffset[0].vx;
                tipCoord->coord.t[1]   = _gM4a1BayonetTrailTipOffset[0].vy;
                tipCoord->coord.t[2]   = _gM4a1BayonetTrailTipOffset[0].vz;
                tipCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(tipCoord);
                task->state = M4A1_BAYONET_TRAIL_RECORD;

                // Only endpoint translations feed the ribbon; the temporary rotation is left uninitialized.
                // The seed chains the second endpoint beneath the tip; recording uses the weapon parent.
                // This retained one-past access reads the adjacent near-offset bytes in the image,
                // but is outside the C array's bounds. A bounded pair still needs rematching.
                offsetX                   = _gM4a1BayonetTrailTipOffset[1].vx;
                nearOffset                = &_gM4a1BayonetTrailTipOffset[1];
                offsetY                   = nearOffset->vy;
                offsetZ                   = nearOffset->vz;
                nearEndpoint.parent       = tipCoord;
                nearEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                nearEndpoint.coord.t[0]   = offsetX;
                nearEndpoint.coord.t[1]   = offsetY;
                nearEndpoint.coord.t[2]   = offsetZ;
                actorRenderComposeCoord(&nearEndpoint);

                // Seed every slot with the same world pose, detached from the moving weapon.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(gBladeTrailBase); historyIndex++) {
                    historyFrame = &gBladeTrailBase[historyIndex];
                    _m4a1BayonetStoreTrailFrame(historyFrame, tipCoord);

                    historyFrame = &gBladeTrailTip[historyIndex];
                    _m4a1BayonetStoreTrailFrame(historyFrame, &nearEndpoint);
                }
                break;
            case M4A1_BAYONET_TRAIL_RECORD:
                tipCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(tipCoord);

                nearEndpoint.parent       = effectWork->parent;
                nearEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                nearEndpoint.coord.t[0]   = _gM4a1BayonetTrailNearOffset.vx;
                nearEndpoint.coord.t[1]   = _gM4a1BayonetTrailNearOffset.vy;
                nearEndpoint.coord.t[2]   = _gM4a1BayonetTrailNearOffset.vz;
                actorRenderComposeCoord(&nearEndpoint);

                historyFrame = &gBladeTrailBase[effectWork->age & (ARRAY_SIZE(gBladeTrailBase) - 1)];
                _m4a1BayonetStoreTrailFrame(historyFrame, tipCoord);

                historyFrame = &gBladeTrailTip[effectWork->age & (ARRAY_SIZE(gBladeTrailTip) - 1)];
                _m4a1BayonetStoreTrailFrame(historyFrame, &nearEndpoint);

                // Recompose all retained world poses against the current view before drawing.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(gBladeTrailBase); historyIndex++) {
                    historyFrame               = &gBladeTrailBase[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &gBladeTrailTip[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(gBladeTrailBase) - 1), BLADE_TRAIL_TINT_BLUE_WHITE);
                break;
        }
        keepTask = effectWork->age < M4A1_BAYONET_TRAIL_LIFETIME_TICKS;
    } else {
        keepTask = effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    }
    if (!keepTask) {
        effectKillTask(effectWork, task);
    }
}

/// Binds the bayonet's far-endpoint ring to the first side of the thrust ribbon.
///
/// Supplies a side-effect-free `GfxCoord[8]` array identifier, paired by slot
/// with `BLADE_TRAIL_SECOND_ENDPOINT_HISTORY`. The drawer borrows composed view-space
/// `workm.t` translations in game-coordinate units for corners 0 and 2;
/// the task seeds every slot and refreshes the caches before drawing.
/// This binding exists only around the shared drawer inclusion.
#define BLADE_TRAIL_FIRST_ENDPOINT_HISTORY gBladeTrailBase

/// Binds the bayonet's near-endpoint ring to the second side of the thrust ribbon.
///
/// Supplies a side-effect-free, complete `GfxCoord[8]` array identifier, paired
/// by slot with `BLADE_TRAIL_FIRST_ENDPOINT_HISTORY`. Seed every slot and
/// recompose in the current view before drawing. The drawer borrows `workm.t`
/// translations in game-coordinate units for corners 1 and 3, narrowing them
/// to signed 16 bits and retaining no pointers. This binding exists only
/// around the shared drawer inclusion.
#define BLADE_TRAIL_SECOND_ENDPOINT_HISTORY gBladeTrailTip
#include "../../shared/effects/blade_trail_draw.inc.c"
#undef BLADE_TRAIL_FIRST_ENDPOINT_HISTORY
#undef BLADE_TRAIL_SECOND_ENDPOINT_HISTORY

/// The model `gM4a1BayonetWeaponModel`: generated from its asset (weapons/m4a1_bayonet/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m4a1_bayonet/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m4a1_bayonet/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m4a1_bayonet/clips.inc"
#include "assets/weapons/m4a1_bayonet/bank.inc"

GfxCoord gBladeTrailBase[8] = { 0 };
GfxCoord gBladeTrailTip[8]  = { 0 };
