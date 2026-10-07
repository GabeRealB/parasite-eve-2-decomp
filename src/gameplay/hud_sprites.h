#ifndef GAMEPLAY_PRIVATE_HUD_SPRITES_H
#define GAMEPLAY_PRIVATE_HUD_SPRITES_H

#include "types.h"

#include "hud.h"

#include "main/task_types.h"

/// Counts down the death/ending delay, starts its screen fade and advances to the wait phase.
///
/// State 2 decrements signed-halfword killCountdown once per task update. At
/// zero or below it becomes the death-sound completion latch, PE effect state
/// becomes idle and spawnArg1.value is reset for the next phase. Except for
/// preserve-display restarts, queues a subtractive fade using deathFadeFrames;
/// the fade task replaces a nonpositive duration with 32 frames and holds the
/// covered screen until the area changes. Fade allocation failure is ignored.
void playClockStartDeathFade(Task* task);

/// Waits 64 task updates for death presentation, then advances to session restart.
///
/// State 3 starts with spawnArg1.value zero; it counts updates independently
/// of the fade's duration. Preserve-display restarts suppress drawing at the end.
/// killCountdown is the delayed death-sound completion latch during this phase.
void playClockWaitDeathFade(Task* task);

/// Advances delayed player or companion death audio once the CD queue is idle.
///
/// `completed` is a writable signed-halfword latch: zero permits countdown
/// updates, any nonzero value blocks them. A 127 countdown holds playback and
/// sets the latch to 1; otherwise playback is attempted after decrementing below
/// zero and the latch becomes 1 even when the companion has no death script.
/// Preserve-display and ending restarts leave both counters unchanged.
void playClockAdvanceDeathSound(s16* completed);

/// Borrows the active table of learned Parasite Energy levels, without modifiers.
///
/// The twelve spells are fire, wind, water and earth groups of three; levels
/// are bytes 0..3, with zero meaning unlearned. This view establishes those
/// twelve entries; the role of further declared storage is unproven.
/// Shooting-gallery resource variant 4 selects the training table; otherwise
/// the live save supplies it. No allocation or copy occurs. The read-only view
/// remains valid while its owning save/gameplay storage is live; the owner may
/// change the levels between calls.
const u8* attachmentGetLearnedLevels(void);

/// Initializes displayed HP/MP, clears the radar range and resets temporary ability effects.
///
/// Called on play-task initialization with a writable HudState. Also releases
/// the ability swap lock, clears the HUD input delay and battle-reset request,
/// and suppresses controller-disconnect pauses. Other HUD members are retained.
void hudReset(HudState* hud);

/// Queues the Healing sound bank for its effective level, reusing an already loaded bank.
void attachmentEnqueueHealingSoundLoad(void);

/// Inert attachment sound-load hook that always returns zero.
///
/// Item-use callers supply a PE sound file index, which is ignored. The binary
/// evaluates the battle hold/end-delay gate but queues no load or playback.
s32 attachmentSoundLoadStub(s32 unusedFileIndex);

/// Item-backed slots accepted by the pending attachment queue.
enum {
    ATTACHMENT_INDEX_FLARE        = 15,
    ATTACHMENT_INDEX_PEPPER_SPRAY = 16,
    ATTACHMENT_INDEX_COMBAT_LIGHT = 17
};

/// Replaces the pending attachment slot unless the event lock is set.
///
/// `abilityIndex` is a spell/item slot 0..17; zero clears the queue. Stored as
/// a signed byte without validation. The HUD consumes a nonzero slot when
/// input permits, and scripted actor mode or event locking can discard it.
void attachmentQueueIndex(s32 abilityIndex);

/// Cancels the active attachment and releases wheel-controlled actor suspension.
///
/// Armed/casting effects are marked cancelled; earlier modes retain `effectPhase`.
/// Clears the pending slot and preview sound state, restores actor updates and
/// queues a seek to the current view. The caller must leave CD-ring capacity.
void attachmentCancel(void);

/// Applies the live session's mapped area camera and resets view projection.
///
/// Requires populated stage/area camera directories and a nonzero mapped index
/// within the loaded camera array. Copies rotation and origin into separate
/// view nodes, clears the outer offset and invalidates their caches.
void viewApplyCurrentCamera(void);

