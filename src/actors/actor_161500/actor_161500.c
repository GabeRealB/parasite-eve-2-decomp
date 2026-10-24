#include "actors/actor_161500.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/player_actor.h"
#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_1f_heliport.h"
/// Work type for the soldier's reused paced-walk animation and placement fragments.
///
/// The stride spawn allocates `StrideWalkWork` at `Task::work`. Select it
/// before the shared header and keep it through the tick, reset, blend and
/// placement definitions; `stride_walk.h` completes the type before those
/// fragments. The binding follows the `PACED_WALK_WORK_T` contract.
#define PACED_WALK_WORK_T StrideWalkWork
#include "../../shared/walk/paced_walk.h"
#include "../../shared/walk/walker.h"
#include "../../shared/walk/stride_walk.h"

/// CAP file selectors, post-shop command indices and each variant's dialogue extent.
enum {
    ACTOR_161500_ALTERNATE_VARIANT           = 1,
    ACTOR_161500_TALKS_PER_VARIANT           = 4,
    ACTOR_161500_LAST_TALK                   = ACTOR_161500_TALKS_PER_VARIANT - 1,
    ACTOR_161500_CAPTION_RESET               = 0,
    ACTOR_161500_CAPTION_BY_VARIANT          = -1,
    ACTOR_161500_CAPTION_FILE_STANDARD       = 1,
    ACTOR_161500_CAPTION_FILE_ALTERNATE      = 2,
    ACTOR_161500_CAPTION_AFTER_PARKING_SHOP  = 29,
    ACTOR_161500_CAPTION_AFTER_HELIPORT_SHOP = 52,
};

/// Saved stages of the companion's item request.
enum {
    ACTOR_161500_REQUEST_UNSEEN   = 0,
    ACTOR_161500_REQUEST_PENDING  = 1,
    ACTOR_161500_REQUEST_COMPLETE = 2,
};

/// Stock selectors passed to the room's shop, with the initial category zero.
enum {
    ACTOR_161500_SHOP_STOCK_STANDARD        = 0x30,
    ACTOR_161500_SHOP_STOCK_ALTERNATE       = 0x31,
    ACTOR_161500_SHOP_STOCK_EVENT           = 0x32,
    ACTOR_161500_SHOP_STOCK_ALTERNATE_EVENT = 0x33,
};

/// Pending parking remarks and the visitor progress consumed by the departure remark.
enum {
    ACTOR_161500_REMARK_DEFAULT                      = 0,
    ACTOR_161500_REMARK_STERILIZATION_WITH_COMPANION = 1,
    ACTOR_161500_REMARK_STERILIZATION_ALONE          = 2,
    ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP       = 3,
    ACTOR_161500_REMARK_VISITOR_DEPARTURE            = 4,
    ACTOR_161500_VISITOR_DEPARTED                    = 4,
    ACTOR_161500_VISITOR_REMARK_SEEN                 = 5,
};

static void _actor161500SelectCaptionFile(s32 captionFile);
static void _actor161500OpenHeliportShop(void);
static void _actor161500RunCaptionCommand(s32 commandId);
static void _actor161500OpenParkingShop(void);

static void _actor161500CompleteCompanionRequestTask(Task* task);

static AnimationSet _gSoldierBPlayerCrouchClip;
static AnimationSet _gSoldierBPlayerCrouchedIdleClip;
static AnimationSet _gSoldierBPlayerStandUpClip;
static AnimationSet _gSoldierBRequestScenePlayerAnim03Clip;
static AnimationSet _gSoldierBRequestScenePlayerAnim04Clip;
static AnimationSet _gSoldierBRequestScenePlayerAnim05Clip;

static s32  _strideWalkSetTurnMode(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static void _actor161500PreparePlayerFacingCompanion(void);
static void _actor161500SetPlayerUpdateHold(u8 holdPlayerUpdate);
static void _actor161500StrideWalkerTask(Task* task);

#include "assets/characters/soldier_b/shop_clips_1.inc"

/// Stored bank indices; weapon-animation instructions replace these in a copy.
enum {
    ACTOR_161500_PLAYER_BANK_SEED    = 1,
    ACTOR_161500_COMPANION_BANK_SEED = 6,
};

/// Transition durations in whole normal-rate frames and the retained raw copy length.
enum {
    ACTOR_161500_TALK_BLEND_FRAMES           = 8,
    ACTOR_161500_SOLDIER_REMARK_BLEND_FRAMES = 4,
    ACTOR_161500_REQUEST_BANK_COPY_WORDS     = 10,
};

/// Starts the soldier's head aim toward the player during dialogue.
///
/// The stride walker reads only the command; the heliport context is retained
/// even when parking dialogue sends this record.
static ActorCommand _gActor161500AimHeadAtPlayerCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT } },
    .command = STRIDE_WALK_TURN_PLAYER,
};

/// Releases the soldier's head aim back toward its animated pose after dialogue.
static ActorCommand _gActor161500ReleaseHeadAimCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT } },
    .command = STRIDE_WALK_TURN_RELEASE,
};

/// Resets the player's equipped-weapon clip 1 and drops grid participation for dialogue.
///
/// The event instruction resolves the equipped weapon bank in a copy. This
/// record and the selected weapon's clips remain loaded through playback.
static AnimationPlayRequest _gActor161500TalkPlayerBaseAnimationRequest = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Six player clips installed at weapon-bank ids 47-52 for talks, shops and remarks.
///
/// The table and clip data belong to this loaded overlay. Scripts install the
/// extension before playback and use only these six entries as clip pointers.
#include "assets/characters/soldier_b/player_talk_bank.inc"

