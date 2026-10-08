#ifndef MAIN_STAGE_H
#define MAIN_STAGE_H

#include "types.h"

#include "main/stage_types.h"

extern StageMusicParams gStageMusicParams;

/// 0 while a music-load task runs, 0xFF once it has finished or given up; code
/// that must wait for the music checks it.
extern u8 gStageMusicLoadState;

/// The music-table entry a scene selects. A load task spawned with argument 2
/// plays it instead of the area's own entry; rooms and actors set it.
extern u8 gStageSceneMusicEntry;

/// A song a room started itself, outside the music table. Music-volume
/// changes are applied to it too, and the next load stops it and clears this.
extern u8 gStageRoomSong;

/// Requests the active mode task's reload-and-exit path, returning 0.
///
/// The transition task handles this after outstanding view/file transitions,
/// reloads room resources when its entry mode requires them, resumes a suspended
/// movie and returns presentation to the game loop. This call neither exits
/// the caller's task nor waits for the reload.
s32 stageRequestModeTaskExit(void);

/// Restores the static task primitive buffer if this mode selected the heap buffer.
///
/// Clears the mode's selection latch; it does not free heap storage or wait for
/// GPU work. Call before repurposing the primitive heap, after its drawing ends.
void stageReleaseTaskPrimitiveBuffer(void);

/// Applies the ambient-mute flag to the current area's ambient-loop request.
///
/// Requires stage 1..5 and its loaded map music table, with the area selecting
/// a valid row. Only rows whose first entry is `STAGE_MUSIC_AMBIENT_AREA` act.
/// A mute nibble of exactly 1 requests a nominal 30-audio-update fade and clears
/// the start latch, even if already clear. Otherwise a clear latch queues a
/// start with zero pan offset and attenuation, then sets it. The latch records
/// requests, not confirmed playback or queue admission. Other rows leave it
/// unchanged. `unused` is retained and
/// ignored; the load-completion caller passes 1.
void stageMusicUpdateAreaAmbient(s32 unused);

/// Requests a view change within the active mode task and returns the current view.
///
/// An already pending view change leaves the request intact. Otherwise blocks
/// controller input and records `view` until the mode task updates the session.
/// The session stores the view as a byte; it must select a loaded area's view.
/// `transitionKind` is narrowed to a byte: 0 skips intermediate drawing;
/// 1 draws filtered tasks/actors, 2 active actors, 3 or 0x20 all tasks, and
/// 7 no extra drawing. Returns the live view even when the request was ignored.
s32 stageRequestViewTransition(s32 view, s32 transitionKind);

/// Requests a view change with no extra drawing, then the active mode task's exit.
///
/// Uses the view/lifetime contract of `stageRequestViewTransition`. Returns -1
/// without changing an already pending view transition, otherwise returns the
/// current view and queues both the view change and mode-exit request.
s32 stageRequestViewTransitionAndModeExit(s32 view);

/// Configures the active mode task's grey fade overlay, returning 0.
///
/// An argument of zero for `stepPerTick` selects 32; other arguments store the
/// low byte. Nonzero `decreasing` negates the byte; the stepper reads it as signed
/// and multiplies it by nominal frame ticks. Ordinary ramp magnitudes are
/// 1..127; other values retain their byte wrapping. Nonzero `additiveBlend`
/// adds grey, otherwise subtracts it. Nonzero `frontOfOt` selects the first
/// drawn table depth (its last entry), otherwise depth zero.
/// Replaces all fade flags, including the one-step skip, without changing the
/// current level or maximum. The overlay advances only while the mode task runs.
s32 stageConfigureFade(s32 decreasing, s32 additiveBlend, s32 stepPerTick, s32 frontOfOt);

/// Sets the grey fade's upper level (0..255), without immediately clamping it.
///
/// The next advancing step clamps against this maximum. Zero-level status takes
/// precedence when the maximum is zero; changing the maximum does not start a fade.
void stageSetFadeMax(u8 maxLevel);

/// Selects the 0x10000-byte heap task primitive buffer once per active mode.
///
/// Borrows the configured primitive heap; it does not allocate or clear it.
/// The heap must remain writable and reserved for drawing until
/// `stageReleaseTaskPrimitiveBuffer` restores the static selection.
void stageEnsureHeapTaskPrimitiveBuffer(void);

/// Returns 1 while a view change or file-load transition is requested, else 0.
///
/// Capture, keep-view and mode-exit bits alone do not make this predicate true.
s32 stageIsTransitionPending(void);

/// Queues capture of the framebuffer presented when the mode task handles the request.
///
/// Requires an active mode task. Its transition handler captures the then-current
/// stage/area frame into that area's RAM image slot after higher-priority work,
/// then clears the request. Repeated requests coalesce. Returns 0 immediately;
/// neither this return nor the call itself means GPU readback has completed.
s32 stageRequestFrameCapture(void);

/// Queues a start for the current area's scene-selected music-table entry.
///
/// The stage must be 1..5 and its map table must remain loaded; area and the
/// cached scene column must select an entry in that table. No-sequence and
/// never-start entries do nothing. The low halfword of `fadeInTicks` measures
/// audio updates, including PAL timer updates; zero starts without a fade.
/// Records the sequence and reapplies saved music volume without waiting for
/// queue admission or actual playback.
void stageMusicRequestAreaStart(s32 fadeInTicks);

/// Queues a stop for the current area's scene-selected song when it is busy.
///
/// Stage must be 1..5, with its map table loaded and the area/cached scene column
/// selecting a valid entry. A no-sequence entry does nothing; sequence zero
/// selects all loaded songs. The request adds one to `fadeOutTicks`, truncates
/// to 16 bits, then the sound queue rounds down to a multiple of four audio
/// updates (including PAL timer updates). Callers
/// must avoid signed overflow in the increment. Does not wait for admission or
/// actual stopping and leaves the recorded song unchanged.
void stageMusicRequestAreaStop(s32 fadeOutTicks);

/// Classification of the stage fade overlay's current level.
enum {
    STAGE_FADE_CLEAR   = 0,
    STAGE_FADE_AT_MAX  = 1,
    STAGE_FADE_BETWEEN = -1,
};

/// Returns `STAGE_FADE_CLEAR`, `STAGE_FADE_AT_MAX` or `STAGE_FADE_BETWEEN`.
///
/// Zero level takes precedence even when the maximum is zero. This classifies
/// the level without advancing the fade or testing whether its step is stopped.
s32 stageGetFadeStatus(void);

/// Initializes full-depth task ordering tables once per active mode.
///
/// Replaces the small-table selection and clears the presented framebuffer's
/// task table. Mode initialization resets the latch for the next mode.
void stageEnsureTaskOrderingTables(void);

/// Reads the latch recording whether a file-load transition cleared both framebuffers.
///
/// Returns 1 after the clears and GPU synchronization. Mode-task initialization
/// and a subsequent view-transition capture clear the latch to 0; this does not
/// inspect the current framebuffer contents.
s32 stageGetLoadBuffersCleared(void);

/// Clears the mode fade's current level and restores its maximum to 255.
///
/// Retains its step and blend/placement flags, so a configured ramp can resume
/// from zero when the mode task next advances the overlay.
void stageResetFadeLevel(void);

#endif // MAIN_STAGE_H
