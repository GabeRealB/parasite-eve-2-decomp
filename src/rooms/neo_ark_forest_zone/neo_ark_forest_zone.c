#include "rooms/neo_ark_forest_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_forest_zone_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/falling_leaves.h"
#include "../../shared/roaming_enemies.h"

extern s8 D_neo_ark_forest_zone_80182E40;

extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

/// Payload handed to the helper task 0x31 the event may start.
extern RoomFadeStorage gRoomEventFade;

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `RoomFx_TrailOffsets[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_forest_zone_8017DA80(Task* arg0);
static void _neoArkForestZonePauseRoamersState(Task* task);

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_neo_ark_forest_zone_80182E40 = 0;

/// Three bytes stored after the flag; nothing references them.
u8 D_neo_ark_forest_zone_80182E41 = 123;

u8 D_neo_ark_forest_zone_80182E42 = 4;

u8 D_neo_ark_forest_zone_80182E43 = 20;

ActorCommand gRoamerCommand = { 0 };

RoomLatchedEvent gRoomEventLatched = { 0 };

u16 gRoamerReserveHp[5] = {
    0,
    0,
    0,
    0,
    0,
};

#include "../../shared/room_event_staged_task.inc.c"

s32 neoArkForestZoneRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    enum { NEO_ARK_FOREST_ZONE_KEY_ITEM_REFUSED = 0 };

    return NEO_ARK_FOREST_ZONE_KEY_ITEM_REFUSED;
}

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static __inline__ s32 _neoArkForestZoneStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    D_neo_ark_forest_zone_80182E40 = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *transition;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&D_neo_ark_forest_zone_80181DBC, 0, 0, 0);
            D_neo_ark_forest_zone_80182E40 = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

/// Room handler for the save-location message: copies the incoming record onto
/// the outgoing one and forwards both to `mapNeoArkResolveRoomVariant`. On a first pass
/// (`queryOnly` clear, the flag that asks a handler to only report what *would*
/// happen) it also restarts the room's ambience sound. Message 0x1D builds the
/// room's event record - cap command 2, the stage sound, flag 0x140 - and hands
/// it to `_neoArkForestZoneStartEvent`; every other message is not consumed and
/// answers 1.
s32 func_neo_ark_forest_zone_8017D7E4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_NEO_ARK_FOREST_ZONE_AMBIENCE, 0x3C);
    }
    if (in->areaId != GAME_AREA_NEO_ARK_WOODLAND_PATH) {
        return 1;
    }
    event.capCmd   = 2;
    event.stageSnd = 0x550B0003;
    event.flagId   = GAME_FLAG_FOREST_ZONE_TO_WOODLAND_PATH_SCENE;
    event.fade     = 0;
    return _neoArkForestZoneStartEvent(out, &event);
}

s32 neoArkForestZoneIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg)
{
    enum { NEO_ARK_FOREST_ZONE_COMMAND_IGNORED = 0 };

    return NEO_ARK_FOREST_ZONE_COMMAND_IGNORED;
}

/// Room message handler: on the first-visit sub-id (`warp == 1`) with flag
/// 0xBD unset and the session's visit count equal to that sub-id, latches flag
/// 0xBD and starts the room's fade with the record at `D_..._80181E6C`. Then
/// forwards the message to the room's own task, answering -1 while that task
/// does not exist yet.
s32 func_neo_ark_forest_zone_8017D958(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 visit;

    visit = in->warp;
    if (visit == 1) {
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN) == 0 && gGameSession->location.loc.variant == visit) {
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN, 1);
            evsStartScript(D_neo_ark_forest_zone_80181E6C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
    }
    if (D_neo_ark_forest_zone_80181E68 != NULL) {
        return TASK_MESSAGE_DISPATCH_POINTERS(D_neo_ark_forest_zone_80181E68, arg1, in, out);
    }
    return -1;
}

s32 neoArkForestZoneForwardActorEvent(Task* unusedTask, s32 messageId, s32 eventValue, s32 secondArg)
{
    enum { NEO_ARK_FOREST_ZONE_ROAMER_TASK_ABSENT = -1 };

    return D_neo_ark_forest_zone_80181E68 == NULL
               ? NEO_ARK_FOREST_ZONE_ROAMER_TASK_ABSENT
               : taskMessageDispatch(D_neo_ark_forest_zone_80181E68, messageId, eventValue, secondArg);
}

void neoArkForestZoneStartRoamerAmbush(void)
{
    if (D_neo_ark_forest_zone_80181E68 != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(D_neo_ark_forest_zone_80181E68, ACTOR_COMMAND_MESSAGE_APPLY, &D_neo_ark_forest_zone_80181E38, 0);
    }
}

/// State 0 of the room setup task: installs the message table and pointer
/// slot 7, starts the ambience, spawns the room's own task, and on the first
/// visit (`gGameSession->location.loc.variant == 1`) with flag 0xBD unset has the
/// slot-4 task relay message 0x7DA carrying the first payload record. Then
/// advances state.
static void func_neo_ark_forest_zone_8017DA80(Task* arg0)
{
    arg0->msgTable = D_neo_ark_forest_zone_80181DC8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_FOREST_ZONE_AMBIENCE, 0, 0);
    D_neo_ark_forest_zone_80181E68 = taskSpawnFromTable(&D_neo_ark_forest_zone_80182E18, 0, 0, 0);
    if (gGameSession->location.loc.variant == 1 && gameFlagGetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN) == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_neo_ark_forest_zone_80181E30, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    arg0->state = arg0->state + 1;
}

/// Pauses the forest roaming-enemy pool before the first-visit ambush scene.
///
/// Room-task state 1 sends the pause command only in variant 1 while the
/// first-visit flag is clear, then advances to the room task's idle state.
/// The live pool-B task must already have been created by initialization.
static void _neoArkForestZonePauseRoamersState(Task* task)
{
    enum { NEO_ARK_FOREST_ZONE_FIRST_VISIT_VARIANT = 1 };

    if (gGameSession->location.loc.variant == NEO_ARK_FOREST_ZONE_FIRST_VISIT_VARIANT && gameFlagGetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN) == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(D_neo_ark_forest_zone_80181E68, ACTOR_COMMAND_MESSAGE_APPLY, &D_neo_ark_forest_zone_80181E30, 0);
    }
    task->state = task->state + 1;
}

/// Holds the room setup task in state 2 with its message handlers installed.
static void _neoArkForestZoneSetupIdleState(Task* task)
{
    // Preserve the 16-byte stack frame despite having no runtime work in this state.
    char stackFrame[0x10];
}

/// State table of the room setup task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D5D8 = { {
    func_neo_ark_forest_zone_8017DA80,
    _neoArkForestZonePauseRoamersState,
    _neoArkForestZoneSetupIdleState,
    taskKill,
} };

void neoArkForestZoneRoomTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_forest_zone_8017D5D8;
    handlers.funcs[task->state](task);
}

#include "../../shared/falling_leaves_task.inc.c"

void neoArkForestZoneLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/falling_leaves_draw_neo_ark.inc.c"

void neoArkForestZoneConfigureEffectsTask(Task* task)
{
    enum { NEO_ARK_FOREST_ZONE_EFFECTS_INITIALIZE,
           NEO_ARK_FOREST_ZONE_EFFECTS_ACTIVE };

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    if (task->state == NEO_ARK_FOREST_ZONE_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_FOREST_ZONE_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_FOREST_ZONE_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_FOREST_ZONE_SPARK_BURST;
        task->state             = NEO_ARK_FOREST_ZONE_EFFECTS_ACTIVE;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void neoArkForestZoneRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void neoArkForestZoneRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_forest_zone_8017F76C(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
