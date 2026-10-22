#include "actors/actor_160900.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
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
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/screen/screen_wave.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of one light quad, a gradient quad the cutscene blends
/// additively over the scene.
///
/// The spawner allocates the block zeroed for the quad's task, keeps it at
/// `Task::work` and fills in the corners; the task's teardown frees it. The
/// quad's task projects the corners through its own coordinate each frame and
/// shades each corner black or white, in one of six patterns its first spawn
/// argument selects (0 corner 3 white, 1 corner 2, 2 corners 1 and 3,
/// 3 corners 0 and 2, 4 corners 2 and 3, 5 all four).
///
/// Every quad the package spawns is an upright rectangle in the Y/Z plane:
/// `vx` is zero in every corner, corners 0 and 1 share the upper edge and
/// corners 2 and 3 the lower, with the greater Z first on each. No corner's
/// `pad` is written or read.
typedef struct {
    SVECTOR corners[4]; // The quad's corners as offsets from its task's coordinate, in the order the primitive takes its vertices
} _Actor160900LightQuadWork;
STATIC_ASSERT_SIZEOF(_Actor160900LightQuadWork, 0x20);

/// Cues of `_Actor160900CutsceneWork::playerCue`. The clips are those of
/// `_gActor160900PlayerAnimationSets`.
enum {
    ACTOR_160900_PLAYER_CUE_TAKE_PLACE   = 1,  // places Aya for the scene and restarts her in clip 10
    ACTOR_160900_PLAYER_CUE_CLIP_1       = 2,  // restarts her in clip 1
    ACTOR_160900_PLAYER_CUE_WALK_TO_MARK = 3,  // once: clip 1 of her own weapon's bank, a second placement, and a walk to a fixed point; stays posted afterwards
    ACTOR_160900_PLAYER_CUE_WAVE_START   = 4,  // starts the screen wave on an eight-frame rise
    ACTOR_160900_PLAYER_CUE_WAVE_END     = 5,  // ends the screen wave's task
    ACTOR_160900_PLAYER_CUE_CLIP_2       = 6,  // restarts her in clip 2, which chains on through clips 3, 4 and 5
    ACTOR_160900_PLAYER_CUE_CLIP_6       = 7,  // blends her into clip 6
    ACTOR_160900_PLAYER_CUE_CLIP_7       = 8,  // blends her into clip 7, which chains back to clip 0
    ACTOR_160900_PLAYER_CUE_CLIP_8       = 9,  // blends her into clip 8, which chains back to clip 0
    ACTOR_160900_PLAYER_CUE_CLIP_9       = 10, // blends her into clip 9, which chains back to clip 0
};

/// Cues of `_Actor160900CutsceneWork::kyleCue`.
enum {
    ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE = 1, // hides Kyle's body and places it
    ACTOR_160900_KYLE_CUE_APPEAR         = 2, // shows his hands, his gun if there is one, and his body, and spawns a ground decal at an offset from him
    ACTOR_160900_KYLE_CUE_CLIP_1         = 3, // blends his body into clip 1 of its own animation script
    ACTOR_160900_KYLE_CUE_GROUND_DECAL   = 4, // spawns a ground decal at a second offset from him
};

/// Work block of the task that runs the package's cutscene, allocated and
/// zeroed at its full size by that task's first state and kept at its
/// `Task::work`.
///
/// The scene plays Aya Brea and Kyle Madigan against one event script. The
/// block holds the cast - the player's own task and the model tasks this
/// package spawns for Kyle - and the cues through which the script tells Aya,
/// Kyle and the scene's effects what to do next. Kyle's body hangs below the
/// cutscene task in the teardown tree and his hands below his body, so they
/// die with the scene; the player's task is borrowed and outlives it.
///
/// The player keeps the animation player of an ordinary actor, so the scene
/// tracks which of its own clips Aya is in and chains the next one itself.
/// Kyle runs his animation script inside his own task.
typedef struct {
    ScreenWaveCtx    wave;           // Ramp of the screen-wave task the player cue starts and ends; the scene seeds the rise and strength and leaves the tint off
    Task*            lightQuads[10]; // Additive gradient quads the script puts up a set at a time, of which the largest fills six slots; NULL where a slot is empty, and killed and cleared together when the script takes the set down
    Task*            player;         // The player's task, playing Aya; borrowed, never killed here
    Task*            kyle;           // Kyle Madigan's body
    Task*            kyleGunHand;    // Kyle's hand model on the body part that would also carry the gun
    Task*            kyleFreeHand;   // Kyle's other hand model
    Task*            kyleGun;        // Slot for Kyle's handgun model, shown with his hands when set; nothing here spawns the gun, so it stays NULL
    byte             unknown_48[4];  // Never accessed; role unproven
    ActorCutsceneCue playerCue;      // Aya's cue (`ACTOR_160900_PLAYER_CUE_*`): her placement and clips, and the screen wave; only the walk to the mark reads `step`
    ActorCutsceneCue kyleCue;        // Kyle's cue (`ACTOR_160900_KYLE_CUE_*`): hiding, placing and showing him, his clip, and the ground decals; `step` is never read
    ActorCutsceneCue effectCue;      // Which of the five point lists the scene spawns its effect over, one frame in eight (0 none, 1-5 the list); stays posted until the script changes it, and `step` is never read
    u16              playerAnimId;   // Clip Aya is playing, an index into the package's player animation sets and into the script that names its successor
    s16              playerAnimHold; // Frames the current player clip has been held, for a script step that lasts a fixed time
} _Actor160900CutsceneWork;
STATIC_ASSERT_SIZEOF(_Actor160900CutsceneWork, 0x68);

/// Work block of one of Kyle Madigan's models: his body, and a hand or handgun
/// that rides on a part of the body.
///
/// Each of those tasks allocates the block zeroed at its full size and keeps
/// it at `Task::work`; the model object borrows `light` and `color` for as
/// long as the block lives. The body binds `rig` to its model and plays one
/// clip at a time over slots 1 to 19, following `animChain` from clip to clip,
/// and the cutscene's cue handler changes the clip from outside. An attachment
/// uses the two matrices and leaves the rest zero.
typedef struct {
    ActorAnimRig20      rig;       // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    MATRIX              light;     // Light-direction matrix lent to the model object
    MATRIX              color;     // Light-colour matrix lent to the model object
    ActorAnimChainLink* animChain; // The body's chain of clips: one link per clip, indexed by `animId`
    u16                 animId;    // Clip the slots were last seeded with, an index into the body's animation sets
    s16                 animHold;  // Frames `animId` has been held, for a link that lasts a fixed time
} _Actor160900KyleModelWork;
STATIC_ASSERT_SIZEOF(_Actor160900KyleModelWork, 0x4BC);

static s32 _gScreenWaveAmplitude;

static ScreenWaveCtx* _gScreenWaveContext;

/// Published task of the Aya/Kyle cutscene, borrowed by its script callbacks.
///
/// Set after work allocation, before Kyle and the scripts start. Valid only
/// while that task and its work live; teardown does not clear the pointer.
static Task* _gActor160900CutsceneTask;

ScreenWaveOscillator _gScreenWaveColumnWaves[13];

ScreenWaveOscillator _gScreenWaveRowWaves[30];

/// Clip transitions are measured in normal-rate frames; cue zero means idle.
enum {
    ACTOR_160900_ANIM_BLEND_FRAMES = 10,
    ACTOR_160900_CUE_NONE          = 0,
};

extern TmdSource gKyleBody;
extern TmdSource gKyleP229;
extern TmdSource gKyleHandLeft2;
extern TmdSource gKyleHandRight;
static void      _actor160900KyleAttachmentTask(Task* task);
static void      _actor160900KyleBodyTask(Task* task);
static void      _actor160900DrawLightQuadTask(Task* task);
static void      _actor160900SpawnOpeningLightPair(void);
static void      _actor160900SpawnKyleRevealLightQuads(void);
static void      _actor160900SpawnWalkLightPair(void);
static void      _actor160900CutsceneTask(Task* task);
static void      _actor160900FadeOutTask(Task* task);
static void      _actor160900FadeInTask(Task* task);
static void      _actor160900SpawnFadeIn(s32 intensityStep);
static void      _actor160900SpawnFadeOut(s32 intensityStep);
static void      _actor160900KillLightQuads(void);
static void      _actor160900PostPlayerCue(s16 cueId);
static void      _actor160900PostKyleCue(s16 cueId);
static void      _actor160900PostEffectCue(s16 cueId);
static void      _actor160900SkipCutscene(void);
static void      _actor160900StageSceneAudioStart(void);
static void      _actor160900EnqueueScenePlayback(void);
static void      _actor160900FinishScenePlayback(void);

static void _actor160900SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg);

