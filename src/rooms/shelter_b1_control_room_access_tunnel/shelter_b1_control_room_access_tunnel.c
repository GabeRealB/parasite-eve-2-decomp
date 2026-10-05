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

/// The smoke trail's two spawn offsets: `[0]` places the effect's own
/// coordinate and `[1]` the second trail's origin. State 1 reads `[1]` again
/// under its own name.

s32 func_shelter_b1_control_room_access_tunnel_8017D5E4(Task*, s32, s32, s32);
s32 func_shelter_b1_control_room_access_tunnel_8017D5EC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_control_room_access_tunnel_8017D630(Task*, s32, s32, s32);
s32 func_shelter_b1_control_room_access_tunnel_8017D638(Task*, s32, s32, s32);

TaskMessageEntry D_shelter_b1_control_room_access_tunnel_80181E74[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_control_room_access_tunnel_8017D5EC },
    { 5105, func_shelter_b1_control_room_access_tunnel_8017D5E4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_control_room_access_tunnel_8017D638 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_control_room_access_tunnel_8017D630 },
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

static void func_shelter_b1_control_room_access_tunnel_8017D640(Task* task);
static void func_shelter_b1_control_room_access_tunnel_8017D684(Task* task);

s32 func_shelter_b1_control_room_access_tunnel_8017D5E4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one,
/// passes both to `func_map_shelter_80179A04` and returns 1.
s32 func_shelter_b1_control_room_access_tunnel_8017D5EC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_shelter_b1_control_room_access_tunnel_8017D630(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_control_room_access_tunnel_8017D638(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room's task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to state 1.
static void func_shelter_b1_control_room_access_tunnel_8017D640(Task* task)
{
    task->msgTable = D_shelter_b1_control_room_access_tunnel_80181E74;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's task: does nothing until the task is killed.
static void func_shelter_b1_control_room_access_tunnel_8017D684(Task* task)
{
}

/// State handlers of the task `func_shelter_b1_control_room_access_tunnel_8017D68C`
/// runs, which copies the table to the stack and calls the entry for the
/// task's state: the room's setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b1_control_room_access_tunnel_8017D5C4 = {
    { func_shelter_b1_control_room_access_tunnel_8017D640, func_shelter_b1_control_room_access_tunnel_8017D684, taskKill }
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

/// On its first tick stores six effect ids in gameplay's `D_801157xx` slots;
/// every tick then draws the room's gouraud cones and disc for the current
/// camera view (views 2 and 3).
void func_shelter_b1_control_room_access_tunnel_8017E1BC(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        gRoomEffectFlashId        = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_SPARK_BURST;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_ORANGE_BURST_2;
        arg0->state               = 1;
    }
    view = viewGetMappedIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_control_room_access_tunnel_80181E9C;
            glowDrawCone(&p[0], 0x200, 0x400);
            glowDrawCone(&p[4], 0x200, 0x400);
            glowDrawRedDisc(&p[8], 0x200);
        } break;
        case 3: {
            SVECTOR* p = D_shelter_b1_control_room_access_tunnel_80181EAC;
            glowDrawCone(&p[0], 0x200, -0x400);
            glowDrawCone(&p[4], 0x200, -0x400);
        } break;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_control_room_access_tunnel_8017E2D8(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_control_room_access_tunnel_8017ED3C(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_control_room_access_tunnel_8017F624(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
