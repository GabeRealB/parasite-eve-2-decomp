#include "actors/actor_460200.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "../../shared/screen/screen_negative.h"
#include "../../shared/walk/paced_walk.h"
#include "../../shared/walk/walker.h"
#include "../../shared/walk/stride_walk.h"

/// Saved tent conversation progression; the fourth conversation repeats.
enum {
    ACTOR_460200_TALK_FIRST  = 0,
    ACTOR_460200_TALK_SECOND = 1,
    ACTOR_460200_TALK_THIRD  = 2,
    ACTOR_460200_TALK_REPEAT = 3,
};

enum {
    ACTOR_460200_PLAYER_EXTENSION_SET_COUNT = 15,
    ACTOR_460200_SOLDIER_C_SET_COUNT        = 18,
    ACTOR_460200_PLAYER_CLIP1_BLEND_FRAMES  = 8,
};

/// Local clip-table keys carried by the tent scene playback requests.
enum {
    ACTOR_460200_PLAYER_CLIP_1             = 1,
    ACTOR_460200_PLAYER_CLIP_47            = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_460200_PLAYER_CLIP_48            = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_460200_PLAYER_CLIP_49            = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_460200_PLAYER_CLIP_50            = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_460200_PLAYER_CLIP_51            = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_460200_PLAYER_CLIP_52            = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_460200_PLAYER_CLIP_53            = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_460200_PLAYER_CLIP_54            = ANIMATION_BANK_BASE_SET_COUNT + 7,
    ACTOR_460200_PLAYER_CLIP_55            = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_460200_PLAYER_CLIP_56            = ANIMATION_BANK_BASE_SET_COUNT + 9,
    ACTOR_460200_PLAYER_CLIP_57            = ANIMATION_BANK_BASE_SET_COUNT + 10,
    ACTOR_460200_PLAYER_CLIP_61            = ANIMATION_BANK_BASE_SET_COUNT + 14,
    ACTOR_460200_SOLDIER_A_CLIP_13         = 13,
    ACTOR_460200_SOLDIER_A_CLIP_14         = 14,
    ACTOR_460200_SOLDIER_A_CLIP_15         = 15,
    ACTOR_460200_SOLDIER_B_CLIP_1          = 1,
    ACTOR_460200_SOLDIER_B_CLIP_4          = 4,
    ACTOR_460200_SOLDIER_B_CLIP_5          = 5,
    ACTOR_460200_SOLDIER_B_CLIP_6          = 6,
    ACTOR_460200_SOLDIER_B_CLIP_7          = 7,
    ACTOR_460200_SOLDIER_B_CLIP_8          = 8,
    ACTOR_460200_SOLDIER_B_CLIP_9          = 9,
    ACTOR_460200_SOLDIER_B_CLIP_10         = 10,
    ACTOR_460200_SOLDIER_C_CLIP_2          = 2,
    ACTOR_460200_SOLDIER_C_CLIP_3          = 3,
    ACTOR_460200_SOLDIER_C_CLIP_4          = 4,
    ACTOR_460200_SOLDIER_C_CLIP_5          = 5,
    ACTOR_460200_SOLDIER_C_CLIP_6          = 6,
    ACTOR_460200_SOLDIER_C_CLIP_7          = 7,
    ACTOR_460200_SOLDIER_C_CLIP_8          = 8,
    ACTOR_460200_SOLDIER_C_CLIP_9          = 9,
    ACTOR_460200_SOLDIER_C_CLIP_10         = 10,
    ACTOR_460200_SOLDIER_C_CLIP_11         = 11,
    ACTOR_460200_SOLDIER_C_CLIP_12         = 12,
    ACTOR_460200_SOLDIER_C_CLIP_13         = 13,
    ACTOR_460200_SOLDIER_C_CLIP_14         = 14,
    ACTOR_460200_SOLDIER_C_CLIP_15         = 15,
    ACTOR_460200_SOLDIER_C_CLIP_16         = 16,
    ACTOR_460200_SOLDIER_C_CLIP_17         = 17,
    ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX = 0,
    ACTOR_460200_SOLDIER_A_PLACEMENT_INDEX = 2,
};

/// CAP command-table slots for the tent conversations.
enum {
    ACTOR_460200_CAP_SOLDIER_C_FIRST_TALK  = 7,
    ACTOR_460200_CAP_SOLDIER_C_SECOND_TALK = 8,
    ACTOR_460200_CAP_SOLDIER_C_THIRD_TALK  = 9,
    ACTOR_460200_CAP_SOLDIER_C_REPEAT_TALK = 10,
    ACTOR_460200_CAP_SOLDIER_A_FIRST_TALK  = 11,
    ACTOR_460200_CAP_SOLDIER_A_SECOND_TALK = 12,
    ACTOR_460200_CAP_SOLDIER_A_THIRD_TALK  = 13,
    ACTOR_460200_CAP_SOLDIER_A_REPEAT_TALK = 14,
};

static s32 _pacedWalkSetSoldierCModelDraw(Task* task, s32 messageId, s32 flags, s32 unusedArg);

// Message-table callbacks use the argument views required by this TU.

static s32 _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 requestFlags, s32 unusedArg);

static void _actorRenderDrawSecondFixedWalkerGroundShadow(Task* task);
static void _pacedWalkTickSoldierBAnim(Task* task);
static void _pacedWalkResetSoldierBAnim(Task* task);
static void _pacedWalkBlendSoldierBAnim(Task* task);
static void _pacedWalkUpdateSoldierC(Task* task);
static void _pacedWalkSpawnSoldierC(Enemy* enemy, Task* task);
static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _pacedWalkExitSoldierC(Task* task);
static void _actorRenderDrawThirdFixedWalkerGroundShadow(Task* task);
static void _pacedWalkTickSoldierCAnim(Task* task);
static void _pacedWalkResetSoldierCAnim(Task* task);
static void _pacedWalkBlendSoldierCAnim(Task* task);

static TmdSource _gSoldierBRifle;
static TmdSource _gSoldierBBody;
static void      _actor460200SoldierBTask(Task* task);