/// Entries spawned from this package's task table.
enum {
    ACTOR_160900_TASK_FADE_IN        = 1,
    ACTOR_160900_TASK_FADE_OUT       = 2,
    ACTOR_160900_TASK_KYLE_BODY      = 3,
    ACTOR_160900_TASK_KYLE_GUN_HAND  = 5,
    ACTOR_160900_TASK_KYLE_FREE_HAND = 6,
    ACTOR_160900_TASK_LIGHT_QUAD     = 7,
};

/// Attachment selectors and their body coordinates; both hands reuse Kyle's texture placement.
enum {
    ACTOR_160900_KYLE_FREE_HAND      = 0,
    ACTOR_160900_KYLE_GUN_HAND       = 1,
    ACTOR_160900_KYLE_GUN            = 2,
    ACTOR_160900_KYLE_FREE_HAND_PART = 12,
    ACTOR_160900_KYLE_GUN_HAND_PART  = 8,
    ACTOR_160900_KYLE_PLACEMENT_ID   = 0x65,
};

/// White-corner patterns passed to the light-quad task; other corners are black.
enum {
    ACTOR_160900_LIGHT_PATTERN_CORNER_3    = 0,
    ACTOR_160900_LIGHT_PATTERN_CORNER_2    = 1,
    ACTOR_160900_LIGHT_PATTERN_CORNERS_1_3 = 2,
    ACTOR_160900_LIGHT_PATTERN_CORNERS_0_2 = 3,
    ACTOR_160900_LIGHT_PATTERN_CORNERS_2_3 = 4,
    ACTOR_160900_LIGHT_PATTERN_ALL_CORNERS = 5,
};

/// Stored chain/list terminators and cue ids used by the cutscene's data.
enum {
    ACTOR_160900_CHAIN_NO_SUCCESSOR = -1,
    ACTOR_160900_SPRITE_POINTS_END  = -1,
    ACTOR_160900_SCRIPT_POINTS_1    = 1,
    ACTOR_160900_SCRIPT_POINTS_2    = 2,
    ACTOR_160900_SCRIPT_POINTS_3    = 3,
    ACTOR_160900_SCRIPT_POINTS_4    = 4,
    ACTOR_160900_SCRIPT_POINTS_5    = 5,
    ACTOR_160900_SKIP_VIEW          = 43,
    ACTOR_160900_CAP_COMMAND_SLOT   = 1,
};

#include "assets/characters/kyle/body.inc"

#include "assets/characters/kyle/p229.inc"

#include "assets/characters/kyle/hand_left_2.inc"

#include "assets/characters/kyle/hand_right.inc"

#include "assets/rooms/shelter/b1_pod_service_gantry/kyle_clips.inc"

/// Screen-wave spawn recipe at index 0, followed by a descriptor terminator.
///
/// Bodyless, priority 192. Spawn argument 2 borrows the cutscene's
/// `ScreenWaveCtx`; it must outlive the wave task. Argument 1 is unused.
static TaskDesc _gActor160900ScreenWaveTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

/// Aya's eleven cutscene animation sets, indexed by player clip id 0..10.
///
/// Playback borrows these loaded sets and their twenty-part track data.
/// `_gActor160900PlayerAnimationChain` uses the same indices; the equipped
/// weapon's animation bank replaces them when Aya walks away.
#include "assets/rooms/shelter/b1_pod_service_gantry/player_bank.inc"

/// Kyle's body clips 0 and 1, borrowed by its twenty-part animation rig.
///
/// Clip zero starts on initialization; the reveal sequence requests clip one.
/// The table and both sets must remain loaded through body playback.
#include "assets/rooms/shelter/b1_pod_service_gantry/kyle_bank.inc"

/// Successors of Aya's clips 0..10, each selected after playback finishes.
///
/// Clips 2, 3 and 4 advance to 3, 4 and 5; clips 7..9 return to 0.
/// Other clips have no successor. All holds are zero, so no timed hold is used.
static ActorAnimChainLink _gActor160900PlayerAnimationChain[11] = {
    { 0, ACTOR_160900_CHAIN_NO_SUCCESSOR },
    { 0, ACTOR_160900_CHAIN_NO_SUCCESSOR },
    { 0, 3 },
    { 0, 4 },
    { 0, 5 },
    { 0, ACTOR_160900_CHAIN_NO_SUCCESSOR },
    { 0, ACTOR_160900_CHAIN_NO_SUCCESSOR },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, ACTOR_160900_CHAIN_NO_SUCCESSOR },
};

/// Kyle's two terminal clip links, each waiting for playback to settle.
///
/// Neither clip starts another; the cutscene requests clip changes explicitly.
static ActorAnimChainLink _gActor160900KyleAnimationChain[2] = {
    { 0, ACTOR_160900_CHAIN_NO_SUCCESSOR },
    { 0, ACTOR_160900_CHAIN_NO_SUCCESSOR },
};

/// Placement and visibility messages of Kyle's body, hand and handgun tasks.
///
/// Only `ACTOR_MESSAGE_SET_MODEL_DRAW` and `ACTOR_MESSAGE_PLACE` may be sent:
/// this two-entry table has no end marker. Draw control takes a mode word;
/// placement borrows a complete `ActorTransform` during dispatch. Both
/// callbacks return void, so senders must ignore the dispatch result.
static TaskMessageEntry _gActor160900KyleModelMessages[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor160900SetModelDraw },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

