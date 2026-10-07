#include "rooms/shelter_b1_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_control_room_private.h"

#include "actors/task_tables.h"

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
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/streamed_scene.h"

// Indexed views below share one contiguous table.
void func_shelter_b1_control_room_8017F100(Task*);

TaskDesc D_shelter_b1_control_room_80181BBC[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_control_room_8017F100, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, streamedScenePlayThenHold, { .value = 0 } },
};

SVECTOR D_shelter_b1_control_room_80181BD4[18] = {
    { 8480, -3330, -2490, 0 },
    { 9080, -3330, -2490, 0 },
    { 9280, -3330, -2490, 0 },
    { 9880, -3330, -2490, 0 },
    { 10090, -3330, -2490, 0 },
    { 10680, -3330, -2490, 0 },
    { 8480, -3330, -6090, 0 },
    { 9080, -3330, -6090, 0 },
    { 9280, -3330, -6090, 0 },
    { 9880, -3330, -6090, 0 },
    { 10090, -3330, -6090, 0 },
    { 10680, -3330, -6090, 0 },
    { 10970, -3000, -3040, 0 },
    { 1180, -850, -2530, 0 },
    { 2180, -850, -2530, 0 },
    { 3180, -850, -2530, 0 },
    { 4180, -850, -2530, 0 },
    { 5180, -850, -2530, 0 },
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

#include "../../shared/streamed_scene_play_then_hold.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

void func_shelter_b1_control_room_8017F100(Task* arg0)
{
    displaySpawnTaskFromTable(D_shelter_b1_control_room_80181BBC, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(arg0);
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
    enum {
        SHELTER_B1_CONTROL_ROOM_CAPSULE_COUNT       = 6,
        SHELTER_B1_CONTROL_ROOM_CAPSULE_POINT_COUNT = 2,
    };

    // The capsule endpoint pairs precede the orange disc's centre in the table.
    _glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[0], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[SHELTER_B1_CONTROL_ROOM_CAPSULE_POINT_COUNT], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[2 * SHELTER_B1_CONTROL_ROOM_CAPSULE_POINT_COUNT], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[3 * SHELTER_B1_CONTROL_ROOM_CAPSULE_POINT_COUNT], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[4 * SHELTER_B1_CONTROL_ROOM_CAPSULE_POINT_COUNT], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    _glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[5 * SHELTER_B1_CONTROL_ROOM_CAPSULE_POINT_COUNT], SHELTER_B1_CONTROL_ROOM_CAPSULE_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_CAPSULE_RGB_NIBBLES);
    glowDrawDisc(&D_shelter_b1_control_room_80181BD4[SHELTER_B1_CONTROL_ROOM_CAPSULE_COUNT * SHELTER_B1_CONTROL_ROOM_CAPSULE_POINT_COUNT], SHELTER_B1_CONTROL_ROOM_ORANGE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_ORANGE_DISC_RGB_NIBBLES);
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
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[14], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[15], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[16], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[17], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[13], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[14], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[15], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            break;
        case 6:
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[13], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[14], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[15], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[16], SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RADIUS_SCALE, SHELTER_B1_CONTROL_ROOM_BLUE_DISC_RGB_NIBBLES);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"
