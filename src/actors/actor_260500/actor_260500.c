#include "actors/actor_260500.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/walk/footstep_walk.h"
#include "../../shared/walk/walker.h"

static void _footstepWalkQuietUpdate(Task* task);
static void _footstepWalkQuietResetAnim(void);
static void _footstepWalkQuietBlendAnim(void);
static void _footstepWalkTickAnim(void);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);

static FootstepWalkQuietWork* _gFootstepWalkWork;

/// Borrowed singleton task of this carrier's quiet footstep walker.
///
/// Published at spawn for synchronous animation updates and model-draw messages.
/// The task owns its model and work. Teardown leaves this alias unchanged;
/// handlers must stop using it before the task is released or the package unloads.
static Task* _gFootstepWalkTask;

static s16 _gFootstepWalkMode;

// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkQuietSpawn(Enemy* enemy, Task* task);

/// Task-state indices of this package's quiet walker.
enum {
    ACTOR_260500_TASK_SPAWN       = 0,
    ACTOR_260500_TASK_FRAME       = 1,
    ACTOR_260500_TASK_STATE_COUNT = 2
};

static TmdSource _gJodieBody2;
static void      _actor260500Task(Task* task);

static s32 _footstepWalkQuietPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _footstepWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawMode, s32 unusedArgument);
static s32 _footstepWalkApplyTurnCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static s32 _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

static void _actor260500SelectConversationCaptions(s32 useConversationFile);

static AnimationSet _gShelter1fHeliportPlayerJodieOpening1Clip;
static AnimationSet _gShelter1fHeliportPlayerJodieOpening3Clip;
static AnimationSet _gShelter1fHeliportPlayerJodieOpening4Clip;
static AnimationSet _gShelter1fHeliportPlayerJodieOpening2Clip;
static AnimationSet _gAyaConverseClip;
extern AnimationSet gAyaIntroduceHerselfClip;
extern AnimationSet gAyaStandClip;
extern AnimationSet gAyaOnMyWayClip;

#include "assets/rooms/shelter/1f_heliport/jodie_clips_1.inc"

/// Blend duration of this package's conversation requests, in normal-rate frames.
enum { ACTOR_260500_CONVERSATION_BLEND_FRAMES = 8 };

