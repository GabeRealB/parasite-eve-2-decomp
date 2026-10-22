#include "actors/actor_260400.h"

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
#include "main/task.h"
#include "main/tmd_types.h"
// The scripted walk's update and walk-to handler run on this package's block.
#define SCRIPTED_WALK_WORK_T _Actor260400Work
#include "../../shared/walk/scripted_walk.h"
#include "../../shared/walk/walker.h"

/// Work block of the package's scripted walker, the wounded Rupert Broderick,
/// allocated zeroed by the walker's spawn state and kept both at `Task::work`
/// and in `_gScriptedWalkWork`.
///
/// The scripted walk library's fragments are compiled against it through
/// `SCRIPTED_WALK_WORK_T` and use its leading members: the matrices, the rig,
/// the enemy state and the turn counter. What follows is the walker's revolver: a task of
/// its own that draws the one-part Mongoose model and hangs that model's
/// coordinate off part 8 of the walker's rig, so the revolver follows that
/// part. The spawn state starts it and the walker's exit callback kills it.
/// A spawn that fails leaves `mongoose` NULL, which the message handlers do
/// not check for.
///
/// The revolver is drawn only between two actor commands of the package's
/// scripts, and not at all once the player holds a Mongoose.
typedef struct {
    MATRIX          light;         // Light-direction matrix lent to the model object
    MATRIX          color;         // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;           // Playback storage of the twenty-part body model
    ActorEnemyState st;            // Animation request, heading last given the root and frames of walk left
    s16             turnFrames;    // Frames the update still turns the model for while the turn clip plays; command 0 starts 20
    Task*           mongoose;      // Task of the revolver model, started from spawn-table entry 1
    u8              mongooseShown; // Nonzero while the revolver is drawn with the walker: set by command 1 unless the player holds a Mongoose, cleared by command 2. While zero the model-draw message keeps the revolver hidden
} _Actor260400Work;
STATIC_ASSERT_SIZEOF(_Actor260400Work, 0x4F8);

/// Borrowed work block of the scripted walker and its revolver attachment.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// Animation and message handlers require it to remain live; task teardown
/// releases it without clearing this pointer.
static _Actor260400Work* _gScriptedWalkWork;

/// Borrowed singleton task of the wounded Rupert Broderick walker.
///
/// Published after work allocation succeeds and before the Mongoose is spawned.
/// Attachment, animation and draw handlers require this task and its model to
/// remain live. Teardown leaves the pointer unchanged; it owns no allocation.
static Task* _gActor260400ScriptedWalkerTask;

/// Approach mode of the scripted walker, stored as `SCRIPTED_WALK_MODE_*`.
///
/// The walk-to message narrows its argument to this signed halfword. Its value
/// selects a 60-unit forward, 15-unit backward or 25-unit forward step until
/// the next approach message.
static s16 _gScriptedWalkMode;

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor260400ExitScriptedWalker(Task* task);

static TmdSource _gRupertHurtMongoose;
static TmdSource _gRupertHurtBody;
static void      _actor260400ScriptedWalkerTask(Task* task);
static void      _actor260400MongooseTask(Task* task);

static s32 _actor260400PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor260400SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32 _actor260400ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);

static void _actor260400SelectConversationCaptions(s32 useConversationFile);

static AnimationSet _gShelter1fHeliportPlayerRupertMongooseClip;
static AnimationSet _gAyaLetHerGoClip;
static AnimationSet _gShelter1fHeliportPlayerRupertOpeningTalkClip;
static AnimationSet _gAyaConverseClip;
static AnimationSet _gShelter1fHeliportPlayerRupertFourthTalkClip;
extern AnimationSet gAyaStandClip;
extern AnimationSet gAyaOnMyWayClip;

#include "assets/rooms/shelter/1f_heliport/rupert_clips_1.inc"

/// Player-pose transition duration in whole normal-rate animation frames.
enum { ACTOR_260400_PLAYER_BLEND_FRAMES = 8 };

/// Execution priority shared by the wounded Rupert body and its revolver.
enum { ACTOR_260400_TASK_PRIORITY = 192 };

