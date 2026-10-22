#include "actors/actor_143400.h"

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"

#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/sound_ids.h"

#include "rooms/shelter_r47.h"

static void _actor143400SelectSceneCaptions(s32 enable);
static void _actor143400ApplySceneAreaUpdates(void);
static void _actor143400SetSceneEvent(s8 sceneEvent);

/// Camera, CAP command, music event and area-bank sound slots for the R47 entry.
enum {
    ACTOR_143400_ENTRY_VIEW        = 8,
    ACTOR_143400_ENTRY_CAP_COMMAND = 3,
    ACTOR_143400_ENTRY_SCENE_EVENT = 15,
    ACTOR_143400_ENTRY_SOUND_7     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 7),
    ACTOR_143400_ENTRY_SOUND_8     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 8),
    ACTOR_143400_ENTRY_SOUND_9     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 9),
};

#include "assets/rooms/shelter/r47/companion_entry_scene_clips.inc"

/// Player clips for the companion-entry scene in Shelter R47.
///
/// The complete 11-word table is copied to bank entries 47..57: entry 47 is
/// NULL, and entries 48..57 borrow this overlay's animation sets. The base bank
/// stays intact. Keep this overlay loaded until the copied clips stop playing.
#include "assets/rooms/shelter/r47/entry_player_bank.inc"

/// Copies the entry scene's complete clip table into the player bank extension.
///
/// The count is in 32-bit words, including the leading NULL entry. Dispatch
/// consumes the request synchronously; copied clip pointers remain borrowed
/// from this overlay until their playback ends.
static AnimationBankCopyRequest _gActor143400EntryPlayerBankExtension = {
    .sets  = _gActor143400EntryPlayerAnimationSets,
    .count = ARRAY_SIZE(_gActor143400EntryPlayerAnimationSets),
};

