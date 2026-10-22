#include "actors/actor_443500.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"
#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/shelter_r47.h"
#include "../../shared/actor/model_placement.h"
#include "../../shared/actor/actor_messages.h"

/// Loaded CAP resource and texture origin used by this package's event scripts.
enum {
    ACTOR_443500_EVENT_CAP_FILE_ORDINAL   = 1,
    ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_X = 576,
    ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_Y = 256,
};

/// Tick of each pass of the default clip on which the clip's sound is started.
enum { ACTOR_443500_PIERCE_CARRADINE_LOOP_SOUND_TICK = 15 };

/// Execution priority of this package's actor and fade tasks.
enum { ACTOR_443500_TASK_PRIORITY = 192 };

/// Pierce's playback bank, fallback clip and room placement index.
///
/// The bank drives slots 1..19. His playback handler ignores the requests'
/// world-collision word; collision geometry is installed separately.
enum {
    ACTOR_443500_PIERCE_ANIMATION_BANK  = 0,
    ACTOR_443500_DEFAULT_ANIMATION      = 28,
    ACTOR_443500_PIERCE_PLACEMENT_INDEX = 0,
};

/// Player clip shared by both terminal-tour bank extensions.
enum { ACTOR_443500_PLAYER_SHARED_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 1 };

/// Work block of Pierce Carradine in the shelter's room 47.
///
/// The actor's task allocates it zeroed in its spawn state and keeps it at
/// `Task::work` for the task's life. It opens with the rig and the model state
/// a twenty-part actor that only plays clips keeps (`ActorMotionPlayWork`
/// names the pair); the package's own play handler runs on them, and the model
/// object borrows `model.light` and `model.color` for as long as the block
/// lives.
///
/// What follows is the package's own: the timing of the default clip's sound,
/// the delayed free of the model's buffers once the model has been hidden, and
/// the model's flags as kept across the camera views that hide it.
typedef struct {
    ActorAnimRig20  rig;              // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;            // Clip and bank the rig plays, and the matrices the model is lit with
    byte            unknown_4B8[0x2]; // Never accessed; role unproven
    s16             loopTicks;        // Ticks the default clip has played since a play request or the clip's loop jump last zeroed it; the tick that takes it to `ACTOR_443500_PIERCE_CARRADINE_LOOP_SOUND_TICK` starts the clip's sound
    s32             freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    u32             savedModelFlags;  // The model's `TmdObject::flags` as last recorded: at spawn, after each draw-mode message, and, while no event holds the scene, on each tick in camera views 0 to 3 before that tick hides the model; written back to the model on such a tick in views 4 and 5
} _Actor443500PierceCarradineWork;
STATIC_ASSERT_SIZEOF(_Actor443500PierceCarradineWork, 0x4C4);

static void _actor443500InitPierce(Task* task);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _actor443500TickPierce(Task* task);
static void _actor443500ExitPierce(Task* task);
static void _actor443500BindModelLighting(Task* task);
static s32  _actor443500PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actor443500SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void _actor443500InstallPierceCollision(s32 shiftY);
static void _actor443500ChildModelTask(Task* task);

/// Held-model handlers for task states 0 setup, 1 mirror draw policy and 2 exit.
///
/// There is no terminator; dispatch requires a state in 0..2. The child borrows
/// its parent model's part coordinate and lighting until parent teardown.
static const TaskFuncTable3 _gActor443500ChildModelStates = {
    { _modelPlacementAttachChild, _modelPlacementMirrorParentDrawFlags, taskKill }
};

/// Pierce's main-task handlers for states 0 setup, 1 frame update and 2 exit.
///
/// No terminator or bounds check; the dispatcher requires state 0..2 and
/// skips every handler while scene actor control is paused.
static const TaskFuncTable3 _gActor443500PierceStates = {
    .funcs = { _actor443500InitPierce, _actor443500TickPierce, _actor443500ExitPierce },
};

static TmdSource _gPierceBody;
static void      _actor443500PierceTask(Task* task);

static void _actor443500SelectEventCap(s32 useEventCap);
static void _actor443500SelectEventCapAfterPowerPlantClear(s32 useEventCap);
static void _actor443500RunPowerPlantProgressCap(void);
static void _actor443500OpenTimedMapTerminal(void);
static void _actor443500StartFadeFromBlack(void);
static void _actor443500StartCapCommand5(s16 variantKey);
static void _actor443500ApplyShelterAreaUpdates(void);
static void _actor443500SetSceneEvent(s8 sceneEvent);

extern AnimationSet gAyaStandClip;
static AnimationSet _gShelterR47PlayerConsoleEntryClip;
static AnimationSet _gShelterR47PlayerAskClip;
static AnimationSet _gShelterR47PlayerAskAboutBarrierClip;
static AnimationSet _gShelterR47PlayerMapListenClip;
static AnimationSet _gShelterR47PlayerDayBeforeYesterdayClip;
static AnimationSet _gShelterR47TerminalFollowupPlayerAnim07Clip;
static AnimationSet _gShelterR47PlayerTakeImplantClip;
static AnimationSet _gShelterR47PlayerImplantTakenClip;
static AnimationSet _gShelterR47PlayerThatMeansClip;
static AnimationSet _gShelterR47PlayerMapFollowupStartClip;
static AnimationSet _gShelterR47PlayerISeeClip;
static AnimationSet _gShelterR47PlayerDecideToGoClip;
static AnimationSet _gShelterR47PlayerButYouClip;
static AnimationSet _gShelterR47PlayerListenToPierceClip;
static AnimationSet _gShelterR47PlayerProtestClip;
static AnimationSet _gShelterR47PlayerWhatClip;
static AnimationSet _gShelterR47PlayerSetOffClip;
extern AnimationSet gShelterR47PlayerLookAtMapClip;
extern AnimationSet gAyaTalkStartClip;
extern AnimationSet gAyaTalkClip;
extern AnimationSet gAyaTalkEndClip;
static AnimationSet _gAyaExplainStartClip;
static AnimationSet _gAyaExplainClip;

