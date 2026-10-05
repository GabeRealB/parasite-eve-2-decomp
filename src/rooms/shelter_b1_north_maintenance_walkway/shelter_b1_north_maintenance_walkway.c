#include "rooms/shelter_b1_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "shelter_b1_north_maintenance_walkway_private.h"

#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"

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
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_b1_north_maintenance_walkway_80185B7C[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_b1_north_maintenance_walkway_80185B7C_value __asm__("D_shelter_b1_north_maintenance_walkway_80185B7C");

extern TaskMessageEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc         D_shelter_b1_north_maintenance_walkway_80184AAC[];

extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184AB8[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B08[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B18[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B48[];

static void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0);

extern TaskDesc D_shelter_b1_north_maintenance_walkway_80184A78;

s32  func_shelter_b1_north_maintenance_walkway_8017D7A4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
void func_shelter_b1_north_maintenance_walkway_8017D918(Task*);
s32  func_shelter_b1_north_maintenance_walkway_8017DA34(Task*, s32, s32, s32);
s32  func_shelter_b1_north_maintenance_walkway_8017DA3C(Task*, s32, s32, s32);
s32  func_shelter_b1_north_maintenance_walkway_8017DA44(Task*, s32, s32, s32);

TaskDesc D_shelter_b1_north_maintenance_walkway_80184A78 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_north_maintenance_walkway_80184A84[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_north_maintenance_walkway_8017D7A4 },
    { 5105, func_shelter_b1_north_maintenance_walkway_8017DA34 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_north_maintenance_walkway_8017DA44 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_north_maintenance_walkway_8017DA3C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_north_maintenance_walkway_80184AAC[1] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_north_maintenance_walkway_8017D918, { .value = 0 } },
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184AB8[10] = {
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
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184B08[2] = {
    { 3106, -197, 2648, 0 },
    { 3106, -197, 2036, 0 },
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184B18[6] = {
    { 653, -197, 2896, 0 },
    { -31, -197, 2896, 0 },
    { -42, -197, 5113, 0 },
    { 597, -197, 5113, 0 },
    { -1562, -197, 5113, 0 },
    { -2493, -197, 5113, 0 },
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184B48[1] = {
    { 749, -1283, 2310, 0 },
};

static __inline__ s32 _shelterB1NorthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, RoomLatchedEvent* event);
static void func_shelter_b1_north_maintenance_walkway_8017DA4C(Task* arg0);
static void func_shelter_b1_north_maintenance_walkway_8017DAF4(Task* task);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB1NorthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_north_maintenance_walkway_80185B7C_value = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, 1);
            }
            taskSpawnFromTable(&D_shelter_b1_north_maintenance_walkway_80184A78, 0, 0, 0);
            D_shelter_b1_north_maintenance_walkway_80185B7C_value = 1;
        }
        return 2;
    }
    return 1;
}

#include "../../shared/room_event_staged_task.inc.c"

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Messages 0xB and 0xE start the room's event - command 3 /
/// 2 on flag 0x14D / 0x14E; any other message answers 1.
s32 func_shelter_b1_north_maintenance_walkway_8017D7A4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;
    s32              cmd;
    s32              snd;
    s16              flag;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B1_STOREROOM) {
        goto message0E;
    }
    snd            = 0x540C0001;
    cmd            = 3;
    event.stageSnd = snd;
    flag           = 0x14D;
start_event:
    event.capCmd = cmd;
    event.flagId = flag;
    event.fade   = 0;
    return _shelterB1NorthMaintenanceWalkwayStartEvent(out, &event);
message0E:
    if (in->areaId == GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS) {
        snd            = 0x540C0003;
        cmd            = 2;
        event.stageSnd = snd;
        flag           = 0x14E;
        goto start_event;
    }
    return 1;
}

void func_shelter_b1_north_maintenance_walkway_8017D918(Task* arg0)
{
    SVECTOR unused;

    switch (arg0->state) {
        case 0:
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
                gGameSession->flowFlags |= GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON;
                arg0->state++;
            }
            break;
        case 1:
            if (gSceneCombatState.battleRefs == 0) {
                gSceneCombatState.signals.bytes.endDelayFrames = SCENE_COMBAT_END_DELAY_FRAMES;
                arg0->killCountdown                            = 0x3E;
                arg0->state++;
            }
            break;
        case 2:
            if (arg0->killCountdown == 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    Gp_SpawnIfCapIdle(1, 0);
                    taskKill(arg0);
                }
            } else {
                arg0->killCountdown--;
            }
            break;
    }
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA34(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA3C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA44(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

static void func_shelter_b1_north_maintenance_walkway_8017DA4C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_north_maintenance_walkway_80184A84;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 2) {
        taskSpawnFromTable(D_shelter_b1_north_maintenance_walkway_80184AAC, 0, 0, 0);
        if (gameFlagGetNibble(GAME_FLAG_NORTH_MAINTENANCE_WALKWAY_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_NORTH_MAINTENANCE_WALKWAY_SCENE, 1);
            Gp_SpawnIfCapIdle(4, 0);
        }
    }
    func_shelter_b1_north_maintenance_walkway_8017DB54(gameFlagGetNibble(GAME_FLAG_B2_NORTH_WALKWAY_SCENE_SEEN));
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state: does nothing.
static void func_shelter_b1_north_maintenance_walkway_8017DAF4(Task* task)
{
}

/// The room task's three states: set-up, idle and exit.
static const TaskFuncTable3 D_shelter_b1_north_maintenance_walkway_8017D5D8 = {
    { func_shelter_b1_north_maintenance_walkway_8017DA4C, func_shelter_b1_north_maintenance_walkway_8017DAF4, taskKill },
};

/// The room task. Runs the handler for its current state from the room's
/// three-entry state table: set-up, an idle tick, and `taskKill`.
void func_shelter_b1_north_maintenance_walkway_8017DAFC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_north_maintenance_walkway_8017D5D8;
    sp.funcs[task->state](task);
}

static void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteView*      rec;
    SpriteBatch*     batches;
    s32              mode;

    rec  = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1];
    mode = arg0 & 0xFF;
    if (mode == 0) {
        batches           = rec[2].batches;
        batches[2].hidden = 1;
    } else if (mode == 1) {
        batches           = rec[2].batches;
        batches[2].hidden = 0;
    }
}

void func_shelter_b1_north_maintenance_walkway_8017DBC8(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPARK_BURST;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2;
        arg0->state               = 1;
    }

    switch (gGameSession->location.loc.view) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B18;
            glowDrawCone(&p[0], 0x200, 0x800);
            glowDrawCone(&p[4], 0x200, -0x400);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B08;
            glowDrawCone(&p[0], 0x200, 0x800);
            glowDrawCone(&p[2], 0x200, 0x800);
            glowDrawCone(&p[4], 0x200, -0x400);
            break;
        }
        case 4:
        case 6: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B48;
            glowDrawRedDisc(&p[0], 0x200);
            glowDrawCone(&p[-18], 0x200, 0);
            glowDrawCone(&p[-16], 0x200, 0);
            glowDrawCone(&p[-14], 0x200, 0);
            glowDrawCone(&p[-12], 0x200, -0x400);
            glowDrawCone(&p[-10], 0x200, -0x400);
            glowDrawCone(&p[-8], 0x200, -0x400);
            glowDrawCone(&p[-6], 0x200, 0x800);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184AB8;
            glowDrawCone(&p[0], 0x200, 0);
            glowDrawCone(&p[6], 0x200, -0x400);
            break;
        }
    }
}