/// Aya's initial cutscene position and quarter-turn yaw.
///
/// Position is in the model root's parent frame, in whole game-coordinate
/// units; yaw uses 4096 units per turn. Borrowed only during the place message.
static ActorTransform _gActor160900PlayerInitialPlacement = {
    { 4500, 0, 3200, 0 },
    { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Aya's walk-cue position and three-quarter-turn yaw.
///
/// Position is in the model root's parent frame, in whole game-coordinate
/// units; yaw uses 4096 units per turn. Borrowed only during the place message.
static ActorTransform _gActor160900PlayerWalkPlacement = {
    { 4500, 0, 3200, 0 },
    { 0, 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Kyle's hidden starting position and negative quarter-turn yaw.
///
/// Position is in the model root's parent frame, in whole game-coordinate
/// units; yaw uses 4096 units per turn. Borrowed only during the place message.
static ActorTransform _gActor160900KylePlacement = {
    { 10000, 0, 3200, 0 },
    { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Drifting-sprite origins for effect cue 1: 16 points and a `pad == -1` terminator.
///
/// XYZ are signed game coordinates in the input frame of `GsWSMATRIX`.
/// The spawner jitters X by up to 700 units and copies each position into an effect task.
static SVECTOR _gActor160900DriftingSpritePoints1[17] = {
    { 6000, 1000, 2000, 0 },
    { 6000, 500, 2000, 0 },
    { 6000, 0, 2000, 0 },
    { 6000, 500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, 1000, 3000, 0 },
    { 6000, 500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, 500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, 1000, 4000, 0 },
    { 6000, 500, 4000, 0 },
    { 6000, 0, 4000, 0 },
    { 0, 0, 0, ACTOR_160900_SPRITE_POINTS_END },
};

/// Drifting-sprite origins for effect cue 2: 25 points and a `pad == -1` terminator.
///
/// XYZ are signed game coordinates in the input frame of `GsWSMATRIX`.
/// The spawner jitters X by up to 700 units and copies each position into an effect task.
static SVECTOR _gActor160900DriftingSpritePoints2[26] = {
    { 6000, 500, 2000, 0 },
    { 6000, 0, 2000, 0 },
    { 6000, -500, 2000, 0 },
    { 6000, -1000, 2000, 0 },
    { 6000, -1500, 2000, 0 },
    { 6000, 500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, -1000, 2500, 0 },
    { 6000, -1500, 2500, 0 },
    { 6000, 500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, -1000, 3000, 0 },
    { 6000, -1500, 3000, 0 },
    { 6000, 500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, -1000, 3500, 0 },
    { 6000, -1500, 3500, 0 },
    { 6000, 500, 4000, 0 },
    { 6000, 0, 4000, 0 },
    { 6000, -500, 4000, 0 },
    { 6000, -1000, 4000, 0 },
    { 6000, -1500, 4000, 0 },
    { 0, 0, 0, ACTOR_160900_SPRITE_POINTS_END },
};

/// Drifting-sprite origins for effect cue 3: 9 points and a `pad == -1` terminator.
///
/// XYZ are signed game coordinates in the input frame of `GsWSMATRIX`.
/// The spawner jitters X by up to 700 units and copies each position into an effect task.
static SVECTOR _gActor160900DriftingSpritePoints3[10] = {
    { 6000, -1000, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -1000, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -1000, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 0, 0, 0, ACTOR_160900_SPRITE_POINTS_END },
};

/// Drifting-sprite origins for effect cue 4: 10 points and a `pad == -1` terminator.
///
/// XYZ are signed game coordinates in the input frame of `GsWSMATRIX`.
/// The spawner jitters X by up to 700 units and copies each position into an effect task.
static SVECTOR _gActor160900DriftingSpritePoints4[11] = {
    { 8000, 0, 3000, 0 },
    { 8000, -500, 3000, 0 },
    { 8000, 0, 2900, 0 },
    { 8000, -500, 2900, 0 },
    { 8000, 0, 2850, 0 },
    { 8000, -500, 2850, 0 },
    { 8000, 0, 2800, 0 },
    { 8000, -500, 2800, 0 },
    { 8000, 0, 2750, 0 },
    { 8000, -500, 2750, 0 },
    { 0, 0, 0, ACTOR_160900_SPRITE_POINTS_END },
};

/// Drifting-sprite origins for effect cue 5: 26 points and a `pad == -1` terminator.
///
/// XYZ are signed game coordinates in the input frame of `GsWSMATRIX`.
/// The spawner jitters X by up to 700 units and copies each position into an effect task.
static SVECTOR _gActor160900DriftingSpritePoints5[27] = {
    { 7000, 3000, 3500, 0 },
    { 7000, 3000, 3000, 0 },
    { 7000, 3000, 2500, 0 },
    { 7000, 2000, 3500, 0 },
    { 7000, 2000, 3000, 0 },
    { 7000, 2000, 2500, 0 },
    { 7000, 1000, 3500, 0 },
    { 7000, 1000, 3000, 0 },
    { 7000, 1000, 2500, 0 },
    { 7000, 0, 4000, 0 },
    { 7000, 0, 3500, 0 },
    { 7000, 0, 3000, 0 },
    { 7000, 0, 2500, 0 },
    { 7000, 0, 2000, 0 },
    { 7000, -1500, 4000, 0 },
    { 7000, -1500, 3500, 0 },
    { 7000, -1500, 3000, 0 },
    { 7000, -1500, 2500, 0 },
    { 7000, -1500, 2000, 0 },
    { 7000, -3000, 4500, 0 },
    { 7000, -3000, 4000, 0 },
    { 7000, -3000, 3500, 0 },
    { 7000, -3000, 3000, 0 },
    { 7000, -3000, 2500, 0 },
    { 7000, -3000, 2000, 0 },
    { 7000, -3000, 1500, 0 },
    { 0, 0, 0, ACTOR_160900_SPRITE_POINTS_END },
};

/// Scene/audio stream key of the Aya/Kyle sequence (group 6, stream 9, qualifier 11).
///
/// The interpreter retains this loaded record through scene playback and cleanup.
static EvsSceneKey _gActor160900SceneKey = { 6, 9, 11 };

/// CAP-synchronized Aya/Kyle cutscene, with player, Kyle and effect cues.
///
/// Starts CAP command slot 1, selects the scene stream, and sequences light
/// quads, clips, drifting sprites and the screen wave before ending playback.
/// The command stream and all referenced resources stay loaded until it ends.
static EvsCommand _gActor160900CutsceneScript[58] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_160900_CAP_COMMAND_SLOT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_TAKE_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = ACTOR_160900_SCRIPT_POINTS_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor160900SceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SpawnOpeningLightPair }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor160900SpawnFadeIn }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = ACTOR_160900_SCRIPT_POINTS_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900KillLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SpawnKyleRevealLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_APPEAR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = ACTOR_160900_SCRIPT_POINTS_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900KillLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SpawnWalkLightPair }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_WALK_TO_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_WAVE_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = ACTOR_160900_SCRIPT_POINTS_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900KillLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_WAVE_END }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = ACTOR_160900_SCRIPT_POINTS_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = roomEffectRequestCancelAll }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor160900SpawnFadeOut }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_GROUND_DECAL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900FinishScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Input-skip path that fades, cancels scene playback and restores view 43.
///
/// Clears pending cues before event-scene cleanup, then waits three updates
/// and ends. Borrows the live published cutscene task and its work.
static EvsCommand _gActor160900SkipScript[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SkipCutscene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_160900_SKIP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor160900TaskDescriptors[8] = {
    { { { TASK_BODY_NONE, 192 } }, _actor160900CutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor160900FadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor160900FadeOutTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleBodyTask, { .model = &gKyleBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleAttachmentTask, { .model = &gKyleP229 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleAttachmentTask, { .model = &gKyleHandLeft2 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleAttachmentTask, { .model = &gKyleHandRight } },
    { { { TASK_BODY_COORD, 192 } }, _actor160900DrawLightQuadTask, { .value = 0 } },
};

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
static ScreenWaveCtx* _gScreenWaveContext;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
ScreenWaveOscillator _gScreenWaveColumnWaves[13];

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
ScreenWaveOscillator _gScreenWaveRowWaves[30];

static s32         _actor160900AdvancePlayerAnimChain(Task* task);
static inline void _actor160900BlendKyleAnim(Task* task, u16 animationId);
static s32         _actor160900AdvanceKyleAnimChain(Task* task);
static inline void _actor160900InitKyleAnimation(Task* task, TmdObject* bodyModel);
static inline void _actor160900BlendPlayerAnim(Task* task, u16 animationId);
static inline void _actor160900ResetPlayerAnim(Task* task, u16 animationId);
static void        _actor160900UpdatePlayerCue(Task* task);
static void        _actor160900UpdateKyleCue(Task* task);
static void        _actor160900SpawnDriftingSprites(const SVECTOR* points);

#include "../../shared/screen/screen_wave.inc.c"

/// Advances the cutscene player's clip chain by one update.
///
/// Requires a live cutscene work block and a valid player clip index (0..10).
/// A nonzero hold duration counts updates; zero waits for playback to finish.
/// Successors start with a ten-frame blend and world collision enabled. The
/// borrowed player task and animation data must remain live during dispatch.
/// Returns 1 when the player is absent or a reached link has no successor,
/// otherwise 0; this does not clear a cue or stop the last clip.
static s32 _actor160900AdvancePlayerAnimChain(Task* task)
{
    _Actor160900CutsceneWork* work;
    ActorAnimChainLink*       chain;
    ActorAnimChainLink*       link;
    ActorAnimChainLink*       finishedLink;
    AnimationPlayRequest      request;
    u16                       nextAnimationId;
    u16                       finishedAnimationId;

    /// Starts a successor clip and resets its hold counter after dispatch.
    ///
    /// work is a live cutscene-work pointer, clipId a valid u16 set index,
    /// and request a writable AnimationPlayRequest lvalue. Arguments must be
    /// stable locals without side effects: each is evaluated repeatedly.
    /// Borrows the package's player animation table and retains no payload;
    /// dispatch is synchronous and its result is ignored.
#define ACTOR_160900_START_PLAYER_CHAIN_CLIP(work, clipId, request)                                       \
    do {                                                                                                  \
        (request).source.sets          = _gActor160900PlayerAnimationSets;                                \
        (work)->playerAnimId           = (clipId);                                                        \
        (request).animationId          = (clipId);                                                        \
        (request).blend                = ANIMATION_BLEND_INTERPOLATE;                                     \
        (request).blendFrames          = ACTOR_160900_ANIM_BLEND_FRAMES;                                  \
        (request).enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                \
        TASK_MESSAGE_DISPATCH_POINTER((work)->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(request), 0); \
        (work)->playerAnimHold = 0;                                                                       \
    } while (0)

    work = task->work;
    if (work->player == NULL) {
        return 1;
    }
    chain = _gActor160900PlayerAnimationChain;
    link  = &chain[work->playerAnimId];
    if (link->holdFrames != 0) {
        if (work->playerAnimHold >= link->holdFrames) {
            if (link->nextAnimId < 0) {
                return 1;
            }
            nextAnimationId = link->nextAnimId;
            ACTOR_160900_START_PLAYER_CHAIN_CLIP(work, nextAnimationId, request);
        } else {
            work->playerAnimHold += 1;
        }
    } else {
        if (taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
            return 0;
        }
        finishedLink = &_gActor160900PlayerAnimationChain[work->playerAnimId];
        if (finishedLink->nextAnimId < 0) {
            return 1;
        }
        work = task->work;
        if (work->player != NULL) {
            finishedAnimationId = finishedLink->nextAnimId;
            ACTOR_160900_START_PLAYER_CHAIN_CLIP(work, finishedAnimationId, request);
        }
    }
#undef ACTOR_160900_START_PLAYER_CHAIN_CLIP
    return 0;
}
/// Clears Kyle's current clip hold counter in a live model work block.
static inline void _actor160900ResetKyleAnimHold(_Actor160900KyleModelWork* work)
{
    // Fitted branch: identical arms preserve the blend loop's instruction order.
    // The original condition is unknown; work must be non-NULL.
    if (work != NULL) {
        work->animHold = 0;
    } else {
        work->animHold = 0;
    }
}

/// Restarts Kyle's clip chain on a selected clip with a ten-frame pose blend.
///
/// Requires the body's live, bound twenty-part rig and an animation id in 0..1.
/// Resets the hold counter and seeks slots 1..19 to record zero; slot 0 and
/// the slots' playback rates are retained. The model and clip data are borrowed.
static inline void _actor160900BlendKyleAnim(Task* task, u16 animationId)
{
    _Actor160900KyleModelWork* work;
    u16                        slotIndex;

    work         = task->work;
    work->animId = animationId;
    _actor160900ResetKyleAnimHold(work);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, animationId, 0, ACTOR_160900_ANIM_BLEND_FRAMES);
    }
}

/// Ticks Kyle's visible body and advances its two-clip animation chain.
///
/// Requires live model work, a bound twenty-part rig and clip id 0..1. A hidden
/// body leaves slots and hold time unchanged and returns 0. Slots 1..19 tick;
/// a zero-duration link advances only when all are settled, otherwise its hold
/// counter counts callback updates. Successors use a ten-frame blend. Returns
/// 1 when a reached link has no successor, otherwise 0; the last clip remains
/// selected. The work, model, chain and clip resources stay borrowed throughout.
static s32 _actor160900AdvanceKyleAnimChain(Task* task)
{
    _Actor160900KyleModelWork* work;
    ActorAnimChainLink*        chain;
    u16                        slotIndex;
    u16                        allSettled;

    work = task->work;
    if (task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
        return 0;
    }
    // Tick every driven part before testing whether the whole clip has settled.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    slotIndex  = 1;
    allSettled = 1;
    for (; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            allSettled = 0;
            break;
        }
    }
    chain = work->animChain;
    if (chain[work->animId].holdFrames != 0) {
        if (work->animHold >= chain[work->animId].holdFrames) {
            if (chain[work->animId].nextAnimId >= 0) {
                _actor160900BlendKyleAnim(task, chain[work->animId].nextAnimId);
            } else {
                return 1;
            }
        } else {
            work->animHold++;
        }
    } else if (allSettled) {
        if (chain[work->animId].nextAnimId >= 0) {
            _actor160900BlendKyleAnim(task, chain[work->animId].nextAnimId);
        } else {
            return 1;
        }
    }
    return 0;
}

/// Attaches one of Kyle's hand or handgun models and updates its room lighting.
///
/// Start at state zero with a live TMD body and Kyle's body task in spawnArg2.
/// spawnArg1 selects the free hand (0, part 12), gun hand (1, part 8), or gun
/// (2, part 8). The body must provide those coordinates and outlive this task.
/// Hands use placement 0x65's texture offsets; the gun uses zero offsets.
/// Allocates owned primary-heap work for the model's lighting matrices and joins
/// the body's teardown tree. Allocation failure kills the task. Initialization
/// returns without sampling lighting; later updates use the composed root's XYZ.
static void _actor160900KyleAttachmentTask(Task* task)
{
    enum { ACTOR_160900_ATTACHMENT_INITIALIZE = 0 };
    VECTOR worldPosition;

    if (task->state == ACTOR_160900_ATTACHMENT_INITIALIZE) {
        TmdObject*                 model     = task->extra.tmd;
        Task*                      bodyTask  = task->spawnArg2.pointer;
        GfxCoord*                  rootCoord = model->coords;
        _Actor160900KyleModelWork* work;
        _Actor160900KyleModelWork* allocatedWork;
        AreaPlacement*             placement;
        u8                         entryId;

        allocatedWork = memMalloc(sizeof(*allocatedWork), false);
        task->work    = allocatedWork;
        if (allocatedWork == NULL) {
            taskKill(task);
            return;
        }
        work = allocatedWork;
        // Coordinate parenting follows the hand; task parenting owns teardown.
        switch (task->spawnArg1.value) {
            case ACTOR_160900_KYLE_FREE_HAND:
                rootCoord->parent = &bodyTask->extra.tmd->coords[ACTOR_160900_KYLE_FREE_HAND_PART];
                break;
            case ACTOR_160900_KYLE_GUN_HAND:
            case ACTOR_160900_KYLE_GUN:
                rootCoord->parent = &bodyTask->extra.tmd->coords[ACTOR_160900_KYLE_GUN_HAND_PART];
                break;
        }
        memFillBytes(task->work, 0, sizeof(*work));
        model->lightMtx = &work->light;
        model->colorMtx = &work->color;
        if (task->spawnArg1.value < ACTOR_160900_KYLE_GUN) {
            placement = areaGetVariant(&gGameSession->location)->placements;
            entryId   = placement->entryId;
            while (entryId != AREA_PLACEMENT_END) {
                if (entryId == ACTOR_160900_KYLE_PLACEMENT_ID) {
                    break;
                }
                placement++;
                entryId = placement->entryId;
            }
            tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
        } else if (task->spawnArg1.value == ACTOR_160900_KYLE_GUN) {
            tmdSetTextureOffsets(task->extra.tmd, 0, 0);
        }
        taskReparent(bodyTask, task);
        task->msgTable = _gActor160900KyleModelMessages;
        task->state   += 1;
        return;
    } else {
        TmdObject* model = task->extra.tmd;

        worldPosition.vx = model->coords->workm.t[0];
        worldPosition.vy = task->extra.tmd->coords->workm.t[1];
        worldPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(model, &worldPosition, 0, 3);
    }
}

/// Restarts Kyle's body at clip zero and normal playback rate.
///
/// Requires live model work with a bound twenty-part rig. Clears the clip id
/// and hold counter and resets slots 1..19; slot 0 remains untouched.
static inline void _actor160900ResetKyleAnimSlots(Task* task)
{
    _Actor160900KyleModelWork* work;
    u16                        slotIndex;

    work           = task->work;
    work->animId   = 0;
    work->animHold = 0;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, slotIndex, 0);
    }
}

