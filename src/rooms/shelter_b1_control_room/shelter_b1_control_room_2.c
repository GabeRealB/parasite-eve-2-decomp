#include "rooms/shelter_b1_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_control_room_private.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/streamed_scene.h"

static void _shelterB1ControlRoomStartMovieTask(Task* task);

TaskDesc gShelterB1ControlRoomMovieTaskDescs[2] = {
    { .header = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, .callback = _shelterB1ControlRoomStartMovieTask, .data = { .value = 0 } },
    { .header = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, .callback = streamedScenePlayThenHold, .data = { .value = 0 } },
};

/// The control room's capsule endpoint pairs and disc centres in signed world units.
///
/// Positions are fixed whole world coordinates, borrowed only during drawing.
/// Every vector includes the fourth halfword read by the GTE.
typedef struct {
    SVECTOR capsules[6][2]; // Six endpoint pairs common to mapped views 2 and 3
    SVECTOR orangeDisc;     // Orange disc centre common to mapped views 2 and 3
    SVECTOR blueDiscs[5];   // Blue disc centres selected by mapped views 3, 4 and 6
} _ShelterB1ControlRoomGlowWorldPoints;
STATIC_ASSERT_SIZEOF(_ShelterB1ControlRoomGlowWorldPoints, 0x90);

/// Fixed world points for the control room's view-selected capsule and disc glows.
///
/// Drawing borrows selected points while the room overlay is loaded.
static _ShelterB1ControlRoomGlowWorldPoints _gShelterB1ControlRoomGlowWorldPoints = {
    .capsules = {
        { { 8480, -3330, -2490, 0 }, { 9080, -3330, -2490, 0 } },
        { { 9280, -3330, -2490, 0 }, { 9880, -3330, -2490, 0 } },
        { { 10090, -3330, -2490, 0 }, { 10680, -3330, -2490, 0 } },
        { { 8480, -3330, -6090, 0 }, { 9080, -3330, -6090, 0 } },
        { { 9280, -3330, -6090, 0 }, { 9880, -3330, -6090, 0 } },
        { { 10090, -3330, -6090, 0 }, { 10680, -3330, -6090, 0 } },
    },
    .orangeDisc = { 10970, -3000, -3040, 0 },
    .blueDiscs  = {
        { 1180, -850, -2530, 0 },
        { 2180, -850, -2530, 0 },
        { 3180, -850, -2530, 0 },
        { 4180, -850, -2530, 0 },
        { 5180, -850, -2530, 0 },
    },
};

/// Glow radii use scale * 64 / camera depth (camera Z / 4) in pixels.
///
/// Colours pack red, green and blue nibbles, each scaled by 16 before flicker.
enum {
    SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE     = 0x100,
    SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES      = 0x243,
    SHELTER_B1_CONTROL_ROOM_ORANGE_DISC_RADIUS_SCALE = 0x180,
    SHELTER_B1_CONTROL_ROOM_ORANGE_DISC_RGB_NIBBLES  = 0x421,
    SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE   = 0x200,
    SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES    = 0x023,
};

#include "../../shared/room/streamed_scene_play_then_hold.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Hands display presentation to the control room's movie and retires the trigger.
///
/// Requires the room overlay, session and movie resources to remain loaded through
/// playback. Spawns descriptor 1 on the display list, requests task-only flipping
/// and queues the current camera packets. The movie restores game resources and
/// resumes presentation after completion or cancellation; this trigger owns no work.
static void _shelterB1ControlRoomStartMovieTask(Task* task)
{
    enum { SHELTER_B1_CONTROL_ROOM_MOVIE_DESCRIPTOR = 1 };

    displaySpawnTaskFromTable(gShelterB1ControlRoomMovieTaskDescs, SHELTER_B1_CONTROL_ROOM_MOVIE_DESCRIPTOR, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Queues the six capsule glows and orange disc common to mapped views 2 and 3.
///
/// Borrows the room's fixed world points for this call. Requires the composed
/// view matrix, an initialized scratch stack and the current frame's depth
/// ordering table and packet arena. Accepted points must have nonzero projected
/// depth (camera Z / 4); negative projection flags skip the affected glow. The arena needs
/// space for up to 40 Gouraud quads and their additive blend commands, and
/// queued packets remain live until the frame's GPU work completes.
static inline void _shelterB1ControlRoomDrawCapsuleAndDiscGlows(void)
{
    // The capsule endpoint pairs precede the orange disc's centre in the table.
    _glowDrawCapsule(&_gShelterB1ControlRoomGlowWorldPoints.capsules[0][0], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&_gShelterB1ControlRoomGlowWorldPoints.capsules[1][0], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&_gShelterB1ControlRoomGlowWorldPoints.capsules[2][0], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&_gShelterB1ControlRoomGlowWorldPoints.capsules[3][0], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&_gShelterB1ControlRoomGlowWorldPoints.capsules[4][0], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&_gShelterB1ControlRoomGlowWorldPoints.capsules[5][0], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.orangeDisc, SHELTER_B1_CONTROL_ROOM_ORANGE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_ORANGE_DISC_RGB_NIBBLES);
}

void shelterB1ControlRoomDrawGlowsTask(Task* task)
{
    enum {
        SHELTER_B1_CONTROL_ROOM_GLOW_INITIALIZE = 0,
        SHELTER_B1_CONTROL_ROOM_GLOW_READY      = 1,
    };
    u8 mappedViewIndex;

    // Install the room's actor effects once, including when this view has no glows.
    if (task->state == SHELTER_B1_CONTROL_ROOM_GLOW_INITIALIZE) {
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_CONTROL_ROOM_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_CONTROL_ROOM_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_CONTROL_ROOM_ORANGE_BURST_2;
        task->state               = SHELTER_B1_CONTROL_ROOM_GLOW_READY;
    }

    // Select fixed world points using the current 1-based mapped camera index.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _shelterB1ControlRoomDrawCapsuleAndDiscGlows();
            break;
        case 3:
            _shelterB1ControlRoomDrawCapsuleAndDiscGlows();
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[1], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[2], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[3], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[4], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            break;
        case 4:
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[0], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[1], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[2], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            break;
        case 6:
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[0], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[1], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[2], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&_gShelterB1ControlRoomGlowWorldPoints.blueDiscs[3], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_disc.inc.c"
