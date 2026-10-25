#include "rooms/shelter_r36.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "shelter_r36_private.h"

#include "actors/actor_143900.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"
#include "../../shared/room/streamed_scene.h"

static s32 _shelterR36RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _shelterR36ResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* destination);
static s32 _shelterR36IgnoreCommand(Task* task, s32 messageId, s32 command, s32 unused);
static s32 _shelterR36IgnoreRoomAction(Task* task, s32 messageId, DirectionActionRequest* action, s32 unused);

static void _shelterR36ArrivalSceneTask(Task* task);
static void _shelterR36ReloadForMovie(void);
static void _shelterR36MovieEndingTask(Task* task);
static void _shelterR36InitializeTask(Task* task);
static void _shelterR36SelectCapFile(s32 capFileId);

/// R36's placement indices, resource keys and scripted attachment choices.
enum {
    SHELTER_R36_FIRST_WALKER_PLACEMENT          = 0,
    SHELTER_R36_SECOND_WALKER_PLACEMENT         = 1,
    SHELTER_R36_FIRST_WALKER_RESOURCE           = 111,
    SHELTER_R36_SECOND_WALKER_RESOURCE          = 112,
    SHELTER_R36_SHOW_SECOND_WALKER_ATTACHMENT_2 = 0,
    SHELTER_R36_SHOW_SECOND_WALKER_ATTACHMENT_1 = 1,
    SHELTER_R36_ARRIVAL_SOUND                   = 0x54240001,
    SHELTER_R36_MOVIE_LEAD_IN_MUSIC_EVENT       = 30,
};

/// Holds the arriving player in equipped-weapon clip 1 with collision disabled.
///
/// The event interpreter copies this request and replaces its bank index before
/// synchronous dispatch. The selected weapon bank remains borrowed by playback.
static AnimationPlayRequest _gShelterR36ArrivalPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Shows only attachment 2 of the second scripted walker in the arrival scene.
///
/// The receiver ignores the Mine/Shelter R36 context and consumes the command
/// synchronously. Both independently owned attachment tasks must be live.
static ActorCommand _gShelterR36ShowSecondWalkerAttachment2Command = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R36 } },
    .command = SHELTER_R36_SHOW_SECOND_WALKER_ATTACHMENT_2,
};

/// Shows only attachment 1 of the second scripted walker in the arrival scene.
///
/// The receiver ignores the Mine/Shelter R36 context and consumes the command
/// synchronously. Both independently owned attachment tasks must be live.
static ActorCommand _gShelterR36ShowSecondWalkerAttachment1Command = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R36 } },
    .command = SHELTER_R36_SHOW_SECOND_WALKER_ATTACHMENT_1,
};

