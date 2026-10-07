#ifndef GAMEPLAY_SCENE_COMBAT_H
#define GAMEPLAY_SCENE_COMBAT_H

#include "gameplay/world_state.h"

#include "main/task_types.h"

/// Scene-wide battle state, actor controls, rewards and enemy-group signals.
///
/// The gameplay overlay owns this storage and resets it on scene loading.
/// Room, actor and PE overlays share it while gameplay is loaded; enemy tasks
/// and scripted encounters hold battle references until they die or retire.
extern SceneCombatState gSceneCombatState;

/// Engages an idle scene battle, retaining every other combat field.
///
/// Only the idle phase changes to engaged; engaged, finished and resumed phases
/// are retained. This does not acquire a battle reference or reset rewards.
/// `unusedArg` is ignored and retained for event-script callback compatibility.
void sceneEngageBattle(s32 unusedArg);

/// One-based selectors accepted by `sceneLatchActionSignal`, rather than masks.
enum {
    SCENE_COMBAT_ACTION_SIGNAL_NONE               = 0,
    SCENE_COMBAT_ACTION_SIGNAL_NOISE              = 1,
    SCENE_COMBAT_ACTION_SIGNAL_PE_ACTIVE          = 2,
    SCENE_COMBAT_ACTION_SIGNAL_PE_CAST_OTHER      = 3,
    SCENE_COMBAT_ACTION_SIGNAL_PE_CAST_300_TO_600 = 4,
    SCENE_COMBAT_ACTION_SIGNAL_FOOTSTEP           = 5
};

/// Latches one scene action stimulus until the player update clears it.
///
/// Pass a `SCENE_COMBAT_ACTION_SIGNAL_*` selector; zero leaves the byte intact.
/// Nonzero selectors OR bit (selector - 1) into the stored eight-bit flags.
/// The implementation does not validate the shift range or retain higher bits.
void sceneLatchActionSignal(s32 actionSignal);

/// Replaces the scene's enemy stimulus byte with the low eight bits of `alertClass`.
///
/// Zero clears the stimulus; callers use classes 1 and 2. Enemy kinds react to
/// either nonzero class or a specific class. A universal distinction between
/// the classes is unproven. This replaces the previous value rather than ORing
/// it, and does not engage a battle or acquire a battle reference.
void sceneSetEnemyAlert(s32 alertClass);

/// Acquires one scene battle hold for an enemy or scripted encounter.
///
/// `unusedArg` is ignored and retained for event-script callback compatibility.
/// The 16-bit outstanding-reference count increments without overflow checks;
/// callers must balance holds with the battle-release APIs and avoid overflow.
/// Acquiring a hold does not change the battle phase or pending rewards.
void sceneAcquireBattleRef(s32 unusedArg);

/// Releases one battle hold while preserving accumulated rewards.
///
/// A zero count is a no-op. The last release sets the finished phase, clears
/// action/enemy stimuli and starts the 60-frame end delay. Unless area-music
/// changes are suppressed, it queues a fade of all sequences over 180 audio
/// updates; queue admission failure is ignored. It does not destroy a task.
/// `unusedTask` and `unusedArg` are ignored; their argument slots are retained.
void sceneReleaseBattleRef(Task* unusedTask, s32 unusedArg);

/// Releases one battle hold and credits the enemy's EXP, BP and MP rewards.
///
/// Uses `sceneReleaseBattleRef`'s final-release transition and music policy.
/// Every nonzero-count release adds rewards, including the last; a zero count
/// touches neither the task nor the totals. For a nonzero count, `enemyTask`
/// must be live with an `Enemy` in `spawnArg2.pointer`. A NULL parameter record
/// adds nothing. Totals use signed 32-bit addition without clamping. Ownership
/// and task lifetime are unchanged. `unusedArg` is ignored, including actor IDs
/// supplied by callers; rewards come exclusively from the enemy parameters.
void sceneReleaseBattleRefWithRewards(Task* enemyTask, s32 unusedArg);

/// Releases one battle hold, clearing accumulated rewards only on the last release.
///
/// Uses `sceneReleaseBattleRef`'s final-release transition and music policy.
/// Non-final releases preserve the totals; a zero count is a no-op. Both
/// `unusedTask` and `unusedArg` are ignored; their argument slots are retained.
void sceneReleaseBattleRefAndClearRewards(Task* unusedTask, s32 unusedArg);

#endif // GAMEPLAY_SCENE_COMBAT_H