/// Binds Kyle's body rig and starts clip zero at normal playback rate.
///
/// Requires live model work and the twenty-part body model. The context borrows
/// the work's pose/slot arrays, model coordinates and two loaded animation sets
/// through playback; its clip chain has two entries. Resets slots 1..19 and the
/// hold counter, retaining the placed root in slot 0. Does not allocate storage.
static inline void _actor160900InitKyleAnimation(Task* task, TmdObject* bodyModel)
{
    _Actor160900KyleModelWork* work;

    work = task->work;
    animationInitContext(&work->rig.anim, _gActor160900KyleAnimationSets, bodyModel, work->rig.poses, work->rig.slots);
    work->animChain = _gActor160900KyleAnimationChain;
    _actor160900ResetKyleAnimSlots(task);
}

/// Initializes Kyle's hidden cutscene body, then advances its clip and room lighting.
///
/// Start at state zero with the twenty-part TMD body and a live published
/// cutscene task. Owns primary-heap rig and lighting work, released by teardown;
/// allocation failure kills the task. Placement 0x65 supplies texture offsets.
/// The body joins the cutscene's teardown tree and binds two animation sets.
/// Every update applies the view-dependent part-18 rotation (4096 units/turn)
/// and samples lighting at the already-composed root; hidden clips do not tick.
static void _actor160900KyleBodyTask(Task* task)
{
    enum {
        ACTOR_160900_BODY_INITIALIZE  = 0,
        ACTOR_160900_KYLE_POSE_PART   = 18,
        ACTOR_160900_KYLE_ROLL_VIEW   = 0x2E,
        ACTOR_160900_KYLE_PITCH_ANGLE = 0x79C,
    };
    TmdObject*                 model;
    TmdObject*                 litModel;
    GfxCoord*                  rootCoord;
    _Actor160900KyleModelWork* work;
    AreaPlacement*             placement;
    VECTOR                     worldPosition;
    u16                        allocationFailed;

    if (task->state == ACTOR_160900_BODY_INITIALIZE) {
        model      = task->extra.tmd;
        rootCoord  = model->coords;
        work       = memMalloc(sizeof(*work), false);
        task->work = work;
        if (work == NULL) {
            allocationFailed = true;
        } else {
            rootCoord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, sizeof(*work));
            model->lightMtx = &work->light;
            model->colorMtx = &work->color;
            model->flags   |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            task->msgTable  = _gActor160900KyleModelMessages;
            placement       = areaGetVariant(&gGameSession->location)->placements;
            while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != ACTOR_160900_KYLE_PLACEMENT_ID) {
                placement++;
            }
            tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
            taskReparent(_gActor160900CutsceneTask, task);
            allocationFailed = false;
        }
        if (allocationFailed) {
            taskKill(task);
            return;
        }
        _actor160900InitKyleAnimation(task, task->extra.tmd);
        task->state++;
    }
    _actor160900AdvanceKyleAnimChain(task);
    // Override this part's animated rotation for the current camera pose.
    if (gGameSession->location.view == ACTOR_160900_KYLE_ROLL_VIEW) {
        gfxRotMatrixZ(&task->extra.tmd->coords[ACTOR_160900_KYLE_POSE_PART].coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_REPLACE);
    } else {
        gfxRotMatrixX(&task->extra.tmd->coords[ACTOR_160900_KYLE_POSE_PART].coord, ACTOR_160900_KYLE_PITCH_ANGLE, GRAPHICS_ROTATION_REPLACE);
    }
    litModel         = task->extra.tmd;
    worldPosition.vx = litModel->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(litModel, &worldPosition, 0, 3);
}