/// Commands granting or revoking the scripted walker's Mongoose draw permission.
enum { ACTOR_260400_COMMAND_ENABLE_MONGOOSE  = 1,
       ACTOR_260400_COMMAND_DISABLE_MONGOOSE = 2 };

/// Retained bank-index-1, clip-0 reset request with world collision disabled.
///
/// No code or script references this record; its intended recipient and role
/// are unproven. The complete five-word record remains in the package data.
AnimationPlayRequest D_actor_260400_8014C4C4 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Returns the walker to clip 1 after a conversation or skip.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerRestAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 2 during the opening conversation.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip2AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 3 during the opening Mongoose sequence.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip3AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 4 after the opening conversation returns to its first mark.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip4AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 5 at the start of a dialogue exchange.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip5AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 6 after a dialogue cue or timed pause.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip6AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 7 during the opening conversation.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip7AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 8 after the next opening-conversation cue.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip8AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 9 in the third conversation.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip9AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 10 in the fourth conversation.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip10AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 11 in the repeat conversation.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip11AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts walker clip 12 before the opening conversation shows the Mongoose.
///
/// Sent to placed actor 0. Only the clip and blend words affect this receiver;
/// the bank selector, reset duration and collision word are ignored.
static AnimationPlayRequest _gActor260400WalkerClip12AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained bank-index-1, clip-47 reset request with world collision disabled.
///
/// No code or script references this record; its intended recipient and role
/// are unproven. A separate request supplies clip 47 to the opening script.
AnimationPlayRequest D_actor_260400_8014C5C8 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player clip 47 during the opening Mongoose sequence.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerClip47AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts player clip 48 after the opening conversation hides the walker.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerClip48AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts player clip 49 during the opening conversation.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerClip49AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts player clip 53 in the opening and third conversations.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerClip53AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends into player clip 54 in the fourth conversation.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerClip54AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 54,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_260400_PLAYER_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends into player clip 55 between conversation gestures.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerClip55AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_260400_PLAYER_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends into player clip 56 in the conversation after the gifts are collected.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerClip56AnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = 56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_260400_PLAYER_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player conversation clips installed at bank indices 47..56.
///
/// Entries 3..5 (indices 50..52) are NULL; scripts play only 47..49 and 53..56.
/// The descriptors and clip data are borrowed until player playback ends.
/// `_gActor260400PlayerAnimationBankCopyRequest` reads 32 words starting here,
/// including the adjacent payload records; preserve their order and storage.
#include "assets/rooms/shelter/1f_heliport/rupert_scene_player_bank.inc"

