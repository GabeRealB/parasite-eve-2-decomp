#ifndef GAMEPLAY_EVS_SCRIPTS_H
#define GAMEPLAY_EVS_SCRIPTS_H

#include "types.h"

#include "gameplay/evs.h"

#include "main/task_types.h"

extern u8 D_801156F9;

/// HUD handling selected when an event script starts.
enum {
    EVENT_SCRIPT_HUD_HIDE_RESTORE = 0, // Hide at initialization; show on scene cleanup or script end
    EVENT_SCRIPT_HUD_KEEP         = 1  // Leave HUD visibility unchanged; any nonzero mode has this behavior
};

/// Starts an event script with no initial input skip target.
///
/// Uses `evsStartScriptWithSkip` with a NULL skip script and the same HUD and
/// lifetime contract. The script may enable skipping with its own commands.
void evsStartScript(EvsCommand* script, s32 hudMode);

/// Starts the shared event-script interpreter with an optional input skip script.
///
/// `script` and a non-NULL `skipScript` are borrowed command streams that must
/// remain loaded until the interpreter ends, including any call/jump targets.
/// Stream bounds and call depth are unchecked; allow at most eight pending
/// script calls and return only from a call. Skipping retains pending returns.
/// Run only one event script at a time: starting resets shared event and skip
/// state without cancelling an existing interpreter. A NULL skip script
/// disables input skipping initially; script commands may replace that target.
/// Input skipping becomes eligible after five unpaused interpreter updates.
///
/// `hudMode` is `EVENT_SCRIPT_HUD_HIDE_RESTORE` or `EVENT_SCRIPT_HUD_KEEP`.
/// Zero hides the HUD at initialization and shows it at scene cleanup or end;
/// any nonzero value leaves it unchanged. The HUD change occurs when the task
/// initializes. Requires an initialized task execution list and, in hide/restore
/// mode, a live CAP controller. Allocation failure is not reported and does not
/// clear the reset event state.
void evsStartScriptWithSkip(EvsCommand* script, s32 hudMode, EvsCommand* skipScript);

/// Runs the event-script vertical screen shake with a triangular amplitude envelope.
///
/// Bank 9 type 0x0C takes a packed signed `spawnArg2.value`: bits 0..7 are
/// the half-duration in task updates (1..255), and the arithmetic shift by 8
/// gives the pixel amplitude. `spawnArg1.value` becomes the frame cursor,
/// from minus to plus the half-duration inclusive. Each sample advances the
/// shared random sequence and alternates sign; the display narrows it to s8
/// before clamping to [-8, 8]. The next update clears the offset and ends the
/// task. There is no check for a zero half-duration before division.
void evsScreenShakeTask(Task* task);

/// Runs the event-script linear attenuation fade of one sound-script instance.
///
/// Bank 9 type 0x0E borrows the interpreter's sound-fade record through
/// `spawnArg2.pointer`; it must remain live until the task ends. A zero
/// duration sends the target immediately. Otherwise initialization snapshots
/// the record's current unsigned 16-bit attenuation, then each task update
/// interpolates toward the target and writes the running level back. Requests
/// use the low signed byte and a zero pan offset. The final update ends the
/// task and clears the interpreter's live fade handle. The interpreter keeps
/// one fade record and cancels the previous task before replacing its request.
void evsSoundAttenuationFadeTask(Task* task);

/// Runs the event-script linear fade of the music level.
///
/// Bank 9 type 0x0D borrows the interpreter's music-fade record through
/// `spawnArg2.pointer`; it must remain live until the task ends. A zero
/// duration applies the target immediately. Otherwise initialization snapshots
/// the cached 16-bit music level, then each task update applies an interpolated
/// level with `midiApplyMusicVolume` (zero selects the saved option). The final
/// update ends the task and clears the interpreter's live fade handle. The
/// interpreter cancels the previous fade before replacing its single request.
void evsMusicVolumeFadeTask(Task* task);

/// Dispatches event-script interpreter initialization, execution and teardown.
///
/// Bank 9 type 7 requires `state` 0..2; no bounds check is made. `spawnArg2` borrows
/// an `EvsCommand` stream and its referenced resources until interpreter cleanup.
/// `spawnArg1` selects `EVENT_SCRIPT_HUD_HIDE_RESTORE` or `EVENT_SCRIPT_HUD_KEEP`.
/// Initialization owns interpreter work and takes a display hold; execution
/// releases the hold and optional HUD suppression before the final kill state.
/// A handler may release the task; dispatch makes no subsequent access.
void evsInterpreterTask(Task* task);

/// Slides the HP/MP display upward and back for CAP's demo-scene HUD controls.
///
/// Bank 9 type 8 starts hiding on initialization, replacing `spawnArg1.value`
/// with -1. Subsequent updates advance a signed step counter in `killCountdown`
/// and hold at 8: each step raises the display six pixels, up to 48 pixels.
/// Setting `spawnArg1.value` to +1 reverses the slide; moving below step zero
/// clears the offset and ends the task. Initial spawn arguments are ignored;
/// no work block is allocated.
void capHudSlideTask(Task* task);

#endif // GAMEPLAY_EVS_SCRIPTS_H