/// Shades a light quad from a live task's six-valued spawn pattern.
///
/// quad must be writable; task's first spawn argument is 0..5. Writes only RGB
/// bytes, retaining packet metadata and coordinates. Neither pointer is retained.
static inline void _actor160900ShadeLightQuad(POLY_G4* quad, const Task* task)
{
    enum {
        ACTOR_160900_LIGHT_WHITE_CORNER_3    = 0,
        ACTOR_160900_LIGHT_WHITE_CORNER_2    = 1,
        ACTOR_160900_LIGHT_WHITE_CORNERS_1_3 = 2,
        ACTOR_160900_LIGHT_WHITE_CORNERS_0_2 = 3,
        ACTOR_160900_LIGHT_WHITE_CORNERS_2_3 = 4,
        ACTOR_160900_LIGHT_WHITE_ALL_CORNERS = 5,
        ACTOR_160900_LIGHT_MAX_INTENSITY     = 255,
    };
    quad->r0 = 0;
    quad->g0 = 0;
    quad->b0 = 0;
    quad->r1 = 0;
    quad->g1 = 0;
    quad->b1 = 0;
    switch (task->spawnArg1.value) {
        case ACTOR_160900_LIGHT_WHITE_CORNER_3:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r2 = 0;
            quad->g2 = 0;
            quad->b2 = 0;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNER_2:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r3 = 0;
            quad->g3 = 0;
            quad->b3 = 0;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNERS_1_3:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r2 = 0;
            quad->g2 = 0;
            quad->b2 = 0;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNERS_0_2:
            quad->r0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r3 = 0;
            quad->g3 = 0;
            quad->b3 = 0;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNERS_2_3:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_ALL_CORNERS:
            quad->r0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
    }
}

/// Draws one additive gradient light quad in the cutscene.
///
/// Requires a live coordinate body and four local-space corners in the task's
/// work. The first spawn argument selects white corners (0 corner 3, 1 corner 2,
/// 2 corners 1 and 3, 3 corners 0 and 2, 4 corners 2 and 3, 5 all four);
/// remaining corners are black. Callers must supply one of these six patterns.
/// The coordinate origin supplies the ordering depth, quantized by four bits;
/// that tag must fit the current ordering table. The current frame arena must
/// hold a POLY_G4 and DR_TPAGE through GPU drawing. Projection changes GTE state.
static void _actor160900DrawLightQuadTask(Task* task)
{
    enum { ACTOR_160900_LIGHT_DEPTH_SHIFT = 4 };
    _Actor160900LightQuadWork* work;
    s16                        screenX[ARRAY_SIZE(work->corners)];
    s16                        screenY[ARRAY_SIZE(work->corners)];
    SVECTOR                    origin;
    s32                        screenXY;
    s32                        originDepth;
    GfxCoord*                  coord;
    POLY_G4*                   quad;
    DR_TPAGE*                  drawMode;
    s16                        cornerIndex;

    coord = task->extra.coordBody->coord;
    work  = task->work;
    actorRenderComposeCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    // Sort the whole quad by its origin, then project its four local corners.
    origin.vz = 0;
    origin.vy = 0;
    origin.vx = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&screenXY);
    gte_stszotz(&originDepth);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(work->corners); cornerIndex++) {
        gte_ldv0(&work->corners[cornerIndex]);
        gte_rtps();
        gte_stsxy(&screenXY);
        screenX[cornerIndex] = screenXY;
        screenY[cornerIndex] = screenXY >> 16;
    }

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyG4(quad);
    setSemiTrans(quad, true);
    _actor160900ShadeLightQuad(quad, task);
    quad->x0 = screenX[0];
    quad->y0 = screenY[0];
    quad->x1 = screenX[1];
    quad->y1 = screenY[1];
    quad->x2 = screenX[2];
    quad->y2 = screenY[2];
    quad->x3 = screenX[3];
    quad->y3 = screenY[3];
    addPrim(&gGpuCurrentOt[originDepth >> ACTOR_160900_LIGHT_DEPTH_SHIFT], quad);
    // Prepending the mode packet makes additive blending active before the quad.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, 1);
    drawMode->code[0] = _get_mode(false, true, getTPage(0, GPU_BLEND_ADD, 0, 0));
    addPrim(&gGpuCurrentOt[originDepth >> ACTOR_160900_LIGHT_DEPTH_SHIFT], drawMode);
}

/// Restarts a cutscene player clip with a ten-frame pose blend.
///
/// `task` owns live cutscene work; clip id 0..10 selects its loaded player set
/// and successor-chain entry. An absent borrowed player is a no-op. Dispatch
/// synchronously borrows the request, enables world collision and resets hold
/// time; the player and set resources must stay live throughout playback.
static inline void _actor160900BlendPlayerAnim(Task* task, u16 animationId)
{
    _Actor160900CutsceneWork* work;
    AnimationPlayRequest      request;

    work = task->work;
    if (work->player != NULL) {
        request.source.sets          = _gActor160900PlayerAnimationSets;
        work->playerAnimId           = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_INTERPOLATE;
        request.blendFrames          = ACTOR_160900_ANIM_BLEND_FRAMES;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        work->playerAnimHold = 0;
    }
}

/// Restarts a cutscene player clip immediately, without blending from its pose.
///
/// Requires live cutscene work and a valid player clip id 0..10. A NULL player
/// is a no-op. Synchronous dispatch installs the loaded player sets, enables
/// world collision and resets hold time. The request lasts only through dispatch;
/// the borrowed player and animation data must stay live throughout playback.
static inline void _actor160900ResetPlayerAnim(Task* task, u16 animationId)
{
    _Actor160900CutsceneWork* work;
    AnimationPlayRequest      request;

    work = task->work;
    if (work->player != NULL) {
        request.source.sets          = _gActor160900PlayerAnimationSets;
        work->playerAnimId           = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_RESET;
        request.blendFrames          = 0;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        work->playerAnimHold = 0;
    }
}