/// Blends the player to talk-bank clip 47 after a cue in the first standard talk B.
static AnimationPlayRequest _gActor161500TalkPlayerAnimation47Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends the player to talk-bank clip 48 at the following first-talk-B cue.
static AnimationPlayRequest _gActor161500TalkPlayerAnimation48Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends the player to talk-bank clip 49 in both variants' first talk A.
static AnimationPlayRequest _gActor161500TalkPlayerAnimation49Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends the player to talk-bank clip 50 in talk B, the shop opening and parking remarks.
static AnimationPlayRequest _gActor161500TalkPlayerAnimation50Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends the player to talk-bank clip 51 in the third alternate talk B.
static AnimationPlayRequest _gActor161500TalkPlayerAnimation51Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends the player to talk-bank clip 52 after clip 49 in the first alternate talk A.
static AnimationPlayRequest _gActor161500TalkPlayerAnimation52Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Reseeds soldier B's clip 3 over eight frames for shop dialogue and parking remarks.
///
/// The stride walker ignores the bank index and collision choice and uses its
/// own clip table. The request is borrowed only through message dispatch.
static AnimationPlayRequest _gActor161500SoldierAnimation3Request = {
    .source               = { .index = 0 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Reseeds soldier B's clip 11 over four frames for sterilization and departure remarks.
///
/// The stride walker ignores the bank index and collision choice and uses its
/// own clip table. The request is borrowed only through message dispatch.
static AnimationPlayRequest _gActor161500SoldierAnimation11Request = {
    .source               = { .index = 0 },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_SOLDIER_REMARK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Installs the talk clips and adjacent raw words into the equipped player bank.
///
/// The retained 32-word transfer reads six set pointers, all five request words
/// of clips 47-51 and the bank-index word of clip 52. Entries after the six
/// pointers are never played. Keep the source data in this order and the overlay
/// loaded while copied clip pointers are in use; the count is not an array bound.
static AnimationBankCopyRequest _gActor161500TalkPlayerBankCopyRequest = {
    .sets  = _gActor161500TalkPlayerAnimationSets,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// CAP command indices and the placed soldier addressed by dialogue set A.
enum {
    ACTOR_161500_TALK_A_PLACEMENT_INDEX          = 2,
    ACTOR_161500_CAPTION_TALK_A_FIRST            = 10,
    ACTOR_161500_CAPTION_TALK_A_SECOND           = 11,
    ACTOR_161500_CAPTION_TALK_A_THIRD            = 12,
    ACTOR_161500_CAPTION_TALK_A_REPEAT           = 13,
    ACTOR_161500_CAPTION_TALK_A_ALTERNATE_FIRST  = 34,
    ACTOR_161500_CAPTION_TALK_A_ALTERNATE_SECOND = 35,
    ACTOR_161500_CAPTION_TALK_A_ALTERNATE_THIRD  = 36,
    ACTOR_161500_CAPTION_TALK_A_ALTERNATE_REPEAT = 37,
};

/// First standard conversation in soldier B's heliport dialogue set A.
///
/// Plays player talk clip 49 between CAP cues.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkAFirstScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_FIRST }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second standard conversation in soldier B's heliport dialogue set A.
///
/// Waits for the CAP cue without an extended player clip.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkASecondScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_SECOND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third standard conversation in soldier B's heliport dialogue set A.
///
/// Waits for the CAP cue without an extended player clip.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkAThirdScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_THIRD }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeating standard conversation in soldier B's heliport dialogue set A.
///
/// Selected when the saved talk count has reached 3.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkARepeatScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_REPEAT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First variant-1 conversation in soldier B's heliport dialogue set A.
///
/// Plays player talk clips 49 and 52 at successive CAP cues.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkAAlternateFirstScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_ALTERNATE_FIRST }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second variant-1 conversation in soldier B's heliport dialogue set A.
///
/// Waits for the CAP cue without an extended player clip.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkAAlternateSecondScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_ALTERNATE_SECOND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third variant-1 conversation in soldier B's heliport dialogue set A.
///
/// Waits for the CAP cue without an extended player clip.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkAAlternateThirdScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_ALTERNATE_THIRD }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeating variant-1 conversation in soldier B's heliport dialogue set A.
///
/// Selected when the saved talk count has reached 3.
/// Installs the player talk bank, holds the soldier's head aim through the
/// exchange, then releases aim, restores default CAP and ends player scripting.
/// Keep this overlay, the room CAP resources and the equipped weapon loaded.
static EvsCommand _gActor161500TalkAAlternateRepeatScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_A_ALTERNATE_REPEAT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_A_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier B's heliport dialogue set A, four conversations per variant.
///
/// Slots 0..3 are standard, 4..7 variant 1; each span's final script repeats.
/// The saved talk count must be 0..3. Commands and referenced overlay data
/// remain borrowed until the event interpreter ends.
static EvsCommand* _gActor161500TalkAScripts[2 * ACTOR_161500_TALKS_PER_VARIANT] = {
    _gActor161500TalkAFirstScript,
    _gActor161500TalkASecondScript,
    _gActor161500TalkAThirdScript,
    _gActor161500TalkARepeatScript,
    _gActor161500TalkAAlternateFirstScript,
    _gActor161500TalkAAlternateSecondScript,
    _gActor161500TalkAAlternateThirdScript,
    _gActor161500TalkAAlternateRepeatScript,
};

/// CAP command indices and soldier placements for talk B, shops and request scenes.
enum {
    ACTOR_161500_TALK_B_PLACEMENT_INDEX                       = 1,
    ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX                = 3,
    ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX                 = 0,
    ACTOR_161500_CAPTION_TALK_B_FIRST                         = 14,
    ACTOR_161500_CAPTION_TALK_B_SECOND                        = 15,
    ACTOR_161500_CAPTION_TALK_B_THIRD                         = 16,
    ACTOR_161500_CAPTION_TALK_B_REPEAT                        = 17,
    ACTOR_161500_CAPTION_TALK_B_ALTERNATE_FIRST               = 38,
    ACTOR_161500_CAPTION_TALK_B_ALTERNATE_SECOND              = 39,
    ACTOR_161500_CAPTION_TALK_B_ALTERNATE_THIRD               = 40,
    ACTOR_161500_CAPTION_TALK_B_ALTERNATE_REPEAT              = 41,
    ACTOR_161500_CAPTION_HELIPORT_SHOP_FIRST                  = 50,
    ACTOR_161500_CAPTION_HELIPORT_SHOP_REPEAT                 = 51,
    ACTOR_161500_CAPTION_PARKING_SHOP                         = 24,
    ACTOR_161500_CAPTION_PARKING_STERILIZATION_WITH_COMPANION = 25,
    ACTOR_161500_CAPTION_PARKING_STERILIZATION_ALONE          = 26,
    ACTOR_161500_CAPTION_PARKING_STERILIZATION_FOLLOWUP       = 27,
    ACTOR_161500_CAPTION_PARKING_VISITOR_DEPARTURE            = 28,
    ACTOR_161500_CAPTION_REQUEST_INTRO                        = 22,
    ACTOR_161500_CAPTION_REQUEST_SKIP                         = 23,
};

/// Request-scene view and sound-script keys; sound identities beyond these slots are unproven.
enum {
    ACTOR_161500_REQUEST_VIEW    = 3,
    ACTOR_161500_REQUEST_SOUND_3 = 0x55040003,
    ACTOR_161500_REQUEST_SOUND_4 = 0x55040004,
};

/// First standard conversation in soldier B's heliport dialogue set B.
///
/// Installs the player talk bank and plays clips 50, 47 and 48 between CAP
/// cues. Aims the placed soldier at the player during the exchange, releases
/// aim, restores default CAP and ends player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBFirstScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_FIRST }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second standard conversation in soldier B's heliport dialogue set B.
///
/// Installs the player talk bank and plays clip 50 before aiming the soldier
/// at the player. CAP cues bracket head aim; the end restores default CAP
/// and releases player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBSecondScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_SECOND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third standard conversation in soldier B's heliport dialogue set B.
///
/// Installs the player talk bank, starts the base weapon clip and plays talk
/// clip 50 at a CAP cue. Releases soldier head aim, restores default CAP
/// and ends player scripting after the exchange.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBThirdScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_THIRD }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeating standard conversation in soldier B's heliport dialogue set B.
///
/// Installs the talk bank and starts the base weapon clip. CAP cues bracket
/// soldier head aim; the end restores default CAP and player control.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBRepeatScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_REPEAT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First variant-1 conversation in soldier B's heliport dialogue set B.
///
/// Selects alternate CAP, installs the player talk bank and plays clip 50.
/// CAP cues bracket soldier head aim; the end restores default CAP and
/// releases player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBAlternateFirstScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_ALTERNATE_FIRST }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second variant-1 conversation in soldier B's heliport dialogue set B.
///
/// Selects alternate CAP, installs the player talk bank and plays clip 50.
/// CAP cues bracket soldier head aim; the end restores default CAP and
/// releases player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBAlternateSecondScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_ALTERNATE_SECOND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third variant-1 conversation in soldier B's heliport dialogue set B.
///
/// Selects alternate CAP, installs the talk bank and starts the base weapon
/// clip, then plays clip 51 at a CAP cue. Releases head aim, restores
/// default CAP and ends player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBAlternateThirdScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_ALTERNATE_THIRD }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeating variant-1 conversation in soldier B's heliport dialogue set B.
///
/// Selects alternate CAP, installs the talk bank and starts the base weapon
/// clip. CAP cues bracket head aim; the end restores default CAP and
/// releases player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500TalkBAlternateRepeatScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_TALK_B_ALTERNATE_REPEAT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier B's heliport dialogue set B, four conversations per variant.
///
/// Slots 0..3 are standard, 4..7 variant 1; each span's final script repeats.
/// The saved talk count must be 0..3. Commands and referenced overlay data
/// remain borrowed until the event interpreter ends.
static EvsCommand* _gActor161500TalkBScripts[2 * ACTOR_161500_TALKS_PER_VARIANT] = {
    _gActor161500TalkBFirstScript,
    _gActor161500TalkBSecondScript,
    _gActor161500TalkBThirdScript,
    _gActor161500TalkBRepeatScript,
    _gActor161500TalkBAlternateFirstScript,
    _gActor161500TalkBAlternateSecondScript,
    _gActor161500TalkBAlternateThirdScript,
    _gActor161500TalkBAlternateRepeatScript,
};

/// Soldier B's first heliport shop conversation.
///
/// Entered before the first post-shop exchange. Uses variant-selected CAP,
/// player talk clip 50 and soldier clip 3, opens the heliport shop, then
/// resumes CAP command 52 and releases head aim and player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500HeliportShopFirstScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_BY_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_HELIPORT_SHOP_FIRST }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenHeliportShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_HELIPORT_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier B's repeating heliport shop conversation.
///
/// Entered after at least one post-shop exchange. Uses variant-selected CAP
/// and soldier clip 3, opens the heliport shop, then resumes CAP command 52
/// and releases head aim and player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500HeliportShopRepeatScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_BY_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_HELIPORT_SHOP_REPEAT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenHeliportShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_HELIPORT_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_HELIPORT_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier B's default parking-shop exchange.
///
/// Starts CAP command 24 with the current CAP file, brackets soldier head
/// aim, plays soldier clip 3 and opens progress-selected parking stock.
/// Resumes CAP command 29 before releasing player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500ParkingShopScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_PARKING_SHOP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Parking-shop remark after sterilization with the companion.
///
/// Starts CAP command 25 with the current CAP file, plays soldier clips
/// 3 and 11, and opens progress-selected parking stock. Resumes CAP
/// command 29 and soldier clip 3 before releasing player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500ParkingSterilizationWithCompanionScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_PARKING_STERILIZATION_WITH_COMPANION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Parking-shop remark after sterilization without the companion.
///
/// Starts CAP command 26 with the current CAP file, plays player talk
/// clip 50 and soldier clip 11, then opens progress-selected parking stock.
/// Resumes CAP command 29 and soldier clip 3 before releasing scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500ParkingSterilizationAloneScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_PARKING_STERILIZATION_ALONE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Parking-shop follow-up to either initial sterilization remark.
///
/// Starts CAP command 27 with the current CAP file, brackets soldier head
/// aim, plays soldier clip 3 and opens progress-selected parking stock.
/// Resumes CAP command 29 before releasing player scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500ParkingSterilizationFollowupScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_PARKING_STERILIZATION_FOLLOWUP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500AimHeadAtPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor161500ReleaseHeadAimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Parking-shop remark after the visitor has departed.
///
/// Starts CAP command 28 with the current CAP file, plays soldier clip 11
/// and player talk clip 50, then opens progress-selected parking stock.
/// Resumes CAP command 29 and soldier clip 3 before releasing scripting.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500ParkingVisitorDepartureScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_PARKING_VISITOR_DEPARTURE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500TalkPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500TalkPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor161500SoldierAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/soldier_b/shop_clips_2.inc"

TaskDesc gActor161500CompleteCompanionRequestTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = _actor161500CompleteCompanionRequestTask,
    .data     = { .value = 0 },
};

/// Player placement for the full companion-request introduction.
///
/// Position uses room-coordinate units; Euler angles use 4096 units per turn.
/// Placement reads both vectors; movement reads XYZ and turning reads yaw.
/// The message borrows this record through synchronous dispatch.
static ActorTransform _gActor161500RequestScenePlayerPlacement = {
    .pos = { 850, 0, 4400, 0 },
    .rot = { 0, 568, 0, 0 },
};

/// Player placement restored by the companion-request skip path.
///
/// Position uses room-coordinate units; Euler angles use 4096 units per turn.
/// Placement reads both vectors; movement reads XYZ and turning reads yaw.
/// The message borrows this record through synchronous dispatch.
static ActorTransform _gActor161500RequestSkipPlayerPlacement = {
    .pos = { 850, 0, 4400, 0 },
    .rot = { 0, 568, 0, 0 },
};

/// Companion starting placement in the full item-request introduction.
///
/// Position uses room-coordinate units; Euler angles use 4096 units per turn.
/// Placement reads both vectors; movement reads XYZ and turning reads yaw.
/// The message borrows this record through synchronous dispatch.
static ActorTransform _gActor161500RequestSceneCompanionStartPlacement = {
    .pos = { 2510, 0, 5950, 0 },
    .rot = { 0, 2616, 0, 0 },
};

/// Companion approach position and subsequent facing yaw in the request introduction.
///
/// Position uses room-coordinate units; Euler angles use 4096 units per turn.
/// Placement reads both vectors; movement reads XYZ and turning reads yaw.
/// The message borrows this record through synchronous dispatch.
static ActorTransform _gActor161500RequestSceneCompanionApproachTarget = {
    .pos = { 1330, 0, 4780, 0 },
    .rot = { 0, 2616, 0, 0 },
};

/// Companion placement restored after the first request scene, on skip and on re-entry.
///
/// Position is in room-coordinate units; yaw is a half turn (4096 units per
/// turn). Placement messages consume the complete transform synchronously.
static ActorTransform _gActor161500CompanionPendingRequestPlacement = { { 4224, 0, 5209, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Six player request-scene clips for weapon-bank ids 47-52, followed by NULL at id 53.
///
/// The first exchange, reminders and handover install this table before playing
/// the extended clips. The table and clip data stay borrowed from this overlay;
/// no script plays the NULL or the raw words following the table.
#include "assets/characters/soldier_b/player_request_scene_bank.inc"

/// Spare player request-scene clip-47 reset record, never selected for playback.
///
/// The ten-word bank transfer reads this record's first three words after the
/// seven set-table entries: bank seed, clip id and reset choice. Preserve this
/// twenty-byte record's position; its final two words are outside that transfer.
static AnimationPlayRequest _gActor161500UnusedRequestScenePlayerAnimation47Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Starts player request-bank clip 47 before waiting for its end in every full request scene.
static AnimationPlayRequest _gActor161500RequestScenePlayerAnimation47Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player request-bank clip 48 after clip 47 and between first-exchange cues.
static AnimationPlayRequest _gActor161500RequestScenePlayerAnimation48Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Starts player request-bank clip 49 before the reminder and handover scripts finish.
static AnimationPlayRequest _gActor161500RequestScenePlayerAnimation49Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player request-bank clip 50 during the first item-request exchange.
///
/// The event instruction resolves the equipped weapon bank in a copy.
/// Requires the installed request-scene extension; grid participation is
/// enabled and the overlay clip remains borrowed for playback.
static AnimationPlayRequest _gActor161500RequestScenePlayerAnimation50Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player request-bank clip 51 during the first item-request exchange.
///
/// The event instruction resolves the equipped weapon bank in a copy.
/// Requires the installed request-scene extension; grid participation is
/// enabled and the overlay clip remains borrowed for playback.
static AnimationPlayRequest _gActor161500RequestScenePlayerAnimation51Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player request-bank clip 52 during the first item-request exchange.
///
/// The event instruction resolves the equipped weapon bank in a copy.
/// Requires the installed request-scene extension; grid participation is
/// enabled and the overlay clip remains borrowed for playback.
static AnimationPlayRequest _gActor161500RequestScenePlayerAnimation52Request = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Installs request-scene clips and four other words into the equipped player bank.
///
/// The retained ten-word transfer reads six set pointers, NULL, then the first
/// three words of the neighbouring `_gActor161500UnusedRequestScenePlayerAnimation47Request` play request
/// (bank index, animation id and blend choice). Those four other entries are
/// never played. Keep this source order and the overlay loaded while the clip
/// pointers are in use; the transfer length is neither the table's extent nor
/// the size of an established aggregate.
static AnimationBankCopyRequest _gActor161500RequestScenePlayerBankCopyRequest = {
    .sets  = _gActor161500RequestScenePlayerAnimationSets,
    .count = ACTOR_161500_REQUEST_BANK_COPY_WORDS,
};

/// Resets equipped-weapon clip 1 and drops player grid participation on request-scene entry or skip.
///
/// The event instruction replaces the bank seed in a copy with the current
/// equipped weapon bank. The weapon's clip data must remain loaded for playback.
static AnimationPlayRequest _gActor161500RequestScenePlayerBaseAnimationRequest = {
    .source               = { .index = ACTOR_161500_PLAYER_BANK_SEED },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets companion clip 1 at introduction entry and after the approach.
///
/// The event instruction resolves the saved companion type and variant
/// in a copy; the bank seed is not a fixed bank. Disables grid participation
/// and borrows the selected companion clips for playback.
static AnimationPlayRequest _gActor161500RequestSceneCompanionAnimation1Request = {
    .source               = { .index = ACTOR_161500_COMPANION_BANK_SEED },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the companion to clip 38 over eight frames during the request exchanges.
///
/// The event instruction resolves the saved companion type and variant in a
/// copy; 6 is a bank seed, not a fixed companion bank. Grid participation is
/// disabled, and the selected companion clips remain loaded for playback.
static AnimationPlayRequest _gActor161500RequestSceneCompanionAnimation38Request = {
    .source               = { .index = ACTOR_161500_COMPANION_BANK_SEED },
    .animationId          = 38,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_161500_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets companion clip 7 between introduction CAP cues.
///
/// The event instruction resolves the saved companion type and variant
/// in a copy; the bank seed is not a fixed bank. Disables grid participation
/// and borrows the selected companion clips for playback.
static AnimationPlayRequest _gActor161500RequestSceneCompanionAnimation7Request = {
    .source               = { .index = ACTOR_161500_COMPANION_BANK_SEED },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets companion clip 8 during the completed-request handover.
///
/// The event instruction resolves the saved companion type and variant
/// in a copy; the bank seed is not a fixed bank. Disables grid participation
/// and borrows the selected companion clips for playback.
static AnimationPlayRequest _gActor161500RequestSceneCompanionAnimation8Request = {
    .source               = { .index = ACTOR_161500_COMPANION_BANK_SEED },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets companion clip 9 when restoring the pending request scene's pose.
///
/// Used after the first exchange, on skip, on re-entry and after reminders.
/// The event instruction resolves the saved companion bank in a copy and
/// disables grid participation; the selected clip data stays borrowed.
static AnimationPlayRequest _gActor161500RequestSceneCompanionAnimation9Request = {
    .source               = { .index = ACTOR_161500_COMPANION_BANK_SEED },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restores the pending companion request when its introduction is skipped.
///
/// Stops request sound 3, fades out, places the player and companion, shows
/// the two soldiers and restores weapons and ambient lighting. Restores
/// view 3 and companion clip 9, fades in, starts CAP command 23, releases
/// player scripting and refreshes the companion obstacle.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500CompanionRequestSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_161500_REQUEST_SOUND_3 }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor161500RequestSkipPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor161500CompanionPendingRequestPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_TALK_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_161500_PARKING_SHOP_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_161500_REQUEST_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_REQUEST_SKIP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First companion item-request exchange at the heliport.
///
/// Starts CAP command 22, installs player clips 47..52, stages the player
/// and companion, and synchronizes movement, clips, fades and sounds with
/// CAP cues. Restores companion clip 9 at the pending-request placement,
/// view 3, weapons and lighting, then releases player scripting and
/// refreshes the companion obstacle. The starter records the pending flag.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500CompanionRequestIntroScript[] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_REQUEST_INTRO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500RequestScenePlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor161500RequestScenePlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor161500RequestSceneCompanionStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_161500_REQUEST_SOUND_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor161500RequestSceneCompanionApproachTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor161500RequestSceneCompanionApproachTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation7Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation38Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation38Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_161500_REQUEST_SOUND_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_161500_REQUEST_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor161500CompanionPendingRequestPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restores the companion's pending-request pose and obstacle on re-entry.
///
/// Yields before resetting companion clip 9 and before placing the model.
/// Keeps HUD state, updates the room obstacle and ends without dialogue.
/// Keep this overlay, the required CAP resources and actor clips loaded
/// until the END-terminated command stream finishes.
static EvsCommand _gActor161500CompanionRequestRestoreScript[] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor161500CompanionPendingRequestPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Mutable yaw target that turns the player toward the live companion in request scenes.
///
/// The preparation callback writes yaw in 0..4095 before each turn message.
/// The receiver reads only yaw and copies it for later frames; position and
/// the other rotation components are unused. The initial yaw is -1/4 turn.
static ActorTransform _gActor161500PlayerFacingCompanionTarget = { { 0, 0, 0, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

enum {
    ACTOR_161500_CAPTION_REQUEST_REMINDER                   = 27,
    ACTOR_161500_CAPTION_REQUEST_PENDING_PLACEMENT_REMINDER = 29,
};

/// Companion item-request reminder when placement 3 does not carry the pending state.
///
/// Twenty commands include END. Selects CAP command 27, turns the player toward
/// the live companion, plays player clips 47..49 and companion clips 38 then 9,
/// and ends player scripted control. Requires the pending request, both actors,
/// CAP resources and this overlay's borrowed clips throughout event playback.
static EvsCommand _gActor161500CompanionRequestReminderScript[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500RequestScenePlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_REQUEST_REMINDER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor161500PlayerFacingCompanionTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_161500_REQUEST_SOUND_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation38Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Companion item-request reminder while placement 3 carries the pending state.
///
/// Twenty commands include END. Selects CAP command 29 and uses the same facing,
/// player/companion clips and scripted-control release as the other reminder.
/// Requires the pending request, both actors, CAP resources and this overlay's
/// borrowed clips throughout event playback; the caller hides/restores the HUD.
static EvsCommand _gActor161500CompanionRequestPendingPlacementReminderScript[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500RequestScenePlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_REQUEST_PENDING_PLACEMENT_REMINDER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor161500PlayerFacingCompanionTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_161500_REQUEST_SOUND_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation38Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum { ACTOR_161500_CAPTION_REQUEST_HANDOVER = 28 };

/// Companion item-request handover after the room accepts item 0x124.
///
/// Updates the room obstacle, installs player scene clips, releases the player
/// update hold and turns the player toward the companion. CAP cues coordinate
/// the handover before both actors leave scripted control. All 29 commands
/// include END; there is no skip path. Commands, callbacks and animation data
/// remain borrowed with the actor/heliport overlays loaded until END.
static EvsCommand _gActor161500CompanionRequestHandoverScript[29] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor161500RequestScenePlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_161500_CAPTION_REQUEST_HANDOVER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor161500SetPlayerUpdateHold }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor161500PlayerFacingCompanionTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_161500_REQUEST_SOUND_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation38Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_161500_REQUEST_SOUND_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation38Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestSceneCompanionAnimation8Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor161500RequestScenePlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/soldier_b/rifle.inc"

#include "assets/characters/soldier_b/body.inc"

#include "assets/characters/soldier_b/shop_clips_3.inc"

/// Soldier B's stride-walk message dispatch table, terminated by a null end entry.
///
/// Handles play requests (clip IDs 1..11), draw flags, parent-space
/// placement, head-turn commands and walk targets. The spawned task borrows
/// this table and its callbacks through teardown; unsupported IDs return 0.
static TaskMessageEntry _gStrideWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _strideWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _strideWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _strideWalkSetTurnMode },
    { ACTOR_MESSAGE_WALK_TO, _strideWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor161500SoldierBTasks[2] = {
    {
        .header   = { .fields = { .flags    = TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER,
                                  .priority = TASK_PRIORITY_ACTOR } },
        .callback = _actor161500StrideWalkerTask,
        .data     = { .model = &_gSoldierBBody },
    },
    {
        .header   = { .fields = { .flags    = TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER,
                                  .priority = TASK_PRIORITY_ACTOR } },
        .callback = _strideWalkSubModelTask,
        .data     = { .model = &_gSoldierBRifle },
    },
};

/// Soldier B's stride-walk animation bank: NULL slot 0 and playable clips 1..11.
///
/// The rig borrows this table and its twenty-part animation sets until
/// task teardown. This package owns its own bank and clip data.
#include "assets/characters/soldier_b/stride_walk_bank.inc"

/// Queues soldier B's visitor-departure remark after pending sterilization outcomes.
///
/// The two initial sterilization remarks take priority. When meeting progress
/// reaches departed, records its remark as handled before selecting the line;
/// subsequent calls leave it alone. A sterilization follow-up may be replaced.
static inline void _actor161500PrepareVisitorDepartureRemark(void)
{
    if ((gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE) != ACTOR_161500_REMARK_STERILIZATION_WITH_COMPANION) &&
        (gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE) != ACTOR_161500_REMARK_STERILIZATION_ALONE) &&
        (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == ACTOR_161500_VISITOR_DEPARTED)) {
        gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, ACTOR_161500_VISITOR_REMARK_SEEN);
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_VISITOR_DEPARTURE);
    }
}

void actor161500StartSoldierBRemark(void)
{
    _actor161500PrepareVisitorDepartureRemark();

    switch (gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE)) {
        case ACTOR_161500_REMARK_DEFAULT:
            evsStartScript(_gActor161500ParkingShopScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
        case ACTOR_161500_REMARK_STERILIZATION_WITH_COMPANION:
            evsStartScript(_gActor161500ParkingSterilizationWithCompanionScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP);
            break;
        case ACTOR_161500_REMARK_STERILIZATION_ALONE:
            evsStartScript(_gActor161500ParkingSterilizationAloneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP);
            break;
        case ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP:
            evsStartScript(_gActor161500ParkingSterilizationFollowupScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_DEFAULT);
            break;
        case ACTOR_161500_REMARK_VISITOR_DEPARTURE:
            evsStartScript(_gActor161500ParkingVisitorDepartureScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_DEFAULT);
            break;
    }
}

/// Selects the soldier's loaded CAP data and texture page, or restores the default.
///
/// Zero resets CAP; positive values are loaded data-resource ordinals. Any
/// negative value selects ordinal 2 in variant 1, ordinal 1 otherwise. The
/// selected writable CAP resource and texture page must remain loaded for playback.
static void _actor161500SelectCaptionFile(s32 captionFile)
{
    enum { ACTOR_161500_CAPTION_VRAM_X = 832,
           ACTOR_161500_CAPTION_VRAM_Y = 0 };
    s8 variantCaptionFile;

    if (captionFile != ACTOR_161500_CAPTION_RESET) {
        gCapFile = NULL;
        if (captionFile <= 0) {
            variantCaptionFile = ACTOR_161500_CAPTION_FILE_STANDARD;
            if (gGameSession->location.variant == ACTOR_161500_ALTERNATE_VARIANT) {
                variantCaptionFile = ACTOR_161500_CAPTION_FILE_ALTERNATE;
            }
            captionFile = variantCaptionFile;
        }
        capSelectLoadedFile(captionFile);
        capSetTexturePage(ACTOR_161500_CAPTION_VRAM_X, ACTOR_161500_CAPTION_VRAM_Y);
        return;
    }
    capReset();
}

void actor161500StartSoldierBTalkA(void)
{
    s32 variantScriptOffset;
    s32 talkIndex;

    variantScriptOffset = (gGameSession->location.variant == ACTOR_161500_ALTERNATE_VARIANT) * ACTOR_161500_TALKS_PER_VARIANT;
    talkIndex           = gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_A);
    evsStartScript(_gActor161500TalkAScripts[talkIndex + variantScriptOffset], EVENT_SCRIPT_HUD_HIDE_RESTORE);
    if (talkIndex < ACTOR_161500_LAST_TALK) {
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_A, talkIndex + 1);
    }
}

void actor161500StartSoldierBTalkB(void)
{
    s32 variantScriptOffset;
    s32 talkIndex;

    variantScriptOffset = (gGameSession->location.variant == ACTOR_161500_ALTERNATE_VARIANT) * ACTOR_161500_TALKS_PER_VARIANT;
    talkIndex           = gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B);
    evsStartScript(_gActor161500TalkBScripts[talkIndex + variantScriptOffset], EVENT_SCRIPT_HUD_HIDE_RESTORE);
    if (talkIndex < ACTOR_161500_LAST_TALK) {
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B, talkIndex + 1);
    }
}

/// Opens the heliport shop's standard or variant-1 alternate stock at category zero.
static void _actor161500OpenHeliportShop(void)
{
    GameSession* session;

    session = gGameSession;
    switch (session->location.variant) {
        case ACTOR_161500_ALTERNATE_VARIANT:
            shopOpenSession(ACTOR_161500_SHOP_STOCK_ALTERNATE);
            break;
        default:
            shopOpenSession(ACTOR_161500_SHOP_STOCK_STANDARD);
            break;
    }
}

/// Resumes the selected soldier CAP file with a command after the shop closes.
///
/// `commandId` is a CAP command-table index. Uses the current view in place;
/// the selected CAP file and its texture page must still be loaded.
static void _actor161500RunCaptionCommand(s32 commandId)
{
    capRunCommand(commandId, CAP_PLAYBACK_IN_PLACE);
}

void actor161500StartSoldierBShopConversation(void)
{
    if (gameFlagGetNibble(GAME_FLAG_105) == 0) {
        evsStartScript(_gActor161500HeliportShopFirstScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    } else {
        evsStartScript(_gActor161500HeliportShopRepeatScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
}

/// Opens parking-shop stock selected by the memo follow-up and sterilization outcome.
static void _actor161500OpenParkingShop(void)
{
    enum { ACTOR_161500_STERILIZATION_WITH_COMPANION = 2 };
    if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) != 0) {
        shopOpenSession((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) != ACTOR_161500_STERILIZATION_WITH_COMPANION) ? ACTOR_161500_SHOP_STOCK_ALTERNATE : ACTOR_161500_SHOP_STOCK_ALTERNATE_EVENT);
    } else {
        shopOpenSession((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == ACTOR_161500_STERILIZATION_WITH_COMPANION) ? ACTOR_161500_SHOP_STOCK_EVENT : ACTOR_161500_SHOP_STOCK_STANDARD);
    }
}

/// Completes the accepted item request and starts its handover scene, then kills this task.
///
/// The room has checked item use and companion presence before spawning this
/// one-shot task. Holds the player state tick until the script releases it.
static void _actor161500CompleteCompanionRequestTask(Task* task)
{
    enum { ACTOR_161500_REQUEST_ITEM = 0x124 };
    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
    itemSetIdentified(ACTOR_161500_REQUEST_ITEM, true);
    gameFlagSetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE, ACTOR_161500_REQUEST_COMPLETE);
    evsStartScript(_gActor161500CompanionRequestHandoverScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    taskKill(task);
}

/// Sets the request scene's player-facing yaw toward the live companion.
///
/// Both task slots must contain TMD models whose roots share a parent frame.
/// Stores a 0..4095 yaw, with +Z as zero and +X as the positive quarter turn.
static void _actor161500PreparePlayerFacingCompanion(void)
{
    GfxCoord* companionCoord;
    GfxCoord* playerCoord;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    playerCoord    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(companionCoord);
    actorRenderComposeCoord(playerCoord);
    _gActor161500PlayerFacingCompanionTarget.rot.vy =
        ratan2(companionCoord->coord.t[0] - playerCoord->coord.t[0], companionCoord->coord.t[2] - playerCoord->coord.t[2]) & ACTOR_TRANSFORM_ANGLE_MASK;
}

/// Sets the player state-tick hold byte (zero runs the tick, nonzero holds it).
///
/// Input capture and collision preparation continue while held. The request
/// handover script passes zero to release the hold; all eight bits are retained.
static void _actor161500SetPlayerUpdateHold(u8 holdPlayerUpdate)
{
    gPlayerActorStateTickHeld = holdPlayerUpdate;
}

void actor161500StartCompanionRequestReminder(void)
{
    enum { ACTOR_161500_REQUEST_PLACEMENT = 3 };
    s32 requestState;

    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        requestState = gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE);
        if (requestState == ACTOR_161500_REQUEST_PENDING) {
            if (areaGetCurrentObjectState(ACTOR_161500_REQUEST_PLACEMENT) == requestState) {
                evsStartScript(_gActor161500CompanionRequestPendingPlacementReminderScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(_gActor161500CompanionRequestReminderScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
        }
    }
}

void actor161500RestoreCompanionRequestScene(void)
{
    s32 requestState;

    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        requestState = gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE);
        switch (requestState) {
            case ACTOR_161500_REQUEST_UNSEEN:
                // The skip path establishes the same pending placement as the full scene.
                evsStartScriptWithSkip(_gActor161500CompanionRequestIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor161500CompanionRequestSkipScript);
                gameFlagSetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE, ACTOR_161500_REQUEST_PENDING);
                break;
            case ACTOR_161500_REQUEST_PENDING:
                evsStartScript(_gActor161500CompanionRequestRestoreScript, EVENT_SCRIPT_HUD_KEEP);
                break;
            case ACTOR_161500_REQUEST_COMPLETE:
                break;
        }
    }
}

/// Selects this package's two-entry TaskDesc table for the stride-walk spawn fragment.
///
/// Slot 0 is the body and slot 1 its optional rifle. The identifier is evaluated
/// once when a nonzero first spawn payload requests the child. Keep the table
/// live through spawning and its callbacks and model data live until teardown.
/// Bind only around the fragment inclusion, then undefine it.
#define STRIDE_WALK_TASKS gActor161500SoldierBTasks
#include "../../shared/walk/stride_walk_spawn.inc.c"
#undef STRIDE_WALK_TASKS

#include "../../shared/walk/stride_walk_update.inc.c"

/// Dispatches the soldier's stride walker initialization or frame update.
///
/// Descriptor 0 requires a live Enemy in `spawnArg2.pointer`, a twenty-part
/// TMD body and state 0..1. State 0 allocates the rig and stride-walk work;
/// state 1 composes, lights, moves and animates the model and draws its shadow.
/// State 1 also requires a live player model for the soldier's head aim.
/// Initialization may destroy the enemy and task on allocation failure.
/// The handler array has no bounds check; keep this overlay and clips loaded.
static void _actor161500StrideWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _strideWalkSpawn,
        _strideWalkFrame,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects the stride walker's fixed-shade ground-shadow drawer.
///
/// Bind the static `void(Task*)` function identifier declared in `walker.h`
/// and use the same identifier for `walker_shadow.inc.c`. The frame calls it
/// once after movement, animation and head aim, borrowing its live TMD task
/// and composed root cache. This object-like alias captures no locals;
/// undefine it after the inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/stride_walk_frame.inc.c"

/// Selects the fixed-shade drawer definition used by the stride walker frame.
///
/// Use the same function identifier as the frame binding. The fragment defines
/// `static void(Task*)` at this text position, so keep the identifier alias
/// unparenthesized and undefine it after the inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/walk/paced_walk_tick_anim.inc.c"

#include "../../shared/walk/paced_walk_reset_anim.inc.c"

#include "../../shared/walk/paced_walk_blend_anim.inc.c"

#include "../../shared/walk/stride_walk.inc.c"

#include "../../shared/walk/paced_walk_place.inc.c"

/// Applies a borrowed head-turn command to a live stride walker's work block.
///
/// Retains the command halfword in the signed turn mode: `STRIDE_WALK_TURN_PLAYER`
/// aims toward the player; every other value releases toward the animated pose.
/// The command is read synchronously and never retained. Other message arguments
/// are ignored, and zero acknowledges the command.
static s32 _strideWalkSetTurnMode(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    StrideWalkWork* work = task->work;

    work->turnMode = command->command;
    return 0;
}

#include "../../shared/walk/stride_walk_to.inc.c"