/// Unreferenced bank-1 clip-1 and clip-2 requests with ten-frame blends.
///
/// Both disable world collision. Their intended receiver and original
/// grouping are unproven; no event-script instruction points into these records.
static AnimationPlayRequest _gShelterR36UnreferencedBlendRequests[2] = {
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = 10,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 2,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = 10,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

/// Arrival-scene clip 3 for the second scripted walker, restarting without interpolation.
///
/// Placement 1 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36SecondWalkerAnimation3 = {
    .source               = { .index = 1 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced bank-1 clip-4 reset request with collision disabled; its receiver is unproven.
static AnimationPlayRequest _gShelterR36UnreferencedClip4Request = {
    .source               = { .index = 1 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 5 for the second scripted walker, restarting without interpolation.
///
/// Placement 1 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36SecondWalkerAnimation5 = {
    .source               = { .index = 1 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 6 for the second scripted walker, restarting without interpolation.
///
/// Placement 1 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36SecondWalkerAnimation6 = {
    .source               = { .index = 1 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 7 for the second scripted walker, restarting without interpolation.
///
/// Placement 1 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36SecondWalkerAnimation7 = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 8 for the second scripted walker, restarting without interpolation.
///
/// Placement 1 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36SecondWalkerAnimation8 = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Two unreferenced reset requests for animation 1.
///
/// Both store bank selector 1 and disabled world collision. The intended
/// receiver and original grouping of these consecutive records are unproven.
static AnimationPlayRequest _gShelterR36UnreferencedAnimation1Requests[] = {
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

/// Arrival-scene clip 9 for the first scripted walker, restarting without interpolation.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation9 = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// An unreferenced reset request for animation 10.
///
/// Stores bank selector 1 and disabled world collision; the intended receiver
/// and scene use are unproven.
static AnimationPlayRequest _gShelterR36UnreferencedAnimation10Request = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 11 for the first scripted walker, restarting without interpolation.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation11 = {
    .source               = { .index = 1 },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 12 for the first scripted walker, interpolating for 10 normal-rate frames.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation12 = {
    .source               = { .index = 1 },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 13 for the first scripted walker, restarting without interpolation.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation13 = {
    .source               = { .index = 1 },
    .animationId          = 13,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// An unreferenced reset request for animation 14.
///
/// Stores bank selector 1 and disabled world collision; the intended receiver
/// and scene use are unproven.
static AnimationPlayRequest _gShelterR36UnreferencedAnimation14Request = {
    .source               = { .index = 1 },
    .animationId          = 14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 15 for the first scripted walker, interpolating for 8 normal-rate frames.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation15 = {
    .source               = { .index = 1 },
    .animationId          = 15,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 16 for the first scripted walker, interpolating for 8 normal-rate frames.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation16 = {
    .source               = { .index = 1 },
    .animationId          = 16,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Arrival-scene clip 19 for the first scripted walker, interpolating for 8 normal-rate frames.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation19 = {
    .source               = { .index = 1 },
    .animationId          = 19,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced requests to blend into animation 20 and reset animation 9.
///
/// The blend requests eight normal-rate frames. Both store bank selector 1
/// and disabled world collision. Their intended receivers and original
/// grouping are unproven; animation 20 is outside both room walkers' clip tables.
static AnimationPlayRequest _gShelterR36UnreferencedAnimation20And9Requests[] = {
    {
        .source               = { .index = 1 },
        .animationId          = 20,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = 8,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 9,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

/// Arrival-scene clip 10 for the second scripted walker, restarting without interpolation.
///
/// Placement 1 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36SecondWalkerAnimation10 = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Thirteen unreferenced animation requests preserved in image order.
///
/// Eight reset animation 1, four reset animations 5..8, and the last requests
/// an eight-normal-rate-frame blend into animation 17. All store bank selector
/// 1 and disabled world collision. Their intended receivers and original
/// grouping are unproven.
static AnimationPlayRequest _gShelterR36UnreferencedAnimationRequests[] = {
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 5,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 6,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 7,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 8,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 17,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = 8,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

/// Arrival-scene clip 18 for the first scripted walker, restarting without interpolation.
///
/// Placement 0 consumes this request synchronously and borrows its own loaded
/// clip data during playback; its handler ignores the bank and collision words.
static AnimationPlayRequest _gShelterR36FirstWalkerAnimation18 = {
    .source               = { .index = 1 },
    .animationId          = 18,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Parks the player away from the arrival scene at (3270, -10000, -1630).
///
/// Coordinates are whole world units and all Euler angles are zero. The player
/// placement message copies the transform synchronously and retains no pointer.
static ActorTransform _gShelterR36ArrivalPlayerTransform = {
    .pos = { 3270, -10000, -1630, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Zero-body task recipes for warp 1's arrival scene and warp 2's movie ending.
///
/// Indices 0 and 1 use priority `TASK_PRIORITY_SCENE` and receive zero spawn words. The controller
/// borrows this two-entry table; its callbacks require the room's loaded resources.
static TaskDesc _gShelterR36ArrivalTasks[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterR36ArrivalSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterR36MovieEndingTask, { .value = 0 } },
};

/// CAP-paced first-arrival choreography for both meeting-progress branches.
///
/// Holds and parks the player, animates placement 0 (first walker) and placement
/// 1 (second walker), then fades and cleans up the scene. The controller chooses
/// the accompanying CAP file and requests the ending or movie lead-in afterward.
/// This 69-command stream ends with END; requests, callbacks and CAP/actor data
/// must remain loaded until completion or `_gShelterR36ArrivalSkipScript` ends.
static EvsCommand _gShelterR36ArrivalScript[69] = {
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 5 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gShelterR36ArrivalPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gShelterR36ArrivalPlayerTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation9 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36SecondWalkerAnimation5 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterR36ShowSecondWalkerAttachment2Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36SecondWalkerAnimation6 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation9 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36SecondWalkerAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation11 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36SecondWalkerAnimation10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation19 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation12 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterR36ShowSecondWalkerAttachment1Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36SecondWalkerAnimation7 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36SecondWalkerAnimation8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation12 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation13 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SHELTER_R36_ARRIVAL_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation15 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gShelterR36FirstWalkerAnimation16 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 130 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 5 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path that stops arrival music, cleans up the scene and disables drawing.
///
/// Its eight commands finish with END after cancelling the primary fade.
/// It borrows the room's callbacks and is installed with `_gShelterR36ArrivalScript`.
static EvsCommand _gShelterR36ArrivalSkipScript[8] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Follow-up CAP presentation that hides the walkers and reloads for the movie.
///
/// Selects loaded CAP files 1 then 2, playing command slot 1 from each. Its 25
/// commands finish with END after requesting warp 2 and disabling input skipping.
/// The interpreter borrows this stream, callbacks and loaded CAP data; both its
/// normal path and the separately supplied skip stream request the same reload.
static EvsCommand _gShelterR36MovieLeadInScript[25] = {
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = SHELTER_R36_MOVIE_LEAD_IN_MUSIC_EVENT }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_SECOND_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_R36_FIRST_WALKER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterR36SelectCapFile }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterR36SelectCapFile }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterR36ReloadForMovie }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum {
    SHELTER_R36_MOVIE_SKIP_MUSIC_FADE_TICKS = 15,
    SHELTER_R36_MOVIE_SKIP_CLEANUP_FRAMES   = 8,
    SHELTER_R36_MOVIE_SKIP_RELOAD_FRAMES    = 60,
    SHELTER_R36_MOVIE_SKIP_DISPLAY_ENABLE   = 1,
};

/// Skip stream that cleans up the movie lead-in and requests its movie-arrival reload.
///
/// Eight instructions including END. Requests an area-music fade with argument
/// 15 (16 audio updates after encoding), starts the default primary fade, waits
/// eight interpreter countdown updates,
/// restores scene/display state and requests reload before a 60-update wait.
/// The no-argument reload callback shares the normal path's attract-demo guard.
/// Borrowed by the arrival director; keep this overlay loaded until consumption.
static EvsCommand _gShelterR36MovieLeadInSkipScript[8] = {
    { .opcode = EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, .operand0 = { .value = SHELTER_R36_MOVIE_SKIP_MUSIC_FADE_TICKS } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = GPU_BLEND_AVERAGE } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_R36_MOVIE_SKIP_CLEANUP_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = SetDispMask }, .operand1 = { .value = SHELTER_R36_MOVIE_SKIP_DISPLAY_ENABLE } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterR36ReloadForMovie } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_R36_MOVIE_SKIP_RELOAD_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Room receiver routes for variant resolution, key-item refusal and ignored actions/commands.
///
/// Four live IDs followed by END/NULL; other IDs return zero. Transition and
/// action records are borrowed synchronously; other payloads are integer words.
/// The initialized room receiver borrows this table and loaded callback code.
static TaskMessageEntry _gShelterR36RoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterR36ResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterR36RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterR36IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterR36IgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gShelterR36MovieTasks[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, shelterR36StartMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, streamedScenePlay, { .value = 0 } },
};

u8* gShelterR36RoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterR36ViewCounts[1] = { 11 };

DirectionWarpEntry gShelterR36WarpEntries[2] = {
    { .player = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 0 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 0 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = DIRECTION_WARP_SOUND_NONE, .departureSound = DIRECTION_WARP_SOUND_NONE, .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 0 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 0 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = DIRECTION_WARP_SOUND_NONE, .departureSound = DIRECTION_WARP_SOUND_NONE, .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DIRECTION_WARP_MAP_FLAG_NONE },
};

enum {
    SHELTER_R36_FLOOR_GRID_CELL_COUNT_X = 3,
    SHELTER_R36_FLOOR_GRID_CELL_COUNT_Z = 3,
};

/// Upward floor normal in room coordinates, with 4096 representing unit length.
static SVECTOR _gShelterR36FloorCollisionGridNormals[1] = {
#include "assets/shelter_r36_collision_014F0_normals.inc"
};

/// Four floor corners in signed integer room-coordinate units.
static SVECTOR _gShelterR36FloorCollisionGridVerts[4] = {
#include "assets/shelter_r36_collision_014F0_verts.inc"
};

/// The floor quad, using normal 0, all four vertices and surface class 1.
static WorldCollisionGridFace _gShelterR36FloorCollisionGridFaces[1] = {
#include "assets/shelter_r36_collision_014F0_faces.inc"
};

/// Nine face-index lists, each containing face 0 then WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterR36FloorCollisionGridCells[18] = {
#include "assets/shelter_r36_collision_014F0_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterR36FloorCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..17
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_R36_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterR36FloorCollisionGridCells[(halfwordOffset)])
/// X-major cell pointers, indexed by x * cellCountZ + z in the 3 by 3 grid.
static s16* _gShelterR36FloorCollisionGridTable[SHELTER_R36_FLOOR_GRID_CELL_COUNT_X * SHELTER_R36_FLOOR_GRID_CELL_COUNT_Z] = {
#include "assets/shelter_r36_collision_014F0_table.inc"
};
#undef SHELTER_R36_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterR36FloorCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterR36FloorCollisionGridNormals,
    .vertices    = _gShelterR36FloorCollisionGridVerts,
    .faces       = _gShelterR36FloorCollisionGridFaces,
    .cellFaceIds = _gShelterR36FloorCollisionGridTable,
    .xBias       = 4000,
    .zBias       = 4000,
    .cellCountX  = SHELTER_R36_FLOOR_GRID_CELL_COUNT_X,
    .cellCountZ  = SHELTER_R36_FLOOR_GRID_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterR36FloorCollisionGridFaces),
};

ViewCamera gShelterR36ViewCameras[11] = {
    { .transform = { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x32C8, 0 } }, .screenDistance = 257 },
    { .transform = { { { -3897, 0, -1261 }, { 0, 4096, 0 }, { 1261, 0, -3897 } }, { 1260, 1300, -5690 } }, .screenDistance = 680 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
    { .transform = { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, .screenDistance = 257 },
};

/// View 1 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture-page and CLUT encodings shared by R36's authored view sprites.
enum {
    SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT  = getTPage(1, 0, 896, 0),
    SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT = getTPage(1, 0, 960, 0),
    SHELTER_R36_SPRITE_CLUT               = getClut(0, 255),
    SHELTER_R36_SPRITE_NEUTRAL_MODULATION = 128,
};

/// View 2's texture rectangles, split into source ranges 0..53 and 54..61.
///
/// XY and dimensions are pixels relative to the view centre; depth is the
/// unscaled OT sorting value. The two texture pages use 8-bit texels and CLUT
/// row 255. Cached packets snapshot geometry in both frame buffers on view
/// entry. This overlay owns the writable sources throughout drawing.
static SpriteSource _gShelterR36View2SpriteSources[] = {
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 56, 8 } }, 104, 16, 0, { .fields = { 88, 232 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 40, 16 } }, 120, 24, 0, { .fields = { 104, 216 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 40, 0, { .fields = { 56, 232 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 40, 0, { .fields = { 112, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 72, 0, { .fields = { 24, 184 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 24 } }, 64, 64, 0, { .fields = { 8, 192 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, 16, 0, { .fields = { 64, 32 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 64 } }, 56, 16, 0, { .fields = { 120, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 48 } }, 64, 16, 0, { .fields = { 112, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 56 } }, 32, 16, 0, { .fields = { 104, 128 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 56 } }, 8, 16, 0, { .fields = { 104, 184 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 64 } }, 0, 16, 0, { .fields = { 120, 64 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 48 } }, -24, 16, 0, { .fields = { 96, 48 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 40 } }, -48, 16, 0, { .fields = { 80, 136 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, 48, 0, { .fields = { 16, 144 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -80, 48, 0, { .fields = { 16, 72 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -96, 48, 0, { .fields = { 16, 48 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, 48, 0, { .fields = { 8, 120 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 48, 0, { .fields = { 40, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 24 } }, -152, 48, 0, { .fields = { 8, 96 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 40, 32 } }, -160, 16, 0, { .fields = { 24, 200 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 40, 32 } }, -120, 16, 0, { .fields = { 64, 216 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 32 } }, -80, 16, 0, { .fields = { 64, 176 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 56, 1000, { .fields = { 40, 64 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 32 } }, 128, 64, 1000, { .fields = { 64, 40 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 16, 1000, { .fields = { 112, 240 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 40 } }, 72, 24, 1000, { .fields = { 88, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 40 } }, 104, 24, 1000, { .fields = { 88, 96 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 40, 1000, { .fields = { 16, 216 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, 32, 1000, { .fields = { 112, 96 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 48, 8 } }, 80, 64, 1000, { .fields = { 64, 248 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 16 } }, 80, 72, 1000, { .fields = { 104, 96 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 16 } }, 40, 80, 1000, { .fields = { 112, 128 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 72, 1000, { .fields = { 96, 40 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 24 } }, 8, 72, 1000, { .fields = { 24, 232 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 24 } }, -24, 64, 1000, { .fields = { 24, 136 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 32 } }, -48, 56, 1000, { .fields = { 40, 168 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -56, 48, 0, { .fields = { 88, 72 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -72, 48, 0, { .fields = { 24, 48 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -88, 48, 0, { .fields = { 24, 160 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 48, 0, { .fields = { 24, 24 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 8 } }, -136, 80, 1000, { .fields = { 80, 208 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 24 } }, -160, 48, 0, { .fields = { 32, 160 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 72, 1000, { .fields = { 112, 112 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 24 } }, 8, 96, 1000, { .fields = { 120, 24 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 24 } }, 40, 96, 1000, { .fields = { 0, 168 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 32 } }, 64, 88, 1000, { .fields = { 48, 136 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 32 } }, 96, 88, 1000, { .fields = { 32, 32 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 24 } }, 128, 96, 1000, { .fields = { 8, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 64, 32 } }, -160, 88, 1000, { .fields = { 24, 104 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 72, 16 } }, -120, 72, 1000, { .fields = { 72, 144 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 64, 32 } }, -96, 88, 1000, { .fields = { 24, 72 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 32 } }, -32, 88, 1000, { .fields = { 56, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 40 } }, 0, 80, 1000, { .fields = { 96, 176 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 80, 0, { .fields = { 120, 240 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 32 } }, -32, 80, 0, { .fields = { 104, 96 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 64, 8 } }, -88, 112, 250, { .fields = { 80, 160 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 56, 16 } }, -88, 96, 250, { .fields = { 88, 64 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 48, 16 } }, -88, 80, 250, { .fields = { 96, 80 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 56, 16 } }, -144, 80, 250, { .fields = { 96, 240 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 56, 16 } }, -144, 96, 250, { .fields = { 88, 48 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_LEFT, SHELTER_R36_SPRITE_CLUT, { .fields = { 56, 8 } }, -144, 112, 250, { .fields = { 80, 16 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
};

enum { SHELTER_R36_VIEW_2_SECOND_BATCH_FIRST_SPRITE = 54 };

/// View 2's decoded background followed by two sprite ranges and the end marker.
///
/// Both ranges remain visible and participate in cached-packet processing.
/// The first range retains `field_6` = { 1, 0 }; those serialized bytes have no
/// established reader. Ranges cover the complete view-2 source array.
static SpriteBatch _gShelterR36View2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_R36_VIEW_2_SECOND_BATCH_FIRST_SPRITE, 0, 0, { 1, 0 } },
    { SHELTER_R36_VIEW_2_SECOND_BATCH_FIRST_SPRITE, ARRAY_SIZE(_gShelterR36View2SpriteSources) - SHELTER_R36_VIEW_2_SECOND_BATCH_FIRST_SPRITE, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's fourteen texture rectangles and their unscaled OT sorting depths.
///
/// Positions and dimensions are pixels relative to the view centre; textures
/// use the right 8-bit page and CLUT row 255. Cached packets snapshot geometry
/// in both frame buffers on view entry. Sources belong to this loaded overlay.
static SpriteSource _gShelterR36View3SpriteSources[] = {
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 88, 8 } }, 8, 24, 0, { .fields = { 16, 72 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 8 } }, 8, 32, 0, { .fields = { 72, 64 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 48, 8 } }, 48, 32, 0, { .fields = { 56, 56 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 40, 0, { .fields = { 112, 248 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, 24, 0, { .fields = { 72, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 32, 0, { .fields = { 120, 224 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 96, 0, { .fields = { 112, 240 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 72 } }, -32, 32, 961, { .fields = { 112, 152 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 56 } }, -40, 48, 1131, { .fields = { 96, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 64 } }, 8, 40, 1150, { .fields = { 88, 80 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 8, 72 } }, 40, 32, 1375, { .fields = { 120, 80 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 32, 64 } }, 48, 40, 1375, { .fields = { 80, 144 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 16, 48 } }, 80, 48, 1250, { .fields = { 96, 208 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_R36_SPRITE_TEXTURE_PAGE_RIGHT, SHELTER_R36_SPRITE_CLUT, { .fields = { 24, 80 } }, -16, 24, 500, { .fields = { 104, 0 } }, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, SHELTER_R36_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 3's decoded background and one visible batch covering all source sprites.
///
/// The zero-count first record selects the background; `SPRITE_BATCH_END` ends
/// the list after the sprite range. Cached-packet processing includes the range.
static SpriteBatch _gShelterR36View3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterR36View3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11 selects the decoded background with no source sprites, then ends the list.
static SpriteBatch _gShelterR36View11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterR36SpriteViews[11] = {
    { { .empty = _gShelterR36View1SpriteBatches }, _gShelterR36View1SpriteBatches, NULL },
    { { .elements = _gShelterR36View2SpriteSources }, _gShelterR36View2SpriteBatches, NULL },
    { { .elements = _gShelterR36View3SpriteSources }, _gShelterR36View3SpriteBatches, NULL },
    { { .empty = _gShelterR36View4SpriteBatches }, _gShelterR36View4SpriteBatches, NULL },
    { { .empty = _gShelterR36View5SpriteBatches }, _gShelterR36View5SpriteBatches, NULL },
    { { .empty = _gShelterR36View6SpriteBatches }, _gShelterR36View6SpriteBatches, NULL },
    { { .empty = _gShelterR36View7SpriteBatches }, _gShelterR36View7SpriteBatches, NULL },
    { { .empty = _gShelterR36View8SpriteBatches }, _gShelterR36View8SpriteBatches, NULL },
    { { .empty = _gShelterR36View9SpriteBatches }, _gShelterR36View9SpriteBatches, NULL },
    { { .empty = _gShelterR36View10SpriteBatches }, _gShelterR36View10SpriteBatches, NULL },
    { { .empty = _gShelterR36View11SpriteBatches }, _gShelterR36View11SpriteBatches, NULL },
};

/// Four directional lights contributing in every R36 view.
///
/// Translations supply room-space direction vectors; RGB uses 12 fractional
/// bits. Coordinate updates and light selection mutate composition and
/// attenuation, so the overlay owns writable records through every query.
static WorldCoordLight _gShelterR36DirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 409, 409, 409 }, { 0, 0 } },
};

/// Five point lights contributing in every R36 view.
///
/// Positions and inner/outer falloff radii are integer room-coordinate units;
/// RGB uses 12 fractional bits. Every inner radius is less than its outer
/// radius. The loaded overlay owns the mutable composition and attenuation.
static WorldCoordPointLight _gShelterR36PointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4500, -2620, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3200, -2159, -922 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -766, -2000, -4618 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1240, -5082, 2801 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3966, -2581, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1800, 3001 },
};

WorldCoordRoomLights gShelterR36RoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gShelterR36DirectionalLights),
    .directionalLights     = _gShelterR36DirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gShelterR36PointLights),
    .pointLights           = _gShelterR36PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterR36ViewBoundaryTriggers[10] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 5729, -1856, -3601, 0 }, .vertices = { { 2395, -4896, -3, 0 }, { -2395, -4896, 3, 0 }, { 2395, 4896, -3, 0 }, { -2395, 4896, 3, 0 } }, .normal = { 5, 0, 4095, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5442, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 5712, -2032, -3488, 0 }, .vertices = { { -2373, -4816, -3, 0 }, { 2373, -4816, 3, 0 }, { -2373, 4816, -3, 0 }, { 2373, 4816, 3, 0 } }, .normal = { 5, 0, -4110, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5345, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 5360, -1249, -0x2861, 0 }, .vertices = { { -1333, -4816, -3, 0 }, { 1333, -4816, 3, 0 }, { -1333, 4816, -3, 0 }, { 1333, 4816, 3, 0 } }, .normal = { 9, 0, -4109, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4990, .control = 0, .parameter0 = 2, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 5472, -1249, -0x28A1, 0 }, .vertices = { { 1163, -4816, -3, 0 }, { -1163, -4816, 3, 0 }, { 1163, 4816, -3, 0 }, { -1163, 4816, 3, 0 } }, .normal = { 10, 0, 4097, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4937, .control = 0, .parameter0 = 4, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 9985, -1120, -9360, 0 }, .vertices = { { 3467, -4816, -179, 0 }, { -3467, -4816, 179, 0 }, { 3467, 4816, -179, 0 }, { -3467, 4816, 179, 0 } }, .normal = { 211, 0, 4092, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5926, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 9935, -1120, -9248, 0 }, .vertices = { { -3397, -4816, 173, 0 }, { 3397, -4816, -173, 0 }, { -3397, 4816, 173, 0 }, { 3397, 4816, -173, 0 } }, .normal = { -209, 0, -4090, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5882, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x2E40, -1312, -0x2E71, 0 }, .vertices = { { 491, -4816, -611, 0 }, { -491, -4816, 611, 0 }, { 491, 4816, -611, 0 }, { -491, 4816, 611, 0 } }, .normal = { 3194, 0, 2567, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4857, .control = 0, .parameter0 = 6, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x2F50, -1376, -0x2F00, 0 }, .vertices = { { -581, -4816, 733, 0 }, { 581, -4816, -733, 0 }, { -581, 4816, 733, 0 }, { 581, 4816, -733, 0 } }, .normal = { -3229, 0, -2559, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4884, .control = 0, .parameter0 = 2, .parameter1 = 6, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x2EB0, -1376, -0x29A0, 0 }, .vertices = { { 427, -4816, 701, 0 }, { -427, -4816, -701, 0 }, { 427, 4816, 701, 0 }, { -427, 4816, -701, 0 } }, .normal = { -3500, 0, 2131, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4884, .control = 0, .parameter0 = 2, .parameter1 = 6, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x2DD0, -1408, -0x2A80, 0 }, .vertices = { { -357, -4816, -547, 0 }, { 357, -4816, 547, 0 }, { -357, 4816, -547, 0 }, { 357, 4816, 547, 0 } }, .normal = { 3428, 0, -2240, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4857, .control = 0, .parameter0 = 6, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Resource recipes for R36's first walker and second walker with attachments.
///
/// Keys 111/112 match the variant-1 placements; both use descriptor index 0.
/// Files 143900 and 660100 select the first actor package and second model data.
/// The third record terminates the resource walk with AREA_PLACEMENT_END.
/// Loading and spawning borrow this table and the actor package's descriptors.
static AreaResource _gShelterR36Variant1Resources[3] = {
    { .entryId = SHELTER_R36_FIRST_WALKER_RESOURCE, .fileNumber = 439, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor143900ScriptedWalkerTaskDesc },
    { .entryId = SHELTER_R36_SECOND_WALKER_RESOURCE, .fileNumber = 601, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor143900SecondScriptedWalkerTaskDescriptors },
    { .entryId = AREA_PLACEMENT_END, .fileNumber = 0, .fileGroupIndex = 0, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = NULL },
};

/// Variant 1's two scripted walkers, in the arrival script's placement order.
///
/// Placement 0 starts at Z 2850 with half-turn yaw; placement 1 at Z 850 with
/// zero yaw. Coordinates are whole world units, with X/Y zero. Both select mode,
/// variant and parameter row zero. Their texture offsets are (0, 2) and (2, 4).
/// The final record is AREA_PLACEMENT_END. Live walkers borrow these placements
/// until teardown, so the room overlay must remain loaded throughout their life.
static AreaPlacement _gShelterR36Variant1Placements[3] = {
    { .entryId = SHELTER_R36_FIRST_WALKER_RESOURCE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 2850, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_R36_SECOND_WALKER_RESOURCE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 850, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
};

AreaVariant gShelterR36AreaVariants[11] = {
    { .placements = NULL, .resources = NULL },
    { .placements = _gShelterR36Variant1Placements, .resources = _gShelterR36Variant1Resources },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
};

/// Cue-2 bases in the loaded type-1 bank; cue 1 selects the following entry.
enum {
    SHELTER_R36_CLASS_5_FOOTSTEP_WALK_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    SHELTER_R36_CLASS_5_FOOTSTEP_RUN_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
    SHELTER_R36_CLASS_2_FOOTSTEP_WALK_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    SHELTER_R36_CLASS_2_FOOTSTEP_RUN_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    SHELTER_R36_CLASS_3_FOOTSTEP_WALK_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    SHELTER_R36_CLASS_3_FOOTSTEP_RUN_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
};

/// Configured class-5 footstep request bases; walking and scripted jumps share cues.
///
/// Cue 1 adds one to the selected base; companion playback adds 100. The
/// class-5 policy borrows this record, although the room's floor uses class 1.
/// The read-only record retains its serialized position in .data.
static const WorldCollisionFootstepSounds _gShelterR36Class5FootstepSounds __attribute__((section(".data"))) = {
    .walk         = SHELTER_R36_CLASS_5_FOOTSTEP_WALK_BASE,
    .run          = SHELTER_R36_CLASS_5_FOOTSTEP_RUN_BASE,
    .scriptedJump = SHELTER_R36_CLASS_5_FOOTSTEP_WALK_BASE,
};

/// Configured class-2 footstep request bases; walking and scripted jumps share cues.
///
/// Cue 1 adds one to the selected base; companion playback adds 100. The
/// class-2 policy borrows this record, although the room's floor uses class 1.
/// The read-only record retains its serialized position in .data.
static const WorldCollisionFootstepSounds _gShelterR36Class2FootstepSounds __attribute__((section(".data"))) = {
    .walk         = SHELTER_R36_CLASS_2_FOOTSTEP_WALK_BASE,
    .run          = SHELTER_R36_CLASS_2_FOOTSTEP_RUN_BASE,
    .scriptedJump = SHELTER_R36_CLASS_2_FOOTSTEP_WALK_BASE,
};

/// Configured class-3 footstep request bases; walking and scripted jumps share cues.
///
/// Cue 1 adds one to the selected base; companion playback adds 100. The
/// class-3 policy borrows this record, although the room's floor uses class 1.
/// The read-only record retains its serialized position in .data.
static const WorldCollisionFootstepSounds _gShelterR36Class3FootstepSounds __attribute__((section(".data"))) = {
    .walk         = SHELTER_R36_CLASS_3_FOOTSTEP_WALK_BASE,
    .run          = SHELTER_R36_CLASS_3_FOOTSTEP_RUN_BASE,
    .scriptedJump = SHELTER_R36_CLASS_3_FOOTSTEP_WALK_BASE,
};

/// Blocking policy without footstep cues, shared by surface classes 0, 4, 6 and 7.
static WorldCollisionSurfaceProperties _gShelterR36DefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-5 policy: blocking probes, weapon impacts, pushback and configured footsteps.
static WorldCollisionSurfaceProperties _gShelterR36Class5SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR36Class5FootstepSounds,
};

/// Class-2 policy: blocking probes, weapon impacts, pushback and configured footsteps.
static WorldCollisionSurfaceProperties _gShelterR36Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR36Class2FootstepSounds,
};

/// Class-3 policy: blocking probes, weapon impacts, pushback and configured footsteps.
static WorldCollisionSurfaceProperties _gShelterR36Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR36Class3FootstepSounds,
};

/// Class-1 floor policy: probes pass, weapon impacts are ignored and pushback applies.
///
/// The floor contributes collision contacts and shadow height without footstep cues.
static WorldCollisionSurfaceProperties _gShelterR36FloorSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gShelterR36SurfacePropertiesByClass[8] = {
    &_gShelterR36DefaultSurfaceProperties,
    &_gShelterR36FloorSurfaceProperties,
    &_gShelterR36Class2SurfaceProperties,
    &_gShelterR36Class3SurfaceProperties,
    &_gShelterR36DefaultSurfaceProperties,
    &_gShelterR36Class5SurfaceProperties,
    &_gShelterR36DefaultSurfaceProperties,
    &_gShelterR36DefaultSurfaceProperties,
};

static void _shelterR36MessageIdle(Task* task);

/// Runs the first-arrival scene, then requests the ending or its follow-up scene.
///
/// States 0..2 start the scene, wait for event-script completion and start
/// the follow-up. Meeting progress selects the initial CAP sequence and is
/// tested again at completion: no progress requests the ending with a one-tick
/// fade; recorded progress resets CAP and advances to the follow-up scene.
/// Borrows loaded room/actor scripts and CAP data through playback. Other
/// state values do nothing. Each finishing path releases the zero-body task.
static void _shelterR36ArrivalSceneTask(Task* task)
{
    enum {
        SHELTER_R36_ARRIVAL_START           = 0,
        SHELTER_R36_ARRIVAL_WAIT_SCENE      = 1,
        SHELTER_R36_ARRIVAL_START_FOLLOW_UP = 2,
        SHELTER_R36_CAP_INITIAL_SLOT        = 1,
        SHELTER_R36_CAP_PROGRESS_SLOT       = 2,
        SHELTER_R36_CAP_PROGRESS_FILE       = 3,
        SHELTER_R36_CAP_PROGRESS_TEXTURE_X  = 320,
        SHELTER_R36_CAP_PROGRESS_TEXTURE_Y  = 256
    };
    s32 state;
    s16 sequenceSlot;

    state = task->state;
    switch (state) {
        case SHELTER_R36_ARRIVAL_START:
            if (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == 0) {
                sequenceSlot = SHELTER_R36_CAP_INITIAL_SLOT;
            } else {
                gCapFile = NULL;
                capSelectLoadedFile(SHELTER_R36_CAP_PROGRESS_FILE);
                capSetTexturePage(SHELTER_R36_CAP_PROGRESS_TEXTURE_X, SHELTER_R36_CAP_PROGRESS_TEXTURE_Y);
                sequenceSlot = SHELTER_R36_CAP_PROGRESS_SLOT;
            }
            capStartSequenceSlot(sequenceSlot, CAP_PLAYBACK_IN_PLACE, 0);
            evsStartScriptWithSkip(_gShelterR36ArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gShelterR36ArrivalSkipScript);
            task->state++;
            break;
        case SHELTER_R36_ARRIVAL_WAIT_SCENE:
            if (gGameSession->eventState == 0) {
                if (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == 0) {
                    gGameSession->restartMode = GAME_SESSION_RESTART_ENDING;
                    // This wait state's value also supplies the one-tick ending fade.
                    gGameSession->deathFadeFrames = state;
                    taskKill(task);
                } else {
                    capReset();
                    task->state++;
                }
            }
            break;
        case SHELTER_R36_ARRIVAL_START_FOLLOW_UP:
            evsStartScriptWithSkip(_gShelterR36MovieLeadInScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gShelterR36MovieLeadInSkipScript);
            taskKill(task);
            break;
    }
}

/// Reloads R36 at its movie arrival after the follow-up scene.
///
/// Attract demo 9 suppresses the reload. Otherwise updates the live save to
/// Mine/Shelter, R36, warp 2, variant 1, preserving the saved view. Selects
/// sprite variant 1, queues a frame-capturing reload and begins its alternate
/// loading caption. Used by both normal and skipped follow-up scripts.
static void _shelterR36ReloadForMovie(void)
{
    enum {
        SHELTER_R36_ATTRACT_DEMO         = 9,
        SHELTER_R36_MOVIE_ARRIVAL        = 2,
        SHELTER_R36_MOVIE_ROOM_VARIANT   = 1,
        SHELTER_R36_MOVIE_SPRITE_VARIANT = 1
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != SHELTER_R36_ATTRACT_DEMO) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_MINE_SHELTER;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_R36;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = SHELTER_R36_MOVIE_ARRIVAL;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = SHELTER_R36_MOVIE_ROOM_VARIANT;
        gDisplayState.spriteVariant                                 = SHELTER_R36_MOVIE_SPRITE_VARIANT;
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_ALTERNATE);
    }
}

/// Starts the second-arrival movie and schedules the ending two task calls later.
///
/// States 0 and 1 hold player control while the movie launcher takes over
/// display presentation; state 2 requests the ending with a one-tick fade and
/// releases this zero-body task. Other states do nothing. Requires the room's
/// movie resources and live player/session; it does not wait for movie completion.
static void _shelterR36MovieEndingTask(Task* task)
{
    enum {
        SHELTER_R36_MOVIE_ENDING_START  = 0,
        SHELTER_R36_MOVIE_ENDING_WAIT   = 1,
        SHELTER_R36_MOVIE_ENDING_FINISH = 2,
        SHELTER_R36_MOVIE_LAUNCH_TASK   = 0,
        SHELTER_R36_ENDING_FADE_TICKS   = 1
    };

    switch (task->state) {
        case SHELTER_R36_MOVIE_ENDING_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(gShelterR36MovieTasks, SHELTER_R36_MOVIE_LAUNCH_TASK, 0, 0);
            task->state++;
            break;
        case SHELTER_R36_MOVIE_ENDING_WAIT:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            task->state++;
            break;
        case SHELTER_R36_MOVIE_ENDING_FINISH:
            gGameSession->restartMode     = GAME_SESSION_RESTART_ENDING;
            gGameSession->deathFadeFrames = SHELTER_R36_ENDING_FADE_TICKS;
            taskKill(task);
            break;
    }
}

/// Selects a loaded CAP file and its texture origin for the room's event script.
///
/// Zero resets CAP playback. File 1 uses VRAM X 640; other nonzero loaded
/// file IDs use X 704. Both use Y 256. The event script selects files 1 and 2.
/// Selected CAP data must be loaded and writable for relocation; it and the
/// corresponding text textures must remain live through playback.
static void _shelterR36SelectCapFile(s32 capFileId)
{
    enum {
        SHELTER_R36_CAP_RESET           = 0,
        SHELTER_R36_CAP_FIRST_FILE      = 1,
        SHELTER_R36_CAP_FIRST_TEXTURE_X = 640,
        SHELTER_R36_CAP_OTHER_TEXTURE_X = 704,
        SHELTER_R36_CAP_TEXTURE_Y       = 256
    };
    s16 textureX;

    if (capFileId != SHELTER_R36_CAP_RESET) {
        gCapFile = 0;
        capSelectLoadedFile(capFileId);
        textureX = SHELTER_R36_CAP_OTHER_TEXTURE_X;
        if (capFileId == SHELTER_R36_CAP_FIRST_FILE) {
            textureX = SHELTER_R36_CAP_FIRST_TEXTURE_X;
        }
        capSetTexturePage(textureX, SHELTER_R36_CAP_TEXTURE_Y);
        return;
    }
    capReset();
}

/// Refuses key-item use in this room without consuming the item.
static s32 _shelterR36RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a room-transition destination from the shelter's saved progress.
///
/// Borrows an eight-byte request and writable destination for this dispatch;
/// they may alias. Copies the complete request before resolving its variant,
/// returns 1 and retains neither record.
static s32 _shelterR36ResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* destination)
{
    *destination = *request;
    mapShelterRoomVariantResolve(request, destination);
    return 1;
}

/// Ignores room commands from CAP and returns zero.
static s32 _shelterR36IgnoreCommand(Task* task, s32 messageId, s32 command, s32 unused)
{
    return 0;
}

/// Ignores the borrowed room-action request and returns zero.
static s32 _shelterR36IgnoreRoomAction(Task* task, s32 messageId, DirectionActionRequest* action, s32 unused)
{
    return 0;
}

/// Registers the room controller and starts the scene selected by its arrival.
///
/// State 0 starts the arrival scene at warp 1 or the movie/ending sequence at
/// warp 2, then advances to message service in state 1. Other warps start no
/// event task. Descriptors and their room resources must be loaded; allocation
/// failure is ignored. The registered room-task handle is borrowed.
static void _shelterR36InitializeTask(Task* task)
{
    enum {
        SHELTER_R36_SCENE_ARRIVAL      = 1,
        SHELTER_R36_MOVIE_ARRIVAL      = 2,
        SHELTER_R36_ARRIVAL_SCENE_TASK = 0,
        SHELTER_R36_MOVIE_ENDING_TASK  = 1
    };

    task->msgTable = _gShelterR36RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.warp == SHELTER_R36_SCENE_ARRIVAL) {
        taskSpawnFromTable(_gShelterR36ArrivalTasks, SHELTER_R36_ARRIVAL_SCENE_TASK, 0, 0);
    }
    if (gGameSession->location.warp == SHELTER_R36_MOVIE_ARRIVAL) {
        taskSpawnFromTable(_gShelterR36ArrivalTasks, SHELTER_R36_MOVIE_ENDING_TASK, 0, 0);
    }
    task->state++;
}

/// Keeps the room task available for messages after entry setup.
static void _shelterR36MessageIdle(Task* task)
{
    // The original idle callback retains an otherwise unused 16-byte stack frame.
    char unusedStackFrame[0x10];
}

/// Room-controller callbacks indexed by state: 0 initialize, 1 idle, 2 release.
///
/// Dispatch copies all three pointers before calling; state must be 0..2 and
/// the callback code must remain loaded. There is no terminator or bounds check.
static const TaskFuncTable3 _gShelterR36RoomTaskStates = {
    { _shelterR36InitializeTask, _shelterR36MessageIdle, taskKill },
};

void shelterR36RoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterR36RoomTaskStates;
    states.funcs[task->state](task);
}
