#ifndef INCLUDE_ACTORS_ACTOR_503500_H
#define INCLUDE_ACTORS_ACTOR_503500_H

#include "gameplay/evs.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Descriptor indices for the R48 drift emitters and fade from black.
enum {
    ACTOR_503500_SCENE_TASK_DRIFT_EMITTER   = 0,
    ACTOR_503500_SCENE_TASK_FADE_FROM_BLACK = 1,
    ACTOR_503500_SCENE_TASK_COUNT           = 2,
};

/// Extent of the aligned enemy descriptor, parameter and live-slot tables.
enum { ACTOR_503500_SLOT_COUNT = 17 };

/// Bodyless scene launcher spawned when Shelter R48 accepts a staff card.
///
/// Priority is 192; both spawn arguments are ignored. Requires the room and
/// actor overlays loaded and event control already held. Its callback starts
/// the skippable card scene and kills the launcher; the event task outlives it.
/// The descriptor is borrowed synchronously when spawning.
extern TaskDesc gActor503500StaffCardSceneTaskDesc;

/// Skippable boss intro that Shelter R48 starts on room initialization.
///
/// Selects the intro stream, holds and presents the boss, then places both
/// actors for battle and resumes control. Use `gActor503500BossIntroSkipScript`
/// as the initial skip target with `EVENT_SCRIPT_HUD_HIDE_RESTORE`. The event
/// runner borrows all 56 commands and their referenced data and callbacks;
/// keep the actor overlay and Shelter R48 scene resources loaded until it ends.
extern EvsCommand gActor503500BossIntroScript[56];

/// Skip path that establishes the boss intro's playable battle state.
///
/// Fades across placement, installs player scene clips, resumes the boss,
/// stops scene effects and vibration, then starts music and battle control.
/// Passed as the initial skip target for `gActor503500BossIntroScript`; all
/// 29 commands and their resources are borrowed for the event's lifetime.
extern EvsCommand gActor503500BossIntroSkipScript[29];

/// Boss defeat scene started by Shelter R48's second boss-event notification.
///
/// Selects the defeat stream, collapses the boss, moves and collapses the
/// scripted tentacle, then releases both and enables staff-card use. The room
/// supplies a separate skip path when starting it with hidden/restored HUD.
/// All 61 commands, callbacks and data are borrowed; keep both overlays and
/// their scene resources loaded through completion, including vibration tasks.
extern EvsCommand gActor503500BossDefeatScript[61];

/// Boss phase-transition interruption started by Shelter R48's first boss event.
///
/// Installs player scene clips, saves the player pose and starts the boss's
/// root-saving transition. CAP command 5 supplies two synchronization cues;
/// afterward both roots and the room view are restored and scripted player
/// control ends. All 17 commands and referenced records/callbacks are borrowed.
/// Keep both overlays and their scene resources loaded until the script ends;
/// the room starts this with `EVENT_SCRIPT_HUD_HIDE_RESTORE` and no skip target.
extern EvsCommand gActor503500BossPhaseTransitionScript[17];

/// Scene-effect spawn recipes borrowed synchronously by the actor scripts and R48.
///
/// Drift emitter 0 owns a coordinate body; spawn argument 1 selects room position
/// 0..3 and argument 2 is unused. Fade 1 has no body; argument 1 is its signed
/// hold countdown in actor-running ticks and argument 2 is unused. Both run at
/// priority 192. Keep the actor overlay and R48 effect resources loaded.
extern TaskDesc gActor503500SceneEffectTaskDescs[ACTOR_503500_SCENE_TASK_COUNT];

/// Enemy spawn recipes for the boss at slot 0 and attached parts at slots 1..16.
///
/// The index and spawn argument 1 must select the same slot; enemy spawning
/// supplies the live Enemy in argument 2. Priorities are 96. Slots 4..9 and 12
/// own coordinate bodies; all others own models. Chain and arm recipes skip
/// automatic primitive-buffer allocation. Descriptors are borrowed only during
/// spawn; callbacks and model data require the actor overlay to remain loaded.
/// R48 resource kind 35 exports this table, aligned with the package's parameters.
extern TaskDesc gActor503500EnemyTaskDescs[ACTOR_503500_SLOT_COUNT];

extern TmdSource gActor503500Model14DA0;

extern TmdSource gActor503500Model15820;

/// Dispatches an intro slab slider through initialization, update and exit.
///
/// Requires a live TMD model, its enemy in `spawnArg2.pointer`, and `state`
/// 0..2. Only running scene actor control dispatches any state. Initialization
/// owns allocated work; commands select either 360-update path or stationary
/// shake. Copies the handler table before dispatch; exit may destroy the task.
void actor503500SliderTask(Task* task);

/// Clears the borrowed fade-from-black task handle when Shelter R48 initializes.
///
/// Requires this actor overlay to be loaded. Does not stop or release a task;
/// the room calls this before starting its entry script. `unused` is ignored.
void actor503500ClearFadeFromBlackHandle(s32 unused);

/// Skip path establishing the boss defeat's staff-card-ready room state.
///
/// Twenty-eight commands including END. Fades, restores player placement/clips,
/// releases battle and both scene actors, cancels effects, hides both slabs and
/// enables staff-card use. Pair with `gActor503500BossDefeatScript`; keep the actor
/// and R48 overlays, callbacks and borrowed clip resources loaded through
/// completion. Player control resumes with the default scripted-end mode.
extern EvsCommand gActor503500BossDefeatSkipScript[28];

/// Scripted tentacle used by Shelter R48 resource slot 45.
///
/// Creates the nineteen-part actor_361100 model with explicit primitive-buffer
/// control, then handles placement, animation, translation and a burning collapse.
extern TaskDesc gActor503500TentacleTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_503500_H