static void _actor443500FadeFromBlackTask(Task* task);

#include "assets/rooms/shelter/r47/pierce_clips_1.inc"

/// Bodyless task recipe for the terminal scenes' 30-tick fade from black.
///
/// Spawn at entry 0 with zero arguments. The task owns its elapsed-tick counter;
/// this overlay must remain loaded until the fade kills the task.
static TaskDesc _gActor443500FadeFromBlackTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, ACTOR_443500_TASK_PRIORITY } },
    .callback = _actor443500FadeFromBlackTask,
    .data     = { .value = 0 },
};

/// Aya's extension clips for the opening scene of Pierce's terminal tour.
///
/// Copied to player-bank entries 47..57, including NULL at 47. Clips 48..57
/// borrow this overlay's animation data for playback; the base bank is retained.
#include "assets/rooms/shelter/r47/tour_opening_player_bank.inc"

/// Request to copy the tour-opening clips into Aya's equipped bank extension.
///
/// Copies the complete set-pointer table in 32-bit words. The request is consumed
/// during dispatch; keep this overlay loaded until the copied clips stop playing.
static AnimationBankCopyRequest _gActor443500TourOpeningPlayerBankExtension = {
    .sets  = _gActor443500TourOpeningPlayerAnimationSets,
    .count = ARRAY_SIZE(_gActor443500TourOpeningPlayerAnimationSets),
};

/// Unplayed clip-1 reset preset beside the tour-opening player requests.
///
/// Bank selector 1 and enabled world collision are retained. Its position in
/// the player request run suggests Aya as receiver; no dispatch proves that.
static AnimationPlayRequest _gActor443500UnusedTourOpeningPlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's shared extension clip with a four-frame blend during the opening tour.
///
/// The event interpreter replaces the stored bank selector with the equipped
/// weapon's bank. Requires the copied extension and restores grid participation.
static AnimationPlayRequest _gActor443500PlayerClip48Blend4Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_PLAYER_SHARED_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player-bank set indices supplied by the opening-tour extension.
enum {
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_51 = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_52 = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_53 = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_54 = ANIMATION_BANK_BASE_SET_COUNT + 7,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_55 = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_56 = ANIMATION_BANK_BASE_SET_COUNT + 9,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_57 = ANIMATION_BANK_BASE_SET_COUNT + 10,
};

/// Aya's opening-tour clip 49 preset, resetting playback without a pose blend.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 50 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_50,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 51 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 52 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_52,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 53 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_53,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 54 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_54,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 55 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 56 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 57 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_57,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's placement at the start of Pierce's terminal tour.
///
/// XYZ is in room-coordinate units; yaw is one eighth of a 4096-unit turn.
/// The placement message consumes XYZ and all three Euler angles during dispatch.
static ActorTransform _gActor443500PlayerTourOpeningEntryPlacement = {
    .pos = { 15000, -1000, 7600, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 },
};

/// Aya's console position at the end of the opening tour and on its skip path.
///
/// XYZ is in room-coordinate units; Euler angles use 4096 units per turn.
/// Walking consumes only XYZ; the skip path places the full transform directly.
static ActorTransform _gActor443500PlayerConsolePlacement = {
    .pos = { 15600, -1000, 9000, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 },
};

/// Aya's extension clips for the console and map-terminal follow-up scenes.
///
/// Copied to player-bank entries 47..71, including NULL at 47. Clips 48..71
/// borrow this overlay's animation data; clip 48 is shared with the opening tour.
#include "assets/rooms/shelter/r47/terminal_followup_player_bank.inc"

/// Request to copy the terminal follow-up clips into Aya's bank extension.
///
/// Copies the complete set-pointer table in 32-bit words, including its NULL
/// entry. Keep this overlay loaded while any copied clip remains in playback.
static AnimationBankCopyRequest _gActor443500TerminalFollowupPlayerBankExtension = {
    .sets  = _gActor443500TerminalFollowupPlayerAnimationSets,
    .count = ARRAY_SIZE(_gActor443500TerminalFollowupPlayerAnimationSets),
};

/// Unplayed clip-1 reset preset beside the terminal-follow-up player requests.
///
/// Bank selector 1 and enabled world collision are retained. Its position in
/// the player request run suggests Aya as receiver; no dispatch proves that.
static AnimationPlayRequest _gActor443500UnusedTerminalFollowupPlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's shared extension clip with a ten-frame blend in terminal follow-ups.
///
/// The event interpreter replaces the stored bank selector with the equipped
/// weapon's bank. Requires the copied extension and restores grid participation.
static AnimationPlayRequest _gActor443500PlayerClip48Blend10Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_PLAYER_SHARED_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player-bank set indices supplied by the terminal-follow-up extension.
enum {
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_51 = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_52 = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_53 = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_55 = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_56 = ANIMATION_BANK_BASE_SET_COUNT + 9,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_57 = ANIMATION_BANK_BASE_SET_COUNT + 10,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_58 = ANIMATION_BANK_BASE_SET_COUNT + 11,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_59 = ANIMATION_BANK_BASE_SET_COUNT + 12,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_60 = ANIMATION_BANK_BASE_SET_COUNT + 13,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_61 = ANIMATION_BANK_BASE_SET_COUNT + 14,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_62 = ANIMATION_BANK_BASE_SET_COUNT + 15,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_63 = ANIMATION_BANK_BASE_SET_COUNT + 16,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_64 = ANIMATION_BANK_BASE_SET_COUNT + 17,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_65 = ANIMATION_BANK_BASE_SET_COUNT + 18,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_66 = ANIMATION_BANK_BASE_SET_COUNT + 19,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_67 = ANIMATION_BANK_BASE_SET_COUNT + 20,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_69 = ANIMATION_BANK_BASE_SET_COUNT + 22,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_70 = ANIMATION_BANK_BASE_SET_COUNT + 23,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_71 = ANIMATION_BANK_BASE_SET_COUNT + 24,
};

