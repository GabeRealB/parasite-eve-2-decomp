#ifndef GAMEPLAY_CAPTIONS_H
#define GAMEPLAY_CAPTIONS_H

#include "types.h"

#include "gameplay/cap.h"

#include "main/task_types.h"
#include "main/text.h"

// CAP dialogue commands, text state, relocation and task control.

/// Messages of the CAP control task in `GAME_TASK_SLOT_CAP_CONTROL`.
///
/// Event scripts, actors and overlays bracket a scripted presentation with
/// these. In demo scene 9 the HUD messages manage the demo's presentation task
/// instead of the HUD flag. Every handler here returns 0 unless noted.
enum {
    /// Starts the CAP sequence in the slot given by the first argument and clears
    /// the control's completion flag (`Gp_StartCapAndClear`).
    CAP_CONTROL_MESSAGE_START = 0xFA0,
    /// Clears the completion flag and aborts the running CAP sequence, returning
    /// `capAbortPlayback`'s result (`Gp_AbortCapClear`).
    CAP_CONTROL_MESSAGE_ABORT = 0xFA2,
    /// Returns nonzero while a CAP sequence table is loaded (`capIsBusy`).
    CAP_CONTROL_MESSAGE_IS_BUSY = 0xFA3,
    /// Hides the HUD for a presentation; in demo scene 9 spawns its task.
    CAP_CONTROL_MESSAGE_HIDE_HUD = 0xFA4,
    /// Shows the HUD again; in demo scene 9 lets its task finish.
    CAP_CONTROL_MESSAGE_SHOW_HUD = 0xFA5,
    /// Shows the HUD again; in demo scene 9 kills its task at once.
    CAP_CONTROL_MESSAGE_SHOW_HUD_ABORT = 0xFA8,
};

extern TextGlyphCell D_8010FB70[4];

/// CAP sequence presentation modes accepted by the command API.
enum {
    CAP_PLAYBACK_IN_PLACE           = 0,
    CAP_PLAYBACK_DISPLAY_TRANSITION = 1,
    CAP_PLAYBACK_ACTION_CAPTURE     = 2,
    CAP_PLAYBACK_CLEAR_IF_UNSTARTED = 3
};

/// Bits copied to the normal CAP event task's first spawn argument.
enum {
    CAP_EVENT_NO_FLAGS       = 0,
    CAP_EVENT_PAUSE_ACTORS   = 1, // Hold the player in scripted idle and pause other actors until completion.
    CAP_EVENT_HIDE_PLAYER    = 2, // Hide the player while the event runs, then show it.
    CAP_EVENT_ACTION_CAPTURE = 4  // Use action-capture playback; otherwise paused events use mode 0, others mode 3.
};

/// Interprets a loaded CAP command and starts its selected playback variant.
///
/// `commandIndex` must be in 0..32767 and below the selected command table's
/// count. It and every followed branch must name a non-null command in a live,
/// relocated writable CAP file. Branch chains must terminate; no bounds, null
/// or cycle checks occur. Referenced nibble ids must be 0..503 and tally object
/// ids 0..63 in the current stage. Playback resources and text must satisfy
/// `capStartSequenceSlot`'s contract and remain loaded through playback.
///
/// Opcodes select variant zero, a counter, a flag nibble, or the count of
/// two-bit object states equal to 0, 1 or 3; the room opcode instead sends
/// the index to the room without starting CAP playback. Counter commands
/// advance/store their counter after the start attempt even when CAP is busy
/// or playback fails. Unknown opcodes do nothing. `playbackMode` uses the
/// `CAP_PLAYBACK_*` modes and is forwarded as a signed halfword.
void capRunCommand(s32 commandIndex, s16 playbackMode);

/// Spawns a CAP event task only while no CAP sequence is selected.
///
/// `commandIndex` is copied to spawnArg2 and `eventFlags` to spawnArg1 of the
/// normal event task. `CAP_EVENT_*` bits hold/pause actors, hide the player and
/// select action capture; unassigned bits have no effect in that task.
/// The command and its resources must satisfy `capRunCommand` when the task
/// runs and remain loaded through playback. Busy playback and allocation
/// failure silently do nothing. An idle check does not reserve playback, so
/// several calls before the tasks tick can queue several event tasks.
void capSpawnEventIfIdle(s32 commandIndex, s32 eventFlags);

/// Runs a loaded CAP command using the queued display-transition playback mode.
///
/// Command bounds, branch termination, counter side effects and borrowed-resource
/// lifetime follow `capRunCommand`. This fixes playbackMode to
/// `CAP_PLAYBACK_DISPLAY_TRANSITION`; it does not wait for playback or report success.
void capRunCommandWithTransition(s32 commandIndex);

void Gp_FillPlayerHpMp(void);

