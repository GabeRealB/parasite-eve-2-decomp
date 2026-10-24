#include "actors/actor_146300.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/display.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/walk/scripted_walk.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of the package's actor, a twenty-part figure that event scripts
/// place and give clips to but that never walks.
///
/// The spawn state allocates it zeroed and keeps it both at `Task::work` and
/// in `_gScriptedWalkWork`. The model object borrows `light` and `color` for as
/// long as the block lives. The matrices, the rig and `st` sit where the
/// scripted walkers' blocks keep theirs, which is what lets the package carry
/// that library's slot tick, its two reseeds and its placement under the
/// library's name for the block. The block ends at `st`: there is no turn
/// countdown behind it, and the walk's frame count in `st` stays zero.
typedef struct {
    MATRIX          light; // Light-direction matrix lent to the model object
    MATRIX          color; // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;   // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;    // Animation request and the heading the last placement gave the root
} _Actor146300Work;
STATIC_ASSERT_SIZEOF(_Actor146300Work, 0x4EC);

/// Borrowed work block used by this actor's scripted-walk animation and placement.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// The included fragments require it to remain live; task teardown releases
/// it without clearing this pointer. This carrier has no walking countdown.
static _Actor146300Work* _gScriptedWalkWork;

/// Borrowed body task of this package's active scripted actor.
///
/// Published after successful spawn for animation messages, paired visibility
/// and model attachments. The body has 20 coordinates. Only the current live
/// instance may be used: a later spawn replaces the pointer, and teardown
/// leaves it dangling rather than clearing it. This cache owns no task.
static Task* _gActor146300BodyTask;

/// Borrowed helper-model task paired with this carrier's published body task.
///
/// Successful body setup publishes the handle for paired visibility and model
/// parenting. Teardown kills the helper before releasing the body; neither
/// published handle is cleared. Consumers require the current pair to be live.
static Task* _gActorMsgHelperTask;

static void _actor146300UpdateModel(Enemy* unusedEnemy, Task* task);
static void _actor146300Destroy(Task* task);
static void _actor146300UpdateAnimation(Task* unusedTask);

static TmdSource _gPierceBody2;
static TmdSource _gPierceGlasses;
static void      _actor146300ScriptedFigureTask(Task* task);
static void      _actor146300AttachmentTask(Task* task);

static s32 _actor146300PlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor146300IgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);

static void _actor146300ApplyHandoverChoice(s32 phase);

static void _actor146300BeginWaterTankEvent(void);

static void _actor146300IceBagHandoverTask(Task* task);

// Progress counts the three bags consumed after the initial water-tank event.
enum {
    ACTOR146300_HANDOVER_AWAIT_FIRST_BAG  = 2,
    ACTOR146300_HANDOVER_AWAIT_SECOND_BAG = 3,
    ACTOR146300_HANDOVER_AWAIT_THIRD_BAG  = 4,
    ACTOR146300_HANDOVER_COMPLETE         = 5,
};

// CAP slots and two-bit object records for successive handover conversations.
enum {
    ACTOR146300_CAP_NO_ICE_BAG             = 0x12,
    ACTOR146300_CAP_FIRST_BAG              = 0x13,
    ACTOR146300_CAP_SECOND_BAG             = 0x14,
    ACTOR146300_CAP_FINAL_TALK             = 0x15,
    ACTOR146300_FIRST_BAG_TALK_OBJECT      = 0x1F,
    ACTOR146300_SECOND_BAG_TALK_OBJECT     = 0x20,
    ACTOR146300_FINAL_TALK_OBJECT          = 0x21,
    ACTOR146300_HANDOVER_OBJECT_TALK_READY = 1,
};

#include "assets/rooms/dryfield/night_water_tank/pierce_clips_1.inc"

TaskDesc gActor146300IceBagHandoverTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _actor146300IceBagHandoverTask,
    .data     = { .value = 0 },
};

/// Player animation sets installed as weapon-bank clips 47 through 64.
///
/// The player borrows the descriptors while this package stays loaded. The
/// fixed-capacity copy also reads fourteen words after the eighteen pointers:
/// all of `_gActor146300PlayerClip47ResetRequest` and `_gActor146300PlayerClip47Request`,
/// then the first four words of `_gActor146300PlayerClip48Request`.
/// Keep this table and those records contiguous. Only the first eighteen
/// copied entries are playable clips; the remaining words are request data.
#include "assets/rooms/dryfield/night_water_tank/player_scene_bank.inc"

