#include "actors/actor_136300.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/companion_load.h"
#include "gameplay/animation.h"
#include "gameplay/display.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_garage.h"
#include "../../shared/screen/screen_wave.h"

static s32 _gScreenWaveAmplitude;

static ScreenWaveCtx* _gScreenWaveContext;

static ScreenWaveOscillator _gScreenWaveColumnWaves[13];
static ScreenWaveOscillator _gScreenWaveRowWaves[32];

static AnimationSet _gPierceRestClip;
static AnimationSet _gKyleTalkStartClip;
static AnimationSet _gKyleTalkClip;
static AnimationSet _gKyleTalkEndClip;
static AnimationSet _gKyleExplainVirusStartClip;
static AnimationSet _gKyleExplainVirusClip;
static AnimationSet _gKyleKyleIdleClip;
static AnimationSet _gDryfieldNightGarageCompanionLetsGoClip;
static AnimationSet _gKyleLeaveDrivingToMeClip;
static AnimationSet _gPierceAnim12Clip;
static AnimationSet _gPierceExplainAnalysisClip;
static AnimationSet _gPierceParkingArrival12Clip;
static AnimationSet _gPierceAnim21Clip;
static AnimationSet _gDryfieldNightGarageCompanionShrugClip;
static AnimationSet _gKyleMentionMonstersClip;
static void         _actor136300GarageDepartureTask(Task* task);
static void         _actor136300StageExitCountdownTask(Task* task);

extern AnimationSet gAyaStandClip;
static AnimationSet _gDryfieldNightGaragePlayerAnim01Clip;
static AnimationSet _gDryfieldNightGaragePlayerAnim02Clip;
static AnimationSet _gAyaCallOutClip;
static AnimationSet _gAyaCallOutEndClip;
static AnimationSet _gAyaInsistClip;
static AnimationSet _gAyaAnim06Clip;
extern AnimationSet gAyaAnim07Clip;
static AnimationSet _gAyaAnim08Clip;
static AnimationSet _gAyaConverseClip;
extern AnimationSet gAyaOnMyWayClip;
static AnimationSet _gDryfieldNightGaragePlayerAskWhatDoesClip;
static AnimationSet _gDryfieldNightGaragePlayerAskWhatDoesEndClip;
static AnimationSet _gDryfieldNightGaragePlayerListenRetrovirusesClip;
extern AnimationSet gAyaTalkStartClip;
extern AnimationSet gAyaTalkClip;
extern AnimationSet gAyaTalkEndClip;
static AnimationSet _gAyaShockedClip;
static AnimationSet _gAyaBeforeAgreeingClip;
static AnimationSet _gAyaAskWhyClip;
static AnimationSet _gAyaAskWhyEndClip;
static AnimationSet _gAyaReasonStartClip;
static AnimationSet _gAyaReasonClip;
static AnimationSet _gAyaRealizeClip;
extern AnimationSet gAyaTurnPaleClip;
extern AnimationSet gAyaItsNothingClip;
/// Scene callback request that prepares mask-bit RGB16 decoding without starting a wave.
enum { ACTOR_136300_WAVE_PREPARE_DECODE = -2 };

static void _actor136300SetSceneEvent(s8 sceneEvent);
static void _actor136300StartGarageMovie(s32 alternateMovie);
static void _actor136300ControlScreenWave(s32 request);
static void _actor136300StartGarageDialogue(void);
static void _actor136300PlayCompanionDialogueAnimation(void);
static void _actor136300QueueStageExitCountdown(s32 countdownTicks);
static void _actor136300SelectSceneCaptions(s32 restoreDefaults);

/// Workless screen-wave task recipe, followed by the descriptor terminator.
///
/// Entry 0 borrows the package's persistent ramp context through its final draw.
/// It runs at priority `TASK_PRIORITY_CONTROL` and uses the package's singleton wave storage.
static TaskDesc _gActor136300ScreenWaveTaskTable[] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

#include "assets/rooms/dryfield/night_garage/flint_scene_clips.inc"

TaskDesc gActor136300TaskTable[ACTOR_136300_TASK_COUNT] = {
    // The empty placement list never spawns entry 0; preserve its stored address.
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, (TaskFunc)enemyDestroy, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _actor136300GarageDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _actor136300StageExitCountdownTask, { .value = 0 } },
};