/// Returns 1 while an engaged battle has holds or the post-battle delay is nonzero.
///
/// The end delay keeps this gate active regardless of the battle phase.
s32 sceneIsBattleActive(void);

/// Empty hook called when the view gate differs from the live save's view.
void viewChangeStub(void);

/// Delays HUD menu input and ability-category switching for five eligible HUD updates.
///
/// Replaces the shared delay when the menu closes. Updates that suppress the
/// HUD or run an event do not consume it.
void hudDelayInputAfterMenu(void);

void Gp_DrawHudSprites(HudState* hud);

/// HP readout layouts and the maximum-HP sentinel that hides the amount.
enum {
    HUD_HP_READOUT_COMPANION  = 0,
    HUD_HP_READOUT_ENEMY      = 1,
    HUD_HP_READOUT_HIDDEN_MAX = -1
};

/// Draws an HP label, current amount, fill bar and frame for a companion or target.
///
/// X/Y are screen-centered pixels; drawing accounts for the VRAM Y offset.
/// Layout 0 uses a short green bar; other values use the wider red enemy bar.
/// Negative hpMax shows ???? and omits the bar; zero draws a full bar.
/// Negative hp is clamped to zero. Supply HP values whose hp * bar width fits
/// s32. Requires loaded UI textures/fonts and space in the current GPU arena;
/// packets survive until GPU completion. The debug hide-HUD flag skips drawing.
void hudDrawHpReadout(s32 x, s32 y, s32 hp, s32 hpMax, s32 layout);

/// Refreshes tracked enemy body anchors in the player's coordinate frame.
///
/// A missing player slot leaves stored positions unchanged. Otherwise requires
/// its first model coordinate and each eligible enemy coordinate to have live,
/// current work matrices composed into the same frame, plus an acyclic target
/// list of embedded `Enemy` nodes. Entries marked only `WORLD_TARGET_NOT_LOCKABLE`
/// by the scan mask are skipped; `WORLD_TARGET_KEEP_SCANNED` permits them.
/// No coordinate cache is refreshed.
/// Local XYZ narrows to signed halfwords, each GTE rotation saturates to signed
/// halfwords, and translation/subtraction narrows after each step. The player's
/// transposed rotation inverts an orthonormal frame (ONE-scaled coefficients).
/// Results are widened into `Enemy::playerRelPos` in game-coordinate units.
/// Requires 72 free scratch-stack bytes, released before return; changes GTE state.
void worldTargetUpdatePlayerRelativePositions(void);

s32 func_800A7550(void);

void func_800A7824(s32 arg0, s32 arg1, s32 arg2);

/// Draws the player's locked enemy's HP, retaining its readout placement between frames.
///
/// Draws only if the player and its work exist and the lock points into the
/// live target list at a lockable node. Otherwise the writable readout is
/// retained. Requires 28 free scratch-stack bytes when a target is drawn.
void hudDrawLockedTargetHp(HudTargetHpReadout* readout);

/// Draws an item-grant notice in the UI object borrowed from spawnArg2.pointer.
///
/// Spawn argument 1 value 2 selects Bonus item!! and adjusts the panel once at
/// task state 0; other values select Item obtained!. The owning UI task keeps
/// the object live and handles its lifetime and panel drawing.
void itemPickupNoticeTask(Task* task);

/// Draws the Item title and publishes confirm when an active panel is acknowledged.
///
/// Borrows the live UI object in spawnArg2.pointer, clearing its result each
/// update. Both confirm and cancel buttons publish USER_INTERFACE_RESULT_CONFIRM.
/// The owning UI task handles the object's lifetime.
void itemPickupTitleTask(Task* task);

/// Returns a spell's learned level, raised to at least one and modified by Berserker.
///
/// abilityIndex must be nonnegative: 0..11 selects the active learned-level
/// table, while item slots 12..17 return one. Berserker adds one only below
/// level three; it does not unlock an unlearned spell. No bounds check rejects
/// negative indices, and stored values above three are retained.
s32 attachmentGetEffectiveLevel(s32 abilityIndex);

#endif // GAMEPLAY_PRIVATE_HUD_SPRITES_H
