#include "rooms/acropolis_roof_garden.h"
#include "../../stages/akropolis/event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/effects/falling_leaves.h"
#include "../../shared/actor/actor_contacts.h"
#include "../../shared/effects/glow_draw.h"

#include "assets/rooms/akropolis/roof_garden/declarations.inc"

static Task* _gAcropolisRoofGardenCutsceneSoundTask;

static s32 _gAcropolisRoofGardenRequestedAmbiencePercent;

/// Whole-unit X/Y/Z displacement left by `_actorContactApplyGridPushback`.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Initializes a flare quad's command and colours, with vertex 2 lit and a black rim.
///
/// `quad` must be a stable writable POLY_G4 pointer without side effects; it is
/// evaluated repeatedly. `red` and `green` each evaluate once, after command and
/// rim writes, and narrow to bytes. No identifiers are captured or retained.
#define ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, red, green) \
    ((void)(setPolyG4((quad)),                                  \
            setRGB0((quad), 0, 0, 0),                           \
            setRGB1((quad), 0, 0, 0),                           \
            setRGB2((quad), (red), (green), 0),                 \
            setRGB3((quad), 0, 0, 0)))

static void _acropolisRoofGardenInitRoomTask(Task* task);
static void _acropolisRoofGardenTickRoom(Task* task);

/// Room-task lifecycle: register the receiver, start the arrival scene, release.
///
/// Copied by value for unchecked state 0..2 dispatch; callbacks require this overlay loaded.
static const TaskFuncTable3 _gAcropolisRoofGardenRoomTaskStates = {
    { _acropolisRoofGardenInitRoomTask, _acropolisRoofGardenTickRoom, taskKill },
};

static void _acropolisRoofGardenReleaseMaggotCaterpillarEntrance(void);

/// Sound-task actions passed by the arrival scene and its skip script.
enum {
    ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_OPENING = 0,
    ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CLOSING = 1,
    ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CANCEL  = 2,
};
static void _acropolisRoofGardenControlCutsceneSound(s32 soundAction);

static AnimationSet _gAkropolisRoofGardenAcropolisRoofGardenArrivalPlayerAnim00Clip;
static AnimationSet _gAkropolisRoofGardenAcropolisRoofGardenArrivalPlayerAnim01Clip;
static AnimationSet _gAkropolisRoofGardenAcropolisRoofGardenArrivalPlayerAnim02Clip;

static s32  _acropolisRoofGardenResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisRoofGardenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisRoofGardenHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _acropolisRoofGardenHandleSoundMessage(Task* task, s32 messageId, s32 soundCue, s32 unusedArg);
static s32  _acropolisRoofGardenHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandIndex, s32 unusedArg);
static void _acropolisRoofGardenAmbienceTask(Task* task);
static void _acropolisRoofGardenCutsceneOpeningSoundTask(Task* task);
static void _acropolisRoofGardenCutsceneClosingSoundTask(Task* task);

#include "assets/rooms/akropolis/roof_garden/clips_1.inc"

enum { ACROPOLIS_ROOF_GARDEN_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Room-transition, action, command, key-item and sound-cue dispatch.
///
/// The room task borrows this six-record list while the overlay is loaded.
/// The final TASK_MESSAGE_TABLE_END record terminates lookup; payload types
/// are determined by each message, and unsupported IDs return zero.
static TaskMessageEntry _gAcropolisRoofGardenRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisRoofGardenResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisRoofGardenHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _acropolisRoofGardenHandleCommandMessage },
    { ACROPOLIS_ROOF_GARDEN_MESSAGE_USE_KEY_ITEM, _acropolisRoofGardenRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisRoofGardenHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Borrowed handle of the arrival scene's active opening or closing sound task.
///
/// NULL means no cancellable sequence. A final cue clears it before teardown;
/// cancellation kills the task before clearing it. Starts assume the previous
/// sequence has finished and replace the handle even when allocation fails.
static Task* _gAcropolisRoofGardenCutsceneSoundTask = NULL;

enum { ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY = 32 };

/// Bodyless sound-task recipes: 0 ambience, 1 arrival opening, 2 arrival closing.
///
/// All run at priority 32 with zero spawn arguments; TASK_DESC_END closes the
/// directory. The recipes and callbacks live with the loaded room overlay.
static TaskDesc _gAcropolisRoofGardenSoundTaskDescriptors[4] = {
    { { { TASK_BODY_NONE, ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY } }, _acropolisRoofGardenAmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY } }, _acropolisRoofGardenCutsceneOpeningSoundTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY } }, _acropolisRoofGardenCutsceneClosingSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Borrowed player clips for arrival opening, closing and dialogue, in that order.
///
/// The bank-copy request installs all three at extension slots 47..49. The
/// table and clip assets belong to the room overlay and stay live during playback.