/// Retained player clip-47 reset request included in the raw bank-extension copy.
///
/// No script dispatches it. Its five words occupy copied bank entries 65..69,
/// beyond the playable extension's clips 47..64. Keep it immediately after
/// `_gActor146300PlayerBankExtension`; the separately dispatched clip-47 request
/// blends over five frames and has different contents.
static AnimationPlayRequest _gActor146300PlayerClip47ResetRequest = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 47 during the opening water-tank event, blending over 5 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip47Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 48 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip48Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 49 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 50 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 51 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 52 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 53 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 54 during the opening water-tank event, blending over 10 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 55 during the opening water-tank event, blending over 10 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 56 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 57 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 58 during the opening water-tank event, blending over 10 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip58Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 59 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip59Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 60 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip60Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 13,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 61 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip61Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 62 during an ice-bag handover, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip62Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 63 during an ice-bag handover, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip63Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 16,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained clip-0 reset request beside the figure's playable clip requests.
///
/// No code dispatches this record. Figure bank entry 0 is NULL, so this request
/// cannot be played by the initialized figure. Preserve its five stored words.
static AnimationPlayRequest _gActor146300FigureClip0Request = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 1 at the first opening-dialogue cue, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 2 at the second opening-dialogue cue, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip2Request = {
    .source               = { .index = 1 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 3 at the third opening-dialogue cue, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip3Request = {
    .source               = { .index = 1 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 4 at water-tank and handover dialogue cues, blending over 10 normal-rate frames.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip4Request = {
    .source               = { .index = 1 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 5 for the handover conversation and its restored pose, blending over 10 normal-rate frames.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip5Request = {
    .source               = { .index = 1 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 6 at the water-tank event's timed exchange, restarting its pose.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip6Request = {
    .source               = { .index = 1 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 7 after the player clip-55 cue, restarting without a transition.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip7Request = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 8 at the next timed water-tank cue, restarting its pose.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip8Request = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 9 with player clip 61 during the water-tank event.
///
/// Restarts without a transition. Placement 0 consumes the request synchronously;
/// its fixed clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip9Request = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 10 for the first retained CAP choice, blending over 30 normal-rate frames.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip10Request = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 30,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts figure clip 11 before the water-tank event places the player.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and enabled world-collision word.
static AnimationPlayRequest _gActor146300FigureClip11Request = {
    .source               = { .index = 1 },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays figure clip 12 when restoring the pose awaiting an ice bag, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip12Request = {
    .source               = { .index = 1 },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 13 at a water-tank dialogue cue, blending over 10 normal-rate frames.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip13Request = {
    .source               = { .index = 1 },
    .animationId          = 13,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 14 when restoring the final handover pose, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip14Request = {
    .source               = { .index = 1 },
    .animationId          = 14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts figure clip 15 between the final ice-bag handover's CAP cues.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip15Request = {
    .source               = { .index = 1 },
    .animationId          = 15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 16 at the final-bag and final-talk dialogue cues, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip16Request = {
    .source               = { .index = 1 },
    .animationId          = 16,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 1 on return from the water-tank event, restarting without a pose transition.
///
/// Uses the weapon bank's base clip. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into weapon-bank clip 1 over 20 normal-rate frames at event entry.
///
/// The event runner copies this request and replaces its bank index with the
/// equipped weapon's index before dispatch. Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip1BlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 64 during an ice-bag handover, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip64Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 17,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the water-tank player clips in the equipped weapon's bank extension.
///
/// Copies 32 words without starting playback: eighteen clip pointers followed
/// by fourteen adjacent request words. The playable range is 47..64; the copy
/// length is the bank capacity, rather than the number of clips. The request
/// is borrowed through dispatch; the copied clip data must remain loaded.
static AnimationBankCopyRequest _gActor146300CopyPlayerBankExtensionRequest = {
    .sets  = _gActor146300PlayerBankExtension,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Player placement at the first animation of the water-tank event.
///
/// Position uses whole room-coordinate units; yaw is 1820/4096 of a turn.
/// The placement message consumes both vectors synchronously.
static ActorTransform _gActor146300PlayerEventTransform = {
    .pos = { 1110, -12000, -2500, 0 },
    .rot = { 0, 1820, 0, 0 },
};

/// Player placement restored after completion or skipping of the water-tank event.
///
/// Position uses whole room-coordinate units; yaw is 1820/4096 of a turn.
/// The placement message consumes both vectors synchronously before control resumes.
static ActorTransform _gActor146300PlayerResumeTransform = {
    .pos = { 1500, -12000, -1744, 0 },
    .rot = { 0, 1820, 0, 0 },
};

/// Figure placement restored for the final ice-bag handover pose.
///
/// Position uses whole room-coordinate units; yaw is 682/4096 of a turn.
/// Placement 0 consumes both vectors synchronously when progress is complete
/// or a third bag is already held.
static ActorTransform _gActor146300FigureFinalHandoverTransform = {
    .pos = { 1110, -12000, -2000, 0 },
    .rot = { 0, 682, 0, 0 },
};

// CAP command slots, placement selection and sound cues of these linear streams.
enum {
    ACTOR146300_CAP_WATER_TANK_EVENT         = 16,
    ACTOR146300_CAP_AFTER_WATER_TANK_EVENT   = 17,
    ACTOR146300_CAP_HANDOVER_REPEAT_TALK     = 22,
    ACTOR146300_EVENT_FIGURE_PLACEMENT       = 0,
    ACTOR146300_EVENT_RESUME_VIEW            = 3,
    ACTOR146300_EVENT_WEAPONS_PLAYER         = 0,
    ACTOR146300_HANDOVER_CHOICE_INITIAL_CUE  = 0,
    ACTOR146300_HANDOVER_CHOICE_FOLLOWUP_CUE = 1,
    ACTOR146300_EVENT_SOUND_CUE10            = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 10),
    ACTOR146300_EVENT_SOUND_CUE11            = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 11),
    ACTOR146300_EVENT_SOUND_CUE12            = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 12),
    ACTOR146300_EVENT_STOP_SOUND_TYPE        = 13,
};

EvsCommand gActor146300WaterTankEventScript[99] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor146300BeginWaterTankEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_WATER_TANK_EVENT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146300CopyPlayerBankExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR146300_EVENT_STOP_SOUND_TYPE }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR146300_EVENT_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146300PlayerEventTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR146300_EVENT_SOUND_CUE10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR146300_EVENT_SOUND_CUE11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR146300_EVENT_SOUND_CUE12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146300PlayerResumeTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR146300_EVENT_RESUME_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR146300_EVENT_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor146300WaterTankEventSkipScript[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR146300_EVENT_RESUME_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR146300_EVENT_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146300PlayerResumeTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_AFTER_WATER_TANK_EVENT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Synchronizes the first or second bag handover with the selected CAP slot's variant 0.
///
/// The task starts CAP slot 19 or 20 first. Hides the HUD, installs the player's
/// extension and plays clips 64, 62 and 63 at CAP cues while the figure uses
/// clip 4. All 14 commands, their actor requests and sets must stay loaded until
/// the stream ends; the task starts the choice variant afterwards.
static EvsCommand _gActor146300IceBagHandoverScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146300CopyPlayerBankExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Applies both retained-choice cues during a handover CAP slot's variant 1.
///
/// The task starts slot 19 or 20 first. The two callbacks select figure clips
/// from the CAP variant key, then the stream restores the HUD. Requires the
/// initialized placement-0 figure and loaded CAP/script resources through all
/// eight commands; the handover task releases player control after completion.
static EvsCommand _gActor146300HandoverChoiceScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor146300ApplyHandoverChoice }, { .value = ACTOR146300_HANDOVER_CHOICE_INITIAL_CUE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor146300ApplyHandoverChoice }, { .value = ACTOR146300_HANDOVER_CHOICE_FOLLOWUP_CUE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Starts CAP slot 21 and synchronizes the final bag handover's animation cues.
///
/// Hides the HUD, installs the player extension and alternates player clip 64
/// with figure clips 16 and 15. Requires live player/placement-0 tasks and loaded
/// CAP slots, request records and sets through all 15 commands. The handover task
/// starts slot 21's choice variant after this stream ends.
static EvsCommand _gActor146300FinalIceBagHandoverScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_FINAL_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146300CopyPlayerBankExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Finishes the final handover's CAP variant-1 conversation and restores its pose.
///
/// The task starts slot 21 first. Plays figure clip 16 through the CAP cue,
/// restores the HUD and requests clip 14 before ending. The six-command stream
/// borrows the initialized placement-0 figure and loaded requests; the task
/// releases player control after completion.
static EvsCommand _gActor146300FinalHandoverTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip14Request } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs CAP slot 22 for a repeat conversation after the final talk is no longer ready.
///
/// Plays figure clip 16 through the CAP cue, releases the player's scripted
/// control and restores figure clip 14. The spawner kills its handover task
/// immediately, so this seven-command stream completes independently. Requires
/// the player, initialized placement-0 figure and loaded CAP/script resources
/// until event completion; the interpreter restores the HUD at its end.
static EvsCommand _gActor146300HandoverRepeatTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_HANDOVER_REPEAT_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip14Request } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/pierce/body_2.inc"

#include "assets/characters/pierce/glasses.inc"

#include "assets/rooms/dryfield/night_water_tank/pierce_clips_2.inc"

/// Latched duration of the next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword. Plain resets leave it intact; this walker has no walk-completion
/// transition. Zero requests no transition time; 0..2047 keeps the playback
/// timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

/// Messages accepted by the live water-tank figure, terminated by the task-message sentinel.
///
/// Animation and placement borrow their payloads through synchronous dispatch;
/// visibility controls the body/helper pair and actor commands are acknowledged
/// without action. Unsupported IDs return zero. The table and callbacks must
/// remain loaded while the figure's task retains the table.
static TaskMessageEntry _gActor146300FigureMessages[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor146300PlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor146300IgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor146300FigureTasks[ACTOR146300_TASK_COUNT] = {
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor146300ScriptedFigureTask,
        .data     = { .model = &_gPierceBody2 },
    },
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor146300AttachmentTask,
        .data     = { .model = &_gPierceGlasses },
    },
};

/// Local animation bank of the twenty-part water-tank figure.
///
/// Entry 0 is NULL; clips 1..16 borrow this package's sixteen animation sets.
/// The animation context retains the table and reseeds child tracks through it,
/// so the table and sets must remain loaded for the figure's lifetime. Requests
/// must select 1..16; the message handler's signed upper-bound check also admits
/// zero and negative values, which do not name playable entries.
#include "assets/rooms/dryfield/night_water_tank/pierce_bank.inc"

/// Selected CAP slot for the first two ice-bag handovers and their follow-up choices.
///
/// Written as slot 19 or 20 before either scene starts and reused for variants
/// 0 (bag handover) and 1 (choice). Zero is the initial, unselected value; the
/// handover task sets the latch before reading it; run only one at a time. Keep word-sized
/// storage: the CAP start calls narrow the selected value to their signed halfword.
static s32 _gActor146300HandoverCapSlot = 0;

static void _actor146300SpawnScriptedWalker(Enemy* enemy, Task* task);

/// Runs one ice-bag handover or follow-up conversation, then releases scripted control.
///
/// The room holds player control before spawning this bodyless task in state 0.
/// Consumes the live-save collection bit only while advancing handover progress;
/// expired bags are melted before the first check. The loaded CAP slots and event
/// scripts must remain available until the scene ends. Event completion is the
/// session's idle event state, not CAP selection alone. A final repeat conversation
/// delegates control release to its script and kills this task immediately.
static void _actor146300IceBagHandoverTask(Task* task)
{
    enum {
        ACTOR146300_HANDOVER_CHECK                 = 0,
        ACTOR146300_HANDOVER_RELEASE_CONTROL       = 1,
        ACTOR146300_HANDOVER_START_BAG_SCENE       = 10,
        ACTOR146300_HANDOVER_WAIT_BAG_SCENE        = 11,
        ACTOR146300_HANDOVER_START_CHOICE_SCENE    = 20,
        ACTOR146300_HANDOVER_WAIT_CHOICE_SCENE     = 21,
        ACTOR146300_HANDOVER_START_FINAL_BAG_SCENE = 30,
        ACTOR146300_HANDOVER_WAIT_FINAL_BAG_SCENE  = 31,
        ACTOR146300_HANDOVER_START_FINAL_TALK      = 40,
        ACTOR146300_HANDOVER_WAIT_FINAL_TALK       = 41,
        ACTOR146300_CAP_HANDOVER_VARIANT           = 0,
        ACTOR146300_CAP_CHOICE_VARIANT             = 1,
    };

    /// Consumes a bag, selects its CAP slot, advances progress and queues the scene.
    ///
    /// Each argument is evaluated once: captionIndex, nextProgress, then handoverTask,
    /// after clearing the collection bit. Requires a live task, loaded CAP slot and
    /// next handover progress. Captures the overlay's caption latch and this function's
    /// start-scene state constant. Ends a block; use as a statement inside braces.
#define ACTOR_146300_ACCEPT_ICE_BAG(handoverTask, captionIndex, nextProgress)    \
    {                                                                            \
        inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG);             \
        _gActor146300HandoverCapSlot = (captionIndex);                           \
        gameFlagSetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS, (nextProgress)); \
        (handoverTask)->state = ACTOR146300_HANDOVER_START_BAG_SCENE;            \
    }

    switch (task->state) {
        case ACTOR146300_HANDOVER_CHECK:
            // Expiration must be applied before testing whether a bag can be handed over.
            inventoryMeltIceBagIfExpired();
            switch (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS)) {
                case ACTOR146300_HANDOVER_AWAIT_FIRST_BAG:
                    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                        capRunCommandWithTransition(ACTOR146300_CAP_NO_ICE_BAG);
                        task->state++;
                    } else {
                        ACTOR_146300_ACCEPT_ICE_BAG(task, ACTOR146300_CAP_FIRST_BAG, ACTOR146300_HANDOVER_AWAIT_SECOND_BAG);
                    }
                    break;
                case ACTOR146300_HANDOVER_AWAIT_SECOND_BAG:
                    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                        if (areaGetCurrentObjectState(ACTOR146300_FIRST_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
                            _gActor146300HandoverCapSlot = ACTOR146300_CAP_FIRST_BAG;
                            task->state                  = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
                        } else {
                            capRunCommandWithTransition(ACTOR146300_CAP_NO_ICE_BAG);
                            task->state++;
                        }
                    } else {
                        ACTOR_146300_ACCEPT_ICE_BAG(task, ACTOR146300_CAP_SECOND_BAG, ACTOR146300_HANDOVER_AWAIT_THIRD_BAG);
                    }
                    break;
                case ACTOR146300_HANDOVER_AWAIT_THIRD_BAG:
                    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                        if (areaGetCurrentObjectState(ACTOR146300_SECOND_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
                            _gActor146300HandoverCapSlot = ACTOR146300_CAP_SECOND_BAG;
                            task->state                  = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
                        } else {
                            capRunCommandWithTransition(ACTOR146300_CAP_NO_ICE_BAG);
                            task->state++;
                        }
                    } else {
                        inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG);
                        gameFlagSetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS, ACTOR146300_HANDOVER_COMPLETE);
                        task->state = ACTOR146300_HANDOVER_START_FINAL_BAG_SCENE;
                    }
                    break;
                case ACTOR146300_HANDOVER_COMPLETE:
                    if (areaGetCurrentObjectState(ACTOR146300_FINAL_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
                        task->state = ACTOR146300_HANDOVER_START_FINAL_TALK;
                    } else {
                        evsStartScript(_gActor146300HandoverRepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                        taskKill(task);
                    }
                    break;
                default:
                    task->state++;
                    break;
            }
            break;
        case ACTOR146300_HANDOVER_RELEASE_CONTROL:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            break;
        case ACTOR146300_HANDOVER_START_BAG_SCENE:
            // The same CAP slot serves the handover and its later choice conversation.
            capStartSequenceSlot((s16)_gActor146300HandoverCapSlot, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_HANDOVER_VARIANT);
            evsStartScript(_gActor146300IceBagHandoverScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_BAG_SCENE:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
            }
            break;
        case ACTOR146300_HANDOVER_START_CHOICE_SCENE:
            capStartSequenceSlot((s16)_gActor146300HandoverCapSlot, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_CHOICE_VARIANT);
            evsStartScript(_gActor146300HandoverChoiceScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_START_FINAL_BAG_SCENE:
            evsStartScript(_gActor146300FinalIceBagHandoverScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_FINAL_BAG_SCENE:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_START_FINAL_TALK;
            }
            break;
        case ACTOR146300_HANDOVER_START_FINAL_TALK:
            capStartSequenceSlot(ACTOR146300_CAP_FINAL_TALK, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_CHOICE_VARIANT);
            evsStartScript(_gActor146300FinalHandoverTalkScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_CHOICE_SCENE:
        case ACTOR146300_HANDOVER_WAIT_FINAL_TALK:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_RELEASE_CONTROL;
            }
            break;
    }
#undef ACTOR_146300_ACCEPT_ICE_BAG
}

void actor146300RestoreHandoverPose(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS)) {
        case ACTOR146300_HANDOVER_AWAIT_FIRST_BAG:
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
            break;
        case ACTOR146300_HANDOVER_AWAIT_SECOND_BAG:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                if (areaGetCurrentObjectState(ACTOR146300_FIRST_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip5Request, 0);
                } else {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
                }
            } else {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
            }
            break;
        case ACTOR146300_HANDOVER_AWAIT_THIRD_BAG:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                if (areaGetCurrentObjectState(ACTOR146300_SECOND_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip5Request, 0);
                } else {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
                }
                break;
            }
            // A third bag already present restores the final pose before its handover.
            /* fallthrough */
        case ACTOR146300_HANDOVER_COMPLETE:
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLACE, &_gActor146300FigureFinalHandoverTransform, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip14Request, 0);
            break;
    }
}

/// Cancels room effects and locks attachment actions for the water-tank event.
///
/// The opening script calls this before taking control of player presentation.
static void _actor146300BeginWaterTankEvent(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Applies the retained CAP choice at either cue of the handover conversation.
///
/// Phase 0 selects clip 10 for key 1; phase 1 selects clip 5 for key 2.
/// Other phases and keys leave the actor alone. Requires the package's live
/// initialized placement-0 actor and its loaded request records.
static void _actor146300ApplyHandoverChoice(s32 phase)
{
    enum {
        ACTOR146300_CHOICE_FIRST_CUE  = 0,
        ACTOR146300_CHOICE_SECOND_CUE = 1,
        ACTOR146300_CHOICE_FIRST_KEY  = 1,
        ACTOR146300_CHOICE_SECOND_KEY = 2,
    };

    switch (phase) {
        case ACTOR146300_CHOICE_FIRST_CUE:
            if (capGetVariantKey() == ACTOR146300_CHOICE_FIRST_KEY) {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip10Request, 0);
            }
            break;
        case ACTOR146300_CHOICE_SECOND_CUE:
            if (capGetVariantKey() == ACTOR146300_CHOICE_SECOND_KEY) {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip5Request, 0);
            }
            break;
    }
}

/// Samples three model light rows at cached root XYZ with Y decreased by 800.
///
/// Requires a live model with writable light/color matrices and stable
/// MATRIX-bearing rootCoord; workm is read without composition. lightPosition
/// is a writable VECTOR lvalue. Root/sample occur repeatedly; model occurs
/// once. Captures the local Y-offset and light-count constants. The query
/// consumes XYZ synchronously; invoke within a braced statement body.
#define ACTOR_146300_RELIGHT_SPAWN(model, rootCoord, lightPosition)                         \
    {                                                                                       \
        (lightPosition).vx = (rootCoord)->workm.t[0];                                       \
        (lightPosition).vy = (rootCoord)->workm.t[1] - ACTOR_146300_LIGHT_SAMPLE_Y_OFFSET;  \
        (lightPosition).vz = (rootCoord)->workm.t[2];                                       \
        worldCoordSetModelLighting((model), &(lightPosition), 0, ACTOR_146300_LIGHT_COUNT); \
    }

/// Initializes the ice-bag handover scripted walker and creates its attachment task.
///
/// Allocates zeroed task-owned work, destroying the enemy on failure. Requires
/// a live model/Enemy and descriptor slot 1 to produce a live attachment. Parents
/// the untargetable root to the view, textures the attachment from the owner’s
/// area placement and adopts it into task teardown. The model borrows work-owned
/// lighting matrices and samples cached XYZ with Y minus 800, before composition.
/// Binds the loaded clip table, requests clip 11, installs messages/exit, updates
/// once and advances spawn state 0 to running state 1. Resources must outlive work.
static void _actor146300SpawnScriptedWalker(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_146300_SPAWN_CLIP            = 11,
        ACTOR_146300_LIGHT_SAMPLE_Y_OFFSET = 800,
        ACTOR_146300_LIGHT_COUNT           = 3,
    };

    VECTOR            lightPosition;
    _Actor146300Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    Task*             attachmentTask;

    model              = task->extra.tmd;
    rootCoord          = model->coords;
    work               = memCalloc(sizeof(_Actor146300Work), false);
    _gScriptedWalkWork = work;
    task->work         = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor146300Destroy;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = 1;
    model->flags                     = 0;
    _gActor146300BodyTask            = task;
    attachmentTask                   = taskSpawnFromTable(gActor146300FigureTasks, ACTOR146300_TASK_ATTACHMENT, 0, 0);
    _gActorMsgHelperTask             = attachmentTask;
    _actorRenderApplyTaskPlacementTextureOffsets(attachmentTask, enemy);
    taskReparent(task, _gActorMsgHelperTask);
    model->lightMtx = &_gScriptedWalkWork->light;
    model->colorMtx = &_gScriptedWalkWork->color;
    ACTOR_146300_RELIGHT_SPAWN(model, rootCoord, lightPosition);
    animationInitContext(&_gScriptedWalkWork->rig.anim, _gActor146300FigureAnimationSets, model,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = ACTOR_146300_SPAWN_CLIP;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable                = _gActor146300FigureMessages;
    _actor146300UpdateAnimation(task);
    task->state++;
}
#undef ACTOR_146300_RELIGHT_SPAWN