/// Advances Aya's clip chain and applies the cutscene's pending player cue.
///
/// Requires live cutscene work and loaded player clips 0..10. Placement and walk
/// cues require the borrowed player task. Messages consume stack requests
/// synchronously, while playback borrows the loaded animation resources.
/// The walk cue runs once at step zero and remains posted; other cues clear
/// after handling. Wave start lends the embedded context to a separate task
/// for an eight-update rise, so the cutscene work must outlive that task.
/// The walk cue's resident animation bank requires characterId 1 and weapon 0..32.
static void _actor160900UpdatePlayerCue(Task* task)
{
    enum {
        ACTOR_160900_PLAYER_PLACE_CLIP          = 10,
        ACTOR_160900_PLAYER_READY_CLIP          = 1,
        ACTOR_160900_PRIMARY_CHARACTER          = 1,
        ACTOR_160900_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_160900_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        ACTOR_160900_WAVE_RISE_UPDATES          = 8,
        ACTOR_160900_WAVE_FULL_SCALE            = 0x80,
    };
    _Actor160900CutsceneWork* work;
    union {
        AnimationPlayRequest animation;
        VECTOR3              destination;
    } message;

    _Actor160900CutsceneWork* requestWork;
    s32                       animationBankIndex;
    s32                       weaponId;

/// Restarts a player scene clip with collision enabled and clears its chain hold.
///
/// Reloads task->work into the writable cueWork pointer before checking player.
/// Requires live cutscene work and loaded player sets. clipId selects a valid
/// u16 clip; request is a writable AnimationPlayRequest lvalue borrowed only for
/// synchronous dispatch, while the player borrows the set table during playback.
/// Stable side-effect-free arguments are required: cueWork, clipId and request
/// occur repeatedly. An absent player leaves the request and clip/hold unchanged.
/// The hold is cleared after dispatch; overwrites no caller locals implicitly.
#define ACTOR_160900_RESET_PLAYER_CUE_CLIP(task, clipId, cueWork, request)                                       \
    do {                                                                                                         \
        (cueWork) = (task)->work;                                                                                \
        if ((cueWork)->player != NULL) {                                                                         \
            (request).source.sets          = _gActor160900PlayerAnimationSets;                                   \
            (cueWork)->playerAnimId        = (clipId);                                                           \
            (request).animationId          = (clipId);                                                           \
            (request).blend                = ANIMATION_BLEND_RESET;                                              \
            (request).blendFrames          = 0;                                                                  \
            (request).enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                   \
            TASK_MESSAGE_DISPATCH_POINTER((cueWork)->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(request), 0); \
            (cueWork)->playerAnimHold = 0;                                                                       \
        }                                                                                                        \
    } while (0)

    work = task->work;
    _actor160900AdvancePlayerAnimChain(task);
    switch (work->playerCue.id) {
        case ACTOR_160900_CUE_NONE:
            break;
        case ACTOR_160900_PLAYER_CUE_TAKE_PLACE:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gActor160900PlayerInitialPlacement, 0);
            ACTOR_160900_RESET_PLAYER_CUE_CLIP(task, ACTOR_160900_PLAYER_PLACE_CLIP, requestWork, message.animation);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_1:
            ACTOR_160900_RESET_PLAYER_CUE_CLIP(task, ACTOR_160900_PLAYER_READY_CLIP, requestWork, message.animation);
            break;
        case ACTOR_160900_PLAYER_CUE_WALK_TO_MARK:
            if (work->playerCue.step == 0) {
                weaponId = gPlayerStatus.weapon;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_160900_PRIMARY_CHARACTER) {
                    animationBankIndex = weaponId + ACTOR_160900_PRIMARY_WEAPON_BANK_BASE;
                } else {
                    animationBankIndex = weaponId + ACTOR_160900_ALTERNATE_WEAPON_BANK_BASE;
                }
                // Return to the equipped bank before taking scripted movement control.
                message.animation.source.index         = animationBankIndex;
                message.animation.animationId          = ACTOR_160900_PLAYER_READY_CLIP;
                message.animation.blend                = ANIMATION_BLEND_RESET;
                message.animation.blendFrames          = 0;
                message.animation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &message.animation, 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gActor160900PlayerWalkPlacement, 0);
                message.destination.vx = -0x7D0;
                message.destination.vy = 0;
                message.destination.vz = 0xC80;
                TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_RUN_TO, &message.destination, 0);
                work->playerCue.step++;
            }
            return;
        case ACTOR_160900_PLAYER_CUE_WAVE_START:
            work->wave.span  = ACTOR_160900_WAVE_RISE_UPDATES;
            work->wave.scale = ACTOR_160900_WAVE_FULL_SCALE;
            taskSpawnFromTable(_gActor160900ScreenWaveTaskDescriptors, 0, 0, &work->wave);
            work->playerCue.id = ACTOR_160900_CUE_NONE;
            return;
        case ACTOR_160900_PLAYER_CUE_WAVE_END:
            work->wave.state = SCREEN_WAVE_RAMP_FINISHED;
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_2:
            _actor160900ResetPlayerAnim(task, 2);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_6:
            _actor160900BlendPlayerAnim(task, 6);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_7:
            _actor160900BlendPlayerAnim(task, 7);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_8:
            _actor160900BlendPlayerAnim(task, 8);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_9:
            _actor160900BlendPlayerAnim(task, 9);
            break;
    }
    work->playerCue.id = ACTOR_160900_CUE_NONE;
#undef ACTOR_160900_RESET_PLAYER_CUE_CLIP
}

/// Applies and consumes one posted Kyle cue for the cutscene.
///
/// Requires live cutscene work and Kyle's body and two hand tasks. Cue ids 1..4
/// hide/place him, show his models and spawn a decal, restart clip 1, or spawn
/// another decal. The optional handgun may be NULL. Decal offsets are signed
/// game-coordinate units in Kyle's model-root frame; spawns copy them. Clears
/// the cue id after handling, including zero or an unsupported id; step is kept.
static void _actor160900UpdateKyleCue(Task* task)
{
    enum { KYLE_CUE_CLIP        = 1,
           KYLE_DECAL_HALF_SIDE = 256 };
    _Actor160900CutsceneWork* work;
    SVECTOR                   appearDecalOffset;
    SVECTOR                   secondDecalOffset;

    work = task->work;
    switch (work->kyleCue.id) {
        case ACTOR_160900_CUE_NONE:
            break;
        case ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->kyle, ACTOR_MESSAGE_PLACE, &_gActor160900KylePlacement, 0);
            break;
        case ACTOR_160900_KYLE_CUE_APPEAR:
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            appearDecalOffset.vx = -100;
            appearDecalOffset.vy = 100;
            appearDecalOffset.vz = -1200;
            effectSpawn(EFFECT_GROUND_DECAL, work->kyle->extra.tmd->coords,
                        EFFECT_GROUND_DECAL_START_FULL_BRIGHT | KYLE_DECAL_HALF_SIDE, &appearDecalOffset);
            break;
        case ACTOR_160900_KYLE_CUE_CLIP_1:
            _actor160900BlendKyleAnim(work->kyle, KYLE_CUE_CLIP);
            break;
        case ACTOR_160900_KYLE_CUE_GROUND_DECAL:
            secondDecalOffset.vx = -200;
            secondDecalOffset.vy = 100;
            secondDecalOffset.vz = -400;
            effectSpawn(EFFECT_GROUND_DECAL, work->kyle->extra.tmd->coords,
                        EFFECT_GROUND_DECAL_START_FULL_BRIGHT | KYLE_DECAL_HALF_SIDE, &secondDecalOffset);
            break;
    }
    work->kyleCue.id = ACTOR_160900_CUE_NONE;
}