#define ROOM_PART D_acropolis_roof_garden_80183C40
#include "assets/rooms/akropolis/roof_garden/events.inc"

#include "assets/rooms/akropolis/roof_garden/acropolis_roof_garden_arrival_player_bank.inc"

enum {
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_OPENING_SET  = ANIMATION_BANK_BASE_SET_COUNT,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_CLOSING_SET  = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_DIALOGUE_SET = ANIMATION_BANK_BASE_SET_COUNT + 2,
};

// Arrival waits count interpreter ticks; sound cues use type-3 bank 0x108.
enum {
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_CAP_COMMAND               = 1,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_HIDE               = 0,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_SHOW               = 1,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_OPENING_WAIT_FRAMES       = 154,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_WAIT_FRAMES        = 180,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_CLIP4_FRAMES       = 79,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_CLOSING_WAIT_FRAMES       = 115,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_FADE_TICKS          = 120,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SKIP_FADE_FRAMES          = 8,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SKIP_VIEW                 = 3,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_1               = SOUND_ID(3, 0x108, 1),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_2               = SOUND_ID(3, 0x108, 2),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_3               = SOUND_ID(3, 0x108, 3),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_4               = SOUND_ID(3, 0x108, 4),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_5               = SOUND_ID(3, 0x108, 5),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_ALL_TYPE_3          = SOUND_ID(3, 0, 0),
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_SECOND_CLIP_DELAY_FRAMES = 30,
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_BATTLE_DELAY_FRAMES      = 40,
};

#define ROOM_PART _gAcropolisRoofGardenArrivalPlayerBankCopy
#include "assets/rooms/akropolis/roof_garden/events.inc"
/// Arrival-scene latch (0 unstarted, 1 attempted), reset by loading the overlay.
///
/// Set before starting the warp-2 script; a failed start is not retried.
static s32 _gAcropolisRoofGardenArrivalSceneStarted = 0;

#include "assets/rooms/akropolis/roof_garden/clips_2.inc"

/// Player extension slots for the Maggot/Caterpillar entrance: empty, start, end.
///
/// Copying all three clears slot 47 and installs clips 48/49. Only those two
/// installed clips are played; their room-owned assets stay live through playback.
#include "assets/rooms/akropolis/roof_garden/acropolis_roof_garden_entrance_player_bank.inc"

enum {
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_START_SET    = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_END_SET      = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_BLEND_FRAMES = 20,
};

/// Logical-view bits used by the roof garden's placed light glows.
enum {
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_2 = 1 << (2 - 1),
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_3 = 1 << (3 - 1),
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_4 = 1 << (4 - 1),
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_5 = 1 << (5 - 1),
};

enum {
    ACROPOLIS_ROOF_GARDEN_WARP_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 2),
    ACROPOLIS_ROOF_GARDEN_WARP_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 1),
};

enum {
    ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_X = 3,
    ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_Z = 3,
};

/// Trigger payloads selecting the sanctuary endpoint and the room's scripted actions.
enum {
    ACROPOLIS_ROOF_GARDEN_SANCTUARY_WARP_ENDPOINTS   = (1 << 4) | 2, // Departure endpoint 1, sanctuary arrival endpoint 2
    ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_UNHANDLED      = 0,
    ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_ARM_ENTRANCE   = 1,
    ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_START_ENTRANCE = 2,
    ACROPOLIS_ROOF_GARDEN_ROOM_COMMAND_STORY         = 4,
};

/// Placement-entry keys and decimal file numbers used by the resource lists.
///
/// Entry keys match the placement tables. File numbers select actor libraries
/// together with the base-10 or base-20 file-group selector in each resource.
enum {
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_VIEW_FIGURE = 110,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MAGGOT      = 55,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_CATERPILLAR = 26,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MOTH        = 8,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_VIEW_FIGURE  = 108,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MAGGOT       = 55,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_CATERPILLAR  = 26,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MOTH         = 7,
};

/// Texture bindings and colour-command values shared by the view sprite sources.
///
/// Both pages use 8-bit texels at VRAM Y zero; the CLUT is row 255, X zero.
/// Neutral RGB is 128, and zero code flags request opaque colour modulation.
enum {
    ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1 = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2 = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT           = getClut(0, 255),
    ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB    = 128,
    ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE         = 0,
};

/// Cue-2 script bases in the currently loaded type-1 sound bank.
enum {
    ACROPOLIS_ROOF_GARDEN_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1D,
    ACROPOLIS_ROOF_GARDEN_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1F,
};

