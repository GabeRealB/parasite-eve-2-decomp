#include "actors/actor_450800.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"
#include "actors/actor_450800.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
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
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b6_nursery.h"
/// Work type for Kyle Madigan's reused paced-walk animation and placement fragments.
///
/// Kyle's spawn allocates this type at `Task::work`; its definition below must
/// precede all four fragment copies. Keep the selection through tick, reset,
/// blend and placement so blend reads Kyle's duration after his three helper
/// task pointers. The binding follows the `PACED_WALK_WORK_T` contract.
#define PACED_WALK_WORK_T _Actor450800KyleMadiganWork
#include "../../shared/walk/paced_walk.h"
#include "../../shared/walk/pair_walk.h"

static s32  _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _pairWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg);
static void _pairWalkSubModelTask(Task* task);

/// How Kyle Madigan covers a walk, kept in
/// `_Actor450800KyleMadiganWork::walkMode`.
///
/// The walk-to message's last argument selects it. Every mode ends at the
/// target: the distance is divided by the mode's stride to give the frames the
/// walk lasts. A value outside these three turns Kyle toward the target and
/// counts the slow stride, but no frame then moves him.
enum {
    ACTOR_450800_WALK_FAST     = 0, // Faces the target and advances 60 units a frame
    ACTOR_450800_WALK_BACKWARD = 1, // Faces away from the target and backs up to it, 15 units a frame
    ACTOR_450800_WALK_SLOW     = 2, // Faces the target and advances 25 units a frame
};

/// Kyle's parent-coordinate distance per travel frame in each walk mode.
enum {
    ACTOR_450800_KYLE_FAST_STRIDE     = 60,
    ACTOR_450800_KYLE_BACKWARD_STRIDE = 15,
    ACTOR_450800_KYLE_SLOW_STRIDE     = 25,
};

/// Work block of the package's Kyle Madigan actor, allocated zeroed at its
/// full size by the actor's spawn state and kept at `Task::work`.
///
/// Both bodies the package's task table runs through the actor's states use
/// it: the Kyle Madigan body and the second twenty-part body. The pawn golem
/// the package also carries keeps a `PairWalkWork` instead.
///
/// The block opens like the other scripted walkers' - matrices, twenty-slot
/// rig, animation state - so the paced walk library's slot tick, slot reset
/// and placement run on it unchanged. Behind that it holds what is Kyle's
/// own: the tasks of the two hands and the gun, drawn as separate models hung
/// off parts of the body, and the way he walks to a target. The model object
/// borrows `light` and `color`, and the three tasks are killed with the
/// actor, so all of it lives exactly as long as the actor's task.
typedef struct {
    MATRIX          light;         // Light-direction matrix lent to the body's model object
    MATRIX          color;         // Light-colour matrix lent to the body's model object
    ActorAnimRig20  rig;           // Playback storage of the body; slots 1 to 19 are driven
    ActorEnemyState st;            // Animation request, heading and frames of walk left
    s16             turnFrames;    // Frames left of a turn of 51/4096 a frame while clip 3 plays; only ever cleared here, so the turn never runs
    byte            pad_4EE[0x2];
    Task*           handLeftTask;  // Task drawing the left-hand model
    Task*           handRightTask; // Task drawing the right-hand model
    Task*           gunTask;       // Task drawing the gun model
    s16             blendFrames;   // Whole frames the next blended reseed takes to reach the requested clip
    s16             walkMode;      // How the walk in progress covers its distance (`ACTOR_450800_WALK_FAST`, `_BACKWARD` or `_SLOW`)
    u8              gunShown;      // 1 while the gun is drawn with the body, 0 while it stays hidden whatever the body's draw mode
    byte            pad_501[0x3];
} _Actor450800KyleMadiganWork;
STATIC_ASSERT_SIZEOF(_Actor450800KyleMadiganWork, 0x504);

/// Fixed placement of the nursery gunfire's impact flash and sparks.
///
/// XYZ is in signed whole game units in the SDK placement matrix's input frame;
/// the fourth SDK component is unused.
static const SVECTOR _gActor450800NurseryImpactSparkPosition = { 6600, -1400, 960, 0 };

static inline void _actor450800ApplyHelperPlacementTexture(Task* spawned, Task* actor);
static void        _actor450800SpawnKyleMadigan(Enemy* enemy, Task* task);
static void        _actor450800UpdateKyleMadigan(Task* task);
static void        _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void        _actor450800ExitKyleMadigan(Task* task);
static void        _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void        _actorRenderDrawWalkerGroundShadow(Task* task);
static void        _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static TmdSource _gKyleAlternateBody;
extern TmdSource gKyleBody;
extern TmdSource gKyleHandRight;
extern TmdSource gKyleHandLeft;
extern TmdSource gKyleP229;
static void      _actor450800KyleMadiganTask(Task* task);
static void      _actor450800KyleMadiganAttachmentTask(Task* task);

static s32 _actor450800PlayKyleMadiganAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor450800SetKyleMadiganDrawFlags(Task* task, s32 messageId, s32 flags, s32 unusedArgument);
static s32 _actor450800ApplyKyleMadiganGunCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static s32 _actor450800SetKyleMadiganWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

static TmdSource _gGolemPawnBody;
static TmdSource _gGolemBeamSword;
static s32       _actor450800IgnorePawnGolemCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static void      _actor450800PawnGolemWalkerTask(Task* task);

static void _actor450800SetNurseryCaptions(s32 useNurseryCaptions);
static void _actor450800EnterGrowthRoom(void);

static void _actor450800SetNurseryView13SpriteHidden(s32 hidden);
static void _actor450800SpawnNurseryImpactSpark(void);

static void _actor450800SetNurseryEffectCues(u32 packedCues);
// Packed high/low halfwords: fast glint pulse on, spark shower scale zero.
enum { ACTOR_450800_NURSERY_FAST_GLINT_NO_SHOWER = (1 << 16) | 0 };
static void _actor450800StartNurseryRepeatDialogue(s32 dialogueSelector);

#include "assets/rooms/shelter/b6_nursery/kyle_clips_1.inc"

/// Clip keys used by the nursery's base-player and fixed Kyle banks.
enum {
    ACTOR_450800_NURSERY_PLAYER_BASE_CLIP       = 1,
    ACTOR_450800_NURSERY_KYLE_INITIAL_CLIP      = 1,
    ACTOR_450800_NURSERY_KYLE_CLIP_9            = 9,
    ACTOR_450800_NURSERY_KYLE_CLIP_10           = 10,
    ACTOR_450800_NURSERY_KYLE_CLIP_11           = 11,
    ACTOR_450800_NURSERY_KYLE_CLIP_12           = 12,
    ACTOR_450800_NURSERY_KYLE_ARRIVAL_IDLE_CLIP = 13,
    ACTOR_450800_NURSERY_KYLE_CLIP_19           = 19,
    ACTOR_450800_NURSERY_KYLE_CLIP_20           = 20,
};

/// Additional fixed-bank clip keys requested by the nursery's scene scripts.
enum {
    ACTOR_450800_NURSERY_KYLE_CLIP_3         = 3,
    ACTOR_450800_NURSERY_KYLE_CLIP_4         = 4,
    ACTOR_450800_NURSERY_KYLE_CLIP_5         = 5,
    ACTOR_450800_NURSERY_KYLE_CLIP_6         = 6,
    ACTOR_450800_NURSERY_KYLE_CLIP_7         = 7,
    ACTOR_450800_NURSERY_KYLE_CLIP_14        = 14,
    ACTOR_450800_NURSERY_KYLE_CLIP_15        = 15,
    ACTOR_450800_NURSERY_KYLE_CLIP_16        = 16,
    ACTOR_450800_NURSERY_KYLE_CLIP_17        = 17,
    ACTOR_450800_NURSERY_KYLE_CLIP_18        = 18,
    ACTOR_450800_NURSERY_KYLE_CLIP_21        = 21,
    ACTOR_450800_NURSERY_KYLE_CLIP_22        = 22,
    ACTOR_450800_NURSERY_SECOND_BODY_CLIP_23 = 23,
    ACTOR_450800_NURSERY_SECOND_BODY_CLIP_24 = 24,
    ACTOR_450800_NURSERY_KYLE_CLIP_25        = 25,
    ACTOR_450800_NURSERY_KYLE_CLIP_26        = 26,
};

/// Nursery interactions during progress state 1, capped at 3.
///
/// Starts at 0 on package load and stays in 0..3. Repeat dialogue selects
/// CAP sequence 2 plus the counter's low halfword.
static s32 _gActor450800NurseryInteractionCount = 0;

/// Nursery clips installed at player bank indices 47..63.
///
/// The copy request transfers 32 words, including 15 words beyond this
/// table's 17 pointers. This original read into adjacent request data is
/// retained; only the pointers in this table are playable extension entries.
/// The copied tail covers the following three request records; it is never
/// selected for playback. Keep these objects contiguous in their stored order.
#include "assets/rooms/shelter/b6_nursery/player_bank.inc"