/// Unreferenced indexed request for bank selector 1, animation 1.
///
/// Resets playback and enables world collision. Its intended recipient is
/// unproven; no scene command selects this retained record.
static AnimationPlayRequest _gActor143400UnusedClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 48 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip48Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 49 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 50 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 51 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 52 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 53 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 54 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 55 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 56 with a reset and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 57 with a 10-frame blend and world collision enabled.
///
/// Playback resolves the equipped player bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryPlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player placement before the entry scene's first animation.
///
/// Position is in room-coordinate units; all Euler angles are zero. Placement
/// consumes the complete transform synchronously and retains no pointer.
static ActorTransform _gActor143400PlayerEntryPlacement = {
    .pos = { 7000, 0, 4040, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Player placement for the later entry dialogue and its skip path.
///
/// Position is in room-coordinate units; all Euler angles are zero. Scripted
/// animation root motion can subsequently offset this placement. Both paths
/// consume the complete transform synchronously and retain no pointer.
static ActorTransform _gActor143400PlayerEntryEndPlacement = {
    .pos = { 6800, 0, 4160, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Companion clips for the entry scene in Shelter R47.
///
/// The complete nine-word table is copied to bank entries 47..55: entry 47 is
/// NULL, and entries 48..55 borrow this overlay's animation sets. The base bank
/// stays intact. Keep this overlay loaded until the copied clips stop playing.
#include "assets/rooms/shelter/r47/entry_companion_bank.inc"

/// Copies the entry scene's complete clip table into the companion bank extension.
///
/// The count is in 32-bit words, including the leading NULL entry. Dispatch
/// consumes the request synchronously; copied clip pointers remain borrowed
/// from this overlay until their playback ends.
static AnimationBankCopyRequest _gActor143400EntryCompanionBankExtension = {
    .sets  = _gActor143400EntryCompanionAnimationSets,
    .count = ARRAY_SIZE(_gActor143400EntryCompanionAnimationSets),
};

/// Unreferenced indexed request for bank selector 1, animation 48.
///
/// Resets playback and enables world collision. Its intended recipient is
/// unproven; no scene command selects this retained record.
static AnimationPlayRequest _gActor143400UnusedClip48Request = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 48 with a reset and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip48Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 49 with a 20-frame blend and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 50 with a reset and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 51 with a reset and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 52 with a reset and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 53 with a reset and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 54 with a reset and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene companion clip 55 with a reset and world collision enabled.
///
/// Playback resolves the selected companion bank in a copy of this request;
/// the stored selector is ignored. Requires this scene's copied bank extension
/// and its borrowed animation data to remain loaded through playback.
static AnimationPlayRequest _gActor143400EntryCompanionClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion placement before walking toward the entry dialogue position.
///
/// Position is in room-coordinate units; yaw is 910 in 4096 units per turn.
/// Placement consumes the complete transform synchronously and retains no pointer.
static ActorTransform _gActor143400CompanionApproachStartPlacement = {
    .pos = { 9440, 0, 3160, 0 },
    .rot = { 0, 910, 0, 0 },
};

/// Companion approach destination, final facing and skipped-scene placement.
///
/// Position is in room-coordinate units; all Euler angles are zero. The walk
/// copies XYZ, the turn reads yaw, and the later full/skip placement reads both
/// vectors. Each message consumes the record synchronously without retaining it.
static ActorTransform _gActor143400CompanionEntryEndPlacement = {
    .pos = { 7650, 0, 4110, 0 },
    .rot = { 0, 0, 0, 0 },
};

EvsCommand gActor143400CompanionEntryScript[97] = {
    // Prepare both bank extensions and the hidden companion before the dialogue.
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = 0 }, .operand1 = { .value = 30 }, .operand2 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor143400EntryPlayerBankExtension } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor143400EntryCompanionBankExtension } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, .operand0 = { .value = 100 }, .operand1 = { .value = 100 }, .operand2 = { .value = 100 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor143400SelectSceneCaptions }, .operand1 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = ACTOR_143400_ENTRY_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, .operand0 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor143400PlayerEntryPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip49Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip48Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = ACTOR_143400_ENTRY_SOUND_7 }, .operand1 = { .value = 0 }, .operand2 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 120 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .message = { .value = ACTOR_143400_ENTRY_CAP_COMMAND } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip50Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = ACTOR_143400_ENTRY_SOUND_9 }, .operand1 = { .value = 0 }, .operand2 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, .operand0 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackS8 = _actor143400SetSceneEvent }, .operand1 = { .value = ACTOR_143400_ENTRY_SCENE_EVENT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip51Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor143400PlayerEntryEndPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip52Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor143400CompanionApproachStartPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, .operand3 = { .message = { .transform = &_gActor143400CompanionEntryEndPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 0 } },
    // Bring the companion to the dialogue position before playing its scene clips.
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = ACTOR_143400_ENTRY_SOUND_8 }, .operand1 = { .value = 0 }, .operand2 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 90 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, .operand3 = { .message = { .transform = &_gActor143400CompanionEntryEndPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip57Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor143400CompanionEntryEndPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip49Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 70 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip56Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip50Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip49Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip51Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 40 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip49Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip54Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 33 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip49Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip57Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip52Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip53Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 70 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip49Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip53Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 12 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip52Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip50Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip54Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip55Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 55 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip49Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip55Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 35 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip57Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = 0 }, .operand1 = { .value = 30 }, .operand2 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, .operand0 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryPlayerClip48Request }, .operand4 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor143400EntryCompanionClip48Request }, .operand4 = { .value = 0 } },
    // Release the actors before restoring caption resources, lighting and view.
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = 0 } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor143400SelectSceneCaptions }, .operand1 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _actor143400ApplySceneAreaUpdates }, .operand1 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_STOP_SOUND, .operand0 = { .value = ACTOR_143400_ENTRY_SOUND_7 }, .operand1 = { .value = 120 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_VIEW },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 3 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor143400CompanionEntrySkipScript[20] = {
    // Commit both final transforms under the fade without playing the scene clips.
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = 0 }, .operand1 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor143400PlayerEntryEndPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor143400CompanionEntryEndPlacement } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = 0 } }, .operand4 = { .message = { .value = 0 } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_VIEW },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, .operand0 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackS8 = _actor143400SetSceneEvent }, .operand1 = { .value = ACTOR_143400_ENTRY_SCENE_EVENT } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor143400SelectSceneCaptions }, .operand1 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _actor143400ApplySceneAreaUpdates }, .operand1 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_STOP_SOUND, .operand0 = { .value = ACTOR_143400_ENTRY_SOUND_7 }, .operand1 = { .value = 60 } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Selects this scene's CAP resources, or restores the bundle's default resources.
///
/// Nonzero `enable` selects data resource ordinal 2 and a VRAM text origin of
/// (320, 256) pixels; zero resets CAP selection after playback has stopped.
/// The selected resource must be loaded, writable and live through playback.
static void _actor143400SelectSceneCaptions(s32 enable)
{
    enum { SCENE_CAP_RESOURCE  = 2,
           SCENE_CAP_TEXTURE_X = 320,
           SCENE_CAP_TEXTURE_Y = 256 };

    if (enable != 0) {
        gCapFile = NULL;
        capSelectLoadedFile(SCENE_CAP_RESOURCE);
        capSetTexturePage(SCENE_CAP_TEXTURE_X, SCENE_CAP_TEXTURE_Y);
        return;
    }
    capReset();
}

/// Commits the R47 scene's saved layouts and map marks across the Shelter.
///
/// Both the full and skipped event scripts apply the same room-owned update
/// list. Requires the R47 overlay and saved-area tables to be live. Applies
/// records synchronously, clearing affected saved enemy poses; queues no reload.
static void _actor143400ApplySceneAreaUpdates(void)
{
    areaApplySavedUpdates(gShelterR47SavedAreaUpdates);
}

/// Sets the live save's scene-event selector for stage music.
///
/// Both event-script paths supply event 15. Stores the signed byte unchanged;
/// this does not start playback or select an area music track itself.
static void _actor143400SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}