/// Companion clip pointers installed at animation-bank indices 47-61.
///
/// Element i supplies clip `ANIMATION_BANK_BASE_SET_COUNT + i`. These fifteen
/// borrowed sets and their data must stay loaded throughout scene playback.
/// The copy request transfers 32 words: after these fifteen pointers it reads
/// all of the requests for clips 47, 48 and 49, then the source index and clip id
/// of the clip-50 request. Those seventeen extra words fill unused bank slots
/// and must never be played as sets. This is the original fixed-count overread;
/// the pointer array and the separately addressed requests retain their extents.
#include "assets/rooms/dryfield/night_garage/companion_bank.inc"

/// Companion clip 47 request with 10-frame blend and world collision disabled.
///
/// Opens the captioned garage scene.
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation47Request = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 48 request with reset and world collision disabled.
///
/// Garage cue sequences play clip 49 nine frames later.
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation48Request = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 49 request with reset and world collision disabled.
///
/// Garage cue sequences select this nine frames after clip 48.
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation49Request = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 50 request with 10-frame blend and world collision disabled.
///
/// Used after garage cue sequences and on scene skipping.
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation50Request = {
    .source               = { .index = 1 },
    .animationId          = 50,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 51 request with reset and world collision disabled.
///
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation51Request = {
    .source               = { .index = 1 },
    .animationId          = 51,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 52 request with reset and world collision disabled.
///
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation52Request = {
    .source               = { .index = 1 },
    .animationId          = 52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 53 request with 10-frame blend and world collision disabled.
///
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation53Request = {
    .source               = { .index = 1 },
    .animationId          = 53,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 54 request with reset and world collision disabled.
///
/// Selected for CAP variant one in the departure dialogue.
/// Dialogue playback rewrites this record's bank index before dispatch; scripts
/// resolve the bank on a local copy. Requires the installed companion clips.
static AnimationPlayRequest _gActor136300CompanionAnimation54Request = {
    .source               = { .index = 1 },
    .animationId          = 54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 55 request with 8-frame blend and world collision disabled.
///
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation55Request = {
    .source               = { .index = 1 },
    .animationId          = 55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 56 request with reset and world collision disabled.
///
/// Used by garage cues and by departure dialogue for other CAP variants.
/// Dialogue playback rewrites this record's bank index before dispatch; scripts
/// resolve the bank on a local copy. Requires the installed companion clips.
static AnimationPlayRequest _gActor136300CompanionAnimation56Request = {
    .source               = { .index = 1 },
    .animationId          = 56,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 57 request with reset and world collision disabled.
///
/// Garage cue sequences select clip 58 after a 26-frame interval.
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation57Request = {
    .source               = { .index = 1 },
    .animationId          = 57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 58 request with reset and world collision disabled.
///
/// Selected after the 26-frame interval following clip 57.
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation58Request = {
    .source               = { .index = 1 },
    .animationId          = 58,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 59 request with reset and world collision disabled.
///
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation59Request = {
    .source               = { .index = 1 },
    .animationId          = 59,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 60 request with reset and world collision disabled.
///
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation60Request = {
    .source               = { .index = 1 },
    .animationId          = 60,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 61 request with reset and world collision disabled.
///
/// Scripts resolve the companion bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300CompanionAnimation61Request = {
    .source               = { .index = 1 },
    .animationId          = 61,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip pointers installed at animation-bank indices 47-72.
///
/// Element i supplies clip `ANIMATION_BANK_BASE_SET_COUNT + i`. These 26
/// borrowed sets and their data must stay loaded throughout scene playback.
/// The copy request transfers 32 words: after these pointers it reads all five
/// words of the clip-47 request and the source index of the following clip-48
/// request. Those six extra words fill unused bank slots and must never be
/// played as sets. This is the original fixed-count overread; the pointer array
/// and the separately addressed requests retain their extents.
#include "assets/rooms/dryfield/night_garage/player_bank.inc"

/// Player clip 47 request with 10-frame blend and world collision disabled.
///
/// Opens the captioned garage scene.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation47Request = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 48 request with reset and world collision disabled.
///
/// Retained template with no direct playback use. Its source-index word is
/// also read as the 32nd raw word of the player bank-extension copy, filling
/// unused bank slot 78. Preserve this separately addressed 20-byte record.
static AnimationPlayRequest _gActor136300PlayerAnimation48Request = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 49 request with reset and world collision disabled.
///
/// Retained template for the installed player clip; no direct playback use
/// is present. The bank-index word remains the stored value 1.
static AnimationPlayRequest _gActor136300PlayerAnimation49Request = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 50 request with reset and world collision disabled.
///
/// The captioned scene selects clip 51 after 21 frames.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation50Request = {
    .source               = { .index = 1 },
    .animationId          = 50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 51 request with reset and world collision disabled.
///
/// Selected 21 frames after clip 50, before the next caption cue.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation51Request = {
    .source               = { .index = 1 },
    .animationId          = 51,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 52 request with 10-frame blend and world collision disabled.
///
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation52Request = {
    .source               = { .index = 1 },
    .animationId          = 52,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 53 request with reset and world collision disabled.
///
/// Retained template for the installed player clip; no direct playback use
/// is present. The bank-index word remains the stored value 1.
static AnimationPlayRequest _gActor136300PlayerAnimation53Request = {
    .source               = { .index = 1 },
    .animationId          = 53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 54 request with reset and world collision disabled.
///
/// Retained template for the installed player clip; no direct playback use
/// is present. The bank-index word remains the stored value 1.
static AnimationPlayRequest _gActor136300PlayerAnimation54Request = {
    .source               = { .index = 1 },
    .animationId          = 54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 55 request with 10-frame blend and world collision disabled.
///
/// Retained template for the installed player clip; no direct playback use
/// is present. The bank-index word remains the stored value 1.
static AnimationPlayRequest _gActor136300PlayerAnimation55Request = {
    .source               = { .index = 1 },
    .animationId          = 55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 56 request with 10-frame blend and world collision disabled.
///
/// Used in captioned scenes and in the departure dialogue.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation56Request = {
    .source               = { .index = 1 },
    .animationId          = 56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 57 request with reset and world collision disabled.
///
/// Selected near the end of the captioned garage scene.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation57Request = {
    .source               = { .index = 1 },
    .animationId          = 57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 58 request with reset and world collision disabled.
///
/// The captioned scene selects clip 59 after 25 frames.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation58Request = {
    .source               = { .index = 1 },
    .animationId          = 58,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 59 request with reset and world collision disabled.
///
/// Selected 25 frames after clip 58, before the next caption cue.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation59Request = {
    .source               = { .index = 1 },
    .animationId          = 59,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 60 request with reset and world collision disabled.
///
/// Selected at the caption cue following clip 59.
/// Scripts resolve the player bank on a local copy before synchronous
/// dispatch. Requires the installed package clips and loaded receiver bank.
static AnimationPlayRequest _gActor136300PlayerAnimation60Request = {
    .source               = { .index = 1 },
    .animationId          = 60,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 61 request with reset and world collision disabled.
///
/// The first-visit scene queues a 26-frame wait before selecting clip 62.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation61Request = {
    .source               = { .index = 1 },
    .animationId          = 61,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 62 request with reset and world collision disabled.
///
/// Follows clip 61's 26-frame wait, before the next CAP cue.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation62Request = {
    .source               = { .index = 1 },
    .animationId          = 62,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 63 request with reset and world collision disabled.
///
/// Selected at the CAP cue after clip 62.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation63Request = {
    .source               = { .index = 1 },
    .animationId          = 63,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 64 request with reset and world collision disabled.
///
/// Selected late in the post-movie dialogue at a CAP cue.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation64Request = {
    .source               = { .index = 1 },
    .animationId          = 64,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 65 request with reset and world collision disabled.
///
/// Selected after the late companion clip-58 sequence and its CAP cues.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation65Request = {
    .source               = { .index = 1 },
    .animationId          = 65,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 66 request with reset and world collision disabled.
///
/// Selected during the post-movie dialogue, before the next CAP cue.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation66Request = {
    .source               = { .index = 1 },
    .animationId          = 66,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 67 request with reset and world collision disabled.
///
/// Selected at the CAP cue after clip 66.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation67Request = {
    .source               = { .index = 1 },
    .animationId          = 67,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 68 request with reset and world collision disabled.
///
/// The first-visit scene queues a 29-frame wait before selecting clip 69.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation68Request = {
    .source               = { .index = 1 },
    .animationId          = 68,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 69 request with reset and world collision disabled.
///
/// Follows clip 68's 29-frame wait, before the next CAP cue.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation69Request = {
    .source               = { .index = 1 },
    .animationId          = 69,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 70 request with reset and world collision disabled.
///
/// Selected at the CAP cue after clip 69, before the movie transition.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation70Request = {
    .source               = { .index = 1 },
    .animationId          = 70,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 71 request with reset and world collision disabled.
///
/// Opens the post-movie player dialogue after staging has been restored.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation71Request = {
    .source               = { .index = 1 },
    .animationId          = 71,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 72 request with reset and world collision disabled.
///
/// The post-movie scene queues a 19-frame wait before selecting clip 56.
/// The interpreter copies the request and resolves the equipped-player bank
/// before synchronous dispatch. Installed clip data must remain loaded
/// through playback; the template itself is not modified.
static AnimationPlayRequest _gActor136300PlayerAnimation72Request = {
    .source               = { .index = 1 },
    .animationId          = 72,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the garage scene's player bank extension by copying 32 raw words.
///
/// Sent before scene clips play; borrows the source through dispatch. The count
/// includes request words beyond the clip-pointer array, as documented there.
/// Copied clip pointers remain borrowed while playback uses the loaded package.
static AnimationBankCopyRequest _gActor136300PlayerAnimationBankCopyRequest = { _gActor136300PlayerAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Installs the garage scene's companion bank extension by copying 32 raw words.
///
/// Sent before scene clips play; borrows the source through dispatch. The count
/// includes request words beyond the clip-pointer array, as documented there.
/// Copied clip pointers remain borrowed while playback uses the loaded package.
static AnimationBankCopyRequest _gActor136300CompanionAnimationBankCopyRequest = { _gActor136300CompanionAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Player placement at the start of the garage scene's scripted approach.
///
/// Room-coordinate XYZ (2000, 0, 1940), facing yaw zero. The placement message
/// reads both vectors synchronously before the walk-to request is sent.
static ActorTransform _gActor136300PlayerGarageApproachStart = {
    .pos = { 2000, 0, 1940, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Player destination and post-movie placement for the garage dialogue.
///
/// Room-coordinate XYZ (2000, 0, 3940), facing yaw zero. The initial walk
/// copies only XYZ; post-movie placement consumes all three Euler angles too.
static ActorTransform _gActor136300PlayerGarageDialoguePlacement = {
    .pos = { 2000, 0, 3940, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Companion placement during the garage scene, before and after the movie.
///
/// Room-coordinate XYZ (2000, 0, 5240), facing a negative half-turn. This
/// staged placement differs from the companion's ordinary garage placement;
/// the placement message consumes both vectors synchronously.
static ActorTransform _gActor136300CompanionGarageScenePlacement = {
    .pos = { 2000, 0, 5240, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

ActorTransform gActor136300CompanionGaragePlacement = {
    .pos = { 2230, 0, 5240, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Approach and arrival clips for the player's initial garage-scene walk.
///
/// Clip 19 supplies the unarmed walk; arriving selects installed scene clip
/// 47. The move-to message consumes these bank-relative IDs synchronously,
/// retaining their low halfwords. The installed clip data remains borrowed.
static GameActorMoveAnim _gActor136300PlayerGarageApproachAnimations = {
    .approachAnimId = 19,
    .arrivalAnimId  = 47,
};

/// Selectors carried by this package's garage scripts.
enum {
    ACTOR_136300_GARAGE_SCENE_CAP_SLOT        = 14,
    ACTOR_136300_GARAGE_AFTER_MOVIE_CAP_SLOT  = 15,
    ACTOR_136300_GARAGE_RESTORED_VIEW         = 2,
    ACTOR_136300_GARAGE_COMPLETED_SCENE_EVENT = 7,
    ACTOR_136300_GARAGE_MOVIE_HANDOFF_STATE   = 2,
    ACTOR_136300_GARAGE_MOVIE_PRIMARY         = 0,
    ACTOR_136300_GARAGE_MOVIE_DEPARTURE       = 1,
    ACTOR_136300_GARAGE_CAP_SELECT_SCENE      = 0,
    ACTOR_136300_GARAGE_CAP_RESTORE_DEFAULTS  = 1,
    ACTOR_136300_GARAGE_WEAPON_PLAYER         = 0,
    ACTOR_136300_GARAGE_SOUND_BEFORE_MOVIE    = 11,
};

EvsCommand gActor136300GarageCompanionSceneScript[149] = {
    // Select scene captions, install bank extensions and prepare the opening poses.
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300SelectSceneCaptions }, .operand1 = { .value = ACTOR_136300_GARAGE_CAP_SELECT_SCENE } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .message = { .value = ACTOR_136300_GARAGE_SCENE_CAP_SLOT } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor136300PlayerAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor136300CompanionAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation47Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation47Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT }, .operand1 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300ControlScreenWave }, .operand1 = { .value = ACTOR_136300_WAVE_PREPARE_DECODE } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, .operand0 = { .value = ACTOR_136300_GARAGE_WEAPON_PLAYER } },
    // Stage the actors and walk the player to the dialogue position.
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor136300PlayerGarageApproachStart } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, .operand3 = { .message = { .transform = &_gActor136300PlayerGarageDialoguePlacement } }, .operand4 = { .message = { .moveAnimation = &_gActor136300PlayerGarageApproachAnimations } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor136300CompanionGarageScenePlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 21 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation51Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation52Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation48Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 9 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation49Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation56Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation48Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 9 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation49Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation58Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 25 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation59Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation60Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation57Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, .operand0 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 26 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation58Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation59Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation56Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation60Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation61Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 26 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation62Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation63Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation48Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 9 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation49Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation68Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 29 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation69Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation70Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    // Transfer presentation to the primary garage movie.
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = ACTOR_136300_GARAGE_SOUND_BEFORE_MOVIE } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300QueueStageExitCountdown }, .operand1 = { .value = 20 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 15 } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT }, .operand1 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300StartGarageMovie }, .operand1 = { .value = ACTOR_136300_GARAGE_MOVIE_PRIMARY } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = SetDispMask }, .operand1 = { .value = 1 } },
    // Reinstall the scene banks and staging for the post-movie dialogue.
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor136300PlayerAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor136300CompanionAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor136300PlayerGarageDialoguePlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gActor136300CompanionGarageScenePlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .message = { .value = ACTOR_136300_GARAGE_AFTER_MOVIE_CAP_SLOT } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation71Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation53Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation72Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 19 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation56Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation48Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 9 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation49Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation66Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation67Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation61Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation51Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 16 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation52Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation53Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation64Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation57Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 26 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation58Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation65Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation56Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation53Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation55Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation48Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 9 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation49Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation57Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation48Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 9 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation49Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation50Request } },
    // Finish the scene and restore the ordinary garage presentation.
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT }, .operand1 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, .operand0 = { .value = 60 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackS8 = _actor136300SetSceneEvent }, .operand1 = { .value = ACTOR_136300_GARAGE_COMPLETED_SCENE_EVENT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = ACTOR_136300_GARAGE_RESTORED_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, .operand0 = { .value = ACTOR_136300_GARAGE_WEAPON_PLAYER } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &gActor136300CompanionGaragePlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = SetDispMask }, .operand1 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300SelectSceneCaptions }, .operand1 = { .value = ACTOR_136300_GARAGE_CAP_RESTORE_DEFAULTS } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor136300GarageCompanionSceneSkipScript[16] = {
    { .opcode = EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, .operand0 = { .value = 15 } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = ACTOR_136300_GARAGE_RESTORED_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackS8 = _actor136300SetSceneEvent }, .operand1 = { .value = ACTOR_136300_GARAGE_COMPLETED_SCENE_EVENT } },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, .operand0 = { .value = ACTOR_136300_GARAGE_WEAPON_PLAYER } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = SetDispMask }, .operand1 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &gActor136300CompanionGaragePlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300CompanionAnimation50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .message = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300SelectSceneCaptions }, .operand1 = { .value = ACTOR_136300_GARAGE_CAP_RESTORE_DEFAULTS } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Retained garage cleanup stream that releases scene presentation and player control.
///
/// Fades out and back over the default ramp, aborts CAP/secondary effects,
/// ends the player's scripted mode and restores default captions. Ends after
/// eight commands; no entry point currently starts this stream.
static EvsCommand _gActor136300GarageCleanupScript[] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300SelectSceneCaptions }, .operand1 = { .value = ACTOR_136300_GARAGE_CAP_RESTORE_DEFAULTS } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Garage departure dialogue synchronized with player and companion animation cues.
///
/// Installs both borrowed bank extensions, selects the next progression-based
/// CAP slot and plays the responses. The ten-command stream ends after a
/// three-frame wait; its caller then tests the retained CAP choice. Requires
/// live actors and CAP resources, with this package loaded through playback.
static EvsCommand _gActor136300GarageDepartureDialogueScript[] = {
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor136300PlayerAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gActor136300CompanionAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _actor136300StartGarageDialogue } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gActor136300PlayerAnimation56Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _actor136300PlayCompanionDialogueAnimation } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 3 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Retained six-byte record currently declared as scene/audio key (3, 65, 11).
///
/// No consumer establishes the key interpretation or the intended scene.
static EvsSceneKey D_actor_136300_8013C6B8 = { 3, 65, 11 };

/// Garage departure movie launch and event-state handoff to the reload task.
///
/// Fades to black over 30 updates and launches the alternate garage movie.
/// A zero-frame wait yields one update before setting event state 2, allowing
/// the departure task to advance. After a further 30-frame wait it releases
/// player scripting and ends. Both actor and garage packages must remain loaded.
static EvsCommand _gActor136300GarageDepartureMovieScript[] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT }, .operand1 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300StartGarageMovie }, .operand1 = { .value = ACTOR_136300_GARAGE_MOVIE_DEPARTURE } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, .operand0 = { .value = ACTOR_136300_GARAGE_MOVIE_HANDOFF_STATE } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Retained garage stream cleanup that also finishes the screen-wave ramp.
///
/// Fades out, ends scene streaming, publishes event state 2, queues a 30-frame wait,
/// releases scene presentation and requests the wave's finished phase. Fades
/// back and releases player scripting before ending. No entry point currently
/// starts these eleven commands; no caption-resource reset is performed here.
static EvsCommand _gActor136300GarageStreamCleanupScript[] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, .operand0 = { .value = ACTOR_136300_GARAGE_MOVIE_HANDOFF_STATE } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _actor136300ControlScreenWave }, .operand1 = { .value = SCREEN_WAVE_RAMP_FINISHED } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .message = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
static ScreenWaveCtx* _gScreenWaveContext = NULL;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
static ScreenWaveOscillator _gScreenWaveColumnWaves[13] = { 0 };

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
static ScreenWaveOscillator _gScreenWaveRowWaves[32] = { 0 };

/// Persistent ramp state borrowed by the garage scene's screen-wave task.
///
/// The scene seeds a 100- or five-frame rise peaking at strength 256, then can
/// request falling or finished. The task initializes frame/state on its first
/// tick. Zero texture modulation keeps the captured frame's own colours.
/// Keep this actor package loaded through the final draw; only one wave may
/// borrow the context at a time, and it is reused for subsequent starts.
static ScreenWaveCtx _gActor136300ScreenWaveSpawnContext = { 0 };

#include "../../shared/screen/screen_wave.inc.c"

/// Runs the garage departure dialogue, movie and handoff to Mine Mesa.
///
/// Begin in state 0 with garage/actor overlays and scripts loaded. CAP choice 2
/// cancels after dialogue and restores HUD/player control. Other choices start
/// the movie, wait for event state to leave 1 and then wait two task updates
/// before changing story/save state, consuming the truck key and requesting the
/// Mine/Shelter reload. Completion and cancellation kill this workless task.
static void _actor136300GarageDepartureTask(Task* task)
{
    enum {
        ACTOR_136300_DEPART_START              = 0,
        ACTOR_136300_DEPART_WAIT_DIALOGUE      = 1,
        ACTOR_136300_DEPART_CHOOSE             = 2,
        ACTOR_136300_DEPART_WAIT_MOVIE         = 3,
        ACTOR_136300_DEPART_DELAY_FIRST        = 4,
        ACTOR_136300_DEPART_DELAY_SECOND       = 5,
        ACTOR_136300_DEPART_RELOAD             = 6,
        ACTOR_136300_DEPART_CANCEL_KEY         = 2,
        ACTOR_136300_DEPART_STORY_CHAPTER      = 4,
        ACTOR_136300_DEPART_DIALOGUE_INDEX     = 15,
        ACTOR_136300_DEPART_COMPANION_SCHEDULE = 4,
        ACTOR_136300_MESA_SCENE_EVENT          = 9,
    };

    switch (task->state) {
        case ACTOR_136300_DEPART_START:
            gGameSession->hideHud = 1;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            evsStartScript(_gActor136300GarageDepartureDialogueScript, EVENT_SCRIPT_HUD_KEEP);
            task->state += 1;
            return;
        case ACTOR_136300_DEPART_WAIT_DIALOGUE:
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            return;
        case ACTOR_136300_DEPART_CHOOSE:
            if (capGetVariantKey() == ACTOR_136300_DEPART_CANCEL_KEY) {
                gGameSession->hideHud = 0;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
                return;
            }
            evsStartScript(_gActor136300GarageDepartureMovieScript, EVENT_SCRIPT_HUD_KEEP);
            task->state += 1;
            return;
        case ACTOR_136300_DEPART_WAIT_MOVIE:
            if (gGameSession->eventState != 1) {
                task->state += 1;
            }
            return;
        case ACTOR_136300_DEPART_DELAY_FIRST:
        case ACTOR_136300_DEPART_DELAY_SECOND:
            task->state += 1;
            return;
        case ACTOR_136300_DEPART_RELOAD:
            SetDispMask(1);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gameFlagSetNibble(EVENT_FLAG_STORY_CHAPTER, ACTOR_136300_DEPART_STORY_CHAPTER);
            gameFlagSetNibble(GAME_FLAG_097, 0);
            gameFlagSetNibble(GAME_FLAG_098, 1);
            gameFlagSetNibble(GAME_FLAG_09A, 1);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, ACTOR_136300_DEPART_DIALOGUE_INDEX);
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, ACTOR_136300_DEPART_COMPANION_SCHEDULE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = ACTOR_136300_MESA_SCENE_EVENT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_MINE_SHELTER;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_MINE_MESA;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = 1;
            gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_TRUCK_KEY);
            gDisplayState.spriteVariant = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            return;
    }
}

/// Rumbles the controller and requests stage-mode exit after a signed countdown.
///
/// Start at state 0 with spawnArg1.value in whole task updates; the script uses
/// 20. The first update also decrements, and exit occurs on an update entering
/// with a negative value. Owns no work block. After killing this workless task,
/// the binary reloads and decrements its spawn word; preserve that access order.
static void _actor136300StageExitCountdownTask(Task* task)
{
    s32 countdownTicks;

    if (task->state == 0) {
        padScriptSpawn(gPadScriptTransitionCommands, gPadScriptTransitionSegments);
        task->state += 1;
    }
    // Test the entry value; zero takes one more decrement before requesting exit.
    countdownTicks = task->spawnArg1.value;
    if (countdownTicks < 0) {
        stageRequestModeTaskExit();
        taskKill(task);
        countdownTicks = task->spawnArg1.value;
    }
    countdownTicks        = countdownTicks - 1;
    task->spawnArg1.value = countdownTicks;
}

/// Sets the live save's scene-event selector for stage music.
///
/// The event script supplies event 7 on both normal completion and skipping.
/// This stores the signed byte unchanged; it does not start playback itself.
static void _actor136300SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

/// Starts the loaded garage overlay's movie launcher from this scene.
///
/// Zero selects the primary movie and nonzero the alternate; the garage chooses
/// the disc-specific stream. The launcher hands presentation to movie playback.
/// The spawn result is ignored and no task handle is retained.
static void _actor136300StartGarageMovie(s32 alternateMovie)
{
    taskSpawnFromTable(gDryfieldNightGarageMovieTasks, 0, alternateMovie, 0);
}

/// Prepares mask-bit image decoding, starts a screen wave, or changes its ramp phase.
///
/// Request -2 only selects mask-bit RGB16 decoding. Zero starts a 100-frame rise;
/// other negative requests start a five-frame rise, both peaking at strength 256.
/// Positive requests narrow to the context's signed-halfword phase (1 falling,
/// 2 finished). Starting requires no other active wave and loaded wave resources;
/// the package's static context remains borrowed through the wave's final tick.
/// Both span-test arms retain their complete span/scale store pairs for matching.
static void _actor136300ControlScreenWave(s32 request)
{
    enum {
        ACTOR_136300_WAVE_SLOW_RISE        = 0,
        ACTOR_136300_WAVE_SLOW_RISE_FRAMES = 100,
        ACTOR_136300_WAVE_FAST_RISE_FRAMES = 5,
        ACTOR_136300_WAVE_PEAK_STRENGTH    = 256,
    };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (request <= 0) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        // Prepare RGB16 mask-bit decoding before optionally starting distortion.
        if (request != ACTOR_136300_WAVE_PREPARE_DECODE) {
            if (request == ACTOR_136300_WAVE_SLOW_RISE) {
                _gActor136300ScreenWaveSpawnContext.span  = ACTOR_136300_WAVE_SLOW_RISE_FRAMES;
                _gActor136300ScreenWaveSpawnContext.scale = ACTOR_136300_WAVE_PEAK_STRENGTH;
            } else {
                _gActor136300ScreenWaveSpawnContext.span  = ACTOR_136300_WAVE_FAST_RISE_FRAMES;
                _gActor136300ScreenWaveSpawnContext.scale = ACTOR_136300_WAVE_PEAK_STRENGTH;
            }
            taskSpawnFromTable(_gActor136300ScreenWaveTaskTable, 0, 0, &_gActor136300ScreenWaveSpawnContext);
        }
    } else {
        _gActor136300ScreenWaveSpawnContext.state = request;
    }
}

/// Selects the next garage dialogue slot and advances its saved progression to two.
///
/// The flag supplies a nonnegative nibble: slots 16, 17 and then 18 in normal
/// progression, using current-display CAP playback and variant key zero. Values
/// above two remain unchanged and still select slot 16 plus the stored nibble.
/// Requires this scene's live CAP resources. Progress advances even if CAP fails
/// to start or is busy; the playback result is discarded.
static void _actor136300StartGarageDialogue(void)
{
    enum { ACTOR_136300_GARAGE_DIALOGUE_FIRST_SLOT = 16,
           ACTOR_136300_GARAGE_DIALOGUE_LAST_INDEX = 2 };
    s32 dialogueIndex;

    dialogueIndex = gameFlagGetNibble(GAME_FLAG_072);
    capStartSequenceSlot((s16)(dialogueIndex + ACTOR_136300_GARAGE_DIALOGUE_FIRST_SLOT), 0, 0);
    if (dialogueIndex < ACTOR_136300_GARAGE_DIALOGUE_LAST_INDEX) {
        gameFlagSetNibble(GAME_FLAG_072, dialogueIndex + 1);
    }
}

/// Plays the companion's garage-dialogue response selected by the live CAP variant.
///
/// Variant key one selects clip 54; every other key selects clip 56. Requires a
/// live companion and its loaded animation bank. Updates the selected static
/// request's bank index for the current companion, then dispatches synchronously;
/// the request pointer is borrowed only through dispatch.
static void _actor136300PlayCompanionDialogueAnimation(void)
{
    enum { ACTOR_136300_DIALOGUE_VARIANT_FIRST = 1 };
    AnimationPlayRequest* request;

    if (capGetVariantKey() == ACTOR_136300_DIALOGUE_VARIANT_FIRST) {
        request = &_gActor136300CompanionAnimation54Request;
    } else {
        request = &_gActor136300CompanionAnimation56Request;
    }
    companionWriteAnimationBankIndex(&request->source.index);
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, request, 0);
}

/// Queues the garage scene's countdown in the gray captured-frame display mode.
///
/// countdownTicks is the signed whole-update seed; the script supplies 20.
/// The mode controller owns the spawned workless task. A pending display mode
/// causes the request to be ignored; this callback discards that result.
static void _actor136300QueueStageExitCountdown(s32 countdownTicks)
{
    displayQueueModeTask(&gActor136300TaskTable[ACTOR_136300_TASK_STAGE_EXIT_COUNTDOWN], countdownTicks, 0, STAGE_ENTRY_GRAY_CAPTURE);
}

/// Selects this scene's CAP resource and texture page, or restores CAP defaults.
///
/// Zero selects data-resource ordinal 1 and VRAM origin (384, 256) in 16-bit
/// pixels; any nonzero value resets CAP to ordinal 0 and origin (384, 0).
/// The selected bundle and its textures must already be loaded and remain live
/// while CAP uses them. Selection clears the current file even if no resource
/// matches. The event scripts pass 0 on entry and 1 on cleanup.
static void _actor136300SelectSceneCaptions(s32 restoreDefaults)
{
    enum {
        ACTOR_136300_SCENE_CAP_FILE_ORDINAL   = 1,
        ACTOR_136300_SCENE_CAP_TEXTURE_VRAM_X = 384,
        ACTOR_136300_SCENE_CAP_TEXTURE_VRAM_Y = 256,
    };
    if (restoreDefaults == 0) {
        gCapFile = NULL;
        capSelectLoadedFile(ACTOR_136300_SCENE_CAP_FILE_ORDINAL);
        capSetTexturePage(ACTOR_136300_SCENE_CAP_TEXTURE_VRAM_X, ACTOR_136300_SCENE_CAP_TEXTURE_VRAM_Y);
        return;
    }
    capReset();
}