/// Dispatches the scripted figure's initialization or model update.
///
/// Descriptor 0 requires a live Enemy in `spawnArg2.pointer`, a twenty-part
/// TMD body and state 0..1. Publishes the task-owned work for the singleton
/// animation/placement handlers before dispatch; state 0 allocates it, while
/// state 1 composes, relights and animates the stationary figure. Initialization
/// may destroy the enemy and task on allocation failure. There is no bounds check.
static void _actor146300ScriptedFigureTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor146300SpawnScriptedWalker,
        _actor146300UpdateModel,
    };

    _gScriptedWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Relights an actor or its attachment from an offset of the actor's cached origin.
///
/// Samples 800 game-coordinate units along view-space negative Y, requesting
/// all three model-light rows. `actorRoot->workm` must already include the
/// current view transform; this helper neither composes nor changes the root.
/// `model` may be the attachment rather than the actor supplying the root.
/// Both inputs and the model's borrowed writable light/colour matrices must
/// remain live. Scratch and GTE requirements follow `worldCoordSetModelLighting`;
/// the sample's three signed words are consumed synchronously, retaining no pointer.
static inline void _actor146300RelightFromRoot(const TmdObject* model, const GfxCoord* actorRoot)
{
    enum { ACTOR146300_LIGHT_SAMPLE_HEIGHT = 800 };
    VECTOR3 lightPosition;

    lightPosition.vx = actorRoot->workm.t[0];
    lightPosition.vy = actorRoot->workm.t[1] - ACTOR146300_LIGHT_SAMPLE_HEIGHT;
    lightPosition.vz = actorRoot->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ARRAY_SIZE(model->lightMtx->m));
}