#define ROOM_PART _gAcropolisRoofGardenEntrancePlayerBankCopy
#include "assets/rooms/akropolis/roof_garden/events.inc"
#include "assets/rooms/akropolis/roof_garden/markers.inc"
#include "assets/rooms/akropolis/roof_garden/view_masks.inc"
#include "assets/rooms/akropolis/roof_garden/samples.inc"
#include "assets/rooms/akropolis/roof_garden/header.inc"
#include "assets/rooms/akropolis/roof_garden/geometry.inc"
#include "assets/rooms/akropolis/roof_garden/view_boundaries.inc"
#include "assets/rooms/akropolis/roof_garden/action_triggers.inc"
#include "assets/rooms/akropolis/roof_garden/placements.inc"
#include "assets/rooms/akropolis/roof_garden/sprites.inc"
#include "assets/rooms/akropolis/roof_garden/lights.inc"
#include "assets/rooms/akropolis/roof_garden/cameras.inc"
#include "assets/rooms/akropolis/roof_garden/occluders.inc"
#include "assets/rooms/akropolis/roof_garden/surfaces.inc"

#include "assets/rooms/akropolis/roof_garden/acropolis_roof_garden_model_09868.inc"

/// Last ambience volume percentage requested by the view controller (0, 30 or 100).
///
/// Reset to zero on its first tick. Requests update this latch even when the
/// sound queue rejects them; it records no confirmation of playback or fading.
static s32 _gAcropolisRoofGardenRequestedAmbiencePercent = 0;

SVECTOR ActorContact_ScratchPosition;

/// Adjusts the roof-garden ambience to the current camera view.
///
/// The first tick resets the requested-volume latch. Subsequent ticks request
/// 30 percent in view 5, 100 percent in view 7 and silence elsewhere, sending
/// sound commands only when that percentage changes. Leaving an audible view
/// requests a 30-update fade. The latch tracks requests, not driver completion.
static void _acropolisRoofGardenAmbienceTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_INITIALIZE      = 0,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_UPDATE          = 1,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT          = 0,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_VIEW5_PERCENT   = 30,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT    = 100,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_MAX_ATTENUATION = 127,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_FADE_UPDATES    = 30,
    };
    s32 volumePercent;
    u8  view;
    s32 previousPercent;

    switch (task->state) {
        case ACROPOLIS_ROOF_GARDEN_AMBIENCE_INITIALIZE:
            _gAcropolisRoofGardenRequestedAmbiencePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT;
            task->state                                   = task->state + 1;
            return;
        case ACROPOLIS_ROOF_GARDEN_AMBIENCE_UPDATE:
            break;
        default:
            return;
    }

    view = gGameSession->location.view;
    if (view != 5) {
        volumePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT;
        if (view == 7) {
            volumePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT;
        }
    } else {
        volumePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_VIEW5_PERCENT;
    }

    previousPercent = _gAcropolisRoofGardenRequestedAmbiencePercent;
    if (volumePercent == previousPercent) {
        return;
    }
    // Convert the requested percentage to the driver's signed attenuation byte.
    if (previousPercent == ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT - volumePercent) * ACROPOLIS_ROOF_GARDEN_AMBIENCE_MAX_ATTENUATION) / ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT));
    } else if (volumePercent == ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, ACROPOLIS_ROOF_GARDEN_AMBIENCE_FADE_UPDATES);
    } else {
        sndEvtRequestScriptMix(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT - volumePercent) * ACROPOLIS_ROOF_GARDEN_AMBIENCE_MAX_ATTENUATION) / ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT));
    }
    _gAcropolisRoofGardenRequestedAmbiencePercent = volumePercent;
}

/// Allows a room transition and suppresses the untriggered sanctuary event on departure.
///
/// Borrows a complete request and writable reply, which may alias, through
/// synchronous dispatch. The reply preserves all eight bytes. Executing a
/// sanctuary-bound transition with a clear event latch sets it to 2 and hides
/// saved object 0x13; queries change neither. Always returns 1 (allowed).
static s32 _acropolisRoofGardenResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_UNSET      = 0,
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_SUPPRESSED = 2,
        ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID           = 0x13,
        ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN              = 2,
        ACROPOLIS_ROOF_GARDEN_TRANSITION_ALLOWED         = 1,
    };
    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_SANCTUARY && request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_UNSET) {
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_SUPPRESSED);
        areaSetCurrentObjectState(ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID, ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN);
    }
    return ACROPOLIS_ROOF_GARDEN_TRANSITION_ALLOWED;
}

/// Refuses every key-item use request in the roof garden.
///
/// All arguments are ignored. Returning zero selects the inventory's
/// "No use now" notice without consuming the selected item.
static s32 _acropolisRoofGardenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_ROOF_GARDEN_KEY_ITEM_USE_REFUSED = 0 };

    return ACROPOLIS_ROOF_GARDEN_KEY_ITEM_USE_REFUSED;
}