/// Aya's terminal-follow-up clip 49 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 50 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 51 preset, blending over 20 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 52 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 53 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum { ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_54 = ANIMATION_BANK_BASE_SET_COUNT + 7 };

/// Unplayed clip-54 reset preset in the terminal-follow-up player request run.
///
/// Requests world collision with bank selector 1. The receiver is inferred
/// from the neighboring player requests; no dispatch proves it.
static AnimationPlayRequest _gActor443500UnusedTerminalFollowupPlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 55 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_55,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 56 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 57 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 58 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip58Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_58,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 59 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip59Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_59,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 60 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip60Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_60,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 61 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip61Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_61,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 62 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip62Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_62,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 63 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip63Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_63,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 64 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip64Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_64,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 65 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip65Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_65,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 66 preset, blending over 20 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip66Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_66,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 67 preset, blending over 2 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip67Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_67,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 2,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum { ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_68 = ANIMATION_BANK_BASE_SET_COUNT + 21 };

/// Unplayed clip-68 preset with a ten-frame blend in the player request run.
///
/// Requests world collision with bank selector 1. The receiver is inferred
/// from the neighboring player requests; no dispatch proves it.
static AnimationPlayRequest _gActor443500UnusedTerminalFollowupPlayerClip68Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_68,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 69 preset, blending over 20 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip69Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_69,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 70 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip70Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_70,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 71 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip71Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_71,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's entry placement for the scene after using Pierce's console terminal.
///
/// XYZ is in room-coordinate units; yaw is exactly 2047/4096 of a turn.
/// The placement message consumes XYZ and all three Euler angles during dispatch.
static ActorTransform _gActor443500PlayerTerminalFollowupEntryPlacement = {
    .pos = { 16200, -1000, 8000, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1, 0, 0 },
};

/// Aya's map-terminal destination and placement for follow-up and skip scenes.
///
/// XYZ is in room-coordinate units; yaw is one third of a 4096-unit turn,
/// truncated to 1365. Walking consumes XYZ; direct placement also consumes angles.
static ActorTransform _gActor443500PlayerMapTerminalPlacement = {
    .pos = { 16300, -1000, 6950, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 3, 0, 0 },
};

/// Unplayed bank-0 clip-1 reset preset beside Pierce's animation requests.
///
/// Its receiver is inferred from the neighboring Pierce requests. That actor
/// ignores the retained world-collision word; no dispatch proves this preset's use.
static AnimationPlayRequest _gActor443500UnusedPierceClip1ResetRequest = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 1 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip1Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 2 preset for the tour opening, resetting its pose immediately.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip2Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 3 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip3Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 4 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip4Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 5 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip5Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 6 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip6Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 7 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip7Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 8 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip8Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 9 preset, blending for 20 frames in the tour scenes.
static AnimationPlayRequest _gActor443500PierceClip9Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 10 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip10Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 11 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip11Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 12 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip12Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 13 preset, blending for 10 frames in the tour scenes.
static AnimationPlayRequest _gActor443500PierceClip13Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 13,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 14 preset for the console follow-up, requesting an 8-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip14Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 14,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 15 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip15Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 15,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 16 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip16Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 16,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 17 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip17Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 17,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 18 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip18Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 18,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 19 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip19Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 19,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 20 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip20Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 20,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 21 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip21Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 21,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 22 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip22Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 22,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 23 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip23Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 23,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 24 preset for the map follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip24Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 24,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 25 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip25Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 25,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 26 preset for the map follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip26Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 26,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 27 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip27Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 27,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Ten-frame transition back to Pierce's fallback clip after the map tour.
///
/// The same preset ends the skipped scene. Pierce ignores its collision word.
static AnimationPlayRequest _gActor443500PierceDefaultClipBlend10Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = ACTOR_443500_DEFAULT_ANIMATION,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 29 preset for the power-plant-progress dialogue, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip29Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 29,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 30 preset for the power-plant-progress dialogue, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip30Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 30,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 31 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip31Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 31,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 32 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip32Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 32,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 33 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip33Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 33,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 34 preset for the console follow-up, requesting a 2-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip34Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 34,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 2,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 35 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip35Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 35,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's console placement for the opening tour and its skip path.
///
/// XYZ is in room-coordinate units; yaw is five eighths of a 4096-unit turn.
/// The placement message consumes all three position and Euler components.
static ActorTransform _gActor443500PierceConsolePlacement = {
    .pos = { 16000, -1000, 9000, 0 },
    .rot = { 0, 5 * (ACTOR_TRANSFORM_ANGLE_TURN / 8), 0, 0 },
};

