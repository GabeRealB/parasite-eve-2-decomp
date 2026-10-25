#include "actors/actor_451100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/walk/footstep_walk.h"
#include "../../shared/walk/pair_walk.h"

static void _actor451100QuietWalkSpawn(Enemy* enemy, Task* task);
static void _actor451100PairWalkSpawn(Enemy* enemy, Task* task);
static void _footstepWalkQuietUpdate(Task* task);
static void _footstepWalkQuietResetAnim(void);
static void _footstepWalkQuietBlendAnim(void);
static void _footstepWalkTickAnim(void);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

static FootstepWalkQuietWork* _gFootstepWalkWork;

static Task* _gActor451100QuietWalkTask;

static s16 _gFootstepWalkMode;

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor451100QuietWalkExit(Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actor451100PairWalkExit(Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static TmdSource _gGolemNo9BodyDryfield;
static TmdSource _gGolemNo9Radio;
static void      _actor451100PairWalkTask(Task* task);
static void      _actor451100CarriedModelTask(Task* task);

static s32 _actor451100PairWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor451100PairWalkIgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);
static s32 _actor451100PairWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 unusedArgument);

static s32  _actor451100QuietWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _actor451100QuietWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor451100QuietWalkApplyCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static void _actor451100QuietWalkTask(Task* task);

#include "assets/rooms/neo_ark/submarine_tunnel/eve_and_no_9_clips_1.inc"

/// Duration of the scene walkers' requested pose transitions, in whole frames.
enum { ACTOR_451100_SCENE_BLEND_FRAMES = 8 };

/// Restarts clip 20 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip20Reset = { { .index = 0 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 1 on scene child 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkIdleReset = { { .index = 0 }, PAIR_WALK_ANIM_IDLE, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 0, the quiet walker, to clip 20 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip20Blend = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 21 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip21Reset = { { .index = 0 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 22 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip22Reset = { { .index = 0 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 23 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip23Reset = { { .index = 0 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 24 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip24Reset = { { .index = 0 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 25 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip25Reset = { { .index = 0 }, 25, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 0, the quiet walker, to clip 26 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip26Blend = { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 0, the quiet walker, to clip 27 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip27Blend = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 0, the quiet walker, to clip 28 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip28Blend = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 0, the quiet walker, to clip 29 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip29Blend = { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 30 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip30Reset = { { .index = 0 }, 30, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 31 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip31Reset = { { .index = 0 }, 31, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 0, the quiet walker, to clip 32 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip32Blend = { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 33 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip33Reset = { { .index = 0 }, 33, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 34 on scene child 0, the quiet walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip34Reset = { { .index = 0 }, 34, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 0, the quiet walker, to clip 35 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100QuietWalkClip35Blend = { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 1, the paired walker, to clip 1 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkIdleBlend = { { .index = 0 }, PAIR_WALK_ANIM_IDLE, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 2 on scene child 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip2Reset = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 3 on scene child 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip3Reset = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the paired scene walker's translating walk clip.
///
/// Consumed synchronously by scene actor 1; bank and collision fields are ignored.
/// Translation also requires a nonzero remaining travel count.
static AnimationPlayRequest _gActor451100PairWalkWalkReset = { { .index = 0 }, PAIR_WALK_ANIM_WALK, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 5 on scene actor 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip5Reset = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 6 on scene actor 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip6Reset = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene actor 1, the paired walker, to clip 7 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip7Blend = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene actor 1, the paired walker, to clip 8 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip8Blend = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene actor 1, the paired walker, to clip 9 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip9Blend = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene actor 1, the paired walker, to clip 10 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip10Blend = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 11 on scene child 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip11Reset = { { .index = 0 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene child 1, the paired walker, to clip 12 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip12Blend = { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 13 on scene actor 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip13Reset = { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 14 on scene actor 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip14Reset = { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts clip 15 on scene actor 1, the paired walker, without blending.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip15Reset = { { .index = 0 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends scene actor 1, the paired walker, to clip 16 over eight frames.
///
/// Consumed synchronously; the walker ignores the bank and collision fields.
static AnimationPlayRequest _gActor451100PairWalkClip16Blend = { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player placement shared by scene setup, normal completion and cancellation.
///
/// Position is (-1500, 3000, 0) in world-coordinate units; yaw is a quarter
/// turn toward +X. The placement message copies it during dispatch.
static ActorTransform _gActor451100PlayerScenePlacement = { { -1500, 3000, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player placement for the closing walker sequence at (-1500, 3000, 0).
///
/// World-coordinate units; yaw faces +X. Copied during placement dispatch.
static ActorTransform _gActor451100PlayerClosingScenePlacement = { { -1500, 3000, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Opening placement of the quiet scene walker at (1500, 3000, 0), facing +X.
///
/// Coordinates are in the model root's parent frame, in whole world units.
/// Scene actor 0 consumes XYZ and yaw during dispatch; no pointer is retained.
static ActorTransform _gActor451100QuietWalkOpeningPlacement = { { 1500, 3000, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Closing placement of the quiet scene walker at (1000, 3000, 200), facing -X.
///
/// Coordinates are in the model root's parent frame, in whole world units.
/// Scene actor 0 consumes XYZ and yaw during dispatch; no pointer is retained.
static ActorTransform _gActor451100QuietWalkClosingPlacement = { { 1000, 3000, 200, 0 }, { 0, -(ACTOR_TRANSFORM_ANGLE_TURN / 4), 0, 0 } };

/// Opening placement of the paired scene walker at (1500, 3000, 0), facing -X.
///
/// Coordinates are in the model root's parent frame, in whole world units.
/// Scene actor 1 consumes XYZ and yaw during dispatch; no pointer is retained.
static ActorTransform _gActor451100PairWalkOpeningPlacement = { { 1500, 3000, 0, 0 }, { 0, -(ACTOR_TRANSFORM_ANGLE_TURN / 4), 0, 0 } };

/// Closing placement of the paired scene walker at (1000, 3000, 200), facing +X.
///
/// Coordinates are in the model root's parent frame, in whole world units.
/// Scene actor 1 consumes XYZ and yaw during dispatch; no pointer is retained.
static ActorTransform _gActor451100PairWalkClosingPlacement = { { 1000, 3000, 200, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Restarts player clip 1 in the equipped weapon's animation bank.
///
/// Event playback resolves the bank in a copy of this borrowed request.
/// Requests world-grid participation; the stored bank selector is replaced.
static AnimationPlayRequest _gActor451100PlayerClip1Reset = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Restarts player clip 7 in the equipped weapon's animation bank.
///
/// Event dispatch replaces the bank selector in a copy of this borrowed request.
/// Playback takes scripted control and requests world-grid participation.
static AnimationPlayRequest _gActor451100PlayerClip7Reset = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Restarts player clip 8 in the equipped weapon's animation bank.
///
/// Event dispatch replaces the bank selector in a copy of this borrowed request.
/// Playback takes scripted control and requests world-grid participation.
static AnimationPlayRequest _gActor451100PlayerClip8Reset = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Player animation sets installed at extended clip ids 47 through 50.
///
/// The scene installs these borrowed clips before playback. Its adjacent
/// `_gActor451100PlayerAnimationBankCopy` request copies 32 words from this four-word table,
/// continuing through that request, five following playback requests and
/// the first opcode of `_gActor451100WalkerClip11And30Script`. The trailing 28 words are
/// copied data, not animation sets; playback selects only ids 47 through 50.
/// Preserve the original copy span and declaration order. Clip storage must
/// remain loaded while the player uses the installed pointers.
#include "assets/rooms/neo_ark/submarine_tunnel/player_scene_bank.inc"

/// Installs four player clips and the following raw words into the equipped bank.
///
/// Copies the full 32-word extension synchronously: four set addresses, this
/// request's two words, the next five playback requests and the following
/// script's first opcode. Only entries 47..50 are playable sets. Preserve this
/// declaration order and copy length; the cast exposes the stored word span,
/// including non-pointer data. Clip storage stays borrowed until bank playback ends.
static AnimationBankCopyRequest _gActor451100PlayerAnimationBankCopy = { _gActor451100PlayerAnimationExtensionSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Blends the player to installed clip 47 over eight normal-rate frames.
///
/// Resolves the equipped weapon's bank in a request copy and enables grid
/// participation. The installed set and its data must remain loaded for playback.
static AnimationPlayRequest _gActor451100PlayerClip47Blend = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends the player to installed clip 48 over eight normal-rate frames.
///
/// Resolves the equipped weapon's bank in a request copy and enables grid
/// participation. The installed set and its data must remain loaded for playback.
static AnimationPlayRequest _gActor451100PlayerClip48Blend = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Restarts installed player clip 48 without blending for the scene's final pause.
///
/// Resolves the equipped weapon's bank in a request copy and enables grid
/// participation. The installed set and its data must remain loaded for playback.
static AnimationPlayRequest _gActor451100PlayerClip48Reset = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends the player to installed clip 49 over eight normal-rate frames.
///
/// Resolves the equipped weapon's bank in a request copy and enables grid
/// participation. The installed set and its data must remain loaded for playback.
static AnimationPlayRequest _gActor451100PlayerClip49Blend = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 2, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends the player to installed clip 50 over eight normal-rate frames.
///
/// Resolves the equipped weapon's bank in a request copy and enables grid
/// participation. The installed set and its data must remain loaded for playback.
static AnimationPlayRequest _gActor451100PlayerClip50Blend = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 3, ANIMATION_BLEND_INTERPOLATE, ACTOR_451100_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Placed-actor selectors, CAP command slots, draw flags and restored tunnel view.
enum {
    ACTOR_451100_SCENE_QUIET_WALK     = 0,
    ACTOR_451100_SCENE_PAIR_WALK      = 1,
    ACTOR_451100_CAP_STORY_SCENE      = 2,
    ACTOR_451100_CAP_TUNNEL_ENTRY     = 3,
    ACTOR_451100_TUNNEL_GAMEPLAY_VIEW = 4,
};

/// Sound-script key started by the tunnel's warp-2 arrival event.
enum { ACTOR_451100_TUNNEL_ENTRY_SOUND = 0x550C0004 };

/// Plays paired-walker clip 11 and quiet-walker clip 30, then their next clips.
///
/// Requires both placed actors and their animation data to be live. Waits 31
/// interpreter frames, resets quiet clip 31, blends pair clip 12, then returns
/// to a pending script call. The story scene calls this sequence six times;
/// it consumes one return-stack entry and must not be started as a root script.
static EvsCommand _gActor451100WalkerClip11And30Script[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip11Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip30Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip31Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip12Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Scene/audio selection key retained for the submarine-tunnel story scene.
///
/// Selects group 5, stream 11, qualifier 11 (second qualifier 0). The event
/// interpreter keeps this address for CAP scene playback until script end.
static EvsSceneKey _gActor451100SubmarineTunnelSceneKey = { 5, 11, 11 };

EvsCommand gActor451100SubmarineTunnelSceneScript[] = {
    // Install the scene clips and place both walkers before CAP playback.
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor451100SubmarineTunnelSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_451100_CAP_STORY_SCENE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor451100PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100PlayerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100QuietWalkOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100PairWalkOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleReset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip7Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip28Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip9Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip29Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip10Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip8Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip49Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip21Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip2Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip3Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip22Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip48Blend }, { .value = 0 } },
    // CAP cues repeat the same two-walker clip exchange.
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor451100WalkerClip11And30Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor451100WalkerClip11And30Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor451100WalkerClip11And30Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor451100WalkerClip11And30Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor451100WalkerClip11And30Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor451100WalkerClip11And30Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip13Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip32Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 61 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip31Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip12Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkWalkReset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip23Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip48Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip24Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip5Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip21Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip2Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip24Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip5Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip21Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip2Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip24Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip5Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip49Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip35Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip16Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip21Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip2Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip20Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkIdleBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip50Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip33Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip14Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip25Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip6Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Reposition the actors for the closing exchange.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100PlayerClosingScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100QuietWalkClosingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100PairWalkClosingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip34Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip15Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip7Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip26Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip7Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100QuietWalkClip27Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor451100PairWalkClip8Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Hide scene actors and hand presentation back to the room.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100PlayerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip48Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip47Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_451100_TUNNEL_GAMEPLAY_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor451100SubmarineTunnelSceneSkipScript[] = {
    // Abort presentation under the fade, then restore the room's visible state.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_451100_TUNNEL_GAMEPLAY_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor451100PlayerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_QUIET_WALK }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_451100_SCENE_PAIR_WALK }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor451100SubmarineTunnelEntryScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_451100_CAP_TUNNEL_ENTRY }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_451100_TUNNEL_ENTRY_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor451100PlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/eve/body.inc"

#include "assets/rooms/neo_ark/submarine_tunnel/eve_and_no_9_clips_2.inc"

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

/// Synchronous animation, draw, placement, command and walk messages for the quiet walker.
///
/// The spawned task borrows this table. Singleton handlers require its live
/// published task/work pair; the terminal entry returns zero for other ids.
static TaskMessageEntry _gActor451100QuietWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor451100QuietWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor451100QuietWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor451100QuietWalkApplyCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor451100QuietWalkTaskDesc = { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _actor451100QuietWalkTask, { .model = &_gEveBody } };

/// Sparse animation-set table borrowed by the quiet walker's nineteen-part rig.
///
/// Clips 1 and 20 alias the startup set; clips 21 through 35 select the
/// remaining fifteen sets. Slots 0, 2 through 19 and 36 are NULL. Only
/// loaded clips with tracks 1 through 18 may be played; the handler
/// checks the upper bound, but does not reject negative ids or NULL slots.
/// The table and borrowed records remain live throughout playback.
#include "assets/rooms/neo_ark/submarine_tunnel/eve_walker_bank.inc"

#include "assets/enemies/golem/no_9/body_dryfield.inc"

#include "assets/enemies/golem/no_9/radio.inc"

#include "assets/rooms/neo_ark/submarine_tunnel/eve_and_no_9_clips_3.inc"

/// Synchronous messages accepted by the paired walker and its carried model.
///
/// The initialized task borrows this table while this package is loaded.
/// Playback takes `AnimationPlayRequest`, visibility takes draw bits, placement
/// takes `ActorTransform`, commands are ignored, and walking takes a `VECTOR`
/// target. The five callbacks share the task/ID/two-word message ABI; the final
/// sentinel returns zero for unsupported IDs. Never dispatch the sentinel itself.
static TaskMessageEntry _gActor451100PairWalkMessages[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor451100PairWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor451100PairWalkIgnoreCommand },
    { ACTOR_MESSAGE_WALK_TO, _actor451100PairWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor451100PairWalkTaskDescs[2] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor451100PairWalkTask, { .model = &_gGolemNo9BodyDryfield } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor451100CarriedModelTask, { .model = &_gGolemNo9Radio } },
};

/// Animation-set table borrowed by the paired walker's nineteen-part rig.
///
/// Clips 1 through 16 select local sets; slots 0 and 17 are NULL. Idle is
/// clip 1 and translating walk is clip 4. Playback checks only the upper
/// bound, so requests must exclude negative ids and the NULL slots. Sets
/// provide tracks 1 through 18 and stay loaded while the rig uses them.
#include "assets/rooms/neo_ark/submarine_tunnel/no_9_walker_bank.inc"

/// Borrowed pointer to the quiet walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkQuietWork* _gFootstepWalkWork = NULL;

/// Borrowed quiet-walker task published by spawn for singleton animation and draw messages.
///
/// Must belong to the live published work block. This pointer owns nothing;
/// teardown leaves it dangling, so it must not be used after task release.
static Task* _gActor451100QuietWalkTask;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

/// Binds the published quiet walker's rig and queues startup clip 20 for a reset.
///
/// `model` must be a live nineteen-part object returned by `tmdCreateModel`,
/// and `_gFootstepWalkWork` must name its writable task-owned work block.
/// The package's native clip table must have clip 20 loaded with tracks 1 to 18.
/// The context borrows the model allocation's coordinate tail, clip table,
/// slots and word-aligned encoded-pose buffer. Keep the work, model allocation
/// and clip data live throughout playback; this call transfers no ownership.
///
/// Clears the remaining travel and turn-update counts. Slots and poses are left
/// untouched: the caller must process the pending reset before ticking slots
/// 1 to 18. Root slot 0 is not driven by the quiet walker's animation helpers.
static inline void _actor451100QuietWalkPrepareAnimation(TmdObject* model)
{
    enum { ACTOR_451100_QUIET_WALK_STARTUP_CLIP = 20 };

    animationInitContext(&_gFootstepWalkWork->rig.anim, _gActor451100QuietWalkAnimationSets, model,
                         _gFootstepWalkWork->rig.poses, _gFootstepWalkWork->rig.slots);
    _gFootstepWalkWork->st.animId  = ACTOR_451100_QUIET_WALK_STARTUP_CLIP;
    _gFootstepWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    _gFootstepWalkWork->st.travel  = 0;
    _gFootstepWalkWork->turnFrames = 0;
}

/// Initializes the package's quiet nineteen-part scene walker in task state 0.
///
/// Requires a live TMD task with its `Enemy` in `spawnArg2` and no work allocation.
/// The primary heap, view, lighting query and animation scratch/GTE state must be
/// ready; startup clip 20 and tracks 1 to 18 must be loaded. The task owns the
/// zeroed `FootstepWalkQuietWork`; its model borrows the matrices and rig storage.
/// Publishes the work and task for singleton handlers, resets the startup clip
/// without a pose tick, and advances to state 1 with travel and turning cleared.
/// Allocation failure destroys the enemy and begins task teardown immediately.
/// The exit callback releases the enemy and starts task/work/model teardown;
/// the published pointers must not be used afterwards.
static void _actor451100QuietWalkSpawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_451100_QUIET_WALK_OT_OFFSET             = 1,
           ACTOR_451100_QUIET_WALK_LIGHT_SAMPLE_Y_OFFSET = 800 };

    VECTOR3                lightingSample;
    FootstepWalkQuietWork* allocatedWork;
    TmdObject*             model;
    GfxCoord*              rootCoord;

    model              = task->extra.tmd;
    rootCoord          = model->coords;
    allocatedWork      = memCalloc(sizeof(*allocatedWork), false);
    _gFootstepWalkWork = allocatedWork;
    task->work         = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The task owns the block; the model borrows its lighting matrices.
    task->exitCallback               = _actor451100QuietWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = false;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_451100_QUIET_WALK_OT_OFFSET;
    model->lightMtx                  = &_gFootstepWalkWork->light;
    model->colorMtx                  = &_gFootstepWalkWork->color;
    // Preserve the existing cached position and its composition frame.
    lightingSample.vx          = rootCoord->workm.t[0];
    lightingSample.vy          = rootCoord->workm.t[1] - ACTOR_451100_QUIET_WALK_LIGHT_SAMPLE_Y_OFFSET;
    _gActor451100QuietWalkTask = task;
    lightingSample.vz          = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, ARRAY_SIZE(model->lightMtx->m));
    // Reset driven slots now; the first frame writes their poses.
    _actor451100QuietWalkPrepareAnimation(model);
    task->msgTable = _gActor451100QuietWalkMessages;
    _footstepWalkQuietUpdate(task);
    task->state += 1;
}

#include "../../shared/walk/footstep_walk_quiet_update.inc.c"

/// Dispatches initialization and frames for the package's quiet scene walker.
///
/// Task state must be 0 (spawn) or 1 (lighting, movement/animation and shadow).
/// Requires a live nineteen-part TMD model and `Enemy` in `spawnArg2`. Refreshes the
/// published work before every state call; after spawning, the model, work,
/// message table and clips must stay live. Only one published walker can receive
/// singleton messages at a time. A failed spawn may tear the task down.
static void _actor451100QuietWalkTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor451100QuietWalkSpawn,
        _actorRenderWalkerFrame,
    };

    _gFootstepWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(`Enemy`*, Task*) function declared in the prologue.
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
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the quiet scene walker's enemy and begins task teardown.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2`. The task releases
/// its work and model through default teardown; the published work and task
/// pointers remain dangling and must not be used after this call.
static void _actor451100QuietWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walk/footstep_walk_tick_anim.inc.c"

#include "../../shared/walk/footstep_walk_quiet.inc.c"

/// Applies an animation request synchronously to the published quiet walker.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` with live published task/work and a
/// borrowed, word-aligned request. Returns -1 for IDs 37 and above without
/// changing playback; accepted IDs must still select loaded clips (1 or 20..35)
/// with tracks 1 to 18. Negative IDs and empty entries are not checked.
/// Nonzero blend captures initialized poses and narrows `blendFrames` to a signed
/// halfword in whole normal-rate frames (0..2047 avoids blend-time overflow).
/// Reset ignores that duration. Source-bank and collision options are ignored.
/// Returns 0 after applying the reset/blend; retains no request pointer.
/// The receiver, message ID and second payload are ignored.
static s32 _actor451100QuietWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_451100_QUIET_WALK_CLIP_LIMIT = ARRAY_SIZE(_gActor451100QuietWalkAnimationSets) };

    if (request->animationId < ACTOR_451100_QUIET_WALK_CLIP_LIMIT) {
        _gFootstepWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gFootstepWalkBlendFrames    = request->blendFrames;
        } else {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gFootstepWalkWork->st.field_6 = 0;
        _footstepWalkQuietUpdate(_gActor451100QuietWalkTask);
        return 0;
    }
    return -1;
}

/// Replaces the published quiet walker's model flags from visibility bits.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` with a live published TMD task.
/// `ACTOR_MESSAGE_PAIR_SHOW` clears all flags; without it, only active-draw
/// exclusion remains. `ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER` adds automatic-buffer
/// exclusion. Other bits are ignored, and no buffer is allocated or released.
/// The receiver, message ID and second payload are ignored. Returns 0.
static s32 _actor451100QuietWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* model;

    model = _gActor451100QuietWalkTask->extra.tmd;
    if (drawFlags & ACTOR_MESSAGE_PAIR_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/footstep_walk_place.inc.c"

/// Schedules twenty turning updates for command 0 on the published quiet walker.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with live published work and a borrowed,
/// two-byte-aligned `ActorCommand`. Context tags are ignored. Command 0 replaces
/// the remaining turn count; other commands do nothing. Turning consumes one
/// count and adds 51/4096 turns only while the turn clip plays in tick state;
/// this request neither selects that clip nor updates the model immediately.
/// The receiver, message ID and second payload are ignored. Always returns 0.
static s32 _actor451100QuietWalkApplyCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum { ACTOR_451100_QUIET_WALK_COMMAND_TURN = 0,
           ACTOR_451100_QUIET_WALK_TURN_UPDATES = 20 };

    if (command->command == ACTOR_451100_QUIET_WALK_COMMAND_TURN) {
        _gFootstepWalkWork->turnFrames = ACTOR_451100_QUIET_WALK_TURN_UPDATES;
    }
    return 0;
}

/// Selects this quiet walker's allocated work type for the walk-target handler.
///
/// The receiver owns a live `FootstepWalkQuietWork`; the fragment writes its
/// signed-halfword `st.yaw` (4096 units per turn) and `st.travel` (whole moving
/// updates). This type-name alias evaluates no expressions and lasts one inclusion.
#define FOOTSTEP_WALK_WORK_T FootstepWalkQuietWork
#include "../../shared/walk/footstep_walk_to.inc.c"
#undef FOOTSTEP_WALK_WORK_T

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Binds a pair walker's rig and queues idle clip 1 for a reset.
///
/// `work` must be the walker's writable, zeroed task-owned block, and `model`
/// a live nineteen-part object returned by `tmdCreateModel`. The package's
/// word-aligned native clip table must have clip 1 loaded with tracks 1 to 18.
/// The context borrows the model allocation's coordinate tail, clip table,
/// slots and word-aligned encoded-pose buffer. Keep the work, model allocation
/// and clip data live throughout playback; this call transfers no ownership.
///
/// Slots and poses are left untouched: the caller must process the pending
/// reset before ticking slots 1 to 18. Root slot 0 is not driven by the pair
/// walker's animation helpers. Travel and blend duration retain their values.
static inline void _actor451100PairWalkPrepareAnimation(PairWalkWork* work, TmdObject* model)
{
    animationInitContext(&work->rig.anim, _gActor451100PairWalkAnimationSets, model, work->rig.poses,
                         work->rig.slots);
    work->st.animId = PAIR_WALK_ANIM_IDLE;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
}

/// Initializes the package's paired scene walker and its carried model.
///
/// Requires a live nineteen-part TMD task with its `Enemy` in `spawnArg2` and no
/// work allocation. The primary heap, view, lighting query and animation
/// scratch/GTE state must be ready, with idle clip 1 and tracks 1 to 18 loaded.
/// The task owns its zeroed `PairWalkWork`. Descriptor 1 supplies a carried-model
/// task parented under this task; child spawning is assumed to succeed.
/// Both models borrow the parent's light/color matrices. Resets the idle clip
/// without a pose tick and advances to state 1. Work-allocation failure destroys
/// the enemy and begins task teardown. Default teardown exits the child before
/// releasing the parent work; model and clip storage must remain live meanwhile.
static void _actor451100PairWalkSpawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_451100_PAIR_WALK_OT_OFFSET             = 1,
           ACTOR_451100_PAIR_WALK_LIGHT_SAMPLE_Y_OFFSET = 800,
           ACTOR_451100_CARRIED_MODEL_DESCRIPTOR        = 1 };

    VECTOR3       lightingSample;
    PairWalkWork* work;
    GfxCoord*     rootCoord;
    TmdObject*    model;
    Enemy*        carriedEnemy;
    PairWalkWork* allocatedWork;

    model         = task->extra.tmd;
    rootCoord     = model->coords;
    allocatedWork = memCalloc(sizeof(*allocatedWork), false);
    work          = allocatedWork;
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The task owns the work and the carried model belongs to its teardown tree.
    task->exitCallback               = _actor451100PairWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = false;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_451100_PAIR_WALK_OT_OFFSET;
    work->enemy                      = enemy;
    carriedEnemy                     = enemySpawnFromTable(gActor451100PairWalkTaskDescs, ACTOR_451100_CARRIED_MODEL_DESCRIPTOR, 0, enemy);
    taskReparent(task, carriedEnemy->task);
    work->pairTask  = carriedEnemy->task;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    // Sample the cached root position before the first animation update.
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1] - ACTOR_451100_PAIR_WALK_LIGHT_SAMPLE_Y_OFFSET;
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, ARRAY_SIZE(model->lightMtx->m));
    _actor451100PairWalkPrepareAnimation(work, model);
    task->msgTable = _gActor451100PairWalkMessages;
    _pairWalkUpdate(task);
    task->state += 1;
}

#include "../../shared/walk/pair_walk_update.inc.c"

/// Dispatches initialization and frames for the package's paired scene walker.
///
/// Task state must be 0 (spawn) or 1 (lighting, movement/animation and shadow).
/// Requires a live nineteen-part TMD task and its `Enemy` in `spawnArg2`; after
/// spawning, `PairWalkWork` and the carried model must remain live. Frame updates
/// use this task's work directly. A failed work allocation may tear the task down.
static void _actor451100PairWalkTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor451100PairWalkSpawn,
        _actorRenderWalkerFrameSecond,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(`Enemy`*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrameSecond
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER             _pairWalkUpdate
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the paired scene walker's enemy and begins task teardown.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2`. Default teardown
/// exits the carried model before releasing the parent's work, then arranges
/// model release. Neither the task nor enemy may be accessed afterwards.
static void _actor451100PairWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/walk/pair_walk.inc.c"

/// Applies an animation request synchronously to the paired scene walker.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` on a live TMD task with `PairWalkWork` and
/// its rig bound to loaded tracks 1 to 18. Borrows the word-aligned request only
/// during dispatch. Returns -1 for IDs 18 and above without changing playback;
/// accepted IDs must select loaded clips 1..16. Negative IDs and empty entries
/// are not checked. Source-bank and collision options are ignored.
/// Nonzero blend captures initialized poses and narrows `blendFrames` to a signed
/// halfword in whole normal-rate frames (0..2047 avoids blend-time overflow).
/// Reset ignores that duration. Returns 0 after applying the reset/blend.
/// The message ID and second payload are ignored.
static s32 _actor451100PairWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_451100_PAIR_WALK_CLIP_LIMIT = ARRAY_SIZE(_gActor451100PairWalkAnimationSets) };
    PairWalkWork* work;

    work = task->work;
    if (request->animationId < ACTOR_451100_PAIR_WALK_CLIP_LIMIT) {
        work->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = request->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        _pairWalkUpdate(task);
        return 0;
    }
    return -1;
}

#include "../../shared/walk/pair_walk_visibility.inc.c"

/// Accepts paired-walker actor commands without taking any action.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` and returns 0. All arguments are ignored,
/// including the borrowed command pointer; no payload storage is read or retained.
static s32 _actor451100PairWalkIgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

/// Faces the paired walker toward a destination and records its travel ticks.
///
/// Handles ACTOR_MESSAGE_WALK_TO on a live TMD task with `PairWalkWork`. Borrows a
/// word-aligned target through dispatch, reading only X/Z in the root parent's
/// coordinate frame. Y is ignored. Replaces the rotation with a unit-scale yaw
/// and records the signed-halfword heading in 4096 units per turn.
/// Stores floor(horizontal distance / 17) in signed-halfword travel, without
/// clamping. Differences and their squared sum must fit signed 32 bits; the tick
/// count must fit 0..32767. Does not invalidate composition or select a walk
/// clip; a separate animation request starts movement. The target pointer is not
/// retained. The message ID and second payload are ignored. Returns 0.
static s32 _actor451100PairWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 unusedArgument)
{
    enum { ACTOR_451100_PAIR_WALK_DISTANCE_PER_TICK = 17 };
    GfxCoord*     rootCoord;
    PairWalkWork* work;
    s32           deltaX;
    s32           deltaZ;
    s16           yaw;

    rootCoord    = task->extra.tmd->coords;
    work         = task->work;
    deltaX       = target->vx - rootCoord->coord.t[0];
    deltaZ       = target->vz - rootCoord->coord.t[2];
    yaw          = ratan2(deltaX, deltaZ);
    work->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    work->st.travel = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ) / ACTOR_451100_PAIR_WALK_DISTANCE_PER_TICK;
    return 0;
}

/// Attaches the paired walker's carried model to part 8 and keeps it following.
///
/// Requires a live TMD child task whose parent has a nineteen-part TMD model and
/// `PairWalkWork`; both outlive the child. State 0 borrows the parent's light/color
/// matrices, links the child's root to part 8 and enters state 1. Both states
/// invalidate the root composition each frame. The child owns no animation work
/// or lighting matrices; the parent's teardown tree controls its lifetime.
static void _actor451100CarriedModelTask(Task* task)
{
    enum { ACTOR_451100_CARRIED_MODEL_ATTACH = 0,
           ACTOR_451100_CARRIED_MODEL_FOLLOW = 1,
           ACTOR_451100_CARRIED_MODEL_PART   = 8 };

    char          unusedStack[0x10]; // Retains the otherwise unused 16-byte stack frame.
    Task*         parentTask      = task->parent;
    TmdObject*    model           = task->extra.tmd;
    GfxCoord*     rootCoord       = model->coords;
    GfxCoord*     attachmentCoord = &parentTask->extra.tmd->coords[ACTOR_451100_CARRIED_MODEL_PART];
    PairWalkWork* parentWork      = parentTask->work;

    switch (task->state) {
        case ACTOR_451100_CARRIED_MODEL_ATTACH:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            model->lightMtx         = &parentWork->light;
            model->colorMtx         = &parentWork->color;
            rootCoord->parent       = attachmentCoord;
            task->state++;
            break;
        case ACTOR_451100_CARRIED_MODEL_FOLLOW:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