/// Composes and relights the actor model before updating its child-part animation.
///
/// Running state 1 requires a live TMD task, a root parented to the view and the
/// receiver's initialized work published in `_gScriptedWalkWork`. Samples all
/// three light rows at the root's view-space negative-Y offset of 800 units.
/// `unusedEnemy` is ignored.
static void _actor146300UpdateModel(Enemy* unusedEnemy, Task* task)
{
    TmdObject* model;
    GfxCoord*  rootCoord;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    actorRenderComposeCoord(rootCoord);
    _actor146300RelightFromRoot(model, rootCoord);
    _actor146300UpdateAnimation(task);
}

/// Destroys this actor's enemy and begins task teardown when its task exits.
///
/// Requires the live enemy borrowed in `spawnArg2.pointer` and its owning task;
/// destruction follows `enemyDestroy`. Task teardown owns the model and work.
static void _actor146300Destroy(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Reseeds or advances the published actor's child-part animation without moving its root.
///
/// Requires live initialized `_gScriptedWalkWork` and loaded clip data for its
/// twenty-part rig. Blend/reset seed slots 1..19 and enter tick state without
/// ticking again in that call; tick advances those slots. Blend uses the latched
/// whole-frame duration. State 0 and other states do nothing. `unusedTask` is ignored.
static void _actor146300UpdateAnimation(Task* unusedTask)
{
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _scriptedWalkBlendAnim();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _scriptedWalkResetAnim();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _scriptedWalkTickAnim();
    }
}

