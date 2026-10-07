#include "rooms/shelter_b1_control_room_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

/// The room's message table, installed on its task by state 0.
extern TaskMessageEntry D_shelter_b1_control_room_access_tunnel_80181E74[];

extern SVECTOR D_shelter_b1_control_room_access_tunnel_80181E9C[];
extern SVECTOR D_shelter_b1_control_room_access_tunnel_80181EAC[];

static s32 _shelterB1ControlRoomAccessTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB1ControlRoomAccessTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1ControlRoomAccessTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB1ControlRoomAccessTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

enum { SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskMessageEntry D_shelter_b1_control_room_access_tunnel_80181E74[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1ControlRoomAccessTunnelResolveRoomEvent },
    { SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_MESSAGE_USE_KEY_ITEM, _shelterB1ControlRoomAccessTunnelRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1ControlRoomAccessTunnelIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1ControlRoomAccessTunnelIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181E9C[2] = {
    { 5017, -196, 1184, 0 },
    { 4172, -196, 1184, 0 },
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181EAC[7] = {
    { 2313, -196, 1184, 0 },
    { 1527, -196, 1184, 0 },
    { 5017, -196, -1028, 0 },
    { 4172, -196, -1028, 0 },
    { 2313, -196, -1028, 0 },
    { 1527, -196, -1028, 0 },
    { 4266, -1272, -1160, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

static void _shelterB1ControlRoomAccessTunnelInitializeRoom(Task* task);

/// Refuses key-item use in this room, selecting the inventory's cannot-use notice.
///
/// Ignores the collected `itemId` and all other arguments; always returns zero.
static s32 _shelterB1ControlRoomAccessTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Accepts a Mine/Shelter transition and resolves its destination room variant.
///
/// Copies the complete eight-byte request into the reply before resolving it.
/// Queries preserve the requested room; execution uses current game progress.
/// Both records must be live and may alias. Neither pointer is retained, and
/// task and messageId are unused. Returns 1 to permit the ordinary transition.
static s32 _shelterB1ControlRoomAccessTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_ROOM_EVENT_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_ROOM_EVENT_ACCEPTED;
}

/// Ignores room commands from scripts and triggers and always returns zero.
///
/// Neither command word nor any other argument is read or retained.
static s32 _shelterB1ControlRoomAccessTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room-action requests from direction triggers and always returns zero.
///
/// The borrowed `request` and all other arguments are neither read nor retained.
static s32 _shelterB1ControlRoomAccessTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs and publishes the room controller for message dispatch.
///
/// Requires entry state 0 and the loaded room message table. Registers this
/// live task in `GAME_TASK_SLOT_ROOM`, then advances to idle state 1. Allocates
/// no work; the task and table must remain live while room messages are sent.
static void _shelterB1ControlRoomAccessTunnelInitializeRoom(Task* task)
{
    task->msgTable = D_shelter_b1_control_room_access_tunnel_80181E74;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Keeps the initialized room task available for messages without per-frame work.
static void _shelterB1ControlRoomAccessTunnelIdle(Task* task)
{
}

/// State handlers of the task `func_shelter_b1_control_room_access_tunnel_8017D68C`
/// runs, which copies the table to the stack and calls the entry for the
/// task's state: the room's setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b1_control_room_access_tunnel_8017D5C4 = {
    { _shelterB1ControlRoomAccessTunnelInitializeRoom, _shelterB1ControlRoomAccessTunnelIdle, taskKill }
};

/// Runs the room's task through its three-state handler table, copied onto
/// the stack before the call.
void func_shelter_b1_control_room_access_tunnel_8017D68C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_control_room_access_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/glow_draw_red_disc.inc.c"

/// Binds the six actor-effect selectors to this room's effect-task instances.
///
/// Installs packed bank/type IDs for the flash, twin trail, spark burst, glow
/// disc, flying spark and orange burst. Call after room-effect controller
/// initialization and before spawning these effects. The selectors retain
/// these IDs until reset or rebound; this room overlay must remain loaded
/// while any selected task runs.
static inline void _shelterB1ControlRoomAccessTunnelBindEffects(void)
{
    gRoomEffectFlashId        = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_SPARK_BURST;
    gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GLOW_DISC;
    gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FLYING_SPARK;
    gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_ORANGE_BURST_2;
}

void shelterB1ControlRoomAccessTunnelDrawGlowsTask(Task* task)
{
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           GLOW_RADIUS_SCALE = 0x200 };

    u8 mappedView;

    if (task->state == GLOWS_INITIALIZE) {
        _shelterB1ControlRoomAccessTunnelBindEffects();
        task->state = GLOWS_DRAW;
    }
    // Each view exposes two capsule glows; view 2 also exposes a red disc.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2: {
            // These retained indices span both adjacent glow-point tables.
            const SVECTOR* glowPoints = D_shelter_b1_control_room_access_tunnel_80181E9C;
            glowDrawDimGreyCapsule(&glowPoints[0], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawRedDisc(&glowPoints[8], GLOW_RADIUS_SCALE);
        } break;
        case 3: {
            const SVECTOR* glowPoints = D_shelter_b1_control_room_access_tunnel_80181EAC;
            glowDrawDimGreyCapsule(&glowPoints[0], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
        } break;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB1ControlRoomAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_control_room_access_tunnel_8017F624(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
