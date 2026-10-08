#ifndef GAMEPLAY_PRIVATE_CAPTIONS_H
#define GAMEPLAY_PRIVATE_CAPTIONS_H

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"

#include "main/task_types.h"
#include "main/text.h"

// CAP dialogue commands, text state, relocation and task control.

extern CapActionRequest D_801155A0;

extern TaskDesc D_8010FB4C[3];

extern s32 D_8010FB80;

extern s32 D_8010FB84;

extern s32 Gp_CapCaretGrey;

extern s32 Gp_CapCaretDir;

extern const char Gp_StrCapMagic[];

extern const CapTextLayout D_80097518;

extern const char Gp_StrEvsFmt[];

extern const TaskFuncTable3 Gp_CapTaskStates;

void Gp_SpawnEvt1(s32 arg0, s32 arg1);

extern s16 D_801156BC;

/// Scene-sync completion bit and delay measured in active CAP control ticks.
enum {
    CAP_CONTROL_SCENE_SYNC_COMPLETE     = 0x20,
    CAP_CONTROL_SCENE_SYNC_DELAY_FRAMES = 30
};

void Gp_InitCapTask(Task* task);

/// Relocates the loaded CAP file and advances the scene-sync gate each task tick.
///
/// Debug mode also runs the two debug control hooks. File validity and borrowed
/// lifetime follow `capRelocateFile`. An armed gate counts only while a CAP
/// sequence is selected and publishes completion at thirty active ticks, or on
/// the next tick for a request that primes the elapsed count to thirty.
/// The task argument is unused; all control state belongs to the CAP singleton.
void capUpdateControlTask(Task* unusedTask);

extern CapChoice D_801155D0[CAP_CHOICE_CAPACITY];

extern u8 D_80115648;

extern s16 D_8011564A;

extern u16 Gp_CapCaretX;

extern u16 Gp_CapCaretY;

extern s16 D_80115650;

extern s16 D_80115652;

extern s16 D_80115654;

extern s16 D_80115656;

extern u8 Gp_CapCaretDelay;

extern u8 D_80115659;

extern u8 D_8011565A;

extern u16 D_8011565C;

extern CapTextUpdateCallback D_80115660;

extern s16 D_80115664;

extern s16 D_80115666;

extern s16 Gp_CapEventKey;

extern s16 D_8011566A;

extern u8 D_8011566C;

extern u8 D_8011566D;

extern u8 D_8011566E;

extern u8 D_8011566F;

extern u8 D_80115670;

extern Task* Gp_CapTask;

extern s16 D_80115678;

extern s16 D_8011567A;

extern TextGlyphCell* Gp_CapGlyphs;

/// Relocates a loaded CAP file in place and selects its glyph and command tables.
///
/// Returns false without changing the file or active tables when the first
/// three magic bytes are not "CAP"; otherwise returns true. The fourth byte
/// is ignored.
/// A positive glyph offset triggers relocation of all three header offsets,
/// nonterminal text references and nonzero command references. A relocated
/// KSEG0 glyph address is negative, so repeated calls only select the tables.
///
/// `file` must be non-null, word-aligned, writable KSEG0 storage containing a
/// complete serialized or already relocated CAP file. The signed sequence count
/// counts playback records, including terminators, but excludes command slots
/// skipped after terminators; the signed command count counts reference words.
/// Neither offsets nor table extents are checked. The file must remain loaded
/// while its published tables or text are used.
bool capRelocateFile(CapFile* file);

extern CapSequenceRecord* Gp_CapTable;

extern s16 D_801155AC;

extern u16 D_801155AE;

extern s16 D_801155B0;

extern s16 D_801155B2;

extern s16 D_801155B4;

extern s16 D_801155B6;

extern u8 D_801155B8;

extern s8 D_801155B9;

extern u8 D_801155BA;

extern u8 D_801155BB;

extern s16 D_801155BC;

extern s16 D_801155BE;

extern s16 D_801155C0;