/// Arms and starts the Maggot/Caterpillar entrance at the room's action triggers.
///
/// Borrows a four-byte `DirectionActionRequest` through synchronous dispatch;
/// only `actionId` is read and no pointer is retained. In variants 1/7, action
/// 1 arms clear progress and action 2 starts the script only while armed.
/// Other actions do nothing. The zero second word is ignored; returns 1.
static s32 _acropolisRoofGardenHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_ACTION_ARM_ENTRANCE       = 1,
        ACROPOLIS_ROOF_GARDEN_ACTION_START_ENTRANCE     = 2,
        ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_CLEAR   = 0,
        ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_ARMED   = 1,
        ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_STARTED = 2,
        ACROPOLIS_ROOF_GARDEN_ACTION_REPLY              = 1,
    };
    switch (request->actionId) {
        case ACROPOLIS_ROOF_GARDEN_ACTION_ARM_ENTRANCE:
            if (((gGameSession->location.variant == 1) || (gGameSession->location.variant == 7)) && (gameFlagGetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_CLEAR)) {
                gameFlagSetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS, ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_ARMED);
            }
            break;
        case ACROPOLIS_ROOF_GARDEN_ACTION_START_ENTRANCE:
            if (((gGameSession->location.variant == 1) || (gGameSession->location.variant == 7)) && (gameFlagGetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_ARMED)) {
                evsStartScript(_gAcropolisRoofGardenMaggotCaterpillarEntranceScript, EVENT_SCRIPT_HUD_KEEP);
                gameFlagSetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS, ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_STARTED);
            }
            break;
    }
    return ACROPOLIS_ROOF_GARDEN_ACTION_REPLY;
}

/// Plays roof-garden sound entry 3 for room sound cue 3; other cues do nothing.
///
/// Integer payloads are consumed synchronously; the second is ignored.
/// Always returns zero. The empty switch arms preserve the compiled dispatch
/// tree; the last arm's original cue value is unproven.
static s32 _acropolisRoofGardenHandleSoundMessage(Task* task, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum { ACROPOLIS_ROOF_GARDEN_SOUND_CUE_ENTRY3 = 3 };
    switch (soundCue) {
        case ACROPOLIS_ROOF_GARDEN_SOUND_CUE_ENTRY3:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, ACROPOLIS_ROOF_GARDEN_SOUND_CUE_ENTRY3), 0, 0);
            break;
        case 5:
            break;
        case 9:
            break;
    }
    return 0;
}

/// Selects the pickup dialogue or starts the roof-garden story dialogue.
///
/// `ROOM_MESSAGE_COMMAND` carries the CAP command index as its first integer
/// payload. Command 2 chooses pickup command 5 for saved object states 0/1,
/// otherwise sequence slot 2, variant 0. Command 4 advances dialogue progress
/// to at least 6, pauses actor updates, plays CAP 4 in place and sets objective 7.
/// Other commands do nothing; all paths return zero. Requires live flags and
/// relocated CAP data; the receiver, message ID and second payload are unused.
static s32 _acropolisRoofGardenHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandIndex, s32 unusedArg)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP           = 2,
        ACROPOLIS_ROOF_GARDEN_COMMAND_STORY            = 4,
        ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP_AVAILABLE = 5,
        ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID         = 0x13,
        ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_UNSET       = 0,
        ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_PRESENT     = 1,
        ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_SLOT          = 2,
        ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_VARIANT       = 0,
        ACROPOLIS_ROOF_GARDEN_STORY_OBJECTIVE          = 7,
    };
    if (commandIndex == ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP) {
        // Retain the separate reads of the saved two-bit object state.
        if ((areaGetCurrentObjectState(ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID) == ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_UNSET) || (areaGetCurrentObjectState(ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID) == ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_PRESENT)) {
            capRunCommandWithTransition(ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP_AVAILABLE);
        } else {
            capStartSequenceSlot(ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_VARIANT);
        }
    }
    if (commandIndex == ACROPOLIS_ROOF_GARDEN_COMMAND_STORY) {
        if (gameFlagGetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX) < AKROPOLIS_STORY_DIALOGUE_HELICOPTER) {
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_HELICOPTER);
        }
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        capRunCommand(ACROPOLIS_ROOF_GARDEN_COMMAND_STORY, CAP_PLAYBACK_IN_PLACE);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_ROOF_GARDEN_STORY_OBJECTIVE);
    }
    return 0;
}

