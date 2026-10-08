#include "rooms/shelter_b2_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b2_north_maintenance_walkway_private.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
#include "../../shared/room_events.h"

/// Anchor points of the glows the room task draws.
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183B90[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183BB0[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C20[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C28[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C30[];

/// Per-palette channel shifts for the halo, indexed by the palette the spawn
/// argument selects.

/// Offsets from the anchor of the two points the smoke trail follows. The
/// second is also reached under its own name.

TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_north_maintenance_walkway_80183B60[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_north_maintenance_walkway_8017DA88 },
    { ROOM_MESSAGE_USE_KEY_ITEM, shelterB2NorthMaintenanceWalkwayRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_north_maintenance_walkway_8017DC54 },
    { ROOM_MESSAGE_COMMAND, shelterB2NorthMaintenanceWalkwayIgnoreCommandMessage },
    { ROOM_MESSAGE_SOUND, shelterB2NorthMaintenanceWalkwayHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183B90[4] = {
    { 894, -197, -2271, 0 },
    { 894, -197, -3102, 0 },
    { 894, -197, 376, 0 },
    { 894, -197, -396, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183BB0[14] = {
    { 894, -197, 2648, 0 },
    { 894, -197, 2036, 0 },
    { 3106, -197, -2271, 0 },
    { 3106, -197, -3102, 0 },
    { 3106, -197, 376, 0 },
    { 3106, -197, -396, 0 },
    { 3106, -197, 2648, 0 },
    { 3106, -197, 2036, 0 },
    { 653, -197, 2896, 0 },
    { -31, -197, 2896, 0 },
    { -42, -197, 5113, 0 },
    { 597, -197, 5113, 0 },
    { -1562, -197, 5113, 0 },
    { -2493, -197, 5113, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C20[1] = {
    { 749, -1283, 2310, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C28[1] = {
    { 1107, -1181, -4935, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C30[1] = {
    { 1107, -1125, -4900, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x9620 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static void _glowDrawTintedDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

/// Installs this room's seven task IDs in gameplay's enemy-effect selectors.
///
/// Each ID encodes bank 6 and the descriptor index of this room's effect task.
/// Call after room-effect controller setup clears the selectors and before
/// enemy effects spawn. The selection persists until reset or overwritten;
/// keep this room overlay loaded while its selected effect tasks run.
static inline void _shelterB2NorthMaintenanceWalkwayInstallRoomEffectIds(void)
{
    gRoomEffectMoteId         = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_MOTE;
    gRoomEffectHaloId         = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_HALO;
    gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST;
    gRoomEffectSparkEmitterId = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SPARK_EMITTER;
    gRoomEffectFlashId        = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SPARK_BURST;
}

void shelterB2NorthMaintenanceWalkwayGlowTask(Task* task)
{
    enum {
        INITIALIZE_ROOM_EFFECTS,
        DRAW_VIEW_GLOWS,
        LAMP_RADIUS_SCALE   = 0x200,
        DOOR_RADIUS_SCALE   = 0x100,
        LOCKED_DOOR_COLOR   = 0x5C40, // RGB nibbles C:4:0; odd frames add 1 << 5
        UNLOCKED_DOOR_COLOR = 0x504C  // RGB nibbles 0:4:C; odd frames add 1 << 5
    };

    // Select this room's implementations for enemy-spawned effects.
    if (task->state == INITIALIZE_ROOM_EFFECTS) {
        _shelterB2NorthMaintenanceWalkwayInstallRoomEffectIds();
        task->state = DRAW_VIEW_GLOWS;
    }

    // Only queue glows visible from this mapped camera.
    // Capsule indexing relies on the adjacent anchor tables' storage order.
    switch ((u8)viewGetMappedIndex()) {
        case 2: {
            SVECTOR* glowPoints;
            if (gameFlagGetNibble(GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED) != 0) {
                _glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C28, DOOR_RADIUS_SCALE, UNLOCKED_DOOR_COLOR);
            } else {
                _glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C30, DOOR_RADIUS_SCALE, LOCKED_DOOR_COLOR);
            }
            glowPoints = D_shelter_b2_north_maintenance_walkway_80183B90;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[6], LAMP_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 3: {
            SVECTOR* glowPoints;
            glowPoints = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(glowPoints, LAMP_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-18], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-16], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-12], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-10], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-8], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-4], LAMP_RADIUS_SCALE, GLOW_HALF_TURN);
            break;
        }
        case 4: {
            SVECTOR* glowPoints;
            glowPoints = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(glowPoints, LAMP_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-8], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-4], LAMP_RADIUS_SCALE, GLOW_HALF_TURN);
            break;
        }
        case 5: {
            SVECTOR* glowPoints;
            glowPoints = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(glowPoints, LAMP_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-6], LAMP_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-4], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-2], LAMP_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 6: {
            SVECTOR* glowPoints;
            glowPoints = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(glowPoints, LAMP_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-4], LAMP_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-2], LAMP_RADIUS_SCALE, 0);
            break;
        }
        case 7:
            if (gameFlagGetNibble(GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED) != 0) {
                _glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C28, DOOR_RADIUS_SCALE, UNLOCKED_DOOR_COLOR);
            } else {
                _glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C30, DOOR_RADIUS_SCALE, LOCKED_DOOR_COLOR);
            }
            break;
        case 8:
            glowDrawDimGreyCapsule(D_shelter_b2_north_maintenance_walkway_80183BB0, LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
    }
}

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/glow_draw_red_disc.inc.c"

#include "../../shared/glow_draw_tinted_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b2_north_maintenance_walkway_80181A80(Task* arg0)
{
    _roomVisualEffectsSparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_north_maintenance_walkway_80182F00(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
