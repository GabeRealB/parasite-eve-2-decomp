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

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
#include "../../shared/room/room_events.h"

/// Per-palette channel shifts for the halo, indexed by the palette the spawn
/// argument selects.

TaskDesc gShelterB2NorthMaintenanceWalkwayStagedEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

TaskMessageEntry gShelterB2NorthMaintenanceWalkwayRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, shelterB2NorthMaintenanceWalkwayResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, shelterB2NorthMaintenanceWalkwayRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, shelterB2NorthMaintenanceWalkwayHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, shelterB2NorthMaintenanceWalkwayIgnoreCommandMessage },
    { ROOM_MESSAGE_SOUND, shelterB2NorthMaintenanceWalkwayHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space lamp endpoint pairs followed by the red disc centre.
///
/// The first 18 signed world-coordinate points form consecutive lamp
/// pairs; point 18 is the red disc centre. Interior view bases keep
/// every endpoint and centre within this one table. Drawers borrow positions
/// only during the current frame; the room overlay owns the storage.
static SVECTOR _gShelterB2NorthMaintenanceWalkwayGlowPoints[19] = {
    { 894, -197, -2271, 0 },
    { 894, -197, -3102, 0 },
    { 894, -197, 376, 0 },
    { 894, -197, -396, 0 },
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
    { 749, -1283, 2310, 0 },
};

/// World-space cyan indicator centre for the unlocked operating-room north door.
///
/// Drawn in mapped views 2 and 7 while the north-door-unlocked flag is set;
/// the signed world-coordinate centre is borrowed only while drawing.
static SVECTOR _gShelterB2NorthMaintenanceWalkwayUnlockedDoorGlowCentre = { 1107, -1181, -4935, 0 };

/// World-space orange indicator centre for the locked operating-room north door.
///
/// Drawn in mapped views 2 and 7 while the north-door-unlocked flag is clear;
/// the signed world-coordinate centre is borrowed only while drawing.
static SVECTOR _gShelterB2NorthMaintenanceWalkwayLockedDoorGlowCentre = { 1107, -1125, -4900, 0 };

/// Initializes this overlay's three halo tint rows and retained trailing halfword.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxHaloStorage` and an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND`. The trailing u16 is preserved
/// verbatim; its role is unproven and it is not a fourth tint row.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    .entries = {                                       \
        { .rShift = 0, .gShift = 1, .bShift = 2 },     \
        { .rShift = 2, .gShift = 1, .bShift = 0 },     \
        { .rShift = 0, .gShift = 2, .bShift = 1 },     \
    },                                                 \
    .alignmentFill = 0x9620,                           \
}
/// Selects the halo tint storage record, including its retained trailing halfword.
///
/// Expands to the `RoomFxHaloStorage` type token; requires an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` for `entries` and `alignmentFill`.
/// Only the three RGB-shift rows are read, at indices 0..2; the halfword's
/// role is unproven. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxHaloStorage
/// Declares one halo storage record, retaining the halfword after its tint rows.
///
/// This empty suffix requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be
/// `RoomFxHaloStorage`. Bind around `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND
#include "../../shared/room/room_visual_effects_halo_data.inc.c"

static void _glowDrawTintedDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

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
    switch ((u8)viewGetMappedIndex()) {
        case 2: {
            const SVECTOR* glowPoints;
            if (gameFlagGetNibble(GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED) != 0) {
                _glowDrawTintedDisc(&_gShelterB2NorthMaintenanceWalkwayUnlockedDoorGlowCentre, DOOR_RADIUS_SCALE, UNLOCKED_DOOR_COLOR);
            } else {
                _glowDrawTintedDisc(&_gShelterB2NorthMaintenanceWalkwayLockedDoorGlowCentre, DOOR_RADIUS_SCALE, LOCKED_DOOR_COLOR);
            }
            glowPoints = _gShelterB2NorthMaintenanceWalkwayGlowPoints;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[6], LAMP_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 3: {
            const SVECTOR* glowPoints;
            glowPoints = &_gShelterB2NorthMaintenanceWalkwayGlowPoints[18];
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
            const SVECTOR* glowPoints;
            glowPoints = &_gShelterB2NorthMaintenanceWalkwayGlowPoints[18];
            glowDrawRedDisc(glowPoints, LAMP_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-8], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-4], LAMP_RADIUS_SCALE, GLOW_HALF_TURN);
            break;
        }
        case 5: {
            const SVECTOR* glowPoints;
            glowPoints = &_gShelterB2NorthMaintenanceWalkwayGlowPoints[18];
            glowDrawRedDisc(glowPoints, LAMP_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-6], LAMP_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-4], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-2], LAMP_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 6: {
            const SVECTOR* glowPoints;
            glowPoints = &_gShelterB2NorthMaintenanceWalkwayGlowPoints[18];
            glowDrawRedDisc(glowPoints, LAMP_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-4], LAMP_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-2], LAMP_RADIUS_SCALE, 0);
            break;
        }
        case 7:
            if (gameFlagGetNibble(GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED) != 0) {
                _glowDrawTintedDisc(&_gShelterB2NorthMaintenanceWalkwayUnlockedDoorGlowCentre, DOOR_RADIUS_SCALE, UNLOCKED_DOOR_COLOR);
            } else {
                _glowDrawTintedDisc(&_gShelterB2NorthMaintenanceWalkwayLockedDoorGlowCentre, DOOR_RADIUS_SCALE, LOCKED_DOOR_COLOR);
            }
            break;
        case 8:
            glowDrawDimGreyCapsule(&_gShelterB2NorthMaintenanceWalkwayGlowPoints[4], LAMP_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
    }
}

#include "../../shared/effects/glow_draw_cone.inc.c"

#include "../../shared/effects/glow_draw_red_disc.inc.c"

#include "../../shared/effects/glow_draw_tinted_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