#include "../../shared/walk/scripted_walk_tick_anim.inc.c"

#include "../../shared/walk/scripted_walk_reset_anim.inc.c"

#include "../../shared/walk/scripted_walk_blend_anim.inc.c"

/// Applies an animation request immediately to the published stationary actor.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; borrows a readable word-aligned request
/// through dispatch, retaining no request pointer. Requires the live initialized
/// singleton work and its loaded rig/model/table. Playable IDs are 1..16: entry 0
/// is NULL. The signed check rejects only IDs >=17; zero and negative IDs pass,
/// so callers must enforce the playable range before the signed-halfword narrowing.
/// Every nonzero blend value selects a blend and narrows whole normal-rate frames
/// to s16 (0..2047 keeps its playback timer nonnegative); reset leaves that latch
/// intact. Repeated clips restart. Source index and collision mode are ignored,
/// as are the other callback arguments. Returns 0 after reseeding, -1 on rejection.
static s32 _actor146300PlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        ACTOR146300_ANIMATION_REJECTED = -1,
    };

    if (request->animationId < (s32)ARRAY_SIZE(_gActor146300FigureAnimationSets)) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        _actor146300UpdateAnimation(_gActor146300BodyTask);
        return 0;
    }
    return ACTOR146300_ANIMATION_REJECTED;
}

