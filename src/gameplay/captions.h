#ifndef GAMEPLAY_PRIVATE_CAPTIONS_H
#define GAMEPLAY_PRIVATE_CAPTIONS_H

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "gameplay/direction.h"

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

void Gp_ClearAllFlagNibbles(void);

void Gp_SpawnEvt1(s32 arg0, s32 arg1);

/// Location-message fallback of `D_8010FAD4`, the table installed on pointer
/// slot 7: copies the requested location onto the outgoing record and answers
/// 1, leaving the decision to whoever reads the reply.
s32 func_800E3FF0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst);

s32 func_800E4018(Task* task, s32 msgId, s32 firstArg, s32 secondArg);

extern s16 D_801156BC;

void Gp_InitCapTask(Task* task);

void Gp_CapTaskState1(Task* task);

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

s32 Gp_StartCap(CapSequenceRecord* sequence, s16 arg1, s16 arg2);

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

void Gp_ApplyCapEvtFlags(void);

s32 Gp_FindCapEvt(s32 arg0);

/// Clears a selected CAP sequence if queued playback has not begun.
///
/// Used during a queued CAP display transition: waits one task dispatch, then
/// clears the selection only if playback has never run, and kills itself.
/// `task` must be a live task; neither spawn argument is consumed.
void capClearUnstartedSequenceTask(Task* task);

/// `spawnArg1` packs three bytes: bits 0-7 are the message argument, bits
/// 8-15 the delay in frames, and bits 16-23 the recipient - 0 for slot 3, 1
/// for slot 0xA, otherwise `sceneFindPlacedActor(n - 2)`.
void Gp_DelayedMsgTask(Task* task);

#endif // GAMEPLAY_PRIVATE_CAPTIONS_H
