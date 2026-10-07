#include "rooms/shelter_b2_pod_access_tunnel.h"

#include "types.h"

#include "shelter_b2_pod_access_tunnel_private.h"

#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b2_pod_access_tunnel_80185708[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_b2_pod_access_tunnel_80185708_value __asm__("D_shelter_b2_pod_access_tunnel_80185708");

extern RoomFadeStorage  gRoomEventFade;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

s32 D_shelter_b2_pod_access_tunnel_801856A0[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b2_pod_access_tunnel_801856AC = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856B8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856C0[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856C8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_pod_access_tunnel_801856AC },
};

WorldCollisionSurfaceProperties* D_shelter_b2_pod_access_tunnel_801856D8[8] = {
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856C0,
    D_shelter_b2_pod_access_tunnel_801856C8,
    D_shelter_b2_pod_access_tunnel_801856D0,
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856B8,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b2_pod_access_tunnel_80185708[4] = {
    0,
    83,
    70,
    179,
};

RoomLatchedEvent gRoomEventLatched = { 0 };

static void func_shelter_b2_pod_access_tunnel_8017DBA8(Task* arg0);

static void func_shelter_b2_pod_access_tunnel_8017DC0C(Task* task);

static __inline__ s32 _shelterB2PodAccessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

#include "../../shared/room_event_staged_task.inc.c"

static __inline__ s32 _shelterB2PodAccessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b2_pod_access_tunnel_80185708_value = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, 1);
            }
            taskSpawnFromTable(&D_shelter_b2_pod_access_tunnel_80183BC0, 0, 0, 0);
            D_shelter_b2_pod_access_tunnel_80185708_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_b2_pod_access_tunnel_8017D7C4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_R48) {
        if (gameFlagGetNibble(GAME_FLAG_B2_POD_TUNNEL_R48_DOOR_UNLOCKED) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < 6 ? 2 : 6);
            }
            return 0;
        }
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_SEPTIC_TANK) {
        if (gameFlagGetNibble(GAME_FLAG_118) == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd1(4);
            }
            return 2;
        }
        event.capCmd   = 5;
        event.stageSnd = 0x54230001;
        event.flagId   = GAME_FLAG_B2_POD_TUNNEL_TO_SEPTIC_SCENE;
        event.fade     = 0;
        return _shelterB2PodAccessTunnelStartEvent(out, &event);
    }
    return 1;
}

/// The three states `func_shelter_b2_pod_access_tunnel_8017DC14` dispatches
/// the room task through: set-up, an idle tick, and removal.
static const TaskFuncTable3 D_shelter_b2_pod_access_tunnel_8017D5D8 = {
    { func_shelter_b2_pod_access_tunnel_8017DBA8, func_shelter_b2_pod_access_tunnel_8017DC0C, taskKill },
};

void func_shelter_b2_pod_access_tunnel_8017D9A8(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(gameFlagGetNibble(GAME_FLAG_0FC) != 0 ? 3 : 1);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state++;
            return;
        case 1:
            if (capIsBusy() == 0) {
                task->state++;
            }
            return;
        case 2:
            if (capGetVariantKey() != 0xA) {
                if (capGetVariantKey() == 1) {
                    gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, 2);
                }
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_TUNNEL_RIDE_TO_B1, 0, 0);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, 0);
            task->state++;
            return;
        case 3:
            if (SndVoice_HasActiveId(SOUND_SHELTER_B2_POD_TUNNEL_RIDE_TO_B1) == 0) {
                task->state++;
            }
            return;
        case 4:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
            gDisplayState.spriteVariant                                = 1;
            taskSpawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b2_pod_access_tunnel_8017DB28(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB30(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        Gp_MsgPlayerWeapon(0);
        taskSpawnFromTable(&D_shelter_b2_pod_access_tunnel_80183BFC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB70(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB78(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 4) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
    }
    return 0;
}

static void func_shelter_b2_pod_access_tunnel_8017DBA8(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_pod_access_tunnel_80183BCC;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 0x16) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
    }
    arg0->state = arg0->state + 1;
}

static void func_shelter_b2_pod_access_tunnel_8017DC0C(Task* task)
{
}

/// Runs one tick of a room task through the three-state table
/// `D_shelter_b2_pod_access_tunnel_8017D5D8`, copying the table onto the stack
/// and calling the entry for the task's current state.
void func_shelter_b2_pod_access_tunnel_8017DC14(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_pod_access_tunnel_8017D5D8;
    sp.funcs[task->state](task);
}