/// Retained reset-playback parameters for clip 0, with no established consumer.
///
/// The local clip table's slot 0 is NULL; these parameters are not dispatched.
static const AnimationPlayRequest _gActor260500RetainedClip0Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 0, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts Jodie's local clip 1 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip1Request = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts local walk clip 2 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationWalk2Request = { { .index = 1 }, FOOTSTEP_WALK_ANIM_WALK_2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts local turn clip 3 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationTurnRequest = { { .index = 1 }, FOOTSTEP_WALK_ANIM_TURN, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Returns Jodie to local clip 4, the heliport conversation rest pose.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
/// Clip 4 is also the startup pose; this request ends each conversation or its skip.
static AnimationPlayRequest _gActor260500ConversationRestRequest = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 5 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip5Request = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends to Jodie's local clip 6 in the opening and next three conversations.
///
/// Uses an eight-frame blend; the quiet walker ignores bank and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip6Request = { { .index = 1 }, 6, ANIMATION_BLEND_INTERPOLATE, ACTOR_260500_CONVERSATION_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends to Jodie's local clip 7 after clip 6 during heliport conversations.
///
/// Uses an eight-frame blend; the quiet walker ignores bank and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip7Request = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, ACTOR_260500_CONVERSATION_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 8 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip8Request = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 9 after clip 8 in the opening conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip9Request = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 10 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip10Request = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 11 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip11Request = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts local clip 12 after Jodie returns to her first conversation position.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip12Request = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the quiet walker's local idle clip during the opening conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationIdleRequest = { { .index = 1 }, FOOTSTEP_WALK_ANIM_IDLE, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts local walk clip 14 during the opening heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationWalk14Request = { { .index = 1 }, FOOTSTEP_WALK_ANIM_WALK_14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts local walk clip 15 after clip 14 during the opening conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationWalk15Request = { { .index = 1 }, FOOTSTEP_WALK_ANIM_WALK_15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 16 near the end of the opening conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ConversationClip16Request = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 17 in the second heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500SecondConversationRequest = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 18 in the third heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500ThirdConversationRequest = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 19 in the fourth heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500FourthConversationClip19Request = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Jodie's local clip 20 later in the fourth heliport conversation.
///
/// The quiet walker ignores the bank selector, blend duration and collision fields.
static AnimationPlayRequest _gActor260500FourthConversationClip20Request = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends to Jodie's local clip 21 near the end of the fourth heliport conversation.
///
/// The quiet walker uses eight normal-rate frames and ignores bank and collision.
static const AnimationPlayRequest _gActor260500FourthConversationClip21Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 21, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = ACTOR_260500_CONVERSATION_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts Jodie's local clip 22 for the repeat heliport response.
///
/// The quiet walker ignores the bank, blend duration and collision fields.
static const AnimationPlayRequest _gActor260500RepeatConversationRequest SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 22, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts Jodie's local clip 23 before the opening conversation's final CAP cue.
///
/// The quiet walker ignores the bank, blend duration and collision fields.
static const AnimationPlayRequest _gActor260500ConversationClip23Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 23, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Retained reset-playback parameters for clip 24, with no established consumer.
///
/// Local clip 24 is loaded, but no script or function dispatches these parameters.
static const AnimationPlayRequest _gActor260500RetainedClip24Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 24, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts player extension clip 47 during the opening heliport conversation.
///
/// EVS substitutes the equipped weapon bank and playback drops grid participation.
/// The extension clips and this package must remain loaded through playback.
static const AnimationPlayRequest _gActor260500PlayerClip47Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 47, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts player extension clip 48 during the opening and fourth conversations.
///
/// EVS substitutes the equipped weapon bank; playback drops world-grid participation.
static AnimationPlayRequest _gActor260500PlayerClip48Request = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player extension clip 49 after Jodie's second walk sequence.
///
/// EVS substitutes the equipped weapon bank and playback drops grid participation.
/// The extension clips and this package must remain loaded through playback.
static const AnimationPlayRequest _gActor260500PlayerClip49Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 49, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts player extension clip 50 after clip 47 in the opening conversation.
///
/// EVS substitutes the equipped weapon bank and playback drops grid participation.
/// The extension clips and this package must remain loaded through playback.
static const AnimationPlayRequest _gActor260500PlayerClip50Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 50, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts player extension clip 53 during the opening, second and third conversations.
///
/// EVS substitutes the equipped weapon bank; playback drops world-grid participation.
static AnimationPlayRequest _gActor260500PlayerClip53Request = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player extension clip 54 beside Jodie's local clip 7 in the opening conversation.
///
/// EVS substitutes the equipped weapon bank and playback drops grid participation.
/// The extension clips and this package must remain loaded through playback.
static const AnimationPlayRequest _gActor260500PlayerClip54Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 54, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Blends the player into extension clip 55, the heliport conversation rest pose.
///
/// Uses eight normal-rate frames. EVS substitutes the equipped weapon bank;
/// playback drops world-grid participation. The extension clips must remain loaded.
static AnimationPlayRequest _gActor260500PlayerConversationRestRequest = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, ACTOR_260500_CONVERSATION_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player to extension clip 56 near the end of the fourth conversation.
///
/// Uses eight normal-rate frames. EVS substitutes the equipped weapon bank;
/// playback drops grid participation. Extension clips must remain loaded.
static const AnimationPlayRequest _gActor260500PlayerClip56Request SECTION(".data") = {
    .source = { .index = 1 }, .animationId = 56, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = ACTOR_260500_CONVERSATION_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Player extension clips for heliport conversations, indexed from bank ID 47.
///
/// Ten native pointers cover IDs 47..56; IDs 51 and 52 are NULL and never played.
/// The player retains copied pointers, so this package must stay loaded through
/// scripted playback. The copy's 32-word source span continues beyond this table
/// into its adjacent copy request, base-clip request, two complete actor placements
/// and the XYZ words of the next placement. Only these ten words are clip entries.
#include "assets/rooms/shelter/1f_heliport/jodie_scene_player_bank.inc"

/// Copies the heliport player's extension as 32 raw words beginning at the clip table.
///
/// Sent before each conversation. The count is the extension capacity, including
/// neighboring non-clip records; it is intentionally not the ten-table-entry count.
/// The loaded source span and writable equipped weapon bank must survive dispatch.
/// Only IDs 47..50 and 53..56 in that copied span are subsequently played as clips.
static AnimationBankCopyRequest _gActor260500PlayerAnimationExtensionCopyRequest = {
    _gActor260500PlayerAnimationExtensionSets, ANIMATION_BANK_EXTENSION_CAPACITY
};

/// Restarts player base-bank clip 1 on opening-conversation entry, completion or skip.
///
/// This clip predates the extension copy. EVS substitutes the equipped weapon bank;
/// playback drops world-grid participation until scripted control ends.
static AnimationPlayRequest _gActor260500PlayerBaseClip1Request = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Jodie's standing transform restored on heliport entry and opening completion or skip.
///
/// Scene placement 0 receives XYZ in world units and yaw in 1/4096 turns.
/// The quiet walker reads position and yaw only; pitch and roll remain zero here.
/// Borrowed for synchronous placement while this actor package remains loaded.
static ActorTransform _gActor260500HeliportStandingPlacement = {
    .pos = { 860, 0, 6730, 0 }, .rot = { 0, -2161, 0, 0 }
};

/// Jodie's initial and return position within the opening heliport conversation.
///
/// Scene placement 0 borrows XYZ in world units and yaw in 1/4096 turns.
/// The quiet walker ignores pitch and roll; the package must stay loaded.
static const ActorTransform _gActor260500ConversationPlacement1 SECTION(".data") = {
    .pos = { 860, 0, 6730, 0 }, .rot = { 0, -2161, 0, 0 }
};

/// Jodie's starting position for both walk sequences in the opening conversation.
///
/// Scene placement 0 borrows XYZ in world units and yaw in 1/4096 turns.
/// The quiet walker ignores pitch and roll; the package must stay loaded.
static const ActorTransform _gActor260500ConversationWalkStartPlacement SECTION(".data") = {
    .pos = { 860, 0, 6910, 0 }, .rot = { 0, -2048, 0, 0 }
};

/// Jodie's third conversation position, installed while her model is hidden.
///
/// Scene placement 0 borrows XYZ in world units and yaw in 1/4096 turns.
/// The quiet walker ignores pitch and roll; the package must stay loaded.
static const ActorTransform _gActor260500ConversationPlacement3 SECTION(".data") = {
    .pos = { 860, 0, 6640, 0 }, .rot = { 0, -2161, 0, 0 }
};

/// Player position at opening-conversation entry and Jodie's second walk sequence.
///
/// Borrowed through synchronous dispatch. XYZ are world units; all three
/// angles are in 1/4096 turns. The package must stay loaded until dispatch.
static const ActorTransform _gActor260500PlayerConversationPlacement1 SECTION(".data") = {
    .pos = { 920, 0, 6000, 0 }, .rot = { 0, -113, 0, 0 }
};

/// Player's second position, visited twice in the opening conversation.
///
/// Borrowed through synchronous dispatch. XYZ are world units; all three
/// angles are in 1/4096 turns. The package must stay loaded until dispatch.
static const ActorTransform _gActor260500PlayerConversationPlacement2 SECTION(".data") = {
    .pos = { 1210, 0, 5610, 0 }, .rot = { 0, -227, 0, 0 }
};

/// Player position paired with Jodie's third position in the opening conversation.
///
/// Borrowed through synchronous dispatch. XYZ are world units; all three
/// angles are in 1/4096 turns. The package must stay loaded until dispatch.
static const ActorTransform _gActor260500PlayerConversationPlacement3 SECTION(".data") = {
    .pos = { 1010, 0, 5840, 0 }, .rot = { 0, -113, 0, 0 }
};

/// Player transform restored after the opening heliport conversation or its skip.
///
/// XYZ are world units; all three Euler angles are zero, in 1/4096 turns.
/// Borrowed by synchronous placement while the actor package remains loaded.
static ActorTransform _gActor260500PlayerConversationEndPlacement = {
    .pos = { 860, 0, 6180, 0 }, .rot = { 0, 0, 0, 0 }
};

/// Scene placements, CAP command slots and return view used by heliport conversations.
enum {
    ACTOR_260500_CONVERSATION_JODIE_PLACEMENT   = 0,
    ACTOR_260500_CONVERSATION_SOLDIER_PLACEMENT = 1,
    ACTOR_260500_CAP_OPENING_CONVERSATION       = 1,
    ACTOR_260500_CAP_SECOND_CONVERSATION        = 2,
    ACTOR_260500_CAP_THIRD_CONVERSATION         = 3,
    ACTOR_260500_CAP_FOURTH_CONVERSATION        = 4,
    ACTOR_260500_CAP_REPEAT_CONVERSATION        = 5,
    ACTOR_260500_CONVERSATION_END_VIEW          = 4,
    ACTOR_260500_WEAPON_TARGET_PLAYER           = 0
};

/// Opening heliport conversation, started at talk progress 0 with an input skip target.
///
/// Borrows this package's commands, payloads, clips and opening CAP resources
/// through playback. CAP cues and frame delays sequence player/Jodie movement;
/// completion restores placements, weapons, lighting, view and CAP selection,
/// then ends scripted player control. The soldier is hidden during the scene.
static EvsCommand _gActor260500OpeningConversationScript[] = {
    // Select the opening CAP resources before staging the participants.
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260500SelectConversationCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260500_CAP_OPENING_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerBaseClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Stage the conversation after CAP's first cue, then follow its dialogue cues.
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor260500PlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500PlayerConversationPlacement1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500ConversationPlacement1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_SOLDIER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_260500_WEAPON_TARGET_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500ConversationWalkStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationWalk2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationTurnRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationRestRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500PlayerConversationPlacement2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500ConversationPlacement3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500PlayerConversationPlacement3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500ConversationPlacement1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500PlayerConversationPlacement2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500ConversationPlacement1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationIdleRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500ConversationWalkStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500PlayerConversationPlacement1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationWalk14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationWalk15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip23Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore normal presentation before releasing scripted player control.
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_260500_WEAPON_TARGET_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_260500_CONVERSATION_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500HeliportStandingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationRestRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500PlayerConversationEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerBaseClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_SOLDIER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260500SelectConversationCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip cleanup for the opening conversation, restoring presentation under a fade.
///
/// Restores both participants' final placements, Jodie's rest clip, weapons,
/// ambient lighting, the return view, soldier visibility and default CAP selection.
/// The commands and borrowed payloads must remain loaded until script end.
static EvsCommand _gActor260500OpeningConversationSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_260500_CONVERSATION_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_260500_WEAPON_TARGET_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerBaseClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500HeliportStandingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationRestRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor260500PlayerConversationEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_SOLDIER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260500SelectConversationCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second heliport conversation, selected at talk progress 1 and CAP slot 2.
///
/// Installs player extension clips, sequences local clips 6/7/17 with player
/// clips 55/53, and restores Jodie's rest clip before ending player scripting.
/// Commands, payloads, CAP resources and clips are borrowed through playback.
static EvsCommand _gActor260500SecondConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor260500PlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260500_CAP_SECOND_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500SecondConversationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationRestRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third heliport conversation, selected at talk progress 2 and CAP slot 3.
///
/// Installs player extension clips, sequences local clips 6/7/18 with player
/// clips 55/53, and restores Jodie's rest clip before ending player scripting.
/// Commands, payloads, CAP resources and clips are borrowed through playback.
static EvsCommand _gActor260500ThirdConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor260500PlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260500_CAP_THIRD_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ThirdConversationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationRestRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Fourth heliport conversation, selected at talk progress 3 and CAP slot 4.
///
/// Installs player extension clips, sequences local clips 6/7/19/20/21 with
/// player clips 55/48/56, and restores Jodie's rest clip before releasing control.
/// Commands, payloads, CAP resources and clips are borrowed through playback.
static EvsCommand _gActor260500FourthConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor260500PlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260500_CAP_FOURTH_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500FourthConversationClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500FourthConversationClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500FourthConversationClip21Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationRestRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeat heliport response, selected at talk progress 4 and CAP slot 5.
///
/// Installs player extension clips, plays local clip 22 with player clip 55,
/// then restores Jodie's rest clip and ends scripted player control.
/// Commands, payloads, CAP resources and clips are borrowed through playback.
static EvsCommand _gActor260500RepeatConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor260500PlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260500_CAP_REPEAT_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260500PlayerConversationRestRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500RepeatConversationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_260500_CONVERSATION_JODIE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor260500ConversationRestRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/rooms/shelter/1f_heliport/jodie_clips_2.inc"

#include "assets/characters/jodie/body_2.inc"

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

/// Synchronous messages of this carrier's quiet footstep walker.
///
/// Payloads are respectively a borrowed AnimationPlayRequest, integer draw mode,
/// borrowed ActorTransform, borrowed ActorCommand and borrowed VECTOR plus walk
/// mode. Play/draw/turn-command handlers use the published singleton. The final
/// marker makes unsupported IDs return zero. The task borrows this table and
/// callbacks for its lifetime; neither may outlive the loaded package.
static TaskMessageEntry _gFootstepWalkMsgTable[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkQuietPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _footstepWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _footstepWalkApplyTurnCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor260500JodieTask = {
    .header   = { .fields = { TASK_BODY_TMD, 192 } },
    .callback = _actor260500Task,
    .data     = { .model = &_gJodieBody2 },
};

/// Native clip table retained by actor 260500's nineteen-part quiet walker.
///
/// IDs 0..35 index these 36 pointers; 0, 25, 34 and 35 are absent. Playback
/// requires a non-NULL entry with tracks 1..18. Spawn requests clip 4 and the
/// package's scripts request clips 1..23; the remaining populated entries are
/// retained even though those scripts do not select them. Table and clips are
/// borrowed for playback and must remain loaded through the walker's lifetime.
#include "assets/rooms/shelter/1f_heliport/jodie_bank.inc"

/// Borrowed pointer to the quiet walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkQuietWork* _gFootstepWalkWork = NULL;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

/// Selects the opening conversation's CAP file and texture page, or restores defaults.
///
/// Event scripts pass nonzero on entry and zero after completion or skip.
/// Uses already loaded data resource ordinal 2 and texture origin (832, 0)
/// in VRAM pixels. Resources must remain live through playback. A missing
/// resource leaves the cleared file pointer NULL; zero resets CAP selection
/// and restores the bundle's default file and texture origin.
static void _actor260500SelectConversationCaptions(s32 useConversationFile)
{
    enum {
        ACTOR_260500_CONVERSATION_CAP_FILE_ORDINAL = 2,
        ACTOR_260500_CONVERSATION_TEXTURE_VRAM_X   = 832,
        ACTOR_260500_CONVERSATION_TEXTURE_VRAM_Y   = 0
    };

    if (useConversationFile != 0) {
        gCapFile = NULL;
        capSelectLoadedFile(ACTOR_260500_CONVERSATION_CAP_FILE_ORDINAL);
        capSetTexturePage(ACTOR_260500_CONVERSATION_TEXTURE_VRAM_X, ACTOR_260500_CONVERSATION_TEXTURE_VRAM_Y);
        return;
    }
    capReset();
}

void actor260500RestoreHeliportPlacement(void)
{
    enum { ACTOR_260500_HELIPORT_PLACEMENT_INDEX = 0 };
    Task* walkerTask;

    walkerTask = sceneFindPlacedActor(ACTOR_260500_HELIPORT_PLACEMENT_INDEX);
    if (walkerTask != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(walkerTask, ACTOR_MESSAGE_PLACE, &_gActor260500HeliportStandingPlacement, 0);
    }
}

void actor260500StartHeliportConversation(void)
{
    enum {
        ACTOR_260500_TALK_INTRO  = 0,
        ACTOR_260500_TALK_SECOND = 1,
        ACTOR_260500_TALK_THIRD  = 2,
        ACTOR_260500_TALK_FOURTH = 3,
        ACTOR_260500_TALK_REPEAT = 4
    };

    switch (gameFlagGetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS)) {
        case ACTOR_260500_TALK_INTRO:
            evsStartScriptWithSkip(_gActor260500OpeningConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor260500OpeningConversationSkipScript);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_SECOND);
            break;
        case ACTOR_260500_TALK_SECOND:
            evsStartScript(_gActor260500SecondConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_THIRD);
            break;
        case ACTOR_260500_TALK_THIRD:
            evsStartScript(_gActor260500ThirdConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_FOURTH);
            break;
        case ACTOR_260500_TALK_FOURTH:
            evsStartScript(_gActor260500FourthConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_REPEAT);
            break;
        case ACTOR_260500_TALK_REPEAT:
            evsStartScript(_gActor260500RepeatConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

/// Binds the quiet walker's playback storage and requests this package's startup clip.
///
/// The published `_gFootstepWalkWork` must be a live, writable task-owned block.
/// `model` must be a `tmdCreateModel` allocation with nineteen part coordinates.
/// The context borrows that allocation's coordinate tail, the work block's slots
/// and encoded-pose buffer, and the package's native clip table. All borrowed
/// storage and clip data must stay live and unmoved throughout playback.
///
/// Requests clip 4 in `ACTOR_ENEMY_ANIM_RESET`; the caller must process that
/// request before ticking parts 1 through 18. Binding neither initializes slots
/// nor clears poses or writes model coordinates. Remaining travel and turn
/// updates start at zero.
static inline void _actor260500PrepareWalkerAnimation(TmdObject* model)
{
    enum { ACTOR_260500_STARTUP_ANIM_ID = 4 };

    animationInitContext(&_gFootstepWalkWork->rig.anim, _gActor260500AnimationSets, model,
                         _gFootstepWalkWork->rig.poses, _gFootstepWalkWork->rig.slots);
    _gFootstepWalkWork->st.animId  = ACTOR_260500_STARTUP_ANIM_ID;
    _gFootstepWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    _gFootstepWalkWork->st.travel  = 0;
    _gFootstepWalkWork->turnFrames = 0;
}

/// Initializes the quiet walker's task-owned work, lighting and animation in state 0.
///
/// `enemy` is the live allocation in `task->spawnArg2.pointer`; the task must
/// own a nineteen-part model and have no work allocation yet. Its zeroed
/// `FootstepWalkQuietWork` is published for singleton handlers. The model
/// borrows its matrices and the rig borrows the loaded clips and model storage.
/// The view, primary heap and lighting query's scratch/GTE state must be ready.
///
/// Starts clip 4 without a pose tick, installs message and exit callbacks and
/// enters frame state. Allocation failure destroys the enemy and starts task
/// teardown; neither argument may be used afterwards. Exit releases the work
/// without clearing the published pointers, which must then remain unused.
static void _footstepWalkQuietSpawn(Enemy* enemy, Task* task)
{
    enum {
        FOOTSTEP_WALK_OT_ENTRY_OFFSET       = 1,
        FOOTSTEP_WALK_LIGHT_SAMPLE_Y_OFFSET = -800
    };

    VECTOR3                lightSamplePosition;
    GfxCoord*              rootCoord;
    TmdObject*             model;
    FootstepWalkQuietWork* allocatedWork;

    model              = task->extra.tmd;
    rootCoord          = model->coords;
    allocatedWork      = memCalloc(sizeof(*allocatedWork), false);
    _gFootstepWalkWork = allocatedWork;
    task->work         = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The task owns the allocation; the model borrows its lighting matrices.
    task->exitCallback               = _footstepWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = false;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = FOOTSTEP_WALK_OT_ENTRY_OFFSET;
    model->lightMtx                  = &_gFootstepWalkWork->light;
    model->colorMtx                  = &_gFootstepWalkWork->color;

    // Sample the cached root position with the actor's vertical light offset.
    lightSamplePosition.vx = rootCoord->workm.t[0];
    lightSamplePosition.vy = rootCoord->workm.t[1] + FOOTSTEP_WALK_LIGHT_SAMPLE_Y_OFFSET;
    _gFootstepWalkTask     = task;
    lightSamplePosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightSamplePosition, 0, ARRAY_SIZE(model->colorMtx->m[0]));

    // Reset the driven slots before entering the ordinary frame state.
    _actor260500PrepareWalkerAnimation(model);
    task->msgTable = _gFootstepWalkMsgTable;
    _footstepWalkQuietUpdate(task);
    task->state++;
}

#include "../../shared/walk/footstep_walk_quiet_update.inc.c"

/// Dispatches actor 260500's quiet-walker spawn or ordinary model frame.
///
/// The descriptor supplies a nineteen-part model and a live enemy in
/// `spawnArg2.pointer`. State must be `ACTOR_260500_TASK_SPAWN` or
/// `ACTOR_260500_TASK_FRAME`; indexing is unchecked. Publishes the task's
/// current work before dispatch, including its initially NULL work at spawn.
static void _actor260500Task(Task* task)
{
    void (*stateHandlers[ACTOR_260500_TASK_STATE_COUNT])(Enemy*, Task*) = {
        _footstepWalkQuietSpawn,
        _actorRenderWalkerFrame,
    };

    _gFootstepWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _footstepWalkQuietUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy and starts default task/work/model teardown.
///
/// `task` must be live with its enemy in `spawnArg2.pointer`. Published task
/// and work aliases are left unchanged and must not be used afterwards.
static void _footstepWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walk/footstep_walk_tick_anim.inc.c"

#include "../../shared/walk/footstep_walk_quiet.inc.c"

/// Starts the published quiet walker's requested animation synchronously.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; receiver, ID and second payload
/// are ignored. The work and published actor task must be live. Borrows the
/// request through dispatch, selecting this package's native clip table;
/// bank and collision options are ignored. Nonzero blend uses whole-frame
/// `blendFrames` (0..2047), narrowed to s16; reset leaves the duration unchanged.
///
/// Returns -1 without writes for IDs 36 and above, otherwise zero. The upper
/// bound alone admits negative IDs and NULL table entries; callers must select
/// a loaded clip with tracks 1..18 and keep the clip storage live for playback.
static s32 _footstepWalkQuietPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { FOOTSTEP_WALK_CLIP_ID_LIMIT = ARRAY_SIZE(_gActor260500AnimationSets) };

    if (request->animationId < FOOTSTEP_WALK_CLIP_ID_LIMIT) {
        _gFootstepWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gFootstepWalkBlendFrames    = request->blendFrames;
        } else {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gFootstepWalkWork->st.field_6 = 0;
        _footstepWalkQuietUpdate(_gFootstepWalkTask);
        return 0;
    }
    return -1;
}

/// Replaces the published walker's model flags from a draw-mode bitmask.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` for a live published task and model;
/// ignores receiver, ID and second payload. Bit 0 permits active drawing,
/// bit 1 suppresses automatic primitive-buffer allocation, and other bits are
/// ignored. All prior model flags are discarded. Returns zero.
static s32 _footstepWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawMode, s32 unusedArgument)
{
    enum {
        FOOTSTEP_WALK_DRAW_VISIBLE          = 1,
        FOOTSTEP_WALK_DRAW_SKIP_AUTO_BUFFER = 2
    };
    TmdObject* model;

    model = _gFootstepWalkTask->extra.tmd;
    if (drawMode & FOOTSTEP_WALK_DRAW_VISIBLE) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawMode & FOOTSTEP_WALK_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/footstep_walk_place.inc.c"

/// Schedules twenty turning updates for quiet-walker command 0.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` through the live published work block.
/// Borrows the command for this call; ignores its context, receiver, ID and
/// second payload. Other commands do nothing. The count is consumed only while
/// the turn clip is playing, at 51/4096 turns per update; no clip is selected
/// here. Repeated requests restart the count. Returns zero.
static s32 _footstepWalkApplyTurnCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum {
        FOOTSTEP_WALK_COMMAND_TURN = 0,
        FOOTSTEP_WALK_TURN_UPDATES = 20
    };

    if (command->command == FOOTSTEP_WALK_COMMAND_TURN) {
        _gFootstepWalkWork->turnFrames = FOOTSTEP_WALK_TURN_UPDATES;
    }
    return 0;
}

/// Faces a planar target and schedules whole moving updates to approach it.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` for a live quiet-walker task and model.
/// Borrows `target` in the root's parent space; only X/Z are read. Mode must
/// be `FOOTSTEP_WALK_MODE_*` (0 forward 60, 1 backward 15, 2 forward 25
/// units per update). Backward mode faces away from the target. The mode and
/// heading narrow to s16; angles use 4096 units per turn.
///
/// Stores floor(planar distance / step distance) in s16 `st.travel`, without
/// selecting a walk clip or retaining the target. Coordinate differences and
/// their squared sum must fit s32, and callers must keep the count in 0..32767.
/// Mode bounds are unchecked; an unsupported narrowed mode divides by zero.
/// Returns zero; message ID is ignored.
static s32 _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode)
{
    GfxCoord*              rootCoord;
    FootstepWalkQuietWork* work;
    s32                    stepDistance;
    s32                    deltaX;
    s32                    deltaZ;
    s32                    planarDistance;
    s32                    targetYaw;

    stepDistance       = 0;
    rootCoord          = task->extra.tmd->coords;
    work               = task->work;
    _gFootstepWalkMode = mode;
    deltaX             = target->vx - rootCoord->coord.t[0];
    deltaZ             = target->vz - rootCoord->coord.t[2];
    targetYaw          = ratan2(deltaX, deltaZ);
    work->st.yaw       = targetYaw;
    if (_gFootstepWalkMode == FOOTSTEP_WALK_MODE_BACKWARD) {
        work->st.yaw = targetYaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    }
    gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    // Travel counts moving updates; a fractional update is discarded.
    planarDistance = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ);
    switch (_gFootstepWalkMode) {
        case FOOTSTEP_WALK_MODE_FORWARD:
            stepDistance = FOOTSTEP_WALK_FORWARD_DISTANCE;
            break;
        case FOOTSTEP_WALK_MODE_BACKWARD:
            stepDistance = FOOTSTEP_WALK_BACKWARD_DISTANCE;
            break;
        case FOOTSTEP_WALK_MODE_SLOW_FORWARD:
            stepDistance = FOOTSTEP_WALK_SLOW_FORWARD_DISTANCE;
            break;
    }
    work->st.travel = planarDistance / stepDistance;
    return 0;
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW
