#ifndef GAMEPLAY_CAPTIONS_H
#define GAMEPLAY_CAPTIONS_H

#include "types.h"

#include "gameplay/cap.h"

#include "main/sound_ids.h"
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
    /// `Gp_AbortCap`'s result (`Gp_AbortCapClear`).
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

void Gp_RunCapCmd(s32 arg0, s16 arg1);

void Gp_MsgPlayer3F3(s32 arg0);

void Gp_MsgPlayerWeapon(s32 arg0);

void Gp_SpawnIfCapIdle(s32 arg0, s32 arg1);

/// Enqueues a type-6 sound event, substituting the current stage number into
/// the packed id when its stage nibble is set. `arg1` / `arg2` are the pan and
/// volume bytes.
void Gp_EnqueueStageSnd6(s32 arg0, s32 arg1, s32 arg2);

void Gp_MsgAllyWeapon(s32 arg0);

void Gp_RunCapCmd1(s32 arg0);

void Gp_MsgAlly3F3(s32 arg0);

/// Dispatches 0x7D0 to the slot-4 task to resolve a chained task for the
/// current stage/room, then forwards 0x7D5 with `arg1` to it.
void Gp_MsgSlot4Chain(s32 arg0, s32 arg1);

void func_800E3FAC(s32 arg0, s32 arg1);

void Gp_AllyAnimId(s32* arg0);

void Gp_FillAllyHp(void);

void Gp_FillPlayerHpMp(void);

void Gp_SetNibbleIf(s32 arg0, s32 arg1);

s32 Gp_PackStageSndId(s32 arg0);

void Gp_EnqueueStageSnd7(s32 arg0, s32 arg1);

void Gp_PlayerWeaponId(s32* arg0);

void func_800E4028(Task* arg0);

s32 func_800E3FCC(s32 arg0);

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

s32 Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2);

/// Returns 1 while a CAP sequence is selected, otherwise 0.
///
/// Selection includes the interval before a queued display transition starts
/// playback. Completion, abort and reset release it.
s32 capIsBusy(void);

s32 Gp_AbortCap(void);

void Gp_ResetCap(void);

void Gp_LoadCapFile(s32 arg0);

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