/// Emits drifting sprites over a point list every eighth display frame.
///
/// `points` must reach a `pad == -1` terminator within readable storage. XYZ
/// are signed game coordinates in the input frame of `GsWSMATRIX`;
/// X receives jitter in 100-unit steps from -700 through +700 and narrows to a
/// halfword. Each point consumes two LCG samples. Uses perspective-size numerator
/// 1024, three running updates per cell, speed 32 coordinate units per update,
/// upward random drift and the alternate sprite sheet.
/// Spawning borrows the local offset only for placement; this sprite callback
/// uses the copied coordinates. Failed spawns are ignored and still consume RNG.
static void _actor160900SpawnDriftingSprites(const SVECTOR* points)
{
    enum {
        ACTOR_160900_DRIFT_FRAME_MASK     = 7,
        ACTOR_160900_DRIFT_LIST_END       = -1,
        ACTOR_160900_DRIFT_JITTER_MASK    = 7,
        ACTOR_160900_DRIFT_JITTER_STEP    = 100,
        ACTOR_160900_DRIFT_SPAWN_ARGUMENT = 0x81203400,
    };
    SVECTOR spawnOffset;
    s32     jitteredX;
    u32     seed;
    s32     spawnArgument;

    if (!(gDisplayState.animFrame & ACTOR_160900_DRIFT_FRAME_MASK) && points->pad != ACTOR_160900_DRIFT_LIST_END) {
        spawnArgument = ACTOR_160900_DRIFT_SPAWN_ARGUMENT;
        do {
            // Draw sign and magnitude separately; the LCG and sum wrap as unsigned words.
            seed            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = seed;
            jitteredX       = points->vx + (((seed >> 16) & 1) ? ((gRandomLcgState = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_160900_DRIFT_JITTER_MASK
                                                               : -(((gRandomLcgState = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_160900_DRIFT_JITTER_MASK)) *
                                         ACTOR_160900_DRIFT_JITTER_STEP;
            spawnOffset.vx = jitteredX;
            spawnOffset.vy = points->vy;
            spawnOffset.vz = points->vz;
            effectSpawn(EFFECT_1B4, NULL, spawnArgument, &spawnOffset);
            points++;
        } while (points->pad != ACTOR_160900_DRIFT_LIST_END);
    }
}

/// Spawns the two additive light quads used at the opening playback cue.
///
/// Requires the published cutscene task and work, and empty light slots 0..1.
/// Places two 1500-by-1000 rectangles beside Z=3000, at root (6000,1000,3000)
/// in view-parent coordinates; patterns 0 and 1 whiten their outside corners.
/// Each quad owns primary-heap corner work and a coordinate body. Failure stops
/// the sequence without rolling back earlier quads. Work-allocation failure
/// kills that task but retains its saved slot; normal removal kills all slots.
static void _actor160900SpawnOpeningLightPair(void)
{
    _Actor160900CutsceneWork*  cutsceneWork;
    _Actor160900LightQuadWork* allocatedQuadWork;
    _Actor160900LightQuadWork* quadWork;
    Task*                      quadTask;

    cutsceneWork                = _gActor160900CutsceneTask->work;
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_3, 0);
    cutsceneWork->lightQuads[0] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0x3E8;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0x3E8;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = 0;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0x3E8;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = 0;
    quadTask                                     = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_2, 0);
    cutsceneWork->lightQuads[1]                  = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0x3E8;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = -0x3E8;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = -0x3E8;
}
/// Parents one reveal light quad to the view and lays out its local Y/Z rectangle.
///
/// Requires a live coordinate body and four writable corners. The root is placed
/// at X=6000, Y=rootY, Z=2700; rootY is a full-width game coordinate, while the
/// local Z edges are signed halfwords. Corners 0/1 are at Y=-1000 and 2/3 at Y=0,
/// all at X=0. Keeps corner pads and the composition stamp; retains neither pointer.
static inline void _actor160900PlaceRevealLightQuad(Task* quadTask, _Actor160900LightQuadWork* quadWork, s32 rootY, s16 rightZ, s16 leftZ)
{
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 6000;
    quadTask->extra.coordBody->coord->coord.t[1] = rootY;
    quadTask->extra.coordBody->coord->coord.t[2] = 2700;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -1000;
    quadWork->corners[0].vz                      = rightZ;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -1000;
    quadWork->corners[1].vz                      = leftZ;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = rightZ;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = leftZ;
}

/// Spawns the six additive light quads for Kyle's reveal.
///
/// Requires the published cutscene's live work and empty light slots 0..5.
/// Builds two three-quad rows at roots (6000,+/-500,2700), parented to the view.
/// Each 1000-unit-high row spans local Z=-1000..1000, with the middle quad
/// 1000 units wide and the outer quads 500 units wide. Patterns 5,2,3 and
/// 4,0,1 respectively select the white corners. Owns each coordinate body and
/// primary-heap corner block. Failure stops with earlier quads retained; a
/// failed work allocation kills its task without clearing the saved slot.
static void _actor160900SpawnKyleRevealLightQuads(void)
{
    _Actor160900CutsceneWork*  cutsceneWork;
    _Actor160900LightQuadWork* allocatedQuadWork;
    _Actor160900LightQuadWork* quadWork;
    Task*                      quadTask;

    cutsceneWork                = _gActor160900CutsceneTask->work;
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_ALL_CORNERS, 0);
    cutsceneWork->lightQuads[0] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, 0x1F4, 0x1F4, -0x1F4);
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNERS_1_3, 0);
    cutsceneWork->lightQuads[1] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, 0x1F4, 0x3E8, 0x1F4);
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNERS_0_2, 0);
    cutsceneWork->lightQuads[2] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, 0x1F4, -0x1F4, -0x3E8);
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNERS_2_3, 0);
    cutsceneWork->lightQuads[3] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, -0x1F4, 0x1F4, -0x1F4);
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_3, 0);
    cutsceneWork->lightQuads[4] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, -0x1F4, 0x3E8, 0x1F4);
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_2, 0);
    cutsceneWork->lightQuads[5] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, -0x1F4, -0x1F4, -0x3E8);
}
/// Spawns the two additive light quads used when Aya walks to her mark.
///
/// Requires the published cutscene's live work and empty light slots 0..1.
/// Places two 1500-by-1000 rectangles beside Z=3000, at root (6000,0,3000)
/// in view-parent coordinates; patterns 0 and 1 whiten their outside corners.
/// Owns each coordinate body and primary-heap corner block. Failure stops with
/// earlier quads retained; a failed work allocation kills its task without
/// clearing the saved slot. The script removes the set before reusing slots.
static void _actor160900SpawnWalkLightPair(void)
{
    _Actor160900CutsceneWork*  cutsceneWork;
    _Actor160900LightQuadWork* allocatedQuadWork;
    _Actor160900LightQuadWork* quadWork;
    Task*                      quadTask;

    cutsceneWork                = _gActor160900CutsceneTask->work;
    quadTask                    = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_3, 0);
    cutsceneWork->lightQuads[0] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0x3E8;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = 0;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0x3E8;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = 0;
    quadTask                                     = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_2, 0);
    cutsceneWork->lightQuads[1]                  = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = -0x3E8;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = -0x3E8;
}
/// Runs the package's Aya/Kyle cutscene and its three script-controlled cue lanes.
///
/// State zero waits for the attachment wheel and display transition to finish,
/// allocates owned cutscene work and publishes this task for script callbacks.
/// Borrows the player, spawns Kyle's body and hands, and selects CAP file 3.
/// State one starts the main/skip event scripts; state two waits for eventState
/// zero, posts scene event 30 and requests teardown. Both states process cues.
/// Successful setup requires live player/model resources and successful child
/// spawns. Kyle's models join the teardown tree; light and wave task lifetimes
/// are controlled by the scripts and must end before the cutscene work dies.
static void _actor160900CutsceneTask(Task* task)
{
    enum {
        ACTOR_160900_CUTSCENE_INITIALIZE   = 0,
        ACTOR_160900_CUTSCENE_START_SCRIPT = 1,
        ACTOR_160900_CUTSCENE_RUN_SCRIPT   = 2,
        ACTOR_160900_EVENT_FINISHED        = 0,
        ACTOR_160900_CAP_FILE              = 3,
        ACTOR_160900_CAP_TEXTURE_X         = 384,
        ACTOR_160900_CAP_TEXTURE_Y         = 0,
        ACTOR_160900_NEXT_SCENE_EVENT      = 30,
        ACTOR_160900_EFFECT_CUE_LIST_1     = 1,
        ACTOR_160900_EFFECT_CUE_LIST_2     = 2,
        ACTOR_160900_EFFECT_CUE_LIST_3     = 3,
        ACTOR_160900_EFFECT_CUE_LIST_4     = 4,
        ACTOR_160900_EFFECT_CUE_LIST_5     = 5,
    };
    _Actor160900CutsceneWork* work;
    _Actor160900CutsceneWork* cueWork;

    switch (task->state) {
        case ACTOR_160900_CUTSCENE_INITIALIZE:
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = memMalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->player              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                _gActor160900CutsceneTask = task;
                // Publish the cast before event-script callbacks can address it.
                work->kyle         = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_KYLE_BODY, 0, task);
                work->kyleGunHand  = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_KYLE_GUN_HAND, ACTOR_160900_KYLE_GUN_HAND, work->kyle);
                work->kyleFreeHand = taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_KYLE_FREE_HAND, ACTOR_160900_KYLE_FREE_HAND, work->kyle);
            }
            // Caption setup and the state increment also run after allocation failure.
            gCapFile = NULL;
            capSelectLoadedFile(ACTOR_160900_CAP_FILE);
            capSetTexturePage(ACTOR_160900_CAP_TEXTURE_X, ACTOR_160900_CAP_TEXTURE_Y);
            task->state += 1;
            return;
        case ACTOR_160900_CUTSCENE_START_SCRIPT:
            evsStartScriptWithSkip(_gActor160900CutsceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor160900SkipScript);
            task->state += 1;
            break;
        case ACTOR_160900_CUTSCENE_RUN_SCRIPT:
            if (gGameSession->eventState == ACTOR_160900_EVENT_FINISHED) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_160900_NEXT_SCENE_EVENT;
                taskRequestKill(task, 0);
            }
            break;
    }
    _actor160900UpdatePlayerCue(task);
    _actor160900UpdateKyleCue(task);
    // Player and Kyle consume their cues; the sprite lane stays posted across frames.
    cueWork = task->work;
    switch (cueWork->effectCue.id) {
        case ACTOR_160900_EFFECT_CUE_LIST_1:
            _actor160900SpawnDriftingSprites(_gActor160900DriftingSpritePoints1);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_2:
            _actor160900SpawnDriftingSprites(_gActor160900DriftingSpritePoints2);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_3:
            _actor160900SpawnDriftingSprites(_gActor160900DriftingSpritePoints3);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_4:
            _actor160900SpawnDriftingSprites(_gActor160900DriftingSpritePoints4);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_5:
            _actor160900SpawnDriftingSprites(_gActor160900DriftingSpritePoints5);
            break;
        case ACTOR_160900_CUE_NONE:
        default:
            cueWork->effectCue.id = ACTOR_160900_CUE_NONE;
            break;
    }
}