/// Selects the published water-tank figure's body for paired visibility.
///
/// Expands to a `Task*` identifier with no evaluation side effects; this
/// fragment only reads it, independently of the message receiver. Requires
/// the current live TMD body and its matching helper in `_gActorMsgHelperTask`.
/// Successful spawn replaces the borrowed pointer; teardown does not clear it.
/// Bind only around the pair-visibility fragment and never dispatch after teardown.
#define ACTOR_MESSAGE_BODY_TASK _gActor146300BodyTask
#include "../../shared/actor/actor_messages_pair_visibility.inc.c"
#undef ACTOR_MESSAGE_BODY_TASK

#include "../../shared/walk/scripted_walk_place.inc.c"

/// Acknowledges `ACTOR_COMMAND_MESSAGE_APPLY` without changing this actor.
///
/// Returns 0 and ignores every argument, including the borrowed command pointer;
/// the pointer need not address readable storage because it is never dereferenced.
static s32 _actor146300IgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

/// Attaches the companion model to actor part 4, then relights it from the actor root.
///
/// Requires this live TMD task and `_gActor146300BodyTask` with its twenty coordinates;
/// both models remain live while attached. State 0 replaces the companion root's
/// parent, marks composition dirty, enables drawing and enters state 1. Later
/// ticks sample all three light rows at the actor root's view-space negative-Y
/// offset of 800 units. The actor's frame state must compose that root; this task
/// does not compose it.
static void _actor146300AttachmentTask(Task* task)
{
    enum {
        ACTOR146300_ATTACHMENT_ATTACH  = 0,
        ACTOR146300_ATTACHMENT_RELIGHT = 1,
        ACTOR146300_ATTACHMENT_PART    = 4,
    };
    TmdObject* attachmentModel = task->extra.tmd;
    GfxCoord*  attachmentRoot  = attachmentModel->coords;
    GfxCoord*  actorRoot       = _gActor146300BodyTask->extra.tmd->coords;
    GfxCoord*  attachmentPart  = actorRoot + ACTOR146300_ATTACHMENT_PART;

    switch (task->state) {
        case ACTOR146300_ATTACHMENT_ATTACH:
            attachmentRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            attachmentModel->flags       = 0;
            attachmentRoot->parent       = attachmentPart;
            task->state++;
            break;
        case ACTOR146300_ATTACHMENT_RELIGHT:
            _actor146300RelightFromRoot(attachmentModel, actorRoot);
            break;
    }
}