/// Unreferenced indexed-play record containing bank 1, clip 47 and reset controls.
///
/// Its five words are copied after the player extension table, but no playback
/// selects them. The intended receiver and scene role are unproven.
static AnimationPlayRequest D_actor_450800_80139354 = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 47.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip47Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 48.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip48Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player's nursery clip 49 over 8 frames.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip49Blend = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 50.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip50Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced indexed-play record containing bank 1, clip 51 and reset controls.
///
/// The intended receiver and scene role are unproven.
static AnimationPlayRequest D_actor_450800_801393B8 = {
    .source               = { .index = 1 },
    .animationId          = 51,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 52.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip52Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 53.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip53Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 54.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip54Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 55.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryPlayerClip55Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 56.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryPlayerClip56Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 57.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip57Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced indexed-play record containing bank 1, clip 47 and reset controls.
///
/// The intended receiver and scene role are unproven.
static AnimationPlayRequest D_actor_450800_80139444 = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 59.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryPlayerClip59Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 60.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryPlayerClip60Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 13,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 61.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryPlayerClip61Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 62.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryPlayerClip62Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player's nursery clip 63 over 8 frames.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryPlayerClip63Blend = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 16,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Nursery clips installed at companion bank indices 47..57.
///
/// The copy request transfers 32 words, including 21 words beyond this
/// table's 11 pointers. This original read into adjacent request data is
/// retained; only the pointers in this table are playable extension entries.
/// The copied tail covers four following requests and the bank word of the
/// fifth; it is never selected for playback. Keep the stored object order.
#include "assets/rooms/shelter/b6_nursery/companion_bank.inc"

/// Unreferenced indexed-play record containing bank 1, clip 47 and reset controls.
///
/// Its five words are copied after the companion extension table, but no playback
/// selects them. The intended receiver and scene role are unproven.
static AnimationPlayRequest D_actor_450800_801394E8 = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 47.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip47Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 48.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip48Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 49.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip49Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 50.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip50Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 51.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip51Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 52.
///
/// Script playback resolves the companion bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryCompanionClip52Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 53.
///
/// Script playback resolves the companion bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryCompanionClip53Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 54.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip54Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Two unreferenced indexed-play records containing clips 47 and 55.
///
/// Both store bank 1, reset playback and disabled collision. Receiver and role
/// are unproven; this declaration does not establish that playback chains them.
static AnimationPlayRequest D_actor_450800_8013959C[] = {
    {
        .source               = { .index = 1 },
        .animationId          = 47,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 55,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

/// Restarts the companion's nursery clip 56.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip56Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's nursery clip 57.
///
/// Event playback resolves the companion bank in a copy of this request.
/// Disables world-grid collision; the request is borrowed through dispatch.
/// Keep the installed nursery clips loaded while they play.
static AnimationPlayRequest _gActor450800NurseryCompanionClip57Reset = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Three unreferenced indexed-play records containing clips 58, 59 and 0.
///
/// All store bank 1, reset playback and disabled collision. Receiver and role
/// are unproven; clip 0 has no established sentinel meaning here.
static AnimationPlayRequest D_actor_450800_801395EC[] = {
    {
        .source               = { .index = 1 },
        .animationId          = 58,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 59,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 0,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

/// Restarts Kyle's nursery clip 1.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip1Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_INITIAL_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced indexed-play record containing bank 1, clip 2 and reset controls.
///
/// The intended receiver and scene role are unproven.
static AnimationPlayRequest D_actor_450800_8013963C = {
    .source               = { .index = 1 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 3.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip3Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 4.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip4Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends Kyle's nursery clip 5 over 10 frames.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip5Blend = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 6.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip6Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 7.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip7Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced indexed-play record containing bank 1, clip 8 and reset controls.
///
/// The intended receiver and scene role are unproven.
static AnimationPlayRequest D_actor_450800_801396B4 = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends Kyle's nursery clip 9 over 10 frames.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip9Blend = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends Kyle's nursery clip 10 over 10 frames.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip10Blend = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 11.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip11Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_11,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 12.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip12Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 13.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip13Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_ARRIVAL_IDLE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 14.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip14Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 15.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip15Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 16.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip16Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_16,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends Kyle's nursery clip 17 over 8 frames.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip17Blend = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_17,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 18.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip18Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_18,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 19.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip19Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_19,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 20.
///
/// Uses the fixed Kyle bank; the selector and collision words are ignored.
/// The aligned request is borrowed only through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryKyleClip20Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_20,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced indexed-play record containing bank 1, clip 0 and reset controls.
///
/// The receiver, scene role and any sentinel meaning of clip 0 are unproven.
static AnimationPlayRequest D_actor_450800_801397B8 = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 21.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip21Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_21,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 22.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip22Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_22,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts nursery clip 23 on the second body.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
/// Sent to placed actor 3, whose depicted identity is unproven.
static AnimationPlayRequest _gActor450800NurserySecondBodyClip23Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_SECOND_BODY_CLIP_23,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts nursery clip 24 on the second body.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
/// Sent to placed actor 3, whose depicted identity is unproven.
static AnimationPlayRequest _gActor450800NurserySecondBodyClip24Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_SECOND_BODY_CLIP_24,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 25.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip25Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_25,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle's nursery clip 26.
///
/// Uses the fixed Kyle animation bank on tracks 1..19; the bank selector
/// and collision word are ignored. Borrowed only through synchronous
/// dispatch; the clip data must remain loaded during playback.
static AnimationPlayRequest _gActor450800NurseryKyleClip26Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_KYLE_CLIP_26,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Two unreferenced indexed-play records containing clips 27 and 28.
///
/// Both store bank 1, reset playback and disabled collision. Receiver and role
/// are unproven; this declaration does not establish that playback chains them.
static AnimationPlayRequest D_actor_450800_80139844[] = {
    {
        .source               = { .index = 1 },
        .animationId          = 27,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = 1 },
        .animationId          = 28,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

/// Blends the companion's nursery clip 52 over 10 frames.
///
/// Script playback resolves the companion bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryCompanionClip52Blend = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player's nursery clip 58 over 10 frames.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip58Blend = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the player's nursery clip 1.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip1Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_PLAYER_BASE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player's nursery clip 1 over 10 frames.
///
/// Script playback resolves the equipped weapon bank in a copy.
/// The request disables world-grid collision and is borrowed through dispatch.
static AnimationPlayRequest _gActor450800NurseryPlayerClip1Blend = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_PLAYER_BASE_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { ACTOR_450800_NURSERY_COMPANION_BASE_CLIP = 1 };

/// Restarts the companion's base clip for the initial nursery scene.
///
/// Script playback resolves the companion bank in a copy. The request disables
/// world-grid collision and is borrowed through synchronous dispatch.
static AnimationPlayRequest _gActor450800NurseryCompanionClip1Reset = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_450800_NURSERY_COMPANION_BASE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the nursery's clips in the companion's current bank extension.
///
/// Transfers 32 words from `_gActor450800NurseryCompanionAnimationSets`, including
/// 21 words of following request data. The count is the transfer span, not
/// the playable clip count; the source package must stay loaded for playback.
static AnimationBankCopyRequest _gActor450800NurseryCompanionBankCopy = {
    .sets  = _gActor450800NurseryCompanionAnimationSets,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Installs the nursery's clips in the player's current bank extension.
///
/// Transfers 32 words from `_gActor450800NurseryPlayerAnimationSets`, including
/// 15 words of following request data. The count is the transfer span, not
/// the playable clip count; the source package must stay loaded for playback.
static AnimationBankCopyRequest _gActor450800NurseryPlayerBankCopy = {
    .sets  = _gActor450800NurseryPlayerAnimationSets,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Commands of the nursery bodies' gun; their stage/area tags are ignored.
enum {
    ACTOR_450800_GUN_FIRE = 0,
    ACTOR_450800_GUN_SHOW = 1,
    ACTOR_450800_GUN_HIDE = 2,
};

/// Gun command that emits Kyle's muzzle flash during the nursery shooting scene.
///
/// The nursery stage/area tag is carried but ignored. Borrowed through dispatch.
static ActorCommand _gActor450800NurseryGunFireCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY } },
    .command = ACTOR_450800_GUN_FIRE,
};

/// Gun command that shows either nursery body's gun with that body's draw flags.
///
/// Sets the gun-visible latch; the stage/area tag is ignored. Borrowed through dispatch.
static ActorCommand _gActor450800NurseryGunShowCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY } },
    .command = ACTOR_450800_GUN_SHOW,
};

/// Gun command that hides Kyle's gun for the initial nursery scene.
///
/// Clears the gun-visible latch and excludes drawing and automatic buffers.
/// The stage/area tag is ignored; the record is borrowed through dispatch.
static ActorCommand _gActor450800NurseryGunHideCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY } },
    .command = ACTOR_450800_GUN_HIDE,
};

/// Kyle's nursery placement used by scene setup and companion-dialogue reset.
///
/// XYZ uses world units in the view parent's frame; yaw is -1/4 turn.
static ActorTransform _gActor450800NurseryKylePlacement = {
    .pos = { 5650, 0, 2400, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Player placement at the start of the initial nursery scene.
///
/// XYZ is in whole world units; yaw is 853/4096 turns. Borrowed through placement.
static ActorTransform _gActor450800NurseryPlayerScenePlacement = {
    .pos = { 6080, 0, 900, 0 },
    .rot = { 0, 853, 0, 0 },
};

/// Player placement restored when the nursery scene finishes or is skipped.
///
/// XYZ uses world units; yaw is 853/4096 turns. Consumed during placement.
static ActorTransform _gActor450800NurseryPlayerReturnPlacement = {
    .pos = { 5951, 0, 900, 0 },
    .rot = { 0, 853, 0, 0 },
};

/// Companion placement at the start of the initial nursery scene.
///
/// XYZ is in whole world units; yaw is -1137/4096 turns. Borrowed through placement.
static ActorTransform _gActor450800NurseryCompanionScenePlacement = {
    .pos = { 6770, 0, 900, 0 },
    .rot = { 0, -1137, 0, 0 },
};

/// Companion placement restored after scenes and used for nursery dialogue.
///
/// XYZ uses world units; yaw is -1/4 turn. Consumed during placement.
static ActorTransform _gActor450800NurseryCompanionReturnPlacement = {
    .pos = { 6650, 0, 401, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Nursery scene selectors and sound cues carried by the exported programs.
enum {
    ACTOR_450800_NURSERY_INTRO_CAP_SEQUENCE      = 1,
    ACTOR_450800_NURSERY_SHOOTING_CAP_SEQUENCE   = 13,
    ACTOR_450800_NURSERY_SCENE_KYLE              = 0,
    ACTOR_450800_NURSERY_SCENE_FIRST_PAWN_GOLEM  = 1,
    ACTOR_450800_NURSERY_SCENE_SECOND_PAWN_GOLEM = 2,
    ACTOR_450800_NURSERY_SCENE_SECOND_BODY       = 3,
    ACTOR_450800_NURSERY_BODY_HIDE               = 0, // No paired show or auto-buffer-exclusion flags
    ACTOR_450800_NURSERY_PLAYER_WEAPONS          = 0, // EVS weapon selection: suppress/restore only the player
    ACTOR_450800_NURSERY_RETURN_VIEW             = 3,
    ACTOR_450800_NURSERY_RETURN_MUSIC_EVENT      = 25,
    ACTOR_450800_NURSERY_FADE_BRIGHTEN           = 1,
    ACTOR_450800_NURSERY_FADE_DEFAULT_RAMP       = 0, // EVS selects its seven-frame default ramp
    ACTOR_450800_NURSERY_FADE_KEEP_RAMP          = 0, // A secondary-fade return keeps its current ramp
    ACTOR_450800_NURSERY_INTRO_END_SOUND         = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 6),
    ACTOR_450800_NURSERY_SHOOTING_START_SOUND    = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 7),
};

EvsCommand gActor450800NurseryIntroScript[] = {
    // Install nursery playback resources and stage the initial player, companion and Kyle poses.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450800_NURSERY_INTRO_CAP_SEQUENCE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryCompanionBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip1Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor450800NurseryGunHideCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryPlayerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_450800_NURSERY_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP cues interleave the conversation poses with Kyle visibility changes.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip3Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip5Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip17Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_450800_NURSERY_BODY_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip47Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip47Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip48Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip49Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip50Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip18Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_450800_NURSERY_BODY_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip52Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip48Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip49Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip4Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip14Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 61 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip1Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip15Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_450800_NURSERY_BODY_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_450800_NURSERY_BODY_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip3Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip6Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip7Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_450800_NURSERY_BODY_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip53Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip54Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Enable the nursery glint and ambience before the closing animation cues.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _actor450800SetNurseryEffectCues }, { .value = ACTOR_450800_NURSERY_FAST_GLINT_NO_SHOWER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB6NurseryStartAmbience }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip55Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip54Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip50Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip16Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip56Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip51Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_450800_NURSERY_INTRO_END_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Fade back to view 3, return both actors and release scripted player control.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450800_NURSERY_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450800_NURSERY_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryPlayerReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip52Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_450800_NURSERY_FADE_KEEP_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = ACTOR_450800_NURSERY_RETURN_MUSIC_EVENT }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450800NurseryIntroSkipScript[] = {
    // Restore the intro return state under a fade, including cues that playback has not reached.
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_450800_NURSERY_INTRO_END_SOUND }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_450800_NURSERY_FADE_DEFAULT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450800_NURSERY_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _actor450800SetNurseryEffectCues }, { .value = ACTOR_450800_NURSERY_FAST_GLINT_NO_SHOWER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450800_NURSERY_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryPlayerReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = ACTOR_450800_NURSERY_RETURN_MUSIC_EVENT }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB6NurseryStartAmbience }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip52Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip1Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Nursery CAP slots and selectors used by the private dialogue scripts.
enum {
    ACTOR_450800_NURSERY_COMPANION_CAP_SEQUENCE         = 2,
    ACTOR_450800_NURSERY_FIRST_VISIT_CAP_SEQUENCE_6     = 6,
    ACTOR_450800_NURSERY_FIRST_VISIT_CAP_SEQUENCE_7     = 7,
    ACTOR_450800_NURSERY_DIALOGUE_USE_INTERACTION_COUNT = 1,
    ACTOR_450800_NURSERY_DIALOGUE_USE_SCENE_COUNT       = 2,
};

/// Nursery companion dialogue with the player in a scripted base pose.
///
/// Starts CAP sequence 2, installs companion clips and plays clips 53 then 52
/// around CAP cues. Releases player control and restores default captions.
/// Requires the nursery resources and both actors live through playback.
static EvsCommand _gActor450800NurseryCompanionDialogueScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450800_NURSERY_COMPANION_CAP_SEQUENCE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryCompanionBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip53Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip52Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Release scripted player control and restore the default caption resource.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Kyle's first two repeat nursery interactions while progress is 1.
///
/// The interaction count selects CAP sequence 3 or 4. Blends Kyle through clips
/// 9/10 around a CAP cue, then releases player control and restores captions.
/// Requires live nursery resources and player/Kyle tasks until the script ends.
static EvsCommand _gActor450800NurseryKyleRepeatDialogueScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800StartNurseryRepeatDialogue }, { .value = ACTOR_450800_NURSERY_DIALOGUE_USE_INTERACTION_COUNT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip9Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip10Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Release scripted player control and restore the default caption resource.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Kyle's capped repeat nursery interaction while progress is 1.
///
/// Count 3 selects CAP sequence 5 and restarts Kyle's clip 19. Waits for a CAP
/// cue, then releases player control and restores captions. Requires live
/// nursery resources and player/Kyle tasks until the script ends.
static EvsCommand _gActor450800NurseryKyleFinalRepeatDialogueScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800StartNurseryRepeatDialogue }, { .value = ACTOR_450800_NURSERY_DIALOGUE_USE_INTERACTION_COUNT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip19Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Release scripted player control and restore the default caption resource.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450800NurseryReentryScript[] = {
    // Reinstall the companion extension before selecting its nursery pose.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryCompanionBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip52Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip1Reset } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450800NurseryAdvancedReentryScript[] = {
    // Reinstall the companion extension and restore the later Kyle pose and placement.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryCompanionBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip52Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip20Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryKylePlacement } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// CAP sequence 6 for the first saved nursery visit when flag 083 is 1.
///
/// Installs player clips, alternates Kyle's clips 11/12 and ends on clips 13/20;
/// CAP cues coordinate player clips 58/57. Releases player control and restores
/// captions. Requires live nursery resources and player/Kyle tasks throughout.
static EvsCommand _gActor450800NurseryFirstVisitDialogue6Script[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450800_NURSERY_FIRST_VISIT_CAP_SEQUENCE_6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip11Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip12Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip58Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip11Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip12Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip57Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip13Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip20Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Release scripted player control and restore the default caption resource.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// CAP sequence 7 for the first saved nursery visit when flag 083 is not 1.
///
/// Installs player clips and coordinates Kyle's clips 11/12 with player clip 58
/// through CAP cues. Releases player control and restores default captions.
/// Requires live nursery resources and player/Kyle tasks throughout.
static EvsCommand _gActor450800NurseryFirstVisitDialogue7Script[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450800_NURSERY_FIRST_VISIT_CAP_SEQUENCE_7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip11Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip12Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip58Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip11Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip12Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Release scripted player control and restore the default caption resource.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Nursery dialogue for saved visits after the first.
///
/// Saved count 2 selects CAP sequence 8; count 3 selects sequence 9. Coordinates
/// Kyle's clips 11/12 around a CAP cue, then releases player control and restores
/// captions. Requires live nursery resources and player/Kyle tasks throughout.
static EvsCommand _gActor450800NurseryLaterVisitDialogueScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800StartNurseryRepeatDialogue }, { .value = ACTOR_450800_NURSERY_DIALOGUE_USE_SCENE_COUNT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip11Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip12Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Release scripted player control and restore the default caption resource.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum { ACTOR_450800_NURSERY_PAWN_GOLEM_FINAL_CLIP = 5 };

/// Blends the first nursery Pawn GOLEM into its travel clip over ten frames.
///
/// Uses fixed-bank clip 4 on tracks 1..18. The bank selector and collision word
/// are ignored; the request is borrowed through dispatch and the clips through playback.
static AnimationPlayRequest _gActor450800NurseryPawnGolemClip4Blend = {
    .source               = { .index = 0 },
    .animationId          = PAIR_WALK_ANIM_WALK,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts clip 5 on the second nursery Pawn GOLEM near the scene's end.
///
/// Uses fixed-bank tracks 1..18; the bank selector and collision word are ignored.
/// The request is borrowed through dispatch and the clip data through playback.
static AnimationPlayRequest _gActor450800NurseryPawnGolemClip5Reset = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_450800_NURSERY_PAWN_GOLEM_FINAL_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts both nursery Pawn GOLEMs at their fixed-bank idle clip.
///
/// Uses tracks 1..18; the bank selector and collision word are ignored.
/// The request is borrowed through dispatch and the clip data through playback.
static AnimationPlayRequest _gActor450800NurseryPawnGolemClip1Reset = {
    .source               = { .index = 0 },
    .animationId          = PAIR_WALK_ANIM_IDLE,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced two-word record matching approach clip 19 and arrival clip 1.
///
/// No move message supplies it; the clip-pair interpretation and receiver are unproven.
static GameActorMoveAnim D_actor_450800_8013AE28 = {
    .approachAnimId = 19,
    .arrivalAnimId  = 1,
};

/// Kyle's placement for the nursery gunfire sequence.
///
/// Scene actor 0 receives parent-frame XYZ and quarter-turn yaw through dispatch.
static ActorTransform _gActor450800NurseryKyleShootingPlacement = {
    .pos = { 6280, 0, 960, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Kyle's placement for clip 22 after the nursery gunfire sequence.
///
/// Scene actor 0 receives parent-frame XYZ and quarter-turn yaw through dispatch.
static ActorTransform _gActor450800NurseryKylePostShootingPlacement = {
    .pos = { 6400, 0, 960, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Final placement of the nursery's second Kyle-handler body.
///
/// Scene actor 3 receives parent-frame XYZ and negative quarter-turn yaw before clips
/// 23/24 finish. The record is borrowed through placement dispatch.
static ActorTransform _gActor450800NurserySecondBodyFinalPlacement = {
    .pos = { 5350, 0, -20, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Unreferenced transform-shaped record at XYZ (5320, 0, 2900), yaw half a turn.
///
/// No placement or motion message supplies it; the receiver and scene role are unproven.
static ActorTransform D_actor_450800_8013AE78 = {
    .pos = { 5320, 0, 2900, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Player placement after the nursery shooting scene's opening flashes.
///
/// XYZ is in whole world units; yaw is half a turn. Borrowed through placement.
static ActorTransform _gActor450800NurseryPlayerShootingSceneStartPlacement = {
    .pos = { 5320, 0, 1180, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Player placement for nursery shooting-scene clip 59.
///
/// XYZ is in whole world units; yaw is -1877/4096 turns. Borrowed through placement.
static ActorTransform _gActor450800NurseryPlayerClip59Placement = {
    .pos = { 5320, 0, 540, 0 },
    .rot = { 0, -1877, 0, 0 },
};

/// Player placement for nursery shooting-scene clip 61.
///
/// XYZ is in whole world units; yaw is 455/4096 turns. Borrowed through placement.
static ActorTransform _gActor450800NurseryPlayerClip61Placement = {
    .pos = { 5280, 0, -290, 0 },
    .rot = { 0, 455, 0, 0 },
};

/// Player placement for nursery shooting-scene clip 62.
///
/// XYZ is in whole world units; yaw is a quarter turn. Borrowed through placement.
static ActorTransform _gActor450800NurseryPlayerClip62Placement = {
    .pos = { 6500, 0, 1000, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Companion placement for nursery shooting-scene clip 56.
///
/// XYZ is in whole world units; yaw is -455/4096 turns. Borrowed through placement.
static ActorTransform _gActor450800NurseryCompanionClip56Placement = {
    .pos = { 5710, 0, -290, 0 },
    .rot = { 0, -455, 0, 0 },
};

/// Companion placement for nursery shooting-scene clip 57.
///
/// XYZ is in whole world units; yaw is a quarter turn. Borrowed through placement.
static ActorTransform _gActor450800NurseryCompanionClip57Placement = {
    .pos = { 7000, 0, 1200, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Initial shooting-scene placement of the first nursery Pawn GOLEM.
///
/// Scene actor 1 receives parent-frame XYZ and yaw 807/4096 turns through dispatch.
static ActorTransform _gActor450800NurseryFirstPawnGolemPlacement = {
    .pos = { -1200, 0, -220, 0 },
    .rot = { 0, 807, 0, 0 },
};

/// Walk destination of the first nursery Pawn GOLEM during the shooting scene.
///
/// Scene actor 1 reads only X/Z in whole parent-frame units; Y and rotation are
/// unused. The destination is consumed through dispatch without retaining it.
static ActorTransform _gActor450800NurseryFirstPawnGolemWalkTarget = {
    .pos = { 800, 0, 450, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Shooting-scene placement of the second nursery Pawn GOLEM before clip 5.
///
/// Scene actor 2 receives parent-frame XYZ and yaw 853/4096 turns through dispatch.
static ActorTransform _gActor450800NurserySecondPawnGolemPlacement = {
    .pos = { 0, 0, 0, 0 },
    .rot = { 0, 853, 0, 0 },
};

/// Scene/audio stream selection for the nursery shooting sequence.
///
/// Matches group 5, stream 9, qualifier 11, with implicit second qualifier 0.
/// The interpreter retains this key until the scene selection is cleared.
static EvsSceneKey _gActor450800NurseryShootingSceneKey = {
    .group    = 5,
    .streamId = 9,
    .subId    = 11,
};

/// Indices and additional binary-motor repetitions of the gunshot vibration.
enum {
    ACTOR_450800_NURSERY_GUNSHOT_RAMP_SEGMENT  = 0,
    ACTOR_450800_NURSERY_GUNSHOT_PULSE_SEGMENT = 1,
    ACTOR_450800_NURSERY_GUNSHOT_REPEAT_COUNT  = 3,
    ACTOR_450800_NURSERY_GUNSHOT_LOOP_START    = 0,
};

/// Two-lane controller vibration used for nursery gunfire and impact cues.
///
/// The binary motor plays segment 1 four times with one-frame waits; the variable
/// motor plays segment 0 once. Both lanes end within these five steps. Borrowed
/// with the segment table for the spawned vibration task's lifetime.
static PadScriptCmd _gActor450800NurseryGunshotVibrationCommands[] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_450800_NURSERY_GUNSHOT_PULSE_SEGMENT), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_450800_NURSERY_GUNSHOT_RAMP_SEGMENT) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, ACTOR_450800_NURSERY_GUNSHOT_REPEAT_COUNT), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, ACTOR_450800_NURSERY_GUNSHOT_LOOP_START), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

/// Variable-motor ramp and binary-motor pulse for the nursery gunshot script.
///
/// Segment 0 ramps intensity 210 toward 63 over nine script frames; segment 1
/// gives a two-frame binary pulse. The interpreter does not read `field_3`; its
/// stored bytes are retained. Borrowed for the vibration task's lifetime.
static PadScriptVibrationSegment _gActor450800NurseryGunshotVibrationSegments[] = {
    [ACTOR_450800_NURSERY_GUNSHOT_RAMP_SEGMENT]  = { .startIntensity = 210, .endIntensity = 63, .durationFrames = 9, .field_3 = 1 },
    [ACTOR_450800_NURSERY_GUNSHOT_PULSE_SEGMENT] = { .startIntensity = 0, .endIntensity = 0, .durationFrames = 2, .field_3 = 0 },
};

EvsCommand gActor450800NurseryShootingScript[] = {
    // Select the shooting stream and install the nursery animation extensions.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor450800NurseryShootingSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450800_NURSERY_SHOOTING_CAP_SEQUENCE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450800NurseryCompanionBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_450800_NURSERY_SHOOTING_START_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_450800_NURSERY_FADE_KEEP_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip63Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_450800_NURSERY_FADE_KEEP_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip63Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Stage the hidden bodies and player between the opening flashes and CAP cues.
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 150 }, { .value = 40 }, { .value = 40 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_450800_NURSERY_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor450800NurseryGunShowCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_BODY }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor450800NurseryGunShowCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryPlayerShootingSceneStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip63Blend }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryPlayerClip59Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip59Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip60Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = ACTOR_450800_NURSERY_FADE_BRIGHTEN }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_450800_NURSERY_FADE_KEEP_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip26Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryKyleShootingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor450800NurseryGunshotVibrationCommands }, { .vibrationSegments = _gActor450800NurseryGunshotVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Three gunfire cues follow; the first also emits the fixed impact and hides the room sprite.
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip25Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor450800NurseryGunFireCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450800SpawnNurseryImpactSpark }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryView13SpriteHidden }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor450800NurseryGunshotVibrationCommands }, { .vibrationSegments = _gActor450800NurseryGunshotVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip25Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor450800NurseryGunFireCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor450800NurseryGunshotVibrationCommands }, { .vibrationSegments = _gActor450800NurseryGunshotVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip25Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor450800NurseryGunFireCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor450800NurseryGunshotVibrationCommands }, { .vibrationSegments = _gActor450800NurseryGunshotVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip21Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryPlayerClip61Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionClip56Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip61Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip56Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip22Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryKylePostShootingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Replace the visible conversation bodies with the two Pawn GOLEMs.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 50 }, { .value = 10 }, { .value = 10 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_FIRST_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_450800_NURSERY_BODY_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_FIRST_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryPawnGolemClip1Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryPawnGolemClip1Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_FIRST_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryFirstPawnGolemPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_FIRST_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryPawnGolemClip4Blend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_FIRST_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor450800NurseryFirstPawnGolemWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurserySecondPawnGolemPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryPawnGolemClip5Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 150 }, { .value = 40 }, { .value = 40 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_FIRST_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_PAWN_GOLEM }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionClip57Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryPlayerClip62Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip62Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip57Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Play the second body, then fade out and request the growth-room transition.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_BODY }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_BODY }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurserySecondBodyClip23Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_BODY }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurserySecondBodyFinalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_SECOND_BODY }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurserySecondBodyClip24Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450800EnterGrowthRoom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450800NurseryShootingSkipScript[] = {
    // Request the shooting scene destination, then restore the return state under a fade.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_450800_NURSERY_FADE_DEFAULT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450800EnterGrowthRoom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450800_NURSERY_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450800_NURSERY_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450800NurseryCompanionReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryCompanionClip52Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450800NurseryPlayerClip1Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_450800_NURSERY_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor450800NurseryKyleClip1Reset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/kyle/alternate_body.inc"

#include "assets/characters/kyle/body.inc"

#include "assets/rooms/shelter/b6_nursery/kyle_clips_2.inc"

#include "assets/characters/kyle/hand_right.inc"

#include "assets/characters/kyle/hand_left.inc"

#include "assets/rooms/shelter/b6_nursery/kyle_clips_3.inc"

#include "assets/characters/kyle/p229.inc"

/// Message handlers borrowed by Kyle and the nursery's second scene body.
///
/// Supports fixed-bank animation, body/hand/gun draw flags, position/yaw, gun
/// commands and XZ walk targets. The terminal entry returns zero for other IDs.
/// Requires initialized body work and live helpers; keep this table and the
/// handlers loaded until both bodies stop receiving messages.
static TaskMessageEntry _gActor450800KyleMadiganMessages[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor450800PlayKyleMadiganAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor450800SetKyleMadiganDrawFlags },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor450800ApplyKyleMadiganGunCommand },
    { ACTOR_MESSAGE_WALK_TO, _actor450800SetKyleMadiganWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor450800NurseryActorTasks[ACTOR_450800_NURSERY_ACTOR_TASK_COUNT] = {
    [ACTOR_450800_NURSERY_ACTOR_TASK_KYLE] = {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor450800KyleMadiganTask,
        .data     = { .model = &gKyleBody },
    },
    [ACTOR_450800_NURSERY_ACTOR_TASK_LEFT_HAND] = {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor450800KyleMadiganAttachmentTask,
        .data     = { .model = &gKyleHandLeft },
    },
    [ACTOR_450800_NURSERY_ACTOR_TASK_RIGHT_HAND] = {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor450800KyleMadiganAttachmentTask,
        .data     = { .model = &gKyleHandRight },
    },
    [ACTOR_450800_NURSERY_ACTOR_TASK_SECOND_BODY] = {
        .header   = { .fields = { .flags = TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor450800KyleMadiganTask,
        .data     = { .model = &_gKyleAlternateBody },
    },
    [ACTOR_450800_NURSERY_ACTOR_TASK_GUN] = {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor450800KyleMadiganAttachmentTask,
        .data     = { .model = &gKyleP229 },
    },
};

/// Fixed clip bank borrowed by Kyle and the nursery's second scene body.
///
/// Entry 0 is unplayable; entries 1..30 are loaded clips for twenty-part rigs.
/// Clip 1 initializes the body; clips 2/14/15 drive travel, 3 drives turning and
/// 13 is arrival idle. Keep the table and clip data loaded for both bodies'
/// animation lifetime. This is a native pointer table with no bank extension.
#include "assets/rooms/shelter/b6_nursery/kyle_bank.inc"

#include "assets/enemies/golem/pawn/body.inc"

#include "assets/enemies/golem/beam_sword.inc"

#include "assets/rooms/shelter/b6_nursery/kyle_clips_4.inc"

/// Message handlers borrowed by this carrier's pair-walker tasks.
///
/// Supports animation, paired draw flags, placement and XZ walk targets;
/// actor commands are ignored. The terminal entry makes other IDs return 0.
/// Keep this table and its handlers loaded while the walker can receive messages.
static TaskMessageEntry _gPairWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pairWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor450800IgnorePawnGolemCommand },
    { ACTOR_MESSAGE_WALK_TO, _pairWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor450800PawnGolemTasks[PAIR_WALK_TASK_COUNT] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_ACTOR } }, _actor450800PawnGolemWalkerTask, { .model = &_gGolemPawnBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_ACTOR } }, _pairWalkSubModelTask, { .model = &_gGolemBeamSword } },
};

/// Six-entry clip bank borrowed by this carrier's pair-walker animation context.
///
/// Entry 0 is unplayable; entries 1..5 are loaded clips. Entry 1 is the
/// default pose and entry 4 drives travel. The table and clip data must remain
/// loaded for the walker's lifetime; the table holds pointers, not packed bytes.
#include "assets/rooms/shelter/b6_nursery/pawn_golem_bank.inc"

void actor450800StartNurseryInteraction(void)
{
    enum {
        ACTOR_450800_NURSERY_INTERACTION_VIEW = 4,
        ACTOR_450800_NURSERY_REPEAT_PROGRESS  = 1,
        ACTOR_450800_NURSERY_COUNT_LIMIT      = 3,
        ACTOR_450800_NURSERY_FIRST_SCENE      = 1,
        ACTOR_450800_NURSERY_OBJECTIVE        = 0x32
    };
    s32 interactionCount;
    s32 sceneCount;

    if (gGameSession->location.view == ACTOR_450800_NURSERY_INTERACTION_VIEW) {
        if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == ACTOR_450800_NURSERY_REPEAT_PROGRESS) {
            interactionCount                     = _gActor450800NurseryInteractionCount + 1;
            _gActor450800NurseryInteractionCount = interactionCount;
            if (interactionCount >= ACTOR_450800_NURSERY_COUNT_LIMIT) {
                _gActor450800NurseryInteractionCount = ACTOR_450800_NURSERY_COUNT_LIMIT;
                evsStartScript(_gActor450800NurseryKyleFinalRepeatDialogueScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(_gActor450800NurseryKyleRepeatDialogueScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
        } else {
            sceneCount = gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) + 1;
            if (sceneCount >= ACTOR_450800_NURSERY_COUNT_LIMIT + 1) {
                sceneCount = ACTOR_450800_NURSERY_COUNT_LIMIT;
            }
            gameFlagSetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT, sceneCount);
            if (sceneCount == ACTOR_450800_NURSERY_FIRST_SCENE) {
                if (gameFlagGetNibble(GAME_FLAG_083) == sceneCount) {
                    evsStartScript(_gActor450800NurseryFirstVisitDialogue6Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                } else {
                    evsStartScript(_gActor450800NurseryFirstVisitDialogue7Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                }
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, ACTOR_450800_NURSERY_OBJECTIVE);
            } else {
                evsStartScript(_gActor450800NurseryLaterVisitDialogueScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
        }
    }
}

/// Selects the nursery scene's CAP resource and text texture, or restores defaults.
///
/// Nonzero `useNurseryCaptions` selects loaded data resource ordinal 2 with
/// text at VRAM (832, 0). The resource must be loaded and writable for CAP
/// relocation, and remain live through playback. Zero resets CAP playback
/// state and selects the default resource and texture.
static void _actor450800SetNurseryCaptions(s32 useNurseryCaptions)
{
    enum {
        ACTOR_450800_NURSERY_CAP_RESOURCE = 2,
        ACTOR_450800_NURSERY_TEXT_VRAM_X  = 832,
        ACTOR_450800_NURSERY_TEXT_VRAM_Y  = 0,
    };

    if (useNurseryCaptions != 0) {
        gCapFile = NULL;
        capSelectLoadedFile(ACTOR_450800_NURSERY_CAP_RESOURCE);
        capSetTexturePage(ACTOR_450800_NURSERY_TEXT_VRAM_X, ACTOR_450800_NURSERY_TEXT_VRAM_Y);
        return;
    }
    capReset();
}

/// Adapts a script word to the nursery's glint-pulse and spark-shower controls.
///
/// High halfword is fastGlintPulse; low halfword is sparkShowerScale. Requires
/// the nursery effect state to be loaded. Both values are consumed immediately.
static void _actor450800SetNurseryEffectCues(u32 packedCues)
{
    shelterB6NurserySetEffectCues(packedCues >> 16, packedCues & 0xFFFF);
}

/// Starts the CAP sequence for a repeat nursery interaction.
///
/// Requires the nursery CAP resource to be selected. Selector 1 uses the low
/// half of the interaction counter plus 2; other selectors use sequence 8
/// when the saved scene count is 2, and sequence 9 otherwise. Starts ordinary
/// playback with variant key 0; the CAP start result is ignored.
static void _actor450800StartNurseryRepeatDialogue(s32 dialogueSelector)
{
    enum {
        ACTOR_450800_DIALOGUE_FROM_INTERACTION_COUNT = 1,
        ACTOR_450800_INTERACTION_SEQUENCE_BASE       = 2,
        ACTOR_450800_SECOND_REPEAT_SCENE_COUNT       = 2,
        ACTOR_450800_SECOND_REPEAT_SEQUENCE          = 8,
        ACTOR_450800_LATER_REPEAT_SEQUENCE           = 9,
    };
    s16 sequenceIndex;

    // Separate call sites preserve the two arms' argument setup in the binary.
    if (dialogueSelector == ACTOR_450800_DIALOGUE_FROM_INTERACTION_COUNT) {
        sequenceIndex = (u16)_gActor450800NurseryInteractionCount + ACTOR_450800_INTERACTION_SEQUENCE_BASE;
        capStartSequenceSlot(sequenceIndex, 0, 0);
    } else {
        if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) == ACTOR_450800_SECOND_REPEAT_SCENE_COUNT) {
            sequenceIndex = ACTOR_450800_SECOND_REPEAT_SEQUENCE;
        } else {
            sequenceIndex = ACTOR_450800_LATER_REPEAT_SEQUENCE;
        }
        capStartSequenceSlot(sequenceIndex, 0, 0);
    }
}

void actor450800StartNurseryCompanionDialogue(void)
{
    evsStartScript(_gActor450800NurseryCompanionDialogueScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
}

void actor450800PrepareNurseryKyleMadigan(void)
{
    enum { ACTOR_450800_NURSERY_KYLE_SCENE_ACTOR = 0 };

    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACTOR_450800_NURSERY_KYLE_SCENE_ACTOR), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor450800NurseryKyleClip20Reset, 0);
    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACTOR_450800_NURSERY_KYLE_SCENE_ACTOR), ACTOR_MESSAGE_PLACE, &_gActor450800NurseryKylePlacement, 0);
}

/// Restarts play in growth room 1 through arrival 1 after the nursery scene.
///
/// Attract demo scene 9 suppresses the restart. Otherwise updates the live
/// save's destination and selects sprite variant 1 before spawning the
/// resident session-restart task; its spawn result is ignored.
static void _actor450800EnterGrowthRoom(void)
{
    enum {
        ACTOR_450800_NURSERY_DEMO_SCENE    = 9,
        ACTOR_450800_GROWTH_ROOM_ARRIVAL   = 1,
        ACTOR_450800_GROWTH_ROOM           = 1,
        ACTOR_450800_GROWTH_SPRITE_VARIANT = 1,
        ACTOR_450800_SESSION_TASK_BANK     = GAME_FLOW_RELOAD_TASK_BANK,
        ACTOR_450800_SESSION_RESTART_TASK  = GAME_FLOW_RELOAD_TASK_SLOT,
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_450800_NURSERY_DEMO_SCENE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_SHELTER_NEO_ARK;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B6_GROWTH_ROOM;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACTOR_450800_GROWTH_ROOM_ARRIVAL;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_450800_GROWTH_ROOM;
        gDisplayState.spriteVariant                                 = ACTOR_450800_GROWTH_SPRITE_VARIANT;
        taskSpawn(ACTOR_450800_SESSION_TASK_BANK, ACTOR_450800_SESSION_RESTART_TASK, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    }
}

/// Forwards the script argument's low byte to the nursery's view-13 sprite visibility.
///
/// Requires the nursery sprite tables to be loaded. Low byte 0 shows the
/// sprite and 1 hides it; other low-byte values leave visibility unchanged.
static void _actor450800SetNurseryView13SpriteHidden(s32 hidden)
{
    shelterB6NurserySetView13SpriteHidden(hidden & 0xFF);
}

/// Spawns the nursery scene's world-space impact flash and six sparks.
///
/// Placement is copied synchronously from a stack vector. The factory also
/// records that vector's address, but the impact task never reads it after
/// spawning; it uses the resulting coordinate. The flash's depth-scaled size
/// is 512. Spawn failure is ignored.
static void _actor450800SpawnNurseryImpactSpark(void)
{
    enum { ACTOR_450800_NURSERY_IMPACT_FLASH_SIZE = 512 };
    SVECTOR worldPosition;

    worldPosition = _gActor450800NurseryImpactSparkPosition;
    effectSpawn(EFFECT_IMPACT_SPARK, NULL, ACTOR_450800_NURSERY_IMPACT_FLASH_SIZE, &worldPosition);
}

/// Applies the owning actor's placement texture offsets to a helper model.
///
/// Requires live helper/actor models and the actor's enemy placement index in
/// the current area's synchronized variant. Copies texture-page and CLUT-row
/// offsets; if a primitive buffer exists, rebuilds both alternating halves.
/// Borrows all inputs only for this call and changes no lighting or colours.
static inline void _actor450800ApplyHelperPlacementTexture(Task* spawned, Task* actor)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    AreaPlacement*   texturePlacement;
    TmdObject*       helperModel;
    u32              placementIndex;

    sessionKey     = &gGameSession->location;
    placementIndex = ((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    helperModel    = spawned->extra.tmd;
    key.stage      = sessionKey->stage;
    key.area       = sessionKey->area;
    key.roomState  = sessionKey->roomState;
    key.view       = gGameSession->location.view;
    areaSyncLocationVariant(&key);
    texturePlacement               = AREA_PLACEMENT_AT(areaGetVariant(&key)->placements, placementIndex);
    helperModel->texturePageOffset = texturePlacement->texturePageOffset;
    helperModel->clutRowOffset     = texturePlacement->clutRowOffset;
    if (helperModel->buffer != NULL) {
        tmdBuildBufferHalf(helperModel);
        tmdBuildBufferHalf(helperModel);
    }
}

/// Initializes a Kyle scene body, its animation and its hand/gun helper tasks.
///
/// Requires a live enemy, twenty-coordinate body and loaded clip/helper tables.
/// Allocates zeroed owned work; failure destroys the enemy and task. The model
/// borrows the work's lighting matrices. Starts clip 1 on slots 1..19 and spawns
/// helpers for body parts 8/12; hand textures use the actor's placement.
/// Later drawing and exit require all three helper spawns to have succeeded;
/// retained behavior leaves a failed helper NULL and still advances the task.
/// Spawn argument 1's signed high half of 1 makes the body visible.
static void _actor450800SpawnKyleMadigan(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_450800_KYLE_SHOW_ON_SPAWN        = 1,
        ACTOR_450800_KYLE_INITIAL_CLIP         = 1,
        ACTOR_450800_KYLE_INITIAL_BLEND_FRAMES = 8,
        ACTOR_450800_KYLE_LIGHT_HEIGHT         = 800,
        ACTOR_450800_KYLE_HAND_AND_GUN_PART    = 8,
        ACTOR_450800_KYLE_OTHER_HAND_PART      = 12
    };
    VECTOR                       lightingPosition;
    GfxCoord*                    rootCoord;
    TmdObject*                   model;
    _Actor450800KyleMadiganWork* work;
    Task*                        spawned;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = memCalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor450800ExitKyleMadigan;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    if ((s16)(task->spawnArg1.value >> 16) == ACTOR_450800_KYLE_SHOW_ON_SPAWN) {
        model->flags = 0;
    }
    model->otOffset     = 1;
    model->lightMtx     = &work->light;
    model->colorMtx     = &work->color;
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1] - ACTOR_450800_KYLE_LIGHT_HEIGHT;
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingPosition, 0, 3);
    animationInitContext(&work->rig.anim, _gActor450800KyleMadiganAnimationSets, model, work->rig.poses,
                         work->rig.slots);
    work->st.animId = ACTOR_450800_KYLE_INITIAL_CLIP;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;

    // The actor owns the helper tasks and explicitly destroys them on exit.
    spawned = taskSpawnFromTable(gActor450800NurseryActorTasks, ACTOR_450800_NURSERY_ACTOR_TASK_LEFT_HAND, ACTOR_450800_KYLE_HAND_AND_GUN_PART, 0);
    if (spawned != NULL) {
        work->handLeftTask = spawned;
        spawned->parent    = task;
        _actor450800ApplyHelperPlacementTexture(spawned, task);
    }

    spawned = taskSpawnFromTable(gActor450800NurseryActorTasks, ACTOR_450800_NURSERY_ACTOR_TASK_RIGHT_HAND, ACTOR_450800_KYLE_OTHER_HAND_PART, 0);
    if (spawned != NULL) {
        work->handRightTask = spawned;
        spawned->parent     = task;
        _actor450800ApplyHelperPlacementTexture(spawned, task);
    }

    spawned = taskSpawnFromTable(gActor450800NurseryActorTasks, ACTOR_450800_NURSERY_ACTOR_TASK_GUN, ACTOR_450800_KYLE_HAND_AND_GUN_PART, 0);
    if (spawned != NULL) {
        spawned->parent = task;
        work->gunTask   = spawned;
    }

    work->blendFrames = ACTOR_450800_KYLE_INITIAL_BLEND_FRAMES;
    work->st.travel   = 0;
    work->turnFrames  = 0;
    work->gunShown    = 0;
    task->msgTable    = _gActor450800KyleMadiganMessages;
    _actor450800UpdateKyleMadigan(task);
    task->state++;
}

/// Advances Kyle's scheduled turn by 51/4096 of a turn and consumes one update.
///
/// Borrows writable work and its live model root. The caller must select the
/// turn clip and check the nonzero countdown; this helper does neither.
/// Narrows the new heading to signed 16 bits before applying it in the root's
/// parent space. Replaces rotation at unit scale, retains translation and
/// marks composition dirty without composing the root.
/// Requires an initialized scratch stack with 0x24 free word-aligned bytes
/// disjoint from both arguments; the rotation releases them before returning.
static inline void _actor450800StepKyleMadiganTurn(_Actor450800KyleMadiganWork* work, GfxCoord* rootCoord)
{
    enum { ACTOR_450800_KYLE_TURN_ANGLE_STEP = 51 };

    work->st.yaw += ACTOR_450800_KYLE_TURN_ANGLE_STEP;
    gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->turnFrames--;
}

/// Reseeds Kyle's animation or advances one frame of his scripted travel and turn.
///
/// Requires a live TMD task with `_Actor450800KyleMadiganWork` and a loaded
/// twenty-slot rig. Reset and blend requests reseed slots 1..19 and enter
/// normal ticking without consuming travel. In ticking state, clips 2, 14
/// and 15 consume one remaining travel frame, even when actor freezing
/// suppresses movement. Fast, backward and slow strides are 60, -15 and 25
/// parent-coordinate units; other modes count frames without moving.
/// Arrival queues idle clip 13 with a ten-frame blend for the next update.
///
/// Clip 3 advances a nonzero turn countdown by 51/4096 of a turn per frame.
/// This package never seeds that countdown. Slot ticking follows movement
/// and turning; scratch/GTE requirements follow the movement and slot helpers.
static void _actor450800UpdateKyleMadigan(Task* task)
{
    enum {
        ACTOR_450800_KYLE_ANIM_TRAVEL_2     = 2,
        ACTOR_450800_KYLE_ANIM_TURN         = 3,
        ACTOR_450800_KYLE_ANIM_IDLE         = 13,
        ACTOR_450800_KYLE_ANIM_TRAVEL_14    = 14,
        ACTOR_450800_KYLE_ANIM_TRAVEL_15    = 15,
        ACTOR_450800_KYLE_IDLE_BLEND_FRAMES = 10,
    };
    GfxCoord*                    rootCoord = task->extra.tmd->coords;
    _Actor450800KyleMadiganWork* work      = task->work;

    // A reseed occupies the frame; ordinary playback moves only in ticking state.
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        PACED_WALK_BLEND_ANIM(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        PACED_WALK_RESET_ANIM(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == ACTOR_450800_KYLE_ANIM_TRAVEL_14 || work->st.animId == ACTOR_450800_KYLE_ANIM_TRAVEL_2 || work->st.animId == ACTOR_450800_KYLE_ANIM_TRAVEL_15) {
            if (work->st.travel != 0) {
                switch (work->walkMode) {
                    case ACTOR_450800_WALK_FAST:
                        _actorMovementStepModelForward(task, ACTOR_450800_KYLE_FAST_STRIDE);
                        break;
                    case ACTOR_450800_WALK_BACKWARD:
                        _actorMovementStepModelForward(task, -ACTOR_450800_KYLE_BACKWARD_STRIDE);
                        break;
                    case ACTOR_450800_WALK_SLOW:
                        _actorMovementStepModelForward(task, ACTOR_450800_KYLE_SLOW_STRIDE);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
                    work->blendFrames = ACTOR_450800_KYLE_IDLE_BLEND_FRAMES;
                    work->st.animId   = ACTOR_450800_KYLE_ANIM_IDLE;
                }
            }
        }
        if (work->st.animId == ACTOR_450800_KYLE_ANIM_TURN && work->turnFrames != 0) {
            _actor450800StepKyleMadiganTurn(work, rootCoord);
        }
        _pacedWalkTickAnim(task);
    }
}

/// Dispatches a Kyle scene body's spawn or per-frame walker state.
///
/// Requires task state 0 or 1 and its live owning enemy in spawnArg2.pointer.
/// State 0 allocates work and helper tasks; state 1 requires that initialization
/// to have succeeded and composes, lights and draws the body's current pose.
static void _actor450800KyleMadiganTask(Task* task)
{
    EnemyTaskFunc states[2] = { _actor450800SpawnKyleMadigan, _actorRenderWalkerFrame };

    states[task->state](task->spawnArg2.pointer, task);
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
#define ACTOR_RENDER_UPDATE_WALKER _actor450800UpdateKyleMadigan
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases Kyle's enemy and actor task, then kills the two hands and gun tasks.
///
/// Requires a live enemy at `spawnArg2.pointer` and all three helper tasks.
/// The retained teardown order reads the helper pointers from the freed
/// `_Actor450800KyleMadiganWork`; those reads depend on its released storage
/// remaining intact until the helper kills finish.
static void _actor450800ExitKyleMadigan(Task* task)
{
    _Actor450800KyleMadiganWork* work = task->work;

    // The binary frees actor work before reading its three helper pointers.
    enemyDestroy(task->spawnArg2.pointer, task);
    taskKill(work->handLeftTask);
    taskKill(work->handRightTask);
    taskKill(work->gunTask);
}

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Attaches Kyle's hand or gun model to a body part and refreshes its room lighting.
///
/// Requires a live parent TMD task and a valid parent coordinate index in
/// `spawnArg1`. This package supplies part 8 for the left hand and gun, and
/// part 12 for the right hand. Initialization borrows that coordinate until
/// teardown; parent spawn variant 1 clears the attachment's draw flags.
/// Later frames sample lighting 800 world-coordinate units above the body's
/// root, rather than at the selected attachment part. The parent's composed
/// root transform must be current before those frames run.
static void _actor450800KyleMadiganAttachmentTask(Task* task)
{
    enum {
        ACTOR_450800_ATTACHMENT_INITIALIZE                = 0,
        ACTOR_450800_ATTACHMENT_LIGHT                     = 1,
        ACTOR_450800_ATTACHMENT_CLEAR_FLAGS_SPAWN_VARIANT = 1,
        ACTOR_450800_ATTACHMENT_LIGHT_HEIGHT              = 800,
        ACTOR_450800_ATTACHMENT_LIGHT_COUNT               = 3,
    };
    TmdObject* model           = task->extra.tmd;
    GfxCoord*  rootCoord       = model->coords;
    GfxCoord*  bodyCoords      = task->parent->extra.tmd->coords;
    GfxCoord*  attachmentCoord = bodyCoords + task->spawnArg1.value;
    VECTOR     lightSamplePosition;

    switch (task->state) {
        case ACTOR_450800_ATTACHMENT_INITIALIZE:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((s16)(task->parent->spawnArg1.value >> 16) == ACTOR_450800_ATTACHMENT_CLEAR_FLAGS_SPAWN_VARIANT) {
                model->flags = 0;
            }
            rootCoord->parent = attachmentCoord;
            task->state++;
            break;
        case ACTOR_450800_ATTACHMENT_LIGHT:
            lightSamplePosition.vx = bodyCoords->workm.t[0];
            lightSamplePosition.vy = bodyCoords->workm.t[1] - ACTOR_450800_ATTACHMENT_LIGHT_HEIGHT;
            lightSamplePosition.vz = bodyCoords->workm.t[2];
            worldCoordSetModelLighting(model, &lightSamplePosition, 0, ACTOR_450800_ATTACHMENT_LIGHT_COUNT);
            break;
    }
}

#include "../../shared/walk/paced_walk_tick_anim.inc.c"

#include "../../shared/walk/paced_walk_reset_anim.inc.c"

#include "../../shared/walk/paced_walk_blend_anim.inc.c"

/// Records Kyle's animation reseed request without advancing his playing slots.
///
/// Both pointers are borrowed for the call. The caller must supply a loaded
/// clip; the ID and blend duration narrow to signed halfwords. Reset leaves
/// the prior duration intact. Clears `st.field_6`, whose role is unproven.
static inline void _actor450800ApplyKyleMadiganAnimationRequest(_Actor450800KyleMadiganWork* work, const AnimationPlayRequest* request)
{
    work->st.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
        work->blendFrames = request->blendFrames;
    } else {
        work->st.state = ACTOR_ENEMY_ANIM_RESET;
    }
    work->st.field_6 = 0;
}

/// Immediately reseeds Kyle's non-root animation tracks from a scripted request.
///
/// Requires a live `_Actor450800KyleMadiganWork` and its loaded clips 1..30.
/// The signed check rejects only IDs >=31; callers must exclude negative IDs
/// and unloaded ID 0. Borrows the aligned request through dispatch without
/// retaining it. Nonzero blend selects a buffered blend, narrowing its whole
/// frame duration to a signed halfword; 0..2047 avoids a negative slot countdown.
/// Reset ignores the duration. Reseeds slots 1..19, leaving the root and travel
/// count unchanged. The bank selector, collision choice, message ID and last
/// argument are ignored. Returns 0 after reseeding or -1 on rejection.
static s32 _actor450800PlayKyleMadiganAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        ACTOR_450800_KYLE_ANIMATION_ID_LIMIT         = 31,
        ACTOR_450800_KYLE_ANIMATION_REQUEST_APPLIED  = 0,
        ACTOR_450800_KYLE_ANIMATION_REQUEST_REJECTED = -1,
    };
    _Actor450800KyleMadiganWork* work;

    work = task->work;
    if (request->animationId < ACTOR_450800_KYLE_ANIMATION_ID_LIMIT) {
        _actor450800ApplyKyleMadiganAnimationRequest(work, request);
        _actor450800UpdateKyleMadigan(task);
        return ACTOR_450800_KYLE_ANIMATION_REQUEST_APPLIED;
    }
    return ACTOR_450800_KYLE_ANIMATION_REQUEST_REJECTED;
}

/// Replaces draw flags on Kyle's body, hands and gun, retaining the gun's hidden latch.
///
/// Requires all three helper tasks and their models to be live. Bit 0 clears
/// all model flags; without it, flags become active-draw exclusion. Bit 1 adds
/// automatic-buffer exclusion; other request bits are ignored. A zero
/// `gunShown` forces both exclusions on the gun regardless of the request.
/// No buffers are allocated or freed. Message ID and last argument are ignored.
/// Returns 0.
static s32 _actor450800SetKyleMadiganDrawFlags(Task* task, s32 messageId, s32 flags, s32 unusedArgument)
{
    _Actor450800KyleMadiganWork* work;
    TmdObject*                   self;
    TmdObject*                   handLeft;
    TmdObject*                   handRight;
    TmdObject*                   gun;

    work      = task->work;
    self      = task->extra.tmd;
    handLeft  = work->handLeftTask->extra.tmd;
    handRight = work->handRightTask->extra.tmd;
    gun       = work->gunTask->extra.tmd;

    if (flags & ACTOR_MESSAGE_PAIR_SHOW) {
        self->flags      = 0;
        handLeft->flags  = 0;
        handRight->flags = 0;
        gun->flags       = 0;
    } else {
        self->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        handLeft->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        handRight->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        gun->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        self->flags      |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        handLeft->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        handRight->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        gun->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if (work->gunShown == 0) {
        gun->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
    return 0;
}

#include "../../shared/walk/paced_walk_place.inc.c"

/// Fires, shows or hides Kyle's gun in response to an actor command.
///
/// Requires live body work, gun task/model and gun root even for ignored commands.
/// Command 0 spawns the NPC-pistol muzzle flash; 1 copies the body's draw flags
/// and latches gunShown; 2 hides the gun and clears the latch. Other commands
/// leave it unchanged. Borrows the request through this call, ignores its
/// context, messageId and unusedArgument, and returns 0.
static s32 _actor450800ApplyKyleMadiganGunCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    enum {
        ACTOR_450800_GUN_NPC_FLASH_PROFILE = 33
    };
    _Actor450800KyleMadiganWork* work     = task->work;
    TmdObject*                   gun      = work->gunTask->extra.tmd;
    GfxCoord*                    gunCoord = gun->coords;
    TmdObject*                   body     = task->extra.tmd;
    s32                          command  = request->command;

    switch (command) {
        case ACTOR_450800_GUN_FIRE:
            effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, gunCoord, ACTOR_450800_GUN_NPC_FLASH_PROFILE, NULL);
            break;
        case ACTOR_450800_GUN_SHOW:
            work->gunShown = command;
            gun->flags     = body->flags;
            break;
        case ACTOR_450800_GUN_HIDE:
            work->gunShown = 0;
            gun->flags     = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
    }
    return 0;
}

/// Aims Kyle at a borrowed target and records the remaining travel frames.
///
/// Requires a live TMD model and `_Actor450800KyleMadiganWork`. Reads only X
/// and Z of the aligned target, in the model root parent's coordinate units;
/// retains no pointer. Mode narrows to a signed halfword: 0 faces forward
/// with stride 60, 1 faces away with backward stride 15, and 2 faces forward
/// with stride 25. Other
/// modes record the slow-stride count but the update makes no translation.
/// Angles use 4096 units per turn and narrow to signed halfwords. The XZ
/// distance divided by stride truncates to whole frames and narrows to a
/// signed halfword; callers must keep the squared-distance arithmetic in range
/// and the resulting count in 0..32767. Does not start a travel clip or dirty
/// composition. The frame driver composes and draws the model separately.
/// Message ID is ignored. Returns 0.
static s32 _actor450800SetKyleMadiganWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode)
{
    _Actor450800KyleMadiganWork* work;
    GfxCoord*                    coord;
    s32                          deltaX;
    s32                          deltaZ;
    s32                          stride;
    s32                          distance;
    s32                          yaw;

    coord  = task->extra.tmd->coords;
    work   = task->work;
    deltaX = target->vx - coord->coord.t[0];
    deltaZ = target->vz - coord->coord.t[2];
    // Keep this store after the differences to preserve entry-block scheduling.
    work->walkMode = mode;
    yaw            = ratan2(deltaX, deltaZ);
    work->st.yaw   = yaw;
    if (work->walkMode == ACTOR_450800_WALK_BACKWARD) {
        work->st.yaw = yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    }
    gfxRotMatrixY(&coord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    distance = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ);
    stride   = ACTOR_450800_KYLE_SLOW_STRIDE;
    switch (work->walkMode) {
        case ACTOR_450800_WALK_FAST:
            stride = ACTOR_450800_KYLE_FAST_STRIDE;
            break;
        case ACTOR_450800_WALK_BACKWARD:
            stride = ACTOR_450800_KYLE_BACKWARD_STRIDE;
            break;
        case ACTOR_450800_WALK_SLOW:
            break;
    }
    work->st.travel = distance / stride;
    return 0;
}

/// Binds the shared pair-walker spawn to this package's public TaskDesc table.
///
/// The table uses PAIR_WALK_TASK_* indices. This identifier alias evaluates no
/// arguments, captures no locals and is scoped to the following fragment.
#define PAIR_WALK_TASK_TABLE gActor450800PawnGolemTasks
#include "../../shared/walk/pair_walk_spawn.inc.c"

/// Dispatches the scripted Pawn GOLEM walker and its carried Beam Sword.
///
/// Task state must be 0 (initialize) or 1 (frame update), with a live enemy in
/// spawnArg2.pointer and the nineteen-part descriptor model. Initialization
/// owns the walker work and child task; frame updates use the pair-walk motion
/// and animation state with this carrier's lighting and ground shadow.
static void _actor450800PawnGolemWalkerTask(Task* task)
{
    EnemyTaskFunc handlers[2] = { _pairWalkSpawn, _actorRenderWalkerFrameSecond };

    handlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
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

#include "../../shared/walk/pair_walk_exit.inc.c"

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/walk/pair_walk.inc.c"

#include "../../shared/walk/pair_walk_play.inc.c"

#include "../../shared/walk/pair_walk_visibility.inc.c"

/// Ignores every actor-command message delivered to the package's pawn golem.
///
/// The receiver and both payloads are unused; no command storage is read or
/// retained. Returns 0 without changing the golem or its carried model.
static s32 _actor450800IgnorePawnGolemCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    return 0;
}

#include "../../shared/walk/pair_walk_to.inc.c"

#include "../../shared/walk/pair_walk_sub_model.inc.c"