/// Installs the conversation clips and adjacent words in the player's bank extension.
///
/// Copies 32 words, not the ten clip pointers: the remaining 22 are this request,
/// both base-clip requests, the rest placement and the intro placement's position.
/// The final two words of the intro placement are outside the copied span.
/// Only indices 47..49 and 53..56 are played from this extension. The request
/// and readable source span must remain loaded through synchronous dispatch.
static AnimationBankCopyRequest _gActor260400PlayerAnimationBankCopyRequest = {
    .sets  = _gActor260400PlayerAnimationSets,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Restarts the player bank's base clip 1 on conversation entry, completion or skip.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerBaseClipResetRequest = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends into the player bank's base clip 1 for the reminder and repeat conversations.
///
/// The event interpreter copies this request and replaces its bank selector
/// with the equipped weapon's bank. Playback disables world-grid participation;
/// the request is borrowed through dispatch and its clip data through playback.
static AnimationPlayRequest _gActor260400PlayerBaseClipBlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_260400_PLAYER_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Standing mark restored after the opening conversation, a skip or room entry.
///
/// Placed actor 0 consumes XYZ and yaw in its view parent's world frame.
/// Coordinates are whole world units; angles use 4096 units per turn.
/// Borrowed only through synchronous placement dispatch.
static ActorTransform _gActor260400WalkerRestPlacement = {
    .pos = { 860, 0, 6730, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// First walker mark used on entry and returns during the opening conversation.
///
/// Placed actor 0 consumes XYZ and yaw in its view parent's world frame.
/// Coordinates are whole world units; angles use 4096 units per turn.
/// Borrowed only through synchronous placement dispatch.
static ActorTransform _gActor260400WalkerIntroPlacement = {
    .pos = { 860, 0, 6730, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Second walker mark in the opening conversation, before player clip 53.
///
/// Placed actor 0 consumes XYZ and yaw in its view parent's world frame.
/// Coordinates are whole world units; angles use 4096 units per turn.
/// Borrowed only through synchronous placement dispatch.
static ActorTransform _gActor260400WalkerIntroMark2 = {
    .pos = { 860, 0, 6910, 0 },
    .rot = { 0, -2161, 0, 0 },
};

/// Third walker mark in the opening Mongoose sequence, before player clip 47.
///
/// Placed actor 0 consumes XYZ and yaw in its view parent's world frame.
/// Coordinates are whole world units; angles use 4096 units per turn.
/// Borrowed only through synchronous placement dispatch.
static ActorTransform _gActor260400WalkerIntroMark3 = {
    .pos = { 860, 0, 6640, 0 },
    .rot = { 0, -2275, 0, 0 },
};

/// Player mark used on opening-conversation entry and before player clip 53.
///
/// The player consumes XYZ and all Euler angles in the world frame.
/// Coordinates are whole world units; angles use 4096 units per turn.
/// Borrowed only through synchronous placement dispatch.
static ActorTransform _gActor260400PlayerIntroPlacement = {
    .pos = { 920, 0, 6000, 0 },
    .rot = { 0, -113, 0, 0 },
};

/// Player mark before clip 47 in the opening Mongoose presentation.
///
/// XYZ are whole world-coordinate units and Euler angles use 4096 per turn.
/// The player borrows the complete transform through synchronous placement.
static ActorTransform _gActor260400PlayerIntroMark2 = { { 1010, 0, 5840, 0 }, { 0, -113, 0, 0 } };

/// Player mark before clips 48 and 49 in the opening conversation.
///
/// XYZ are whole world-coordinate units and Euler angles use 4096 per turn.
/// The player borrows the complete transform through synchronous placement.
static ActorTransform _gActor260400PlayerIntroMark3 = { { 1210, 0, 5610, 0 }, { 0, -227, 0, 0 } };

/// Player mark restored when the opening conversation completes or is skipped.
///
/// The player consumes XYZ and all Euler angles in the world frame.
/// Coordinates are whole world units; angles use 4096 units per turn.
/// Borrowed only through synchronous placement dispatch.
static ActorTransform _gActor260400PlayerRestPlacement = {
    .pos = { 860, 0, 6180, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Permits the Mongoose presentation if its inventory-limit test returns zero.
///
/// Command 1 enables draw permission and clears the revolver's draw flags;
/// an already-owned Mongoose leaves both unchanged. The receiver ignores the
/// stored heliport context and borrows the command through dispatch.
static ActorCommand _gActor260400EnableMongooseCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT } },
    .command = ACTOR_260400_COMMAND_ENABLE_MONGOOSE,
};

/// Hides the Mongoose after its opening-conversation sequence or a skip.
///
/// Command 2 clears the draw permission and sets both skip-draw and
/// skip-auto-buffer flags. The stored Shelter heliport context is ignored by
/// this receiver; the event script selects placed actor 0. Borrowed through dispatch.
static ActorCommand _gActor260400HideMongooseCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT } },
    .command = ACTOR_260400_COMMAND_DISABLE_MONGOOSE,
};

/// CAP command slots for the heliport conversations and the restored default file.
enum {
    ACTOR_260400_CAP_INTRO               = 1,
    ACTOR_260400_CAP_DEFAULT             = 1,
    ACTOR_260400_CAP_GIFT_REMINDER       = 26,
    ACTOR_260400_CAP_AFTER_GIFTS         = 6,
    ACTOR_260400_CAP_THIRD_CONVERSATION  = 7,
    ACTOR_260400_CAP_FOURTH_CONVERSATION = 8,
    ACTOR_260400_CAP_REPEAT_CONVERSATION = 9,
};

/// Opening heliport conversation and staged Mongoose presentation.
///
/// Runs once at talk progress zero with the paired skip cleanup. Selects the
/// conversation CAP file, borrows walker/player poses and animation requests,
/// then restores presentation and releases scripted player control.
/// All 82 commands, callbacks and referenced resources must stay loaded.
static EvsCommand _gActor260400HeliportIntroScript[82] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260400SelectConversationCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260400_CAP_INTRO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerBaseClipResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor260400PlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400PlayerIntroPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400WalkerIntroPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip2AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400PlayerIntroPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400WalkerIntroMark2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip53AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip12AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor260400EnableMongooseCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400PlayerIntroMark2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400WalkerIntroMark3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip3AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip47AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor260400HideMongooseCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400PlayerIntroMark3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip48AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 65 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400WalkerIntroPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip4AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip5AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip6AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400PlayerIntroMark3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip49AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 72 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400WalkerIntroPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip7AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip8AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip53AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400WalkerRestPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400PlayerRestPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerBaseClipResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260400SelectConversationCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip cleanup that restores the opening heliport conversation's resting scene.
///
/// Fades around restoring actors, weapons, view 4 and default CAP selection,
/// then releases scripted player control. Borrows 20 commands and their payloads.
static EvsCommand _gActor260400HeliportIntroSkipScript[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerBaseClipResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400WalkerRestPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor260400PlayerRestPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor260400HideMongooseCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260400SelectConversationCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260400_CAP_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Heliport reminder while Maeda ammunition or the Mongoose is still available.
///
/// CAP command 26 accompanies walker clips 5 and 6 before rest and player
/// release. Talk progress stays at the gift wait. Borrows ten commands/payloads.
static EvsCommand _gActor260400HeliportGiftReminderScript[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor260400PlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260400_CAP_GIFT_REMINDER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerBaseClipBlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip5AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip6AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Heliport conversation after both gifts cease being available.
///
/// CAP command 6 accompanies player clips 55/56 and walker clips 5/6 before
/// rest and player release. Borrows thirteen commands and their payloads.
static EvsCommand _gActor260400HeliportAfterGiftsScript[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor260400PlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260400_CAP_AFTER_GIFTS }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip5AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip56AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip6AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third progressing heliport conversation, following the after-gifts talk.
///
/// CAP command 7 accompanies walker clip 9 and player clip 53 before rest
/// and player release. Borrows fifteen commands and their payloads.
static EvsCommand _gActor260400HeliportThirdConversationScript[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor260400PlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260400_CAP_THIRD_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip5AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip6AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip9AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip53AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Fourth progressing heliport conversation, before the repeating talk.
///
/// CAP command 8 accompanies walker clip 10 and player clip 54 before rest
/// and player release. Borrows fifteen commands and their payloads.
static EvsCommand _gActor260400HeliportFourthConversationScript[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor260400PlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260400_CAP_FOURTH_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip55AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip5AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip6AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip10AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerClip54AnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeatable heliport conversation at the final talk-progress value.
///
/// CAP command 9 accompanies walker clip 11 before rest and player release.
/// Talk progress remains unchanged. Borrows nine commands and their payloads.
static EvsCommand _gActor260400HeliportRepeatConversationScript[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor260400PlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_260400_CAP_REPEAT_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor260400PlayerBaseClipBlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerClip11AnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor260400WalkerRestAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/rooms/shelter/1f_heliport/rupert_clips_2.inc"

#include "assets/characters/rupert/hurt_mongoose.inc"

#include "assets/characters/rupert/hurt_body.inc"

/// Latched duration of the next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword; walk completion replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`.
/// Plain resets leave it intact. Zero requests no transition time; 0..2047
/// keeps the playback timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

/// Wounded Rupert's animation, draw, place, command and walk-to message dispatch.
///
/// Five callbacks precede the end marker. The singleton walker borrows this
/// table and its callback code while the actor package is loaded.
static TaskMessageEntry _gActor260400ScriptedWalkerMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor260400PlayScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor260400SetScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor260400ApplyScriptedWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _scriptedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor260400RupertBroderickTasks[2] = {
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_260400_TASK_PRIORITY } },
        .callback = _actor260400ScriptedWalkerTask,
        .data     = { .model = &_gRupertHurtBody },
    },
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_260400_TASK_PRIORITY } },
        .callback = _actor260400MongooseTask,
        .data     = { .model = &_gRupertHurtMongoose },
    },
};

