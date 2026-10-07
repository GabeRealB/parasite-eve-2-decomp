#include "rooms/shelter_b1_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "shelter_b1_north_maintenance_walkway_private.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
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

static void _shelterB1NorthMaintenanceWalkwaySetSceneSpriteVisibility(u8 sceneSeen);

extern TaskDesc D_shelter_b1_north_maintenance_walkway_80184A78;

s32        func_shelter_b1_north_maintenance_walkway_8017D7A4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
void       func_shelter_b1_north_maintenance_walkway_8017D918(Task*);
static s32 _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Requests use of the key item in the first payload word.
enum { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc D_shelter_b1_north_maintenance_walkway_80184A78 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_north_maintenance_walkway_80184A84[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_north_maintenance_walkway_8017D7A4 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM, _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand },
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
static void _shelterB1NorthMaintenanceWalkwayRoomIdle(Task* task);

/// Binds the shared enemy-effect selectors to this room's implementations.
///
/// Stores packed bank-6 task IDs for later `effectSpawn` calls. Call after
/// room-effect initialization clears the selectors and before enemies use them.
/// This overlay must remain loaded while the selected IDs are used and while
/// their spawned tasks are live.
static __inline__ void _shelterB1NorthMaintenanceWalkwayBindEffectTasks(void)
{
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
}

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
/// `mapShelterRoomVariantResolve`. Messages 0xB and 0xE start the room's event - command 3 /
/// 2 on flag 0x14D / 0x14E; any other message answers 1.
s32 func_shelter_b1_north_maintenance_walkway_8017D7A4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;
    s32              cmd;
    s32              snd;
    s16              flag;

    *out = *in;
    mapShelterRoomVariantResolve(in, out);
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
                    capSpawnEventIfIdle(1, CAP_EVENT_NO_FLAGS);
                    taskKill(arg0);
                }
            } else {
                arg0->killCountdown--;
            }
            break;
    }
}

/// Rejects key-item use in this room, returning 0 without changing the item or room.
static s32 _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores room commands and both payload words, returning 0.
static s32 _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room actions without reading the borrowed request, returning 0.
static s32 _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
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
            capSpawnEventIfIdle(4, CAP_EVENT_NO_FLAGS);
        }
    }
    _shelterB1NorthMaintenanceWalkwaySetSceneSpriteVisibility(gameFlagGetNibble(GAME_FLAG_B2_NORTH_WALKWAY_SCENE_SEEN));
    arg0->state = (s32)(arg0->state + 1);
}

/// Keeps the initialized room task idle until another owner changes its state.
static void _shelterB1NorthMaintenanceWalkwayRoomIdle(Task* task)
{
}

/// The room task's three states: set-up, idle and exit.
static const TaskFuncTable3 D_shelter_b1_north_maintenance_walkway_8017D5D8 = {
    { func_shelter_b1_north_maintenance_walkway_8017DA4C, _shelterB1NorthMaintenanceWalkwayRoomIdle, taskKill },
};

/// The room task. Runs the handler for its current state from the room's
/// three-entry state table: set-up, an idle tick, and `taskKill`.
void func_shelter_b1_north_maintenance_walkway_8017DAFC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_north_maintenance_walkway_8017D5D8;
    sp.funcs[task->state](task);
}

/// Restores view 3's scene sprite visibility from the B2 north walkway scene flag.
///
/// Requires this room's loaded sprite directory. Flag 0 hides the single-sprite
/// batch, 1 reveals it, and all other byte values preserve its current visibility.
static void _shelterB1NorthMaintenanceWalkwaySetSceneSpriteVisibility(u8 sceneSeen)
{
    enum { SCENE_NOT_SEEN    = 0,
           SCENE_SEEN        = 1,
           SCENE_VIEW_INDEX  = 2,
           SCENE_BATCH_INDEX = 2 };
    const GameLocationKey* location = &gGameSession->location.loc;
    SpriteView*            views;
    SpriteBatch*           batches;

    views = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1];
    if (sceneSeen == SCENE_NOT_SEEN) {
        batches                           = views[SCENE_VIEW_INDEX].batches;
        batches[SCENE_BATCH_INDEX].hidden = 1;
    } else if (sceneSeen == SCENE_SEEN) {
        batches                           = views[SCENE_VIEW_INDEX].batches;
        batches[SCENE_BATCH_INDEX].hidden = 0;
    }
}

void shelterB1NorthMaintenanceWalkwayDrawGlowsTask(Task* task)
{
    enum { GLOW_TASK_INITIALIZE   = 0,
           GLOW_TASK_DRAW         = 1,
           LAMP_GLOW_RADIUS_SCALE = 0x200 };

    if (task->state == GLOW_TASK_INITIALIZE) {
        _shelterB1NorthMaintenanceWalkwayBindEffectTasks();
        task->state = GLOW_TASK_DRAW;
    }

    // The point arrays' boundaries are unresolved; views 3, 4 and 6 cross them.
    switch (gGameSession->location.loc.view) {
        case 2: {
            const SVECTOR* glowPoints;
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184B18;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 3: {
            const SVECTOR* glowPoints;
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184B08;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[2], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 4:
        case 6: {
            const SVECTOR* glowPoints;
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184B48;
            glowDrawRedDisc(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-18], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-16], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-12], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-10], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-8], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-6], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            break;
        }
        case 5: {
            const SVECTOR* glowPoints;
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184AB8;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[6], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
    }
}