/// Starts playback of a relocated CAP sequence for the supplied variant key.
///
/// `sequence` borrows the command header in slot zero; the first matching record
/// or terminator must be reachable from slot one inside the live CAP file, at
/// an index in 1..32767. Text and the active glyph table must satisfy
/// `capGetTextFirstBaselineY` and `capGetTextBlockHeight`'s bounds. Keep the file
/// and glyph table loaded through playback, including any queued transition.
/// The caller must have stopped any previous playback; no busy check is made.
///
/// `playbackMode` 0 spawns in the current display; 1 queues a display transition;
/// 2 also brackets playback with room-effect messages and delays capture for
/// placed-object actions;
/// 3 queues the transition with an unstarted-sequence guard, then becomes mode 1.
/// Other nonzero values queue the transition and are retained without validation.
/// `variantKey` is signed-16 storage matched against each record's unsigned byte;
/// keys outside 0..255 scan to the terminator. Choices and declined actions can
/// replace it later, and completion retains the last key.
///
/// Always returns 0, including NULL input, no matching record and spawn failure.
/// NULL input leaves state intact; a terminal first match clears the selection
/// after initializing playback state. A queued request stores NULL as the task
/// pointer even if accepted; task allocation and queue rejection are unchecked.
s32 capStartSequence(CapSequenceRecord* sequence, s16 playbackMode, s16 variantKey);

/// Pointer to the loaded `.pe2cap2` blob (folder slot type 3).
extern u8 D_80115688;

extern s16 D_80115698;

extern s16 D_8011569A;

extern u8 D_8011569C;

void func_800E44A0(Task* task);

/// Measures the CAP text box's height in pixels from break-terminated lines.
///
/// Each line contributes its greatest glyph height plus two, or two if empty.
/// Negative controls, including icons, add no glyph height. The unfinished tail
/// contributes nothing; a total of two is returned as zero. `text` is borrowed
/// 0xFFFF-terminated storage whose signed-16 element index must not overflow,
/// and the active glyph table must cover every low-ten-bit glyph index.
s16 capGetTextBlockHeight(const u16* text);

/// Places the first CAP text baseline above the fixed screen Y of 208 pixels.
///
/// Subtracts the heights of break-terminated lines after the first; an unfinished
/// final line contributes nothing. Each height is the greatest nonnegative
/// glyph height plus two, or two for an empty line. The result is screen-relative,
/// before the drawer converts to centre-relative coordinates and removes shake.
/// The borrowed stream and glyph table have `capGetTextBlockHeight`'s bounds.
s16 capGetTextFirstBaselineY(const u16* codes);

/// Initializes the selected record's effective text flags and minimum-frame counter.
///
/// Requires a relocated selected sequence and a valid current record index,
/// interpreted as a signed halfword. Copies the sound/text byte, clearing
/// instant text when the minimum interval is nonzero, and seeds that interval
/// in frames. The terminal record may be selected but its text is never read.
void capApplyRecordPlaybackSettings(void);

/// Finds the first record at or after `recordIndex` for the current variant key.
///
/// Returns its slot index, or the terminal record's index regardless of its key.
/// Slot zero is the command header; playback starts at slot one. Requires a
/// relocated selected sequence with a matching record or terminator reachable
/// inside its live CAP file. No bounds check is performed. Playback callers
/// require both the starting index and result to be in slots 1..32767, because
/// they store and address the result as a signed-halfword index.
s32 capFindVariantRecord(s32 recordIndex);

/// Clears a selected CAP sequence if queued playback has not begun.
///
/// Used during a queued CAP display transition: waits one task dispatch, then
/// clears the selection only if playback has never run, and kills itself.
/// `task` must be a live task; neither spawn argument is consumed.
void capClearUnstartedSequenceTask(Task* task);

/// Sends a delayed CAP texture-sequence or eye-mode message, then kills its task.
///
/// `spawnArg1.value` packs mode in bits 0..7, delay in bits 8..15 (0..255 wait
/// dispatches), and recipient in bits 16..23; bits 24..31 and `spawnArg2` are
/// ignored. Recipient 0 targets the player, 1 the companion, and 2..17 placed
/// actors 0..15 in the current scene. Larger selectors are forwarded unchecked.
/// Player/companion receive a texture-sequence mode; placed actors an eye mode.
/// A missing companion or placed actor drops the message. The player and scene
/// manager must exist for their respective paths; receiver overlays stay loaded
/// through synchronous dispatch. The first callback initializes the countdown;
/// a zero delay sends on the following callback. Requires a live bodyless task
/// and deferred collection, since the counter is decremented after teardown.
void capDelayedTextureMessageTask(Task* task);

#endif // GAMEPLAY_PRIVATE_CAPTIONS_H