/// Plays the arrival cutscene's opening sound sequence and retires its task.
///
/// `state` counts callback ticks from zero. Entries 6..10 play at ticks
/// 20, 39, 57, 99 and 114. The last cue clears the script's task handle before
/// teardown; the final counter increment is retained after that call.
static void _acropolisRoofGardenCutsceneOpeningSoundTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE6_TICK  = 20,
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE7_TICK  = 39,
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE8_TICK  = 57,
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE9_TICK  = 99,
        ACROPOLIS_ROOF_GARDEN_OPENING_FINAL_TICK = 114,
    };
    switch (task->state) {
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE6_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 6), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE7_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 7), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE8_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 8), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE9_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 9), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_FINAL_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x0A), 0, 0);
            _gAcropolisRoofGardenCutsceneSoundTask = NULL;
            taskKill(task);
            break;
    }
    task->state += 1;
}

/// Plays the arrival cutscene's closing sound sequence and retires its task.
///
/// `state` counts callback ticks from zero. Entries 15/16 play at ticks 76/100.
/// The final cue clears the script's task handle before teardown, retaining
/// the counter increment after that call.
static void _acropolisRoofGardenCutsceneClosingSoundTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_CLOSING_CUE15_TICK = 76,
        ACROPOLIS_ROOF_GARDEN_CLOSING_FINAL_TICK = 100,
    };
    switch (task->state) {
        case ACROPOLIS_ROOF_GARDEN_CLOSING_CUE15_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x0F), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_CLOSING_FINAL_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x10), 0, 0);
            _gAcropolisRoofGardenCutsceneSoundTask = NULL;
            taskKill(task);
            break;
    }
    task->state += 1;
}

/// Starts or cancels the arrival cutscene's singleton timed-sound task.
///
/// `soundAction` is 0 opening, 1 closing, or 2 cancel; other values do nothing.
/// Starts replace the saved handle even on allocation failure and assume the
/// previous sequence has finished. Cancel kills a live task before clearing
/// its handle. Room sound data must remain loaded until the sequence ends.
static void _acropolisRoofGardenControlCutsceneSound(s32 soundAction)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_OPENING_SOUND_TASK_INDEX = 1,
        ACROPOLIS_ROOF_GARDEN_CLOSING_SOUND_TASK_INDEX = 2,
    };
    switch (soundAction) {
        case ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_OPENING:
            _gAcropolisRoofGardenCutsceneSoundTask = taskSpawnFromTable(_gAcropolisRoofGardenSoundTaskDescriptors, ACROPOLIS_ROOF_GARDEN_OPENING_SOUND_TASK_INDEX, 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CLOSING:
            _gAcropolisRoofGardenCutsceneSoundTask = taskSpawnFromTable(_gAcropolisRoofGardenSoundTaskDescriptors, ACROPOLIS_ROOF_GARDEN_CLOSING_SOUND_TASK_INDEX, 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CANCEL:
            if (_gAcropolisRoofGardenCutsceneSoundTask != NULL) {
                taskKill(_gAcropolisRoofGardenCutsceneSoundTask);
                _gAcropolisRoofGardenCutsceneSoundTask = NULL;
            }
            break;
    }
}

/// Registers the roof-garden room task and starts its ambient-sound controller.
///
/// Called in state 0 with the room resources loaded. Installs the message table
/// and room slot, changes music event 6 to 5 and attempts the ambience spawn.
/// Advances to state 1 even if that spawn fails; it retains no child handle.
static void _acropolisRoofGardenInitRoomTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_EVENT = 6,
        ACROPOLIS_ROOF_GARDEN_ROOM_MUSIC_EVENT    = 5,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_TASK_INDEX = 0,
    };
    task->msgTable = _gAcropolisRoofGardenRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_EVENT) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_ROOF_GARDEN_ROOM_MUSIC_EVENT;
    }
    taskSpawnFromTable(_gAcropolisRoofGardenSoundTaskDescriptors, ACROPOLIS_ROOF_GARDEN_AMBIENCE_TASK_INDEX, 0, 0);
    task->state += 1;
}

/// Starts the skippable arrival scene once per overlay load when arrival warp is 2.
///
/// Marks the sanctuary blocker cleared and immediately selects its placement
/// variant 3, discarding saved poses. The event latch is set before starting
/// the scene; only stage and area of the temporary location key are consumed.
static void _acropolisRoofGardenTickRoom(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_CUTSCENE_WARP             = 2,
        ACROPOLIS_ROOF_GARDEN_CUTSCENE_NOT_STARTED      = 0,
        ACROPOLIS_ROOF_GARDEN_CUTSCENE_STARTED          = 1,
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_CLEARED_VARIANT = 3,
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_BLOCKER_CLEARED = 1,
    };
    GameLocationKey sanctuary;

    if ((gGameSession->location.warp == ACROPOLIS_ROOF_GARDEN_CUTSCENE_WARP) && (_gAcropolisRoofGardenArrivalSceneStarted == ACROPOLIS_ROOF_GARDEN_CUTSCENE_NOT_STARTED)) {
        _gAcropolisRoofGardenArrivalSceneStarted = ACROPOLIS_ROOF_GARDEN_CUTSCENE_STARTED;
        evsStartScriptWithSkip(_gAcropolisRoofGardenArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisRoofGardenArrivalSkipScript);
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED, ACROPOLIS_ROOF_GARDEN_SANCTUARY_BLOCKER_CLEARED);
        sanctuary.stage = GAME_STAGE_AKROPOLIS;
        sanctuary.area  = GAME_AREA_AKROPOLIS_SANCTUARY;
        areaSetPlacementVariant(&sanctuary, ACROPOLIS_ROOF_GARDEN_SANCTUARY_CLEARED_VARIANT, AREA_VARIANT_RESET_ALWAYS);
    }
}

void acropolisRoofGardenRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gAcropolisRoofGardenRoomTaskStates;
    states.funcs[task->state](task);
}

/// Releases the Maggot/Caterpillar scripted entrance for the roof garden encounter.
static void _acropolisRoofGardenReleaseMaggotCaterpillarEntrance(void)
{
    enum { ACROPOLIS_ROOF_GARDEN_ENTRANCE_RELEASED = 1 };

    gSceneCombatState.maggotCaterpillarEntranceReady = ACROPOLIS_ROOF_GARDEN_ENTRANCE_RELEASED;
}

/// Spawns a one-frame flare at (-4770, -220, -3865) in the emitter's local space.
///
/// Overwrites the emitter work's signed-halfword XYZ move vector. Spawning
/// transforms XYZ into a new coordinate under the view root. Both emitter
/// records are borrowed through that call; the flare never reads its retained
/// offset or emitter pointers. A refused spawn still overwrites the move vector.
/// `packedFlareOptions` bits 0..7 give pulse phase steps per animation frame,
/// 8..15 the radius scale, bit 16 selects green instead of red, bit 28 adds
/// streaks to a diamond, and bit 31 selects a disc instead of a diamond.
static inline void _acropolisRoofGardenEmitFlare(EffectWork* emitterWork, GfxCoord* emitterCoord, s32 packedFlareOptions)
{
    emitterWork->move.vx = -4770;
    emitterWork->move.vy = -220;
    emitterWork->move.vz = -3865;
    effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_FLARE, emitterCoord, packedFlareOptions, &emitterWork->move);
}

void acropolisRoofGardenAmbientEffectsTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_EFFECT_INITIALIZE   = 0,
        ACROPOLIS_ROOF_GARDEN_GLOW_SCALE_SHIFT    = 16,
        ACROPOLIS_ROOF_GARDEN_GLOW_CELL_SHIFT     = 8,
        ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_COUNT    = 2,
        ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_INDEX    = 2,
        ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_FIRST    = 3,
        ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_SCALE    = 512,
        ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_SCALE    = 1024,
        ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_CELL     = 1,
        ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_CELL     = 2,
        ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_VIEWS = (1 << (5 - 1)) | (1 << (6 - 1)),
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_STEPS   = 14,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT  = 8,
        ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_SCALE = 6,
        ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SCALE    = 3,
        ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SHAPE    = 0x80000000,
    };
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    lightOffsets;
    s32         lightIndex;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state == ACROPOLIS_ROOF_GARDEN_EFFECT_INITIALIZE) {
        // Place ten persistent glows, keeping one index across both table walks.
        for (lightIndex = 0; lightIndex < ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_COUNT; lightIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, lightIndex + (ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_SCALE << ACROPOLIS_ROOF_GARDEN_GLOW_SCALE_SHIFT), &_gAcropolisRoofGardenLightOffsets[lightIndex]);
        }
        lightOffsets = _gAcropolisRoofGardenLightOffsets;
        effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, (ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_SCALE << ACROPOLIS_ROOF_GARDEN_GLOW_SCALE_SHIFT) | (ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_CELL << ACROPOLIS_ROOF_GARDEN_GLOW_CELL_SHIFT) | ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_INDEX, &lightOffsets[ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_INDEX]);
        for (lightIndex = ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_FIRST; lightIndex < ARRAY_SIZE(_gAcropolisRoofGardenLightOffsets); lightIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, lightIndex + (ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_CELL << ACROPOLIS_ROOF_GARDEN_GLOW_CELL_SHIFT), &lightOffsets[lightIndex]);
        }
        task->state = task->state + 1;
    }
    // Emit one-frame flares only while room effects accept new spawns.
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if ((ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_VIEWS >> (gGameSession->location.view - 1)) & 1) {
            _acropolisRoofGardenEmitFlare(work, coord, (ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_SCALE << ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) | ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_STEPS);
        }
        if (gGameSession->location.view == 7) {
            _acropolisRoofGardenEmitFlare(work, coord, ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SHAPE | (ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SCALE << ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) | ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_STEPS);
        }
    }
}