/// Darkens the cutscene to black and holds its subtractive overlay.
///
/// Start at state 0 with no owned work. Allocates primary-heap fade work,
/// killing the task on failure; normal task teardown releases the work.
/// The unsigned low half of spawnArg1 adds intensity units per update, after
/// drawing the low red/green/red bytes. Stored channels narrow to signed
/// halfwords before the red >= 256 test clamps all three to 255. Large rates
/// can wrap negative; zero holds the current level. Other states do nothing.
/// Requires the frame arena and foreground tag used by `fadeDrawOverlay`.
static void _actor160900FadeOutTask(Task* task)
{
    enum {
        ACTOR_160900_FADE_OUT_INITIALIZE = 0,
        ACTOR_160900_FADE_OUT_RAMP       = 1,
        ACTOR_160900_FADE_MAX_INTENSITY  = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_160900_FADE_OUT_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += 1;
            // Initialization also draws and advances the ramp.
            /* fallthrough */
        case ACTOR_160900_FADE_OUT_RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r += task->spawnArg1.halves.low;
            fade->g += task->spawnArg1.halves.low;
            fade->b += task->spawnArg1.halves.low;
            if (fade->r > ACTOR_160900_FADE_MAX_INTENSITY) {
                fade->b = ACTOR_160900_FADE_MAX_INTENSITY;
                fade->g = ACTOR_160900_FADE_MAX_INTENSITY;
                fade->r = ACTOR_160900_FADE_MAX_INTENSITY;
            }
            break;
    }
}
/// Reveals the cutscene while restoring display output after three updates.
///
/// Start at state 0 with no owned work. Allocates primary-heap fade work at
/// intensity 255, killing the task on failure; teardown releases the work.
/// States 0..4 draw and step each update; state 3 enables display output, then
/// state 4 holds the ramp phase. Other states do nothing. The unsigned low half
/// of spawnArg1 subtracts intensity units after drawing low red/green/red bytes.
/// Channels narrow to signed halfwords; negative stored red kills the task.
/// Large rates can wrap, and zero holds indefinitely. Requires the frame arena
/// and foreground ordering-table tag used by `fadeDrawOverlay`.
static void _actor160900FadeInTask(Task* task)
{
    enum {
        ACTOR_160900_FADE_IN_INITIALIZE     = 0,
        ACTOR_160900_FADE_IN_WAIT_FIRST     = 1,
        ACTOR_160900_FADE_IN_WAIT_SECOND    = 2,
        ACTOR_160900_FADE_IN_ENABLE_DISPLAY = 3,
        ACTOR_160900_FADE_IN_RAMP           = 4,
        ACTOR_160900_FADE_MAX_INTENSITY     = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_160900_FADE_IN_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = ACTOR_160900_FADE_MAX_INTENSITY;
            fade->g      = ACTOR_160900_FADE_MAX_INTENSITY;
            fade->r      = ACTOR_160900_FADE_MAX_INTENSITY;
            task->state += 1;
            break;
        case ACTOR_160900_FADE_IN_ENABLE_DISPLAY:
            SetDispMask(true);
            /* fallthrough */
        case ACTOR_160900_FADE_IN_WAIT_FIRST:
        case ACTOR_160900_FADE_IN_WAIT_SECOND:
            task->state += 1;
            break;
        case ACTOR_160900_FADE_IN_RAMP:
            break;
        default:
            return;
    }
    // Fade progress continues while the display is masked.
    fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
    fade->r -= task->spawnArg1.halves.low;
    fade->g -= task->spawnArg1.halves.low;
    fade->b -= task->spawnArg1.halves.low;
    if (fade->r < 0) {
        taskKill(task);
    }
}
/// Applies a draw request to one of Kyle's live TMD models.
///
/// SHOW clears active-draw and automatic-buffer exclusions; HIDE_SKIP_AUTO_BUFFER
/// sets both. Mode 0 and other values leave all flags intact. Neither allocates
/// nor releases a buffer. The message id and second payload are ignored;
/// callers must ignore the dispatch result because no return value is defined.
static void _actor160900SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    enum { ACTOR_160900_MODEL_DRAW_KEEP = 0 };
    TmdObject* model;

    model = task->extra.tmd;
    switch (mode) {
        case ACTOR_160900_MODEL_DRAW_KEEP:
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

#include "../../shared/actor/actor_messages_place_ypr.inc.c"

/// Starts the cutscene's reveal fade, restoring display output on its fourth update.
///
/// The unsigned low half of intensityStep is subtracted from intensity after
/// each draw; the upper half is ignored. Zero holds black indefinitely, and
/// large rates retain signed-halfword wrap. Task/work allocation may fail.
static void _actor160900SpawnFadeIn(s32 intensityStep)
{
    taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_FADE_IN, intensityStep, 0);
}

/// Starts the cutscene's darkening fade and holds the resulting black overlay.
///
/// The unsigned low half of intensityStep is added to intensity after each draw;
/// the upper half is ignored. Zero holds the starting level, and large rates
/// retain signed-halfword wrap. Task/work allocation may fail.
static void _actor160900SpawnFadeOut(s32 intensityStep)
{
    taskSpawnFromTable(gActor160900TaskDescriptors, ACTOR_160900_TASK_FADE_OUT, intensityStep, 0);
}

/// Kills and clears every light quad held by the running cutscene.
///
/// Requires the published cutscene task and its work to remain live. Empty
/// slots are skipped; killing a quad releases its owned corner work and body.
static void _actor160900KillLightQuads(void)
{
    _Actor160900CutsceneWork* work;
    Task*                     quadTask;
    s16                       quadIndex;

    work = _gActor160900CutsceneTask->work;
    for (quadIndex = 0; quadIndex < ARRAY_SIZE(work->lightQuads); quadIndex++) {
        quadTask = work->lightQuads[quadIndex];
        if (quadTask != NULL) {
            taskKill(quadTask);
            work->lightQuads[quadIndex] = NULL;
        }
    }
}
/// Replaces Aya's pending cutscene cue and restarts its step at zero.
///
/// Requires the running cutscene's published work. cueId is zero for none or
/// an ACTOR_160900_PLAYER_CUE_* value; it narrows to the stored unsigned halfword.
/// Posting retains the cue counter; the action that uses it initializes it.
static void _actor160900PostPlayerCue(s16 cueId)
{
    _Actor160900CutsceneWork* work;

    work                 = _gActor160900CutsceneTask->work;
    work->playerCue.id   = cueId;
    work->playerCue.step = 0;
}
/// Replaces Kyle's pending cutscene cue and restarts its step at zero.
///
/// Requires the running cutscene's published work. cueId is zero for none or
/// an ACTOR_160900_KYLE_CUE_* value; it narrows to the stored unsigned halfword.
/// Posting leaves the cue counter intact.
static void _actor160900PostKyleCue(s16 cueId)
{
    _Actor160900CutsceneWork* work;

    work               = _gActor160900CutsceneTask->work;
    work->kyleCue.id   = cueId;
    work->kyleCue.step = 0;
}

/// Selects the point list for the cutscene's repeating effect cue.
///
/// Requires the running cutscene's published work. cueId is zero for none or
/// 1..5 for a point list, stored as an unsigned halfword. Replaces the pending
/// id and clears its step, retaining its counter; the handler emits every
/// eighth frame until the script changes or clears the cue.
static void _actor160900PostEffectCue(s16 cueId)
{
    _Actor160900CutsceneWork* work;

    work                 = _gActor160900CutsceneTask->work;
    work->effectCue.id   = cueId;
    work->effectCue.step = 0;
}
/// Stops pending cutscene cues and scene playback when the skip script runs.
///
/// Requires the published cutscene work and a previously selected scene.
/// Clears the three cue ids, retaining their progress, requests CD cancellation
/// and restores display output. The following script command handles teardown.
static void _actor160900SkipCutscene(void)
{
    _Actor160900CutsceneWork* work;

    work               = _gActor160900CutsceneTask->work;
    work->playerCue.id = ACTOR_160900_CUE_NONE;
    work->kyleCue.id   = ACTOR_160900_CUE_NONE;
    work->effectCue.id = ACTOR_160900_CUE_NONE;
    cdCmdCancelScene();
    SetDispMask(true);
}

/// Stages the cutscene's selected audio session for deferred start.
///
/// The scene descriptor and playback buffers must survive the later command
/// commit and consumption. Called by the event script after scene selection.
static void _actor160900StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Enqueues playback of the cutscene's selected scene/audio session.
///
/// Requires prepared scene storage that survives command consumption and the
/// free command-ring capacity required by `cdCmdEnqueueScenePlayback`.
static void _actor160900EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Cancels cutscene playback and finishes its scene-streaming state.
///
/// Requires a previously selected scene. Keeps both finish operations: the CD
/// cancellation path finishes the scene first, then this callback repeats it,
/// restoring the saved random generators again. Task teardown is script-owned.
static void _actor160900FinishScenePlayback(void)
{
    cdCmdCancelScene();
    streamFinishScene();
}