/// Wounded Rupert's sixteen-entry body-animation bank.
///
/// Clips 1..12 name local sets; entries 0 and 13..15 are NULL. The scripted
/// walker borrows the bank and its sets for its task's lifetime. Its signed
/// request limit checks the full capacity, without excluding negative keys
/// or NULL holes; callers must choose a loaded clip.
#include "assets/rooms/shelter/1f_heliport/rupert_bank.inc"

static void _actor260400SpawnScriptedWalker(Enemy* enemy, Task* task);

void actor260400StartHeliportConversation(void)
{
    enum {
        ACTOR_260400_TALK_INTRO            = 0,
        ACTOR_260400_TALK_AWAIT_GIFTS      = 1,
        ACTOR_260400_TALK_AFTER_GIFTS      = 2,
        ACTOR_260400_TALK_FOURTH           = 3,
        ACTOR_260400_TALK_REPEAT           = 4,
        ACTOR_260400_MAEDA_AMMO_OBJECT     = 4,
        ACTOR_260400_MONGOOSE_OBJECT       = 5,
        ACTOR_260400_GIFT_OBJECT_AVAILABLE = 1,
    };

    switch (gameFlagGetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS)) {
        case ACTOR_260400_TALK_INTRO:
            evsStartScriptWithSkip(_gActor260400HeliportIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor260400HeliportIntroSkipScript);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_AWAIT_GIFTS);
            break;
        case ACTOR_260400_TALK_AWAIT_GIFTS:
            // Repeat the reminder until neither gift has its available state.
            if ((areaGetCurrentObjectState(ACTOR_260400_MAEDA_AMMO_OBJECT) == ACTOR_260400_GIFT_OBJECT_AVAILABLE) ||
                (areaGetCurrentObjectState(ACTOR_260400_MONGOOSE_OBJECT) == ACTOR_260400_GIFT_OBJECT_AVAILABLE)) {
                evsStartScript(_gActor260400HeliportGiftReminderScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(_gActor260400HeliportAfterGiftsScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_AFTER_GIFTS);
            }
            break;
        case ACTOR_260400_TALK_AFTER_GIFTS:
            evsStartScript(_gActor260400HeliportThirdConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_FOURTH);
            break;
        case ACTOR_260400_TALK_FOURTH:
            evsStartScript(_gActor260400HeliportFourthConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_REPEAT);
            break;
        case ACTOR_260400_TALK_REPEAT:
            evsStartScript(_gActor260400HeliportRepeatConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

/// Selects the opening conversation's CAP file and texture page, or restores defaults.
///
/// Event scripts pass nonzero on entry and zero after completion or skip.
/// The selected file is data resource ordinal 1 of the loaded bundle, with
/// its text texture at (832, 0) VRAM pixels. Resources must already be loaded
/// and remain live through playback. A missing resource leaves the cleared
/// current-file pointer NULL; zero restores the default file and playback state.
static void _actor260400SelectConversationCaptions(s32 useConversationFile)
{
    enum {
        ACTOR_260400_CONVERSATION_CAP_FILE_ORDINAL = 1,
        ACTOR_260400_CONVERSATION_TEXTURE_VRAM_X   = 832,
        ACTOR_260400_CONVERSATION_TEXTURE_VRAM_Y   = 0,
    };

    if (useConversationFile != 0) {
        gCapFile = NULL;
        capSelectLoadedFile(ACTOR_260400_CONVERSATION_CAP_FILE_ORDINAL);
        capSetTexturePage(ACTOR_260400_CONVERSATION_TEXTURE_VRAM_X, ACTOR_260400_CONVERSATION_TEXTURE_VRAM_Y);
        return;
    }
    capReset();
}

void actor260400RestoreHeliportPlacement(void)
{
    enum { ACTOR_260400_HELIPORT_PLACEMENT_INDEX = 0 };
    Task* walkerTask;

    walkerTask = sceneFindPlacedActor(ACTOR_260400_HELIPORT_PLACEMENT_INDEX);
    if (walkerTask != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(walkerTask, ACTOR_MESSAGE_PLACE, &_gActor260400WalkerRestPlacement, 0);
    }
}

/// Initializes wounded Rupert Broderick's scripted walker and Mongoose attachment.
///
/// State 0 owns zeroed work until teardown and publishes borrowed work/task
/// pointers for the package's singleton handlers. Allocation failure destroys
/// the enemy. The view-parented twenty-part model borrows work-owned matrices
/// and animation storage; lighting samples 800 world units above its cached root.
/// Starts clip 1 and optionally attaches the Mongoose at part 8 with placement
/// textures, then applies the first animation update before advancing state.
/// Failed attachment spawning leaves a NULL handle used unchecked by later
/// handlers. Teardown does not clear the published pointers.
static void _actor260400SpawnScriptedWalker(Enemy* enemy, Task* task)
{
    enum { ACTOR_260400_MONGOOSE_TASK        = 1,
           ACTOR_260400_MONGOOSE_ATTACH_PART = 8,
           ACTOR_260400_INITIAL_CLIP         = 1,
           ACTOR_260400_LIGHT_SAMPLE_HEIGHT  = 800,
           ACTOR_260400_LIGHT_COUNT          = 3,
           ACTOR_260400_BODY_OT_OFFSET       = 1 };

    VECTOR            lightPosition;
    GfxCoord*         rootCoord;
    TmdObject*        model;
    Task*             mongooseTask;
    _Actor260400Work* work;

    model              = task->extra.tmd;
    rootCoord          = model->coords;
    work               = memCalloc(sizeof(*work), false);
    _gScriptedWalkWork = work;
    task->work         = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor260400ExitScriptedWalker;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_260400_BODY_OT_OFFSET;
    model->flags                     = 0;
    model->lightMtx                  = &_gScriptedWalkWork->light;
    model->colorMtx                  = &_gScriptedWalkWork->color;
    lightPosition.vx                 = rootCoord->workm.t[0];
    lightPosition.vy                 = rootCoord->workm.t[1] - ACTOR_260400_LIGHT_SAMPLE_HEIGHT;
    _gActor260400ScriptedWalkerTask  = task;
    lightPosition.vz                 = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ACTOR_260400_LIGHT_COUNT);
    animationInitContext(&_gScriptedWalkWork->rig.anim, _gActor260400WalkerAnimationSets, model,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = ACTOR_260400_INITIAL_CLIP;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    mongooseTask                  = taskSpawnFromTable(gActor260400RupertBroderickTasks, ACTOR_260400_MONGOOSE_TASK, ACTOR_260400_MONGOOSE_ATTACH_PART, 0);
    if (mongooseTask != NULL) {
        _gScriptedWalkWork->mongoose = mongooseTask;
        _actorRenderApplyTaskPlacementTextureOffsets(mongooseTask, task->spawnArg2.pointer);
    }
    _gScriptedWalkWork->st.travel     = 0;
    _gScriptedWalkWork->turnFrames    = 0;
    _gScriptedWalkWork->mongooseShown = 0;
    task->msgTable                    = _gActor260400ScriptedWalkerMessages;
    _scriptedWalkUpdate(task);
    task->state++;
}

#include "../../shared/walk/scripted_walk_update.inc.c"

/// Dispatches the hurt Rupert Broderick walker setup or frame update.
///
/// Requires a live TMD body, Enemy placement owner in spawnArg2 and state 0 or
/// 1. Publishes this task's work for synchronous scripted-walk handlers before
/// dispatch; setup replaces it with the newly allocated work. The package and
/// published work must remain live while callbacks use them. Setup failure may
/// destroy the task; this dispatcher does not use it after the selected call.
static void _actor260400ScriptedWalkerTask(Task* task)
{
    EnemyTaskFunc handlers[2] = {
        _actor260400SpawnScriptedWalker,
        _actorRenderWalkerFrame,
    };

    _gScriptedWalkWork = task->work;
    handlers[task->state](task->spawnArg2.pointer, task);
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
#define ACTOR_RENDER_UPDATE_WALKER _scriptedWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the scripted walker's enemy and task, then tears down its Mongoose task.
///
/// Installed as the walker's exit callback. Requires its enemy, allocated work
/// and successfully spawned Mongoose task. The binary reads the Mongoose pointer
/// from the released work after parent teardown; the published walker pointers
/// are left unchanged. Models follow `taskKill`'s delayed or immediate release.
static void _actor260400ExitScriptedWalker(Task* task)
{
    _Actor260400Work* walkerWork = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    // Retain the binary's post-free read and teardown order.
    taskKill(walkerWork->mongoose);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Attaches the one-part Mongoose model to a borrowed walker part.
///
/// `mongooseRoot` is the preloaded `mongooseModel->coords` of the live model.
/// The walker part must be live and its parent chain must not reach that root.
/// The root's local transform is retained in the walker's part space, and its
/// cached composition is marked stale.
/// All model flags are cleared, permitting active drawing and automatic buffer
/// allocation; existing primitive buffers are retained. The parent is borrowed
/// and must remain live whenever the attached root is composed.
static inline void _actor260400AttachMongooseModel(TmdObject* mongooseModel, GfxCoord* mongooseRoot, GfxCoord* walkerPart)
{
    mongooseRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    mongooseModel->flags       = 0;
    mongooseRoot->parent       = walkerPart;
}

/// Parents the Mongoose to the scripted walker and updates its room lighting.
///
/// Requires live TMD models on this task and the published walker task.
/// `spawnArg1.value` must index 0..19 of the walker's coordinates; the spawn
/// uses part 8. State 0 attaches the root, clears draw flags and enters state 1.
/// State 1 samples all three lights at the walker's composed world translation,
/// 800 world units above it in negative Y. The parent coordinate is borrowed
/// until teardown; transforms must be composed and lighting scratch/GTE state
/// available. Other states do nothing.
static void _actor260400MongooseTask(Task* task)
{
    enum {
        ACTOR_260400_MONGOOSE_ATTACH       = 0,
        ACTOR_260400_MONGOOSE_LIGHT        = 1,
        ACTOR_260400_MONGOOSE_LIGHT_HEIGHT = 800,
        ACTOR_260400_MONGOOSE_LIGHT_COUNT  = 3,
    };
    TmdObject* mongooseModel = task->extra.tmd;
    GfxCoord*  mongooseRoot  = mongooseModel->coords;
    GfxCoord*  walkerCoords  = _gActor260400ScriptedWalkerTask->extra.tmd->coords;
    GfxCoord*  walkerPart    = walkerCoords + task->spawnArg1.value;
    VECTOR     lightSample;

    switch (task->state) {
        case ACTOR_260400_MONGOOSE_ATTACH:
            _actor260400AttachMongooseModel(mongooseModel, mongooseRoot, walkerPart);
            task->state++;
            break;
        case ACTOR_260400_MONGOOSE_LIGHT:
            lightSample.vx = walkerCoords->workm.t[0];
            lightSample.vy = walkerCoords->workm.t[1] - ACTOR_260400_MONGOOSE_LIGHT_HEIGHT;
            lightSample.vz = walkerCoords->workm.t[2];
            worldCoordSetModelLighting(mongooseModel, &lightSample, 0, ACTOR_260400_MONGOOSE_LIGHT_COUNT);
            break;
    }
}

#include "../../shared/walk/scripted_walk_tick_anim.inc.c"

#include "../../shared/walk/scripted_walk_reset_anim.inc.c"

#include "../../shared/walk/scripted_walk_blend_anim.inc.c"

/// Reseeds the published scripted walker from a borrowed animation request.
///
/// Requires a live walker and initialized rig. Loaded clip keys are 1..12.
/// The signed check rejects IDs >= 16 with -1, but accepts negative IDs and NULL
/// entries 0/13/14/15; acceptance alone does not establish a playable clip.
/// An accepted ID narrows to `s16`. Nonzero blend latches the low signed halfword
/// of `blendFrames`, in whole normal-rate frames (0..2047 keeps playback time
/// nonnegative); zero blend restarts and leaves that latch intact. Reseeding
/// runs immediately. Receiver, message ID, second payload and the other request
/// words are ignored. Returns 0 after acceptance and retains no request pointer.
static s32 _actor260400PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    if (request->animationId < (s32)ARRAY_SIZE(_gActor260400WalkerAnimationSets)) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        _scriptedWalkUpdate(_gActor260400ScriptedWalkerTask);
        return 0;
    }
    return -1;
}

/// Replaces the scripted walker and Mongoose models' draw flags.
///
/// Requires the live published walker, work block and successfully spawned
/// Mongoose model. Bit 0 permits active drawing; bit 1 suppresses automatic
/// primitive-buffer allocation. All other model flags are cleared and other
/// request bits ignored. The Mongoose stays hidden with automatic buffering
/// suppressed until its command enables it. No buffers are allocated or freed.
/// Receiver, message ID and second payload are ignored. Returns 0.
static s32 _actor260400SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    enum {
        ACTOR_260400_WALKER_DRAW_SHOW             = 1 << 0,
        ACTOR_260400_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1,
    };
    TmdObject* walkerModel;
    TmdObject* mongooseModel;

    walkerModel   = _gActor260400ScriptedWalkerTask->extra.tmd;
    mongooseModel = _gScriptedWalkWork->mongoose->extra.tmd;

    if (drawFlags & ACTOR_260400_WALKER_DRAW_SHOW) {
        walkerModel->flags   = 0;
        mongooseModel->flags = 0;
    } else {
        walkerModel->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        mongooseModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_260400_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        mongooseModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if (_gScriptedWalkWork->mongooseShown == 0) {
        mongooseModel->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
    return 0;
}

#include "../../shared/walk/scripted_walk_place.inc.c"

/// Applies the scripted walker's turn or Mongoose-permission command.
///
/// Requires live published walker work and its successfully spawned revolver.
/// Command zero schedules twenty turn updates; one permits model-draw messages
/// and clears the revolver's draw flags only if the Mongoose inventory test is zero;
/// two clears that permission and hides the revolver with auto buffering disabled.
/// An already-owned Mongoose makes command one a complete no-op. The permission
/// latch does not mirror actual drawing or buffer availability. Unknown commands
/// are ignored. Borrows the read-only command until return; returns 0.
static s32 _actor260400ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    enum {
        ACTOR_260400_COMMAND_TURN  = 0,
        ACTOR_260400_TURN_TICKS    = 20,
        ACTOR_260400_MONGOOSE_ITEM = 0x88,
    };
    TmdObject* mongooseModel;
    s32        command;

    mongooseModel = _gScriptedWalkWork->mongoose->extra.tmd;
    command       = request->command;

    switch (command) {
        case ACTOR_260400_COMMAND_TURN:
            _gScriptedWalkWork->turnFrames = ACTOR_260400_TURN_TICKS;
            break;
        case ACTOR_260400_COMMAND_ENABLE_MONGOOSE:
            if (inventoryIsItemLimitReached(ACTOR_260400_MONGOOSE_ITEM) == 0) {
                _gScriptedWalkWork->mongooseShown = command;
                mongooseModel->flags              = 0;
            }
            break;
        case ACTOR_260400_COMMAND_DISABLE_MONGOOSE:
            _gScriptedWalkWork->mongooseShown = 0;
            mongooseModel->flags              = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
    }
    return 0;
}

#include "../../shared/walk/scripted_walk_to.inc.c"