/// Pierce's placement at the start of the console follow-up.
///
/// XYZ uses room-coordinate units; all Euler angles are zero (4096 per turn).
/// The placement message consumes XYZ and all angles synchronously.
static ActorTransform _gActor443500PierceConsoleFollowupEntryPlacement = {
    .pos = { 16200, -1000, 6200, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Pierce's map-terminal placement for follow-up and skip scenes.
///
/// XYZ is in room-coordinate units with zero Euler orientation. This is also
/// his initial room placement; the message consumes the complete XYZ and angles.
static ActorTransform _gActor443500PierceMapTerminalPlacement = {
    .pos = { 16000, -1000, 5910, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Terminal-tour CAP slots, camera views and saved music-event selector.
enum {
    ACTOR_443500_TOUR_OPENING_CAP_COMMAND     = 4,
    ACTOR_443500_CONSOLE_FOLLOWUP_CAP_COMMAND = 5,
    ACTOR_443500_MAP_FOLLOWUP_CAP_COMMAND     = 6,
    ACTOR_443500_CONSOLE_SKIP_CAP_VARIANT     = 1,
    ACTOR_443500_TOUR_OPENING_VIEW            = 15,
    ACTOR_443500_CONSOLE_FOLLOWUP_VIEW        = 13,
    ACTOR_443500_MAP_FOLLOWUP_VIEW            = 14,
    ACTOR_443500_TOUR_FINISHED_VIEW           = 5,
    ACTOR_443500_TOUR_SCENE_EVENT             = 15,
    ACTOR_443500_CAP_SELECT_DEFAULT           = 0,
    ACTOR_443500_CAP_SELECT_EVENT             = 1,
};

/// Room-bank scripts stopped or started by the terminal-tour scenes.
enum {
    ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1),
    ACTOR_443500_TOUR_OPENING_SOUND      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 6),
    ACTOR_443500_TOUR_SOUND_FADE_TICKS   = 8,
};

EvsCommand gActor443500TourOpeningScript[74] = {
    // Fade out and install the opening player clips before staging both actors.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TourOpeningPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_TOUR_OPENING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_443500_TOUR_OPENING_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerTourOpeningEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip2Request } }, { .value = 0 } },
    // CAP cues and frame delays coordinate the console introduction.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_443500_TOUR_OPENING_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip7Request } }, { .value = 0 } },
    // Move Aya to the console, then restore ordinary room presentation.
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor443500PlayerConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500TourOpeningSkipScript[16] = {
    // Settle the console placement and release the skipped scene under the fade.
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500ConsoleFollowupScript[137] = {
    // Install follow-up clips and stage the scene after the console session.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500StartFadeFromBlack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor443500SetSceneEvent }, { .value = ACTOR_443500_TOUR_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerTerminalFollowupEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceConsoleFollowupEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_CONSOLE_FOLLOWUP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP command 5 drives the actor exchanges and transition to the map.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_443500_CONSOLE_FOLLOWUP_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip66Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip67Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip33Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip33Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip69Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 189 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip70Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip31Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip32Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip35Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip27Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip71Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    // Give Aya the timed map display, then reinstall the scene clips.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500OpenTimedMapTerminal }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip21Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip22Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip25Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip23Request } }, { .value = 0 } },
    // Finish at the map terminal and restore player control and presentation.
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500ConsoleFollowupSkipScript[18] = {
    // Settle the map-terminal placements without replaying the console scene.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor443500SetSceneEvent }, { .value = ACTOR_443500_TOUR_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Request the shortened CAP variant after releasing scene playback.
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor443500StartCapCommand5 }, { .value = ACTOR_443500_CONSOLE_SKIP_CAP_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500MapFollowupScript[73] = {
    // Stage the final map-terminal scene with the follow-up clip bank.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500StartFadeFromBlack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_MAP_FOLLOWUP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP command 6 drives the final actor exchange.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_443500_MAP_FOLLOWUP_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip24Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip25Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip23Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip26Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip65Request }, { .value = 0 } },
    // Restore normal playback and apply the Shelter progress updates.
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceDefaultClipBlend10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500ApplyShelterAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_TOUR_FINISHED_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500MapFollowupSkipScript[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceDefaultClipBlend10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = ACTOR_443500_TOUR_FINISHED_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500ApplyShelterAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500PowerPlantProgressDialogueScript[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip29Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCapAfterPowerPlantClear }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500RunPowerPlantProgressCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip30Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/pierce/body.inc"

#include "assets/characters/pierce/glasses.inc"

#include "assets/rooms/shelter/r47/pierce_clips_2.inc"

/// Pierce's bank-0 clip table: NULL at 0, playable clips at 1..35.
///
/// Slots 1..19 borrow these descriptors and packed streams while the body
/// plays. Clip `ACTOR_443500_DEFAULT_ANIMATION` is the non-event fallback.
#include "assets/rooms/shelter/r47/pierce_bank.inc"

/// Indexed playback banks for Pierce; only bank 0 is valid.
///
/// The animation context retains the selected set table. Keep this overlay's
/// descriptors and packed streams loaded until Pierce's playback ends.
static AnimationSet** _gActor443500PierceAnimationBanks[1] = {
    _gActor443500PierceAnimationSets,
};

/// Pierce's fallback playback preset, used at spawn and outside scene events.
///
/// Selects bank 0 and clip 28, blending for four frames from an active pose or
/// resetting if no pose is playing. The handler ignores the collision word
/// and restarts the loop-sound counter even when the same clip is requested.
static AnimationPlayRequest _gActor443500PierceDefaultAnimationRequest = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = ACTOR_443500_DEFAULT_ANIMATION,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

static TmdSource _gPierceGlasses;

TaskDesc gActor443500PierceTaskDescs[2] = {
    { .header = { .fields = { TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER, ACTOR_443500_TASK_PRIORITY } }, .callback = _actor443500PierceTask, .data = { .model = &_gPierceBody } },
    { .header = { .fields = { TASK_BODY_TMD, ACTOR_443500_TASK_PRIORITY } }, .callback = _actor443500ChildModelTask, .data = { .model = &_gPierceGlasses } },
};

/// Pierce synchronous playback, placement and model-draw messages, followed by the end marker.
///
/// Playback borrows AnimationPlayRequest, placement borrows ActorTransform, and
/// draw policy takes an integer mode. The task borrows this table while the actor
/// overlay is loaded; all three callbacks use the four-word message ABI.
static TaskMessageEntry _gActor443500PierceMessages[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor443500PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor443500SetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Unit normals of Pierce's room collision template, scaled by 4096.
static SVECTOR _gActor443500PierceCollisionNormals[2] = {
#include "assets/actor_443500_collision_269B8_normals.inc"
};

/// Pierce's collision-template vertices in signed room-coordinate units.
static SVECTOR _gActor443500PierceCollisionVerts[6] = {
#include "assets/actor_443500_collision_269B8_verts.inc"
};

/// Two obstacle quads indexing Pierce's six template vertices and two normals.
static WorldCollisionGridFace _gActor443500PierceCollisionFaces[2] = {
#include "assets/actor_443500_collision_269B8_faces.inc"
};

/// Face-index list for Pierce's one grid cell; its first -1 ends the list.
static s16 _gActor443500PierceCollisionCells[4] = {
#include "assets/actor_443500_collision_269B8_cells.inc"
};

/// Addresses a face list in Pierce's packed collision-template halfword pool.
///
/// The generated table supplies a halfword offset, not a grid-cell index. It
/// evaluates the offset once and returns an s16* into the captured private pool;
/// the selected list must reach its -1 terminator within that pool. This asset's
/// sole entry uses offset 0 for faces 0 and 1. The collisionSource manifest entry
/// selects this binding for the include producer; it is undefined after the table.
#define ACTOR_443500_PIERCE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gActor443500PierceCollisionCells[(halfwordOffset)])
/// Cell table of Pierce's collision template, borrowing its face-index list.
static s16* _gActor443500PierceCollisionTable[1] = {
#include "assets/actor_443500_collision_269B8_table.inc"
};
#undef ACTOR_443500_PIERCE_CELL_FACE_LIST_AT_OFFSET

/// Pierce's obstacle mesh template for the leading entries of room 47's grid.
///
/// Installation copies two normals, two faces and six vertices into the live
/// room grid, optionally raising Y by 2000 coordinate units. This template is
/// never installed as the active grid: its view coordinate stays NULL and its
/// one-cell indexing is retained only as part of the embedded asset.
static WorldCollisionGrid _gActor443500PierceCollision = {
    .viewCoord   = NULL,
    .normals     = _gActor443500PierceCollisionNormals,
    .vertices    = _gActor443500PierceCollisionVerts,
    .faces       = _gActor443500PierceCollisionFaces,
    .cellFaceIds = _gActor443500PierceCollisionTable,
    .xBias       = -16120,
    .zBias       = -5156,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gActor443500PierceCollisionFaces),
};

/// Selects the event CAP resource, or restores ordinary room CAP selection.
///
/// Nonzero `useEventCap` clears the current selection before selecting loaded
/// data resource 1 and its texture origin (576, 256) in VRAM pixels. Zero resets
/// CAP to its default resource. Playback must be stopped and the selected CAP
/// data and glyph texture must stay loaded while in use; this performs no I/O.
static void _actor443500SelectEventCap(s32 useEventCap)
{
    if (useEventCap != 0) {
        gCapFile = NULL;
        capSelectLoadedFile(ACTOR_443500_EVENT_CAP_FILE_ORDINAL);
        capSetTexturePage(ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_X, ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_Y);
        return;
    }
    capReset();
}

/// Selects or resets event CAP only after Neo Ark power plant 2 has been cleared.
///
/// Otherwise leaves CAP untouched. Selection and resource lifetime requirements
/// follow `_actor443500SelectEventCap`; zero resets and nonzero selects event CAP.
static void _actor443500SelectEventCapAfterPowerPlantClear(s32 useEventCap)
{
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) > 0) {
        if (useEventCap != 0) {
            gCapFile = NULL;
            capSelectLoadedFile(ACTOR_443500_EVENT_CAP_FILE_ORDINAL);
            capSetTexturePage(ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_X, ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_Y);
            return;
        }
        capReset();
    }
}

/// Runs CAP command 6 before power plant 2 is cleared, or command 9 afterwards.
///
/// Starts in the current display. The active CAP resource must contain the
/// selected slot and remain loaded through playback; busy CAP does not restart.
static void _actor443500RunPowerPlantProgressCap(void)
{
    enum {
        ACTOR_443500_CAP_BEFORE_POWER_PLANT_CLEAR = 6,
        ACTOR_443500_CAP_AFTER_POWER_PLANT_CLEAR  = 9,
    };
    capRunCommand(gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0 ? ACTOR_443500_CAP_BEFORE_POWER_PLANT_CLEAR : ACTOR_443500_CAP_AFTER_POWER_PLANT_CLEAR,
                  CAP_PLAYBACK_IN_PLACE);
}

/// Starts the room's timed map-terminal display and holds the hidden player.
///
/// Shelter room 47 and the player task must be live. The room task owns the
/// terminal's lifetime and later restores player control and presentation.
/// The player is hidden and held even if the terminal task cannot be allocated.
static void _actor443500OpenTimedMapTerminal(void)
{
    taskSpawnFromTable(&gShelterR47MapTerminalTaskDesc, 0, SHELTER_R47_MAP_MODE_TIMED, 0);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
}

/// Starts this package's 30-tick fade from black, if a task can be allocated.
///
/// Keep the actor overlay loaded until the spawned callback finishes.
static void _actor443500StartFadeFromBlack(void)
{
    taskSpawnFromTable(&_gActor443500FadeFromBlackTaskDesc, 0, 0, 0);
}

/// Draws a subtractive black overlay that fades away over 30 task ticks.
///
/// Requires a zero-initialized bodyless task and the current GPU packet arena.
/// `killCountdown` counts elapsed ticks here: the last draw uses tick 29, then
/// the callback kills its task. The signed counter retains 16-bit wrap behavior.
static void _actor443500FadeFromBlackTask(Task* task)
{
    enum { ACTOR_443500_FADE_TICKS    = 30,
           ACTOR_443500_FULL_DARKNESS = 255 };
    s16 nextTick;
    s32 darkness;

    darkness = (((ACTOR_443500_FADE_TICKS - task->killCountdown) * ACTOR_443500_FULL_DARKNESS) /
                ACTOR_443500_FADE_TICKS) &
               0xFF;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextTick            = (u16)task->killCountdown + 1;
    task->killCountdown = nextTick;
    if (nextTick >= ACTOR_443500_FADE_TICKS) {
        taskKill(task);
    }
}

/// Starts CAP command slot 5 with the supplied variant and a display transition.
///
/// `variantKey` is a signed halfword; 0..255 can match CAP record keys. Requires
/// a live relocated command table containing slot 5 and its playback resources.
/// Busy CAP does nothing; the start result is discarded.
static void _actor443500StartCapCommand5(s16 variantKey)
{
    enum { ACTOR_443500_CAP_VARIANT_COMMAND = 5 };
    capStartSequenceSlot(ACTOR_443500_CAP_VARIANT_COMMAND, CAP_PLAYBACK_DISPLAY_TRANSITION, variantKey);
}

/// Applies the Shelter saved-area layouts and map marks after the Pierce event.
///
/// Requires the loaded room-47 record list, live save and saved-area tables.
/// Accepted records also discard saved enemy poses; save-mode policies select
/// the appropriate layouts. Used by both normal and skip event scripts.
static void _actor443500ApplyShelterAreaUpdates(void)
{
    areaApplySavedUpdates(gShelterR47SavedAreaUpdates);
}

/// Stores the saved scene-event byte used to select stage music.
///
/// The event scripts pass 15. This only updates the live save; music selection
/// reads the event later. The signed-byte callback preserves its argument width.
static void _actor443500SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

/// Gives Pierce's held model its owner's placement texture and palette offsets.
///
/// Copies texture-page and CLUT-row offsets, then rebuilds each half of an
/// existing primitive buffer. A NULL buffer updates only the stored offsets.
/// Requires a live placement, model source and sufficient writable buffer space
/// for both halves; the two builds restore the original half selector. Borrows
/// both pointers through this call and neither allocates nor frees storage.
static inline void _actor443500ApplyPiercePlacementTextures(TmdObject* model, const AreaPlacement* placement)
{
    model->texturePageOffset = placement->texturePageOffset;
    model->clutRowOffset     = placement->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

/// Initializes Pierce's hidden body, held model and animation messages.
///
/// Requires the twenty-part body and live owning enemy in spawnArg2.pointer.
/// Allocates primary-heap work owned by the task; allocation failure tears down
/// the enemy. An optional held-model child at part 4 inherits texture offsets
/// from the enemy's area placement. Work-owned lighting survives until teardown;
/// initial animation, messages and exit handling are installed before state 1.
static void _actor443500InitPierce(Task* task)
{
    enum {
        ACTOR_443500_PIERCE_CHILD_DESCRIPTOR  = 1,
        ACTOR_443500_PIERCE_CHILD_PART        = 4,
        ACTOR_443500_PIERCE_NO_BUFFER_RELEASE = -1,
    };

    _Actor443500PierceCarradineWork* work;
    GameLocationKey                  locationKey;
    const GameLocationKey*           sessionLocation;
    u8                               viewIndex;
    AreaVariant*                     areaVariant;
    AreaPlacement*                   placement;
    TmdObject*                       heldModel;
    Task*                            childTask;
    s32                              placementIndex;
    u32                              placementKey;
    Enemy*                           enemy;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work            = work;
    work->model.animId    = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank      = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown   = ACTOR_443500_PIERCE_NO_BUFFER_RELEASE;
    work->savedModelFlags = task->extra.tmd->flags;
    childTask             = taskSpawnFromTable(gActor443500PierceTaskDescs, ACTOR_443500_PIERCE_CHILD_DESCRIPTOR, ACTOR_443500_PIERCE_CHILD_PART, task);
    if (childTask != NULL) {
        sessionLocation       = &gGameSession->location;
        enemy                 = task->spawnArg2.pointer;
        placementKey          = enemy->placeKey;
        heldModel             = childTask->extra.tmd;
        locationKey.stage     = sessionLocation->stage;
        locationKey.area      = sessionLocation->area;
        locationKey.roomState = sessionLocation->roomState;
        viewIndex             = sessionLocation->view;
        placementIndex        = placementKey >> ENEMY_PLACE_INDEX_SHIFT;
        locationKey.view      = viewIndex;
        areaSyncLocationVariant(&locationKey);
        areaVariant = areaGetVariant(&locationKey);
        // The held model inherits its owner's area-placement texture offsets.
        placement = AREA_PLACEMENT_AT(areaVariant->placements, placementIndex);
        _actor443500ApplyPiercePlacementTextures(heldModel, placement);
    }
    _actor443500SetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
    _actor443500PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor443500PierceDefaultAnimationRequest, 0);
    _actor443500BindModelLighting(task);
    task->msgTable     = _gActor443500PierceMessages;
    task->exitCallback = _actor443500ExitPierce;
    task->state++;
}

/// Advances Pierce's view-dependent presentation, animation and buffer lifetime.
///
/// Requires the initialized twenty-part TMD task, its allocated work, live clip
/// data and shelter room 47's writable collision grid. Idle ready views 0..3
/// save and hide the model; views 4..5 restore its saved flags and, when flag
/// `GAME_FLAG_083` is nonzero, install its collision and request visible drawing.
/// Slots 1..19 tick after a play request. Outside an event, a slot-1 boundary
/// restarts the default request. Clip 28 drives a sound at loop tick 15 and
/// resets that counter on a slot-1 control jump. Visible models receive a
/// ground shadow and updated lighting; pending buffer release continues even
/// while the model is hidden. The room overlay and sound bank must stay loaded.
static void _actor443500TickPierce(Task* task)
{
    enum {
        ACTOR_443500_DEFAULT_CLIP_SOUND      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1),
        ACTOR_443500_LOOP_SOUND_FADE_UPDATES = 30,
        ACTOR_443500_SHADOW_HALF_SIZE        = 768,
    };
    _Actor443500PierceCarradineWork* work;
    TmdObject*                       model;
    VECTOR3                          groundPoint;
    s32                              slotIndex;
    u8                               view;

    model = task->extra.tmd;
    work  = task->work;
    // Camera visibility changes are suspended while an event holds the scene.
    if (gGameSession->viewReady != 0 && gGameSession->eventState == 0 &&
        gGameSession->cutsceneHold == 0) {
        view = gGameSession->location.view;
        if (view < 4) {
            work->savedModelFlags = model->flags;
            model->flags          = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else if (view < 6) {
            if (gameFlagGetNibble(GAME_FLAG_083) > 0) {
                _actor443500InstallPierceCollision(0);
                _actor443500SetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            model->flags = work->savedModelFlags;
        }
    }
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        if (gGameSession->eventState == 0 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
            _actor443500PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor443500PierceDefaultAnimationRequest, 0);
        }
    }
    // The cue follows animation loop timing; camera views select its audible mix.
/// Queues the default clip's start or mix request for the current camera view.
///
/// `queueSound` must be the start or mix function identifier; it is called at
/// most once, in views 4 or 5. View 3 instead fades the sound script. Captures
/// `gGameSession` and this block's sound ID/fade constants, requires the live
/// shelter room 47 sound bank, and is undefined after the two uses. Expands to
/// a standalone switch statement; no caller expression is evaluated twice.
#define ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND(queueSound)                                                   \
    switch (gGameSession->location.view) {                                                                  \
        case 5:                                                                                             \
            (queueSound)(ACTOR_443500_DEFAULT_CLIP_SOUND, 9, 0);                                            \
            break;                                                                                          \
        case 4:                                                                                             \
            (queueSound)(ACTOR_443500_DEFAULT_CLIP_SOUND, -10, 64);                                         \
            break;                                                                                          \
        case 3:                                                                                             \
            sndEvtRequestScriptStop(ACTOR_443500_DEFAULT_CLIP_SOUND, ACTOR_443500_LOOP_SOUND_FADE_UPDATES); \
            break;                                                                                          \
    }
    if (work->model.animId == ACTOR_443500_DEFAULT_ANIMATION) {
        work->loopTicks++;
        if (work->loopTicks == ACTOR_443500_PIERCE_CARRADINE_LOOP_SOUND_TICK) {
            ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND(sndEvtRequestScriptStart);
        } else if (gGameSession->viewReady != 0) {
            ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND(sndEvtRequestScriptMix);
        }
#undef ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            work->loopTicks = 0;
        }
    }
    // Keep shadow projection before the current coordinate composition.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_443500_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        if (gGameSession->viewReady != 0) {
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, 3);
        }
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Dispatches the attached child model's setup, flag-mirroring or kill state.
///
/// Requires a live TMD child with `state` in 0..2. Setup borrows its parent task
/// from `spawnArg2.pointer` and the parent part index from `spawnArg1.value`;
/// the parent model and its selected part must outlive the child's drawing.
static void _actor443500ChildModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor443500ChildModelStates;
    states.funcs[task->state](task);
}

#include "../../shared/actor/model_placement_attach.inc.c"

#include "../../shared/actor/model_placement_mirror_parent.inc.c"

/// Dispatches Pierce's setup, frame update or exit while scene actors are running.
///
/// Requires a live TMD task with `state` in 0..2 and this overlay loaded.
/// Setup and exit require the live enemy record in `spawnArg2.pointer`.
/// Paused actor control skips every state, including setup and exit.
static void _actor443500PierceTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor443500PierceStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Releases Pierce's task through the enemy-task teardown path.
///
/// Serves both exit state 2 and the installed exit callback; task and owned
/// work must still be live on entry; `spawnArg2.pointer` must be its live enemy
/// record. Neither may be used afterwards.
static void _actor443500ExitPierce(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the work block's lighting matrices to Pierce's model.
///
/// Requires allocated work and a live TMD object. Its borrowed matrix pointers
/// remain valid only for that work block's lifetime.
static void _actor443500BindModelLighting(Task* task)
{
    TmdObject*                       model;
    _Actor443500PierceCarradineWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Applies a clip request to the actor rig and restarts its loop cue.
///
/// Borrows live work, model coordinates and loaded bank data through playback;
/// the read-only request is consumed during this call and must not overlap work.
/// Bank 0 and clips 1..35 are valid. Blend duration counts whole frames.
static inline void _actor443500ApplyAnimationRequest(_Actor443500PierceCarradineWork* work, TmdObject* model,
                                                     const AnimationPlayRequest* request)
{
    enum { ACTOR_443500_FIRST_DRIVEN_SLOT = 1 };
    s32 slotIndex;

    if (request->source.index != work->model.bank) {
        work->model.bank = request->source.index;
        animationInitContext(&work->rig.anim, _gActor443500PierceAnimationBanks[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    work->model.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (slotIndex = ACTOR_443500_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
        }
    } else {
        for (slotIndex = ACTOR_443500_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
        }
    }
    // Seed the new pose before the next frame and restart its synchronized cue.
    for (slotIndex = ACTOR_443500_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->model.ticking = 1;
    work->loopTicks     = 0;
}

/// Starts the requested clip and restarts Pierce's animation-loop sound timing.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` on initialized twenty-part work.
/// `request` is borrowed read-only through dispatch and must not overlap the rig.
/// Bank index must be 0 and clip index 1..35; their data, model coordinates and
/// work-owned slots/poses stay borrowed through playback. Initialize bank to
/// `ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first request. Slots 1..19 blend when
/// already ticking and requested, using `blendFrames` in whole frames (normally
/// 0..2047), or reset otherwise, then tick once. Even a repeated clip restarts.
/// Ignores message ID, collision choice and fourth argument. Returns 0.
static s32 _actor443500PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor443500PierceCarradineWork* work;
    TmdObject*                       model;

    work  = task->work;
    model = task->extra.tmd;
    _actor443500ApplyAnimationRequest(work, model, request);
    return 0;
}

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Sets Pierce's model visibility and automatic-buffer policy.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` on a live TMD task with allocated work.
/// Modes: 0 hide with automatic buffering; 1 show and request a buffer; 2 hide,
/// disable automatic buffering and start a two-tick countdown; 3 show with
/// automatic buffering disabled. The tick observing countdown zero frees the
/// buffer. Later show requests do not cancel a pending release. Mode 1 returns
/// 0 even if allocation fails. Known modes return 0, others 1; every request
/// saves the resulting flags for camera-view restoration. Ignores message ID
/// and fourth argument. Unrelated flags and existing buffers remain intact.
static s32 _actor443500SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum { ACTOR_443500_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    TmdObject*                       model;
    s32                              result;
    _Actor443500PierceCarradineWork* work;

    model  = task->extra.tmd;
    work   = task->work;
    result = 0;
    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_443500_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    work->savedModelFlags = model->flags;
    return result;
}

/// Installs Pierce's two collision faces into shelter room 47's live grid.
///
/// Both overlays must be loaded and the destination arrays writable. Replaces
/// the first two normals/faces and six vertices, retaining the room grid's
/// other geometry, cell indexing and SDK vector pad words. Nonzero `shiftY`
/// adds 2000 world-coordinate units to these vertices' Y after installation;
/// zero uses the template position. Vertex additions narrow to signed 16 bits.
/// The room grid must remain loaded while collision queries use the result.
static void _actor443500InstallPierceCollision(s32 shiftY)
{
    enum { ACTOR_443500_COLLISION_Y_DISPLACEMENT = 2000 };
    WorldCollisionGrid*       roomGrid;
    const WorldCollisionGrid* actorGrid;
    SVECTOR                   displacement;
    s32                       elementIndex;

    roomGrid  = &gShelterR47CollisionGrid;
    actorGrid = &_gActor443500PierceCollision;

    // The room reserves its leading geometry entries for this actor's faces.
    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500PierceCollisionFaces); elementIndex++) {
        roomGrid->normals[elementIndex].vx = actorGrid->normals[elementIndex].vx;
        roomGrid->normals[elementIndex].vy = actorGrid->normals[elementIndex].vy;
        roomGrid->normals[elementIndex].vz = actorGrid->normals[elementIndex].vz;
        roomGrid->faces[elementIndex]      = actorGrid->faces[elementIndex];
    }

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500PierceCollisionVerts); elementIndex++) {
        roomGrid->vertices[elementIndex].vx = actorGrid->vertices[elementIndex].vx;
        roomGrid->vertices[elementIndex].vy = actorGrid->vertices[elementIndex].vy;
        roomGrid->vertices[elementIndex].vz = actorGrid->vertices[elementIndex].vz;
    }

    if (shiftY == 0) {
        displacement.vx = 0;
        displacement.vy = 0;
    } else {
        displacement.vx = 0;
        displacement.vy = ACTOR_443500_COLLISION_Y_DISPLACEMENT;
    }
    displacement.vz = 0;

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500PierceCollisionVerts); elementIndex++) {
        roomGrid->vertices[elementIndex].vx += displacement.vx;
        roomGrid->vertices[elementIndex].vy += displacement.vy;
        roomGrid->vertices[elementIndex].vz += displacement.vz;
    }
}