static s32 _pacedWalkPlaceSoldierB(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32 _strideWalkSetTurnMode(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);

static TmdSource _gSoldierCBody;
static s32       _pacedWalkPlaySoldierCAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32       _pacedWalkPlaceSoldierC(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32       _actor460200IgnoreSoldierCCommand(Task* unusedTask, s32 unusedMessageId, const ActorCommand* unusedCommand, s32 unusedArgument);
static s32       _pacedWalkSetSoldierCWalkTarget(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument);
static void      _actor460200SoldierCTask(Task* task);

static s32  _pacedWalkStartSmoking(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static void _actor460200SoldierATask(Task* task);

static void _actor460200ModeExitCountdownTask(Task* task);
static void _actor460200SelectCaptionFile(s32 dataResourceOrdinal);
static void _screenNegativeFilterCutscene(void);
static void _actor460200SetSceneEvent(s8 sceneEvent);

static void _actor460200NegativeCaptureTask(Task* task);

static AnimationSet _gShelter1fTentSoldierCWalkIdleClip;
static AnimationSet _gShelter1fTentSoldierCIdleClip;
static AnimationSet _gShelter1fTentSoldierCTalk1Clip;
static AnimationSet _gShelter1fTentSoldierCTalk1EndClip;
static AnimationSet _gShelter1fTentSoldierCTalk2Clip;
static AnimationSet _gShelter1fTentSoldierCTalk2EndClip;
static AnimationSet _gShelter1fTentSoldierCArrival1Clip;
static AnimationSet _gShelter1fTentSoldierCArrival3Clip;
static AnimationSet _gShelter1fTentSoldierCArrival4Clip;
static AnimationSet _gShelter1fTentSoldierCArrival5Clip;
static AnimationSet _gShelter1fTentSoldierCArrival6Clip;
static AnimationSet _gShelter1fTentSoldierCArrival7Clip;
static AnimationSet _gShelter1fTentSoldierCArrival2Clip;
static AnimationSet _gShelter1fTentSoldierCArrival8Clip;
static AnimationSet _gShelter1fTentSoldierCArrival9Clip;
static AnimationSet _gShelter1fTentSoldierCArrival10Clip;
static AnimationSet _gShelter1fTentSoldierCArrival11Clip;

#include "assets/rooms/shelter/1f_tent/soldiers_clips_1.inc"

/// Unspawned negative-frame capture recipe followed by a descriptor terminator.
///
/// Only entry 0 can spawn. The callback requires writable
/// `ScreenNegativeCaptureArgs` in spawnArg2 through completion; spawnArg1 and
/// descriptor data are unused. No spawner establishes the argument's owner.
static TaskDesc _gActor460200UnusedNegativeCaptureTaskDescs[2] = {
    {
        .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
        .callback = _actor460200NegativeCaptureTask,
        .data     = { .value = 0 },
    },
    {
        .header   = { .fields = { TASK_DESC_END, 0 } },
        .callback = NULL,
        .data     = { .value = 0 },
    },
};

/// Fixed VRAM rectangle read as one contiguous RGB555 frame in negative debug mode.
static RECT _gScreenNegativeFrameRect = { 0, 0, FILE_SYSTEM_IMAGE_WIDTH, FILE_SYSTEM_IMAGE_HEIGHT };

/// Mutable VRAM read rectangle for one displayed RGB555 frame strip.
///
/// Capture selects the displayed buffer's Y origin and advances X by strip
/// width. The capture task owns these coordinates through the transfer.
static RECT _gScreenNegativeStripRect = { 0, 0, FILE_SYSTEM_IMAGE_STRIP_WIDTH, FILE_SYSTEM_IMAGE_HEIGHT };

/// Blends the equipped player into clip 1 over eight frames, disabling world-grid participation.
///
/// Event playback replaces the stored source selector with the equipped
/// weapon bank; the request and clip data belong to this loaded package.
static AnimationPlayRequest _gActor460200PlayerClip1BlendRequest = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_1, ANIMATION_BLEND_INTERPOLATE, ACTOR_460200_PLAYER_CLIP1_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clips for the tent scenes, extended ids 47..61; 58..60 are NULL.
///
/// Each player-animation script installs them before playback. The copy reads
/// 32 entries, past these fifteen into the copy request and the Soldier B
/// requests that follow; those words land in extension ids 62..78 and are never
/// played. The clips remain valid while this package is loaded.
#include "assets/rooms/shelter/1f_tent/player_extension_bank.inc"

/// Installs the tent-scene clips; reads 32 entries from a fifteen-clip list.
static AnimationBankCopyRequest _gActor460200PlayerAnimationBankCopy = { _gActor460200PlayerExtensionSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Soldier B clip 1; not dispatched by this package.
static AnimationPlayRequest _gActor460200SoldierBClip1Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier B clip 4 without blending.
static AnimationPlayRequest _gActor460200SoldierBClip4Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier B clip 5 without blending.
static AnimationPlayRequest _gActor460200SoldierBClip5Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier B's clip 6 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierBClip6Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier B's clip 7 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierBClip7Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier B's clip 8 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierBClip8Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier B's clip 9 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierBClip9Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier B's clip 10 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierBClip10Request = { { .index = 1 }, ACTOR_460200_SOLDIER_B_CLIP_10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier A's clip 13 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierAClip13Request = { { .index = 1 }, ACTOR_460200_SOLDIER_A_CLIP_13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier A's clip 14 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierAClip14Request = { { .index = 1 }, ACTOR_460200_SOLDIER_A_CLIP_14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier A's clip 15 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierAClip15Request = { { .index = 1 }, ACTOR_460200_SOLDIER_A_CLIP_15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 61 without blending, disabling world-grid participation.
///
/// Event playback replaces the stored source selector with the equipped
/// weapon bank; the request and clip data belong to this loaded package.
static AnimationPlayRequest _gActor460200PlayerClip61Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Unplayed duplicate of the player's clip-47 reset preset.
///
/// Bank selector 1 and disabled world collision match the adjacent dispatched
/// request. The player receiver is inferred; this record itself has no dispatch.
static AnimationPlayRequest _gActor460200UnusedPlayerClip47Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_460200_PLAYER_CLIP_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the equipped player's clip 47 without blending, disabling world-grid participation.
///
/// Event playback replaces the stored source selector with the equipped
/// weapon bank; the request and clip data belong to this loaded package.
static AnimationPlayRequest _gActor460200PlayerClip47Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 48 without blending, disabling world-grid participation.
///
/// Event playback replaces the stored source selector with the equipped
/// weapon bank; the request and clip data belong to this loaded package.
static AnimationPlayRequest _gActor460200PlayerClip48Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 49 without blending, disabling world-grid participation.
///
/// Event playback replaces the stored source selector with the equipped
/// weapon bank; the request and clip data belong to this loaded package.
static AnimationPlayRequest _gActor460200PlayerClip49Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 50 without blending, disabling world-grid participation.
///
/// Event playback replaces the stored source selector with the equipped
/// weapon bank; the request and clip data belong to this loaded package.
static AnimationPlayRequest _gActor460200PlayerClip50Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 51 without blending, disabling world-grid participation.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// The installed extension clips belong to this package and must remain loaded
/// through playback.
static AnimationPlayRequest _gActor460200PlayerClip51Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 52 without blending, disabling world-grid participation.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// The installed extension clips belong to this package and must remain loaded
/// through playback.
static AnimationPlayRequest _gActor460200PlayerClip52Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 53 without blending, disabling world-grid participation.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// The installed extension clips belong to this package and must remain loaded
/// through playback.
static AnimationPlayRequest _gActor460200PlayerClip53Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 54 without blending, disabling world-grid participation.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// The installed extension clips belong to this package and must remain loaded
/// through playback.
static AnimationPlayRequest _gActor460200PlayerClip54Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 55 without blending, disabling world-grid participation.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// The installed extension clips belong to this package and must remain loaded
/// through playback.
static AnimationPlayRequest _gActor460200PlayerClip55Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 56 without blending, disabling world-grid participation.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// The installed extension clips belong to this package and must remain loaded
/// through playback.
static AnimationPlayRequest _gActor460200PlayerClip56Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the equipped player's clip 57 without blending, disabling world-grid participation.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// The installed extension clips belong to this package and must remain loaded
/// through playback.
static AnimationPlayRequest _gActor460200PlayerClip57Request = { { .index = 1 }, ACTOR_460200_PLAYER_CLIP_57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

enum {
    ACTOR_460200_SOLDIER_C_CLIP_0 = 0,
    ACTOR_460200_SOLDIER_C_CLIP_1 = 1,
};

/// Unplayed clip-0/1 reset presets preceding Soldier C's dispatched requests.
///
/// The receiver and original two-record grouping are unproven. Soldier C's
/// bank has NULL at clip 0 and his play handler ignores source and collision.
static AnimationPlayRequest _gActor460200UnusedSoldierCClip0To1Requests[2] = {
    { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts Soldier C's clip 2 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierCClip2Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 3 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierCClip3Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 4 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierCClip4Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 5 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierCClip5Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 6 without blending.
///
/// The receiver ignores the stored bank selector and collision choice. The
/// request is borrowed through synchronous dispatch while this package is loaded.
static AnimationPlayRequest _gActor460200SoldierCClip6Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 7 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip7Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 8 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip8Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 9 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip9Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 10 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip10Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 11 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip11Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 12 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip12Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 13 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip13Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 14 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip14Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 15 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip15Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 16 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip16Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Soldier C's clip 17 without blending for the tent cutscene.
///
/// The receiver reads the clip and reset choice during dispatch, ignoring the
/// bank selector, blend duration and collision choice. The clip data must
/// remain loaded through playback.
static AnimationPlayRequest _gActor460200SoldierCClip17Request = { { .index = 1 }, ACTOR_460200_SOLDIER_C_CLIP_17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player position and orientation at the start of the tent cutscene.
///
/// Position uses room-coordinate units; Euler angles use 4096 units per turn.
/// The placement is borrowed only through synchronous dispatch.
static ActorTransform _gActor460200PlayerCutsceneStartPlacement = { { -910, 0, 5650, 0 }, { 0, -1592, 0, 0 } };

/// Player placement restored by both normal and skipped tent cutscene endings.
///
/// Position is in room-coordinate units; Euler angles use 4096 units per turn.
static ActorTransform _gActor460200PlayerPostCutscenePlacement = { { -1050, 0, 5418, 0 }, { 0, -1592, 0, 0 } };

/// Soldier C's initial placement, reused when the tent cutscene restores this staging.
///
/// The receiver applies room-coordinate XYZ and yaw in 4096 units per turn,
/// ignoring pitch and roll. The placement is borrowed only through dispatch.
static ActorTransform _gActor460200SoldierCCutsceneStartPlacement = { { -2100, 0, 4600, 0 }, { 0, 398, 0, 0 } };

/// Soldier C's alternate cutscene heading at his initial position.
///
/// The receiver applies room-coordinate XYZ and yaw in 4096 units per turn,
/// ignoring pitch and roll. The placement is borrowed only through dispatch.
static ActorTransform _gActor460200SoldierCCutsceneAlternateFacingPlacement = { { -2100, 0, 4600, 0 }, { 0, 341, 0, 0 } };

/// Soldier C's repositioned stance during the two-soldier cutscene sequence.
///
/// The receiver applies room-coordinate XYZ and yaw in 4096 units per turn,
/// ignoring pitch and roll. The placement is borrowed only through dispatch.
static ActorTransform _gActor460200SoldierCCutsceneRepositionPlacement = { { -2000, 0, 3800, 0 }, { 0, 1479, 0, 0 } };

/// Soldier C's tent placement restored on setup and after either cutscene ending.
///
/// Position is in room-coordinate units; yaw is a half turn toward -Z.
static ActorTransform _gActor460200SoldierCTentPlacement = { { -2000, 0, 4700, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Soldier B's placement before his first cutscene walk.
///
/// Position uses room-coordinate units; yaw is a quarter turn toward -X.
/// The placement is borrowed only through synchronous dispatch.
static ActorTransform _gActor460200SoldierBCutsceneEntryPlacement = { { 800, 0, 4490, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Soldier B's first horizontal destination in the tent cutscene.
///
/// The walk handler reads only room-coordinate X/Z, deriving its heading and
/// travel count. Height and all angles are unused; no target pointer is retained.
static ActorTransform _gActor460200SoldierBFirstWalkTarget = { { -250, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

/// Soldier B's second horizontal destination in the tent cutscene.
///
/// The walk handler reads only room-coordinate X/Z, deriving its heading and
/// travel count. Height and all angles are unused; no target pointer is retained.
static ActorTransform _gActor460200SoldierBSecondWalkTarget = { { -1000, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

/// Soldier B's repositioned stance during the two-soldier cutscene sequence.
///
/// The receiver applies room-coordinate XYZ and yaw in 4096 units per turn,
/// ignoring pitch and roll. The placement is borrowed only through dispatch.
static ActorTransform _gActor460200SoldierBCutsceneRepositionPlacement = { { -1300, 0, 3300, 0 }, { 0, -568, 0, 0 } };

/// Unspawned bodyless recipe for delayed stage-mode teardown.
///
/// Entry 0 takes a signed tick countdown in spawnArg1; a nonnegative N requests
/// exit on invocation N+2. spawnArg2 and descriptor data are unused. This
/// overlay's callback must remain loaded until task completion.
static TaskDesc _gActor460200UnusedModeExitCountdownTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = _actor460200ModeExitCountdownTask,
    .data     = { .value = 0 },
};

enum {
    ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX = 1,
    ACTOR_460200_ARRIVAL_CAP_RESOURCE      = 2,
    ACTOR_460200_ARRIVAL_CAP_COMMAND       = 1,
    ACTOR_460200_POST_ARRIVAL_VIEW         = 2,
    ACTOR_460200_POST_ARRIVAL_SCENE_EVENT  = 28,
    ACTOR_460200_ARRIVAL_SOUND_CUE_7       = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 7),
    ACTOR_460200_ARRIVAL_SOUND_CUE_8       = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 8),
};

EvsCommand gActor460200TentArrivalScript[233] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor460200SelectCaptionFile }, { .value = ACTOR_460200_ARRIVAL_CAP_RESOURCE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_ARRIVAL_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor460200PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 1 }, { .value = 6 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200PlayerCutsceneStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCCutsceneStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_A_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCCutsceneAlternateFacingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_460200_ARRIVAL_SOUND_CUE_7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierBCutsceneEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor460200SoldierBFirstWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor460200SoldierBSecondWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCCutsceneStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_460200_ARRIVAL_SOUND_CUE_8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCCutsceneRepositionPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierBCutsceneRepositionPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierBClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA_EXIT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _screenNegativeFilterCutscene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = gPadScriptTransitionCommands }, { .vibrationSegments = gPadScriptTransitionSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCCutsceneStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCCutsceneAlternateFacingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_460200_POST_ARRIVAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor460200PlayerPostCutscenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCTentPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_A_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor460200SelectCaptionFile }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_SHELTER_1F_TENT_AMBIENCE_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_SHELTER_1F_TENT_AMBIENCE_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor460200SetSceneEvent }, { .value = ACTOR_460200_POST_ARRIVAL_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor460200TentArrivalSkipScript[22] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_460200_ARRIVAL_SOUND_CUE_7 }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_460200_ARRIVAL_SOUND_CUE_8 }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor460200SetSceneEvent }, { .value = ACTOR_460200_POST_ARRIVAL_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_460200_POST_ARRIVAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor460200SoldierCTentPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor460200PlayerPostCutscenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_B_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_A_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor460200SelectCaptionFile }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_SHELTER_1F_TENT_AMBIENCE_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_SHELTER_1F_TENT_AMBIENCE_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier C's first tent conversation.
///
/// Starts CAP dialogue and installs player extension clips, then resets the
/// player to clip 61 and sequences Soldier C's clips 5 and 6 at CAP cues.
/// A 19-update countdown precedes clip 2 and player-control release.
/// The commands, called scripts and clip data stay loaded through playback.
static EvsCommand _gActor460200SoldierCFirstTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_C_FIRST_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor460200PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier C's second tent conversation.
///
/// Starts CAP dialogue and installs player extension clips, then resets the
/// player to clip 61 and sequences Soldier C's clips 5 and 6 at CAP cues.
/// A 19-update countdown precedes clip 2 and player-control release.
/// The commands, called scripts and clip data stay loaded through playback.
static EvsCommand _gActor460200SoldierCSecondTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_C_SECOND_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor460200PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier C's third tent conversation.
///
/// Starts CAP dialogue and installs player extension clips, then blends the
/// player into clip 1 while Soldier C plays clips 3 and 4 across a CAP cue.
/// A 19-update countdown precedes clip 2 and player-control release.
/// The commands, called scripts and clip data stay loaded through playback.
static EvsCommand _gActor460200SoldierCThirdTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_C_THIRD_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor460200PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier C's repeating fourth tent conversation.
///
/// Starts CAP dialogue and installs player extension clips, then blends the
/// player into clip 1 while Soldier C plays clips 3 and 4 across a CAP cue.
/// A 19-update countdown precedes clip 2 and player-control release.
/// The commands, called scripts and clip data stay loaded through playback.
static EvsCommand _gActor460200SoldierCRepeatTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_C_REPEAT_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor460200PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_C_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor460200SoldierCClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Shared animation choreography for Soldier A's four tent conversations.
///
/// The caller starts CAP dialogue before entering this subscript. It installs
/// player clips, sequences Soldier A and the equipped player at CAP cues,
/// restores Soldier A to clip 13, releases player scripted control, and returns.
/// The interpreter and copied clip pointers borrow this package through playback.
static EvsCommand _gActor460200SoldierATalkScript[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor460200PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_A_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor460200SoldierAClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor460200PlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_A_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor460200SoldierAClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_460200_SOLDIER_A_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gActor460200SoldierAClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Soldier A's first tent conversation.
///
/// Starts its CAP dialogue, calls `_gActor460200SoldierATalkScript` for the
/// shared animation choreography and player-control release, then ends.
/// The commands, called scripts and clip data stay loaded through playback.
static EvsCommand _gActor460200SoldierAFirstTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_A_FIRST_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor460200SoldierATalkScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier A's second tent conversation.
///
/// Starts its CAP dialogue, calls `_gActor460200SoldierATalkScript` for the
/// shared animation choreography and player-control release, then ends.
/// The commands, called scripts and clip data stay loaded through playback.
static EvsCommand _gActor460200SoldierASecondTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_A_SECOND_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor460200SoldierATalkScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier A's third tent conversation.
///
/// Starts its CAP dialogue and borrows `_gActor460200SoldierATalkScript` for
/// animation choreography and player-control release, then ends. Both scripts
/// and their clip resources stay loaded through playback.
static EvsCommand _gActor460200SoldierAThirdTalkScript[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_A_THIRD_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor460200SoldierATalkScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Soldier A's repeating fourth tent conversation.
///
/// Starts its CAP dialogue and borrows `_gActor460200SoldierATalkScript` for
/// animation choreography and player-control release, then ends. Both scripts
/// and their clip resources stay loaded through playback.
static EvsCommand _gActor460200SoldierARepeatTalkScript[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_460200_CAP_SOLDIER_A_REPEAT_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor460200SoldierATalkScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/soldier_a/body.inc"

#include "assets/rooms/shelter/1f_tent/soldiers_clips_2.inc"

/// Soldier A's terminated paced-walk message table, borrowed until task teardown.
static TaskMessageEntry _gPacedWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, PACED_WALK_SET_PAIR_MODEL_DRAW },
    { ACTOR_MESSAGE_PLACE, PACED_WALK_PLACE },
    { ACTOR_COMMAND_MESSAGE_APPLY, _pacedWalkStartSmoking },
    { ACTOR_MESSAGE_WALK_TO, PACED_WALK_SET_WALK_TARGET },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor460200SoldierATaskDesc = { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor460200SoldierATask, { .model = &_gSoldierABody } };

/// Soldier A's borrowed animation bank: NULL slot 0 and playable clips 1..15.
///
/// The bank and its twenty-part sets must remain loaded while the rig uses them.
#include "assets/rooms/shelter/1f_tent/soldier_a_bank.inc"

/// Soldier A's smoke coordinate choices, sampled from the first eleven entries.
///
/// Those entries address non-root model parts 1..19. The final zero byte is
/// not sampled; its role is unproven and its storage is preserved.
static u8 _gPacedWalkSmokePartIndices[12] = {
    1,
    3,
    5,
    6,
    9,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

#include "assets/characters/soldier_b/rifle.inc"

#include "assets/characters/soldier_b/body.inc"

#include "assets/rooms/shelter/1f_tent/soldiers_clips_3.inc"

/// Soldier B's stride-walk message dispatch table, terminated by a null end entry.
///
/// Handles play requests (clip IDs 1..11), draw flags, parent-space
/// placement, head-turn commands and walk targets. The spawned task borrows
/// this table and its callbacks through teardown; unsupported IDs return 0.
static TaskMessageEntry _gStrideWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _strideWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _strideWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlaceSoldierB },
    { ACTOR_COMMAND_MESSAGE_APPLY, _strideWalkSetTurnMode },
    { ACTOR_MESSAGE_WALK_TO, _strideWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor460200SoldierBTasks[2] = {
    {
        .header   = { .fields = { .flags    = TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER,
                                  .priority = TASK_PRIORITY_ACTOR } },
        .callback = _actor460200SoldierBTask,
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
#include "assets/rooms/shelter/1f_tent/soldier_b_bank.inc"

#include "assets/characters/soldier_c/body.inc"

#include "assets/rooms/shelter/1f_tent/soldiers_clips_4.inc"

/// Soldier C's synchronous animation, visibility, placement, command and walk dispatch.
///
/// The spawned task borrows this terminated table while the package is loaded.
/// Actor commands are accepted without changing state; other unsupported IDs
/// reach the terminal record and return zero.
static TaskMessageEntry _gActor460200SoldierCMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlaySoldierCAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pacedWalkSetSoldierCModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlaceSoldierC },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor460200IgnoreSoldierCCommand },
    { ACTOR_MESSAGE_WALK_TO, _pacedWalkSetSoldierCWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor460200SoldierCTaskDesc = { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor460200SoldierCTask, { .model = &_gSoldierCBody } };

/// Soldier C's clip bank, borrowed by his rig while this package remains loaded.
///
/// Slot 0 is unloaded; clips 1..17 are valid. Spawning starts clip 2, and the
/// scripted play handler requires a nonnegative, nonzero index below the extent.
#include "assets/rooms/shelter/1f_tent/soldier_c_bank.inc"

#include "../../shared/screen/screen_negative_capture.inc.c"

/// Captures the scene as a grayscale negative and holds drawing until release.
///
/// `spawnArg2.pointer` borrows writable `ScreenNegativeCaptureArgs` until the
/// task ends; use duration 0..32767 for an ordinary signed-halfword countdown.
/// Capturing/filtering exclusively borrows the resident RGB555 workspace.
/// A script gate pauses every phase; expiry or a nonzero done flag resumes
/// drawing. The scene's other tasks own upload and frozen-frame presentation.
static void _actor460200NegativeCaptureTask(Task* task)
{
    _screenNegativeCaptureTask(task);
}

#include "../../shared/screen/screen_negative_filter.inc.c"

/// Requests mode teardown after the signed task countdown becomes negative.
///
/// Decrements `spawnArg1.value` once per invocation, testing its previous value.
/// A nonnegative starting count N requests exit on invocation N+2. The task
/// must remain addressable through `taskKill`; its spawn argument is reread
/// afterwards before the final decrement/store.
static void _actor460200ModeExitCountdownTask(Task* task)
{
    s32 remainingTicks;

    remainingTicks = task->spawnArg1.value;
    if (remainingTicks < 0) {
        stageRequestModeTaskExit();
        taskKill(task);
        // Teardown may change the task; decrement the value left by that call.
        remainingTicks = task->spawnArg1.value;
    }
    remainingTicks        = remainingTicks - 1;
    task->spawnArg1.value = remainingTicks;
}

/// Selects the tent cutscene's loaded CAP resource, or restores default selection.
///
/// Zero calls `capReset`. Other values are data-resource ordinals in the loaded
/// CDF bundle, with `capSelectLoadedFile`'s relocation and borrowed-storage
/// requirements, and use the cutscene texture origin in VRAM pixels.
static void _actor460200SelectCaptionFile(s32 dataResourceOrdinal)
{
    enum {
        ACTOR_460200_DEFAULT_CAP_RESOURCE = 0,
        ACTOR_460200_CUTSCENE_CAP_VRAM_X  = 832,
        ACTOR_460200_CUTSCENE_CAP_VRAM_Y  = 0,
    };

    if (dataResourceOrdinal != ACTOR_460200_DEFAULT_CAP_RESOURCE) {
        gCapFile = NULL;
        capSelectLoadedFile(dataResourceOrdinal);
        capSetTexturePage(ACTOR_460200_CUTSCENE_CAP_VRAM_X, ACTOR_460200_CUTSCENE_CAP_VRAM_Y);
        return;
    }
    capReset();
}

// Select the cutscene callback's separate private filter instance.
#undef SCREEN_NEGATIVE_FILTER
#define SCREEN_NEGATIVE_FILTER _screenNegativeFilterCutscene
#include "../../shared/screen/screen_negative_filter.inc.c"
#undef SCREEN_NEGATIVE_FILTER
#define SCREEN_NEGATIVE_FILTER _screenNegativeFilter

/// Sets the live save's signed-byte scene-event key for subsequent music selection.
static void _actor460200SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

void actor460200SetupTentSoldiers(void)
{
    enum {
        ACTOR_460200_TENT_SOLDIER_C = 0,
        ACTOR_460200_TENT_SOLDIER_B = 1,
        ACTOR_460200_TENT_SOLDIER_A = 2,
    };
    Task* actorTask;

    actorTask = sceneFindPlacedActor(ACTOR_460200_TENT_SOLDIER_C);
    if (actorTask != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(actorTask, ACTOR_MESSAGE_PLACE, &_gActor460200SoldierCTentPlacement, 0);
        TASK_MESSAGE_DISPATCH_POINTER(actorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor460200SoldierCClip2Request, 0);
    }
    if (sceneFindPlacedActor(ACTOR_460200_TENT_SOLDIER_B) != NULL) {
        sceneSetPlacedActorDrawMode(ACTOR_460200_TENT_SOLDIER_B, ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER);
    }
    actorTask = sceneFindPlacedActor(ACTOR_460200_TENT_SOLDIER_A);
    if (actorTask != NULL) {
        sceneSetPlacedActorDrawMode(ACTOR_460200_TENT_SOLDIER_A, ACTOR_MESSAGE_PAIR_SHOW);
        TASK_MESSAGE_DISPATCH_POINTER(actorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor460200SoldierAClip13Request, 0);
    }
}

void actor460200TalkToSoldierC(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A)) {
        case ACTOR_460200_TALK_FIRST:
            evsStartScript(_gActor460200SoldierCFirstTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A, ACTOR_460200_TALK_SECOND);
            break;
        case ACTOR_460200_TALK_SECOND:
            evsStartScript(_gActor460200SoldierCSecondTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A, ACTOR_460200_TALK_THIRD);
            break;
        case ACTOR_460200_TALK_THIRD:
            evsStartScript(_gActor460200SoldierCThirdTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A, ACTOR_460200_TALK_REPEAT);
            break;
        case ACTOR_460200_TALK_REPEAT:
            evsStartScript(_gActor460200SoldierCRepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

void actor460200TalkToSoldierA(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B)) {
        case ACTOR_460200_TALK_FIRST:
            evsStartScript(_gActor460200SoldierAFirstTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B, ACTOR_460200_TALK_SECOND);
            break;
        case ACTOR_460200_TALK_SECOND:
            evsStartScript(_gActor460200SoldierASecondTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B, ACTOR_460200_TALK_THIRD);
            break;
        case ACTOR_460200_TALK_THIRD:
            evsStartScript(_gActor460200SoldierAThirdTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B, ACTOR_460200_TALK_REPEAT);
            break;
        case ACTOR_460200_TALK_REPEAT:
            evsStartScript(_gActor460200SoldierARepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/paced_walk_frame.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/walk/paced_walk_update.inc.c"

/// Dispatches Soldier A's paced scripted walk.
///
/// Task state must be 0 (initialize) or 1 (frame update), with the live enemy in
/// spawnArg2.pointer and the twenty-part descriptor model. The paced-walk
/// handlers own work allocation, motion, animation and teardown.
static void _actor460200SoldierATask(Task* task)
{
    EnemyTaskFunc handlers[2] = { _pacedWalkSpawn, _pacedWalkFrame };

    handlers[task->state](task->spawnArg2.pointer, task);
}

#include "../../shared/walk/paced_walk_spawn.inc.c"

/// Releases the walker's enemy record and tears down its task tree.
///
/// Installed after work allocation. spawnArg2.pointer must retain the live
/// owning enemy through this callback; teardown frees owned work, model and
/// child tasks. Neither argument remains live after the callback.
static void _pacedWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/walk/paced_walk_tick_anim.inc.c"

#include "../../shared/walk/paced_walk_reset_anim.inc.c"

#include "../../shared/walk/paced_walk_blend_anim.inc.c"

#include "../../shared/walk/paced_walk_apply_animation_request.inc.c"
#include "../../shared/walk/paced_walk_play_anim.inc.c"

#include "../../shared/walk/paced_walk_show_pair.inc.c"

#include "../../shared/walk/paced_walk_place.inc.c"

#include "../../shared/walk/paced_walk_start_smoking.inc.c"

#include "../../shared/walk/paced_walk_to.inc.c"

/// Selects this package's two-entry TaskDesc table for the stride-walk spawn fragment.
///
/// Slot 0 is the body and slot 1 its optional rifle. The identifier is evaluated
/// once when a nonzero first spawn payload requests the child. Keep the table
/// live through spawning and its callbacks and model data live until teardown.
/// Bind only around the fragment inclusion, then undefine it.
#define STRIDE_WALK_TASKS gActor460200SoldierBTasks
#include "../../shared/walk/stride_walk_spawn.inc.c"
#undef STRIDE_WALK_TASKS

#undef PACED_WALK_BLEND_ANIM
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierBAnim
#undef PACED_WALK_RESET_ANIM
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierBAnim
#undef PACED_WALK_TICK_ANIM
// Soldier B's stride update uses its own private slot tick.
#define PACED_WALK_TICK_ANIM _pacedWalkTickSoldierBAnim
#include "../../shared/walk/stride_walk_update.inc.c"
#undef PACED_WALK_BLEND_ANIM
#undef PACED_WALK_RESET_ANIM
#undef PACED_WALK_TICK_ANIM

/// Dispatches Soldier B's stride walk with an optional carried rifle.
///
/// Task state must be 0 (initialize) or 1 (frame update), with the live enemy in
/// spawnArg2.pointer and the twenty-part descriptor model. Nonzero spawnArg1
/// requests the rifle child and armed animation; the stride handlers own work
/// and child lifetimes.
static void _actor460200SoldierBTask(Task* task)
{
    EnemyTaskFunc handlers[2] = { _strideWalkSpawn, _strideWalkFrame };

    handlers[task->state](task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondFixedWalkerGroundShadow
#include "../../shared/walk/stride_walk_frame.inc.c"

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#undef PACED_WALK_WORK_T
/// Work type for Soldier B's reused paced-walk animation and placement fragments.
///
/// His stride spawn allocates `StrideWalkWork` at `Task::work`. Keep this
/// selection through his tick, reset, blend and placement copies, then restore
/// the paced-work selection for Soldier C. The binding follows the
/// `PACED_WALK_WORK_T` contract; Soldier A used the header's default.
#define PACED_WALK_WORK_T StrideWalkWork

// Soldier B's tick uses the StrideWalkWork binding above.
#define PACED_WALK_TICK_ANIM _pacedWalkTickSoldierBAnim
#include "../../shared/walk/paced_walk_tick_anim.inc.c"
#undef PACED_WALK_TICK_ANIM

// Soldier B's reset uses the StrideWalkWork binding above.
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierBAnim
#include "../../shared/walk/paced_walk_reset_anim.inc.c"
#undef PACED_WALK_RESET_ANIM

// Soldier B's blend uses the StrideWalkWork binding above.
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierBAnim
#include "../../shared/walk/paced_walk_blend_anim.inc.c"
#undef PACED_WALK_BLEND_ANIM

#include "../../shared/walk/stride_walk.inc.c"

// Soldier B's placement uses the StrideWalkWork binding above.
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlaceSoldierB
#include "../../shared/walk/paced_walk_place.inc.c"
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlace

#undef PACED_WALK_WORK_T
/// Restores the allocated paced-walker type for Soldier C's four reusable fragments.
#define PACED_WALK_WORK_T PacedWalkWork

/// Sets a stride walker's head-turn mode from a borrowed command word.
///
/// Requires live `StrideWalkWork` at `task->work` and a readable command through
/// this call. Narrows its unsigned word to signed 16 bits without validation:
/// `STRIDE_WALK_TURN_PLAYER` aims at the player; every other value releases
/// toward the animation pose. Context, message ID and second payload are
/// ignored. Retains no pointer and returns 0.
static s32 _strideWalkSetTurnMode(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    StrideWalkWork* work = task->work;

    work->turnMode = command->command;
    return 0;
}

#include "../../shared/walk/stride_walk_to.inc.c"

#undef PACED_WALK_UPDATE
// Soldier C's update and animation helpers operate on its PacedWalkWork.
#define PACED_WALK_UPDATE     _pacedWalkUpdateSoldierC
#define PACED_WALK_TICK_ANIM  _pacedWalkTickSoldierCAnim
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierCAnim
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierCAnim
#include "../../shared/walk/paced_walk_update.inc.c"
#undef PACED_WALK_UPDATE
#define PACED_WALK_UPDATE _pacedWalkUpdate
#undef PACED_WALK_TICK_ANIM
#undef PACED_WALK_RESET_ANIM
#undef PACED_WALK_BLEND_ANIM

/// Runs Soldier C's spawn or model-frame state.
///
/// Requires a TMD task with state 0 (spawn) or 1 (frame) and a live borrowed
/// `Enemy*` in `spawnArg2.pointer`. Spawn installs the work, exit and message
/// callbacks and advances to frame state; later calls update and draw the rig.
static void _actor460200SoldierCTask(Task* task)
{
    EnemyTaskFunc states[2] = { _pacedWalkSpawnSoldierC, _actorRenderWalkerFrame };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Binds Soldier C's task-owned lighting matrices and takes its initial light sample.
///
/// `model` and `work` must be live and writable; the model borrows `work->light`
/// and `work->color` while lighting or drawing can use them. `rootCoord` is read
/// only during this call. Its cached translation is used without composition
/// or a change of coordinate frame, with 800 signed coordinate units subtracted
/// from Y. A cache composed through the view includes that view transform.
/// The world-position lighting query receives this sample unchanged.
///
/// Requires initialized room-light, view, scratch-stack and GTE state as for
/// `worldCoordSetModelLighting`. The sample's three 32-bit components are read
/// synchronously; no sample pointer is retained. Both matrix pointers are bound
/// even when the query returns without updating them.
static inline void _pacedWalkInitializeSoldierCModelLighting(TmdObject* model, PacedWalkWork* work, const GfxCoord* rootCoord)
{
    enum {
        PACED_WALK_SOLDIER_C_LIGHT_SAMPLE_Y_OFFSET = 800,
        PACED_WALK_SOLDIER_C_FIRST_LIGHT_INDEX     = 0,
        PACED_WALK_SOLDIER_C_LIGHT_COUNT           = 3,
    };
    VECTOR3 lightingSample;

    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    // Preserve the cached sample used at spawn, before the first animation update.
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1] - PACED_WALK_SOLDIER_C_LIGHT_SAMPLE_Y_OFFSET;
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, PACED_WALK_SOLDIER_C_FIRST_LIGHT_INDEX, PACED_WALK_SOLDIER_C_LIGHT_COUNT);
}

/// Initializes Soldier C's paced-walk rig and starts its initial clip.
///
/// Requires task state 0, a live TMD body with twenty part coordinates and its
/// live enemy work. Allocates zeroed `PacedWalkWork` owned by the task; failure
/// destroys the enemy and starts task teardown. The model borrows the work's
/// lighting matrices, and the rig borrows its model coordinates and clip bank
/// through teardown. Requires the view, room lighting and graphics scratch
/// state initialized. Reseeds non-root tracks once and advances to frame state.
static void _pacedWalkSpawnSoldierC(Enemy* enemy, Task* task)
{
    enum { PACED_WALK_SOLDIER_C_INITIAL_CLIP = 2 };
    PacedWalkWork* work;
    PacedWalkWork* allocation;
    TmdObject*     model;
    GfxCoord*      rootCoord;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    allocation = memCalloc(sizeof(PacedWalkWork), false);
    work       = allocation;
    if ((task->work = work) == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The model borrows task-owned matrices and is excluded from lock-on.
    task->exitCallback               = _pacedWalkExitSoldierC;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->flags                     = 0;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    work->st.animId                  = PACED_WALK_SOLDIER_C_INITIAL_CLIP;
    _pacedWalkInitializeSoldierCModelLighting(model, work, rootCoord);
    // Install the bank before the initial reset primes the non-root tracks.
    animationInitContext(&work->rig.anim, _gActor460200SoldierCAnimationSets, model, work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = _gActor460200SoldierCMessages;
    _pacedWalkUpdateSoldierC(task);
    task->state += 1;
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
#define ACTOR_RENDER_UPDATE_WALKER             _pacedWalkUpdateSoldierC
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawThirdFixedWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases Soldier C's enemy and starts teardown of its model and work block.
///
/// Requires a live task and its live borrowed `Enemy*` in `spawnArg2.pointer`.
/// The enemy is invalid after this call; task/model release follows `taskKill`.
static void _pacedWalkExitSoldierC(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawThirdFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

// Soldier C's tick uses the restored PacedWalkWork binding.
#define PACED_WALK_TICK_ANIM _pacedWalkTickSoldierCAnim
#include "../../shared/walk/paced_walk_tick_anim.inc.c"
#undef PACED_WALK_TICK_ANIM

// Soldier C's reset uses the restored PacedWalkWork binding.
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierCAnim
#include "../../shared/walk/paced_walk_reset_anim.inc.c"
#undef PACED_WALK_RESET_ANIM

// Soldier C's blend uses the restored PacedWalkWork binding.
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierCAnim
#include "../../shared/walk/paced_walk_blend_anim.inc.c"
#undef PACED_WALK_BLEND_ANIM

/// Immediately reseeds Soldier C's non-root tracks from a scripted play request.
///
/// Requires a live TMD task and `PacedWalkWork` rig with loaded clips 1..17.
/// The signed check rejects only IDs >=18; callers must exclude negative IDs
/// and unloaded ID 0. Borrows the word-aligned request for this call only;
/// model, rig and clip storage must remain live throughout playback.
///
/// Nonzero `blend` stores its whole-frame duration as a signed halfword;
/// 0..2047 keeps the slot blend countdowns nonnegative. Reset ignores duration.
/// Reseeding drives parts 1..19, leaving the root and travel unchanged. The
/// bank selector, collision choice, message ID and second payload are ignored.
/// Returns 0 after reseeding, or -1 for a rejected ID without changing playback.
static s32 _pacedWalkPlaySoldierCAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        PACED_WALK_SOLDIER_C_CLIP_LIMIT       = 18,
        PACED_WALK_SOLDIER_C_REQUEST_APPLIED  = 0,
        PACED_WALK_SOLDIER_C_REQUEST_REJECTED = -1,
    };
    PacedWalkWork* work;

    work = task->work;
    if (request->animationId < PACED_WALK_SOLDIER_C_CLIP_LIMIT) {
        _pacedWalkApplyAnimationRequest(work, request);
        _pacedWalkUpdateSoldierC(task);
        return PACED_WALK_SOLDIER_C_REQUEST_APPLIED;
    }
    return PACED_WALK_SOLDIER_C_REQUEST_REJECTED;
}

// Select Soldier C's private draw-message callback.
#undef PACED_WALK_SET_PAIR_MODEL_DRAW
#define PACED_WALK_SET_PAIR_MODEL_DRAW _pacedWalkSetSoldierCModelDraw
#include "../../shared/walk/paced_walk_show_pair.inc.c"
#undef PACED_WALK_SET_PAIR_MODEL_DRAW
#define PACED_WALK_SET_PAIR_MODEL_DRAW _pacedWalkSetPairModelDraw

// Soldier C's placement uses the restored PacedWalkWork binding.
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlaceSoldierC
#include "../../shared/walk/paced_walk_place.inc.c"
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlace

/// Accepts Soldier C's actor-command message without changing any state.
///
/// Reads none of its arguments, retains no pointers and returns 0.
static s32 _actor460200IgnoreSoldierCCommand(Task* unusedTask, s32 unusedMessageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    enum { ACTOR_460200_COMMAND_IGNORED = 0 };

    return ACTOR_460200_COMMAND_IGNORED;
}

// Soldier C records travel in its own PacedWalkWork.
#undef PACED_WALK_SET_WALK_TARGET
#define PACED_WALK_SET_WALK_TARGET _pacedWalkSetSoldierCWalkTarget
#include "../../shared/walk/paced_walk_to.inc.c"
#undef PACED_WALK_SET_WALK_TARGET
#define PACED_WALK_SET_WALK_TARGET _pacedWalkSetWalkTarget