#include "../../shared/effects/acropolis_glow_sprite_bounds.inc.c"

void acropolisRoofGardenLightGlowTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_LIGHT_INITIALIZE    = 0,
        ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK    = 0xF,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_SHIFT    = 8,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_MASK     = 3,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_COUNT    = 3,
        ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT   = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK    = 0xFFF,
        ACROPOLIS_ROOF_GARDEN_LIGHT_DEFAULT_SCALE = 640,
        ACROPOLIS_ROOF_GARDEN_LIGHT_FLICKER_STEP  = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_X_STRIDE = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_Y        = 270,
    };
    EffectWork*            work;
    GfxCoord*              coord;
    RoomGlowSpriteScratch* projection;
    POLY_FT4*              quad;
    s32                    greyLevel;
    s32                    flickerLevel;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        if ((_gAcropolisRoofGardenLightViewMasks[task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK] >> (gGameSession->location.view - 1)) & 1) {
            actorRenderComposeCoord(coord);
            projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
            if (task->state == ACROPOLIS_ROOF_GARDEN_LIGHT_INITIALIZE) {
                // Resting grey of each sheet cell; the room only spawns cells 0..2.
                u8 restingLevels[ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_COUNT] = { 0x40, 0x60, 0x10 };

                // Decode once, retaining only the view-mask index in the spawn argument.
                if (task->spawnArg1.value & (ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK << ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT)) {
                    work->scale = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT) & ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK;
                } else {
                    work->scale = ACROPOLIS_ROOF_GARDEN_LIGHT_DEFAULT_SCALE;
                }
                work->angle           = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_SHIFT) & ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_MASK;
                task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK;
                work->period          = restingLevels[work->angle];
                task->state++;
            }
            // Rejected projections still consume one frame-arena quad.
            projection->worldPos.vx = coord->workm.t[0];
            projection->worldPos.vy = coord->workm.t[1];
            projection->worldPos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&projection->worldPos);
            gte_rtps();
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            gte_stsxy(&projection->screenPos);
            gte_stszotz(&projection->otz);
            if (projection->otz >= GLOW_MIN_DEPTH) {
                flickerLevel = ((u8)gDisplayState.animFrame & 1) * ACROPOLIS_ROOF_GARDEN_LIGHT_FLICKER_STEP;
                greyLevel    = (u8)work->period + flickerLevel;
                quad->tpage  = GLOW_FLARE_TEXTURE_PAGE;
                setRGB0(quad, greyLevel, greyLevel, greyLevel);
                setSemiTrans(quad, true);
                quad->clut = getClut(work->angle * ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_X_STRIDE, ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_Y);
                quad->u0   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v0   = 0;
                quad->u1   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v1   = 0;
                quad->u2   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v2   = GLOW_FLARE_CELL_LAST_TEXEL;
                quad->u3   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v3   = GLOW_FLARE_CELL_LAST_TEXEL;

                projection->halfExtent = (work->scale * GLOW_FLARE_CELL_LAST_TEXEL) / projection->otz;
                _acropolisGlowSetSpriteBounds(quad, projection);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        quad);
            }
            SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
        }
    }
}

/// Initializes a two-segment Gouraud flare streak fading from its centre to black.
///
/// Borrows one word-aligned writable `LINE_G3`, setting its opaque command,
/// packet length and polyline terminator. Vertex 1 receives `centreRed` and
/// `centreGreen` (0..255); blue and both endpoints are zero. The caller supplies all three
/// vertex coordinates, DMA linkage and semitransparency before drawing.
static inline void _acropolisRoofGardenInitFlareStreak(LINE_G3* streak, u8 centreRed, u8 centreGreen)
{
    setLineG3(streak);
    setRGB0(streak, 0, 0, 0);
    setRGB1(streak, centreRed, centreGreen, 0);
    setRGB2(streak, 0, 0, 0);
}

/// Queues a flare packet and prepends its additive draw mode at the same depth.
///
/// Borrows a word-aligned, initialized untextured `POLY_G4` or `LINE_G3` packet
/// to link, and a separate readable `s32` `depthWord` for this call. The depth
/// is projected SZ3 / 4, before `otDepthShift`; the caller rejects values below
/// `GLOW_MIN_DEPTH`. Keep the depth word and display depth shift stable while
/// linking. Unsigned scaling wraps to tag 0..1023 in the current ordering table.
/// Enables packet semitransparency and consumes `sizeof(DR_TPAGE)` bytes of
/// word-aligned frame-arena space without checking capacity. The table must
/// contain the selected tag; both packets must live until GPU completion.
static inline void _acropolisRoofGardenQueueFlare(void* primitive, const s32* depthWord)
{
    enum { ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_BYTE_OFFSET_SHIFT = 2 };

    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)*depthWord << gDisplayState.otDepthShift) >> ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_BYTE_OFFSET_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            primitive);
    // Prepending the draw mode after the primitive makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, *depthWord);
}

void acropolisRoofGardenFlareTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_RATE_MASK      = 0xFF,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT         = 8,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK          = 0xFF,
        ACROPOLIS_ROOF_GARDEN_FLARE_GREEN_SHIFT          = 16,
        ACROPOLIS_ROOF_GARDEN_FLARE_STREAKS              = 0x10000000,
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_FALLING        = 0x80,
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK     = 0x7F,
        ACROPOLIS_ROOF_GARDEN_FLARE_OUTER_RADIUS_SHIFT   = 10,
        ACROPOLIS_ROOF_GARDEN_FLARE_INNER_RADIUS_SHIFT   = 7,
        ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_RADIUS_SHIFT = 9,
        ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS         = 16,
        ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_TAG_SHIFT   = 4,
        ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TAG_MASK       = GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt),
    };
    GfxCoord*             coord;
    EffectWork*           work;
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    s32                   partIndex;
    s32                   pulsePhase;
    s32                   pulseOrOptions; // Folded pulse level, then packed spawn options.
    s32                   radiusScale;
    s16                   intensity;
    s16                   green;
    s16                   halfIntensity;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    // Draw one projected frame before retiring the counted work and task.
    actorRenderComposeCoord(coord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = coord->workm.t[0];
    projection->worldPos.vy = coord->workm.t[1];
    projection->worldPos.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= GLOW_MIN_DEPTH) {
        // Fold the phase into a 0..127 triangle wave, then select red or green.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_RATE_MASK;
        green       = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_FLARE_GREEN_SHIFT) & 1;
        if (pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_FALLING) {
            pulseOrOptions = ~pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK;
        } else {
            pulseOrOptions = pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK;
        }
        intensity      = pulseOrOptions * 2;
        pulseOrOptions = task->spawnArg1.value;
        if (pulseOrOptions < 0) {
            // Layer two concentric wedge fans and four half-bright rays.
            radiusScale             = (pulseOrOptions >> ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) & ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK;
            projection->outerRadius = (radiusScale << ACROPOLIS_ROOF_GARDEN_FLARE_OUTER_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (radiusScale << ACROPOLIS_ROOF_GARDEN_FLARE_INNER_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS; partIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, (intensity * (green ^ 1)) >> 1, (green * intensity) >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 2]) >> GLOW_TRIG_SHIFT);
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, intensity * (green ^ 1), green * intensity);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);
            }
            // The first ray fetches light-view mask 8 (16) through sine index -2.
            halfIntensity = intensity >> 1;
            for (partIndex = 2; partIndex < ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS; partIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, halfIntensity * (green ^ 1), green * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex - 4]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> (GLOW_TRIG_SHIFT - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, halfIntensity * (green ^ 1), green * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 12]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 8]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((pulseOrOptions >> ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) & ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK) << ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < 2; partIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, intensity * (green ^ 1), green * intensity);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->outerRadius) + projection->outerRadius * (partIndex + partIndex);
                addPrim((&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_TAG_SHIFT & ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TAG_MASK]),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz);
            }
            // Optional streaks cross at the diamond centre.
            if (task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_FLARE_STREAKS) {
                for (partIndex = 0; partIndex < 2; partIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    _acropolisRoofGardenInitFlareStreak(streak, intensity * (green ^ 1), green * intensity);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (partIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->outerRadius * (partIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (partIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->outerRadius * (partIndex + 1);
                    _acropolisRoofGardenQueueFlare(streak, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(work, task);
}

#undef ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD

#include "../../shared/effects/falling_leaves_task.inc.c"

void acropolisRoofGardenLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/effects/falling_leaves_draw.inc.c"

#include "../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../shared/actor/actor_contacts_push.inc.c"

void acropolisRoofGardenPickupModelTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_PICKUP_FIRST_VIEW = 5,
        ACROPOLIS_ROOF_GARDEN_PICKUP_VIEW_LIMIT = 8,
        ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN     = 2,
    };
    Enemy*     enemy;
    TmdObject* tmd;
    s32        objectState;
    s32        mappedView;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    // Only the low byte is the saved object ID; mapped views can differ from logical views.
    objectState = areaGetCurrentObjectState((u8)enemy->placeKey);
    mappedView  = viewGetMappedIndex();
    // Separate view tests preserve the two comparisons in the compiled draw gate.
    if (mappedView >= ACROPOLIS_ROOF_GARDEN_PICKUP_VIEW_LIMIT) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (mappedView < ACROPOLIS_ROOF_GARDEN_PICKUP_FIRST_VIEW) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (objectState == ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}