void func_800E4028(Task* arg0);

void func_800E7570(Task* arg0);

extern u8 D_80115680;

void Gp_EndWaitTask(Task* task);

void func_800E70AC(Task* task);

extern CapFile* Gp_CapFile;

extern u8 D_80115690;

extern s32 D_80115694;

extern CapCommandRef* Gp_CapCmds;

extern u8 D_801156A4;

extern s32 D_801156A8;

/// Starts CAP playback from a command-table slot with an explicit variant key.
///
/// If a sequence is already selected, returns 0 without reading the table.
/// Otherwise the active command table must be relocated, and `commandIndex`
/// must be in 0..32767 and below its `CapCommandTable.count`; no bound is checked.
/// This bypasses the slot's command opcode and uses `variantKey` directly.
/// Keys in 0..255 match record keys; other signed-16 values scan to the terminator.
///
/// A non-null slot borrows its command header as sequence slot zero. A matching
/// record or terminator must be reachable in slots 1..32767 within the live CAP
/// file. Text must satisfy `capGetTextBlockHeight` and
/// `capGetTextFirstBaselineY`'s bounds. Keep the table, sequence, text and active
/// glyph storage loaded through playback, including any queued transition.
///
/// `playbackMode` 0 starts in the current display; 1 queues a display transition;
/// 2 also brackets playback with room-effect messages and delays frame capture
/// for placed-object actions; 3 queues playback with an unstarted-sequence guard,
/// then becomes mode 1. Other nonzero modes queue the transition unchecked.
///
/// Returns 1 only for a null command-table entry, otherwise 0. Zero includes
/// busy playback, no matching variant, and task allocation or queue failure;
/// it does not report whether playback began. The result is a signed halfword.
s16 capStartSequenceSlot(s16 commandIndex, s16 playbackMode, s16 variantKey);

/// Returns 1 while a CAP sequence is selected, otherwise 0.
///
/// Selection includes the interval before a queued display transition starts
/// playback. Completion, abort and reset release it.
s32 capIsBusy(void);

/// Stops a selected CAP sequence whose playback task has been allocated.
///
/// Releases the sequence selection and kills its live task. Queued playback
/// requests stage-mode exit and any needed saved-view transition; in-place
/// playback restores actors and the HUD when its event/control gates permit.
/// Returns 0 on that cleanup, or -1 when no sequence or task exists.
/// A queued selection with a NULL task is
/// left selected; this does not cancel the queued display transition.
s32 capAbortPlayback(void);

/// Clears CAP playback selection and restores the bundle's default dialogue resource.
///
/// Clears the retained choice index, playback-started flag and current file,
/// selects data resource zero, restores the texture-page origin to (384, 0)
/// in VRAM pixels, and selects ordinary view lookup. Existing playback must
/// have stopped; this resets selection without killing its task. The default
/// resource follows `capSelectLoadedFile`'s storage and relocation requirements.
void capReset(void);

/// Selects and relocates one already loaded CAP data resource in the current CDF bundle.
///
/// `dataResourceOrdinal` is zero-based among FILE_SYSTEM_RESOURCE_DATA slots,
/// excluding image and empty slots. No file I/O or allocation occurs. A missing
/// ordinal leaves the current file and tables unchanged. A selected resource
/// becomes the current file even if its CAP magic is invalid; failed relocation
/// leaves the previously published glyph and command tables unchanged.
/// The payload must be fully loaded, non-null, word-aligned writable KSEG0
/// storage satisfying `capRelocateFile`'s complete-file bounds. Its storage is
/// borrowed and must stay loaded while its file, tables or text are in use.
void capSelectLoadedFile(s32 dataResourceOrdinal);

/// Sets the update hook for timed CAP text reveal, or clears it with NULL.
///
/// The callback follows `CapTextUpdateCallback`'s cursor and reveal contract.
/// Its overlay must remain loaded until the hook is cleared; starting CAP
/// playback clears it automatically.
void capSetTextUpdateCallback(CapTextUpdateCallback callback);

/// Returns the current CAP sequence's variant key, retaining the last choice.
///
/// Playback starts with the supplied key; choices and declined actions can
/// replace it with their byte-sized key. Completion does not clear the key,
/// and reading it neither advances playback nor consumes the choice.
/// The stored signed-16 value is returned sign-extended to s32.
s32 capGetVariantKey(void);

/// Selects the VRAM origin used by CAP title and text sprites.
///
/// `vramX` and `vramY` are VRAM coordinates in 16-bit pixels, passed to the
/// GPU's 4-bit texture-page encoding when drawing. Texture data must already
/// be loaded there. CAP reset restores (384, 0).
void capSetTexturePage(s16 vramX, s16 vramY);

#endif // GAMEPLAY_CAPTIONS_H
