#ifndef GAMEPLAY_EVS_SCRIPTS_H
#define GAMEPLAY_EVS_SCRIPTS_H

#include "types.h"

#include "gameplay/evs.h"

#include "main/task_types.h"

extern u8 D_801156F9;

Task* Gp_LookupSlot4(s32 arg0);

void func_800E8614(EvsCommand* arg0, s32 arg1);

void func_800E8634(EvsCommand* arg0, s32 arg1, EvsCommand* arg2);

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

void func_800E8830(Task* arg0);

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
