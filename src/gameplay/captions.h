#ifndef GAMEPLAY_PRIVATE_CAPTIONS_H
#define GAMEPLAY_PRIVATE_CAPTIONS_H

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"

#include "main/task_types.h"
#include "main/text.h"

// CAP dialogue commands, text state, relocation and task control.

/// Singleton action request owned by CAP playback and borrowed by its receiver.
///
/// Playback serializes actions and keeps this writable storage live until done.
/// The receiver may retain its address after dispatch; accepted selects whether
/// playback uses the record's fallback key. Prompt receivers also own defaultPrompt.
extern CapActionRequest gCapActionRequest;

/// Bodyless playback helpers: slot 0 unstarted guard, slot 1 delayed message, slot 2 terminator.
///
/// Live entries run at priority 32. The guard ignores both spawn arguments;
/// the message task consumes the packed mode/delay/recipient in spawnArg1.
/// Descriptors are read synchronously at spawn; never spawn slot 2.
extern TaskDesc gCapPlaybackTaskDescriptors[3];

/// Inclusive pulse levels and direction values shared by the two CAP markers.
///
/// The maximum normalizes vertex brightness. A draw advances the level by one
/// and reverses at each endpoint; the initial pair is (minimum, rising).
enum {
    CAP_MARKER_PULSE_MIN     = 8,
    CAP_MARKER_PULSE_MAX     = 15,
    CAP_MARKER_PULSE_RISING  = 0,
    CAP_MARKER_PULSE_FALLING = 1
};

/// Selected-choice marker pulse level in 8..15, advanced once per marker draw.
extern s32 gCapChoiceMarkerPulseLevel;

/// Selected-choice pulse direction (0 rising, 1 falling), reversed at levels 8 and 15.
extern s32 gCapChoiceMarkerPulseFalling;

/// Continue-caret pulse level in 8..15, advanced only after its drawing delay expires.
extern s32 gCapContinueCaretPulseLevel;

/// Continue-caret pulse direction (0 rising, 1 falling), independent of the choice marker.
extern s32 gCapContinueCaretPulseFalling;

/// NUL-terminated CAP signature prefix; relocation compares its first three bytes.
extern const char gCapMagicPrefix[4];

/// Read-only writing-direction record selecting horizontal CAP text.
extern const CapTextLayout gCapTextLayout;

/// Debug filename format evs<group>_<streamId>_<subId>.txt for the selected scene key.
///
/// Supply the three unsigned-halfword EvsSceneKey fields promoted to int for %d;
/// a 25-byte destination holds every resulting name including its terminator.
extern const char gCapSceneKeyFilenameFormat[16];

/// CAP control states: slot 0 initialize, slot 1 update, slot 2 kill the task.
///
/// The dispatcher copies the three callbacks by value and requires state 0..2.
extern const TaskFuncTable3 gCapControlStates;

/// Weapon-event flags selecting a room command instead of CAP playback.
enum { CAP_WEAPON_EVENT_ROOM_COMMAND = 0xFF };

/// Spawns a CAP event bracketed by the player's weapon presentation animations.
///
/// Copies `commandIndex` to spawnArg1 and `eventFlags` to spawnArg2, the reverse
/// of `capSpawnEventIfIdle`. The caller must gate active events and CAP playback;
/// this spawner does not check either and silently drops allocation failure.
/// `CAP_EVENT_*` bits pause actors, hide the player and select action capture.
/// `CAP_WEAPON_EVENT_ROOM_COMMAND` instead sends `commandIndex` to the room in
/// in-place mode, without those flag effects. For every other flag word,
/// `commandIndex` must satisfy `capRunCommand`.
/// Requires a live player and loaded weapon-presentation resources through
/// completion; the room must also remain live for room commands or sound cues.
/// Enabled completion sound routing sends eventFlags + 100 as the cue key.
void capSpawnWeaponEvent(s32 commandIndex, s32 eventFlags);

/// Elapsed active CAP control ticks for the current scene-sync request.
///
/// Starts at zero, or is primed to thirty for an immediate reply on the next
/// active tick. Inactive playback pauses it; disarming retains the last count.
/// Playback subtracts thirty from its unsigned-halfword view to compensate
/// timed text for a delayed reply. The signed-halfword storage is intentional.
extern s16 gCapSceneSyncElapsedTicks;

/// Scene-sync reply delay measured in active CAP control ticks.
enum {
    CAP_CONTROL_SCENE_SYNC_DELAY_FRAMES = 30
};

/// Initializes and publishes the singleton CAP control task and its message receiver.
///
/// Runs in state 0, advances to the update state on success, and kills the task
/// if its four-byte primary-heap allocation fails. That retained allocation has
/// no observed payload access and is released by ordinary task teardown.
/// Resets playback, clears the held demo-HUD task handle and disarms scene sync;
/// the scene-key-presence latch is retained. Requires initialized task/heap and
/// CAP singleton state. Both spawn arguments are unused.
void capInitializeControlTask(Task* task);

/// Relocates the loaded CAP file and advances the scene-sync gate each task tick.
///
/// Debug mode also runs the two debug control hooks. File validity and borrowed
/// lifetime follow `capRelocateFile`. An armed gate counts only while a CAP
/// sequence is selected and publishes completion at thirty active ticks, or on
/// the next tick for a request that primes the elapsed count to thirty.
/// The task argument is unused; all control state belongs to the CAP singleton.
void capUpdateControlTask(Task* unusedTask);

/// Per-record room-sound request latch values.
enum {
    CAP_RECORD_SOUND_NOT_ISSUED = 0,
    CAP_RECORD_SOUND_ISSUED     = 1
};

/// Whether the selected sequence has reached a playback-task update.
enum {
    CAP_PLAYBACK_NOT_STARTED = 0,
    CAP_PLAYBACK_STARTED     = 1
};

/// Visible-choice frames that must elapse before confirmation is accepted.
enum { CAP_CHOICE_CONFIRM_DELAY_FRAMES = 15 };

/// Disabled value for the wrapping playback-tick counter used by debug view cues.
enum { CAP_PLAYBACK_DEBUG_FRAME_DISABLED = -1 };

/// Playback's scene-sync phases; control-task ticks use a separate elapsed counter.
enum {
    CAP_SCENE_SYNC_IDLE     = 0,
    CAP_SCENE_SYNC_DEFERRED = 1,
    CAP_SCENE_SYNC_DISPATCH = 2,
    CAP_SCENE_SYNC_WAITING  = 3
};

/// Live choices laid out by the current CAP text draw.
///
/// Choice control codes write the pen, byte-sized variant key and sound class.
/// Only the prefix covered by the current choice count is valid; the count
/// must fit `CAP_CHOICE_CAPACITY`. Every array consumer must select that prefix.
/// Text is replayed each draw, rebuilding these records in gameplay storage.
extern CapChoice gCapChoices[CAP_CHOICE_CAPACITY];

/// Current record's room-sound request latch (0 not issued, 1 issued).
///
/// Startup and record advancement clear it. A record with a nonzero sound id
/// sets it after dispatch; a record without sound leaves it clear.
extern u8 gCapRecordSoundIssued;

/// Wrapping playback-update count supplied with debug view-change cues.
///
/// -1 disables counting. Debug startup selects -1; the first record-requested
/// view change enables it at zero. Enabled updates increment the low halfword,
/// including held exit updates, before presentation gates are checked.
extern s16 gCapDebugPlaybackFrame;

/// Continue-caret anchor X in centre-relative pixels, stored modulo 65536.
///
/// The latest processed line break records its ending pen X plus four.
/// Unsigned reads and GPU halfword stores preserve the wrapped coordinate.
extern u16 gCapContinueCaretX;

/// Continue-caret anchor Y in centre-relative pixels, stored modulo 65536.
///
/// The latest processed line break records its ending pen Y minus two.
/// Drawing removes vertical shake before storing GPU halfword coordinates.
extern u16 gCapContinueCaretY;

/// Final CAP text pen X from the latest draw, in pixels from the screen centre.
///
/// Published for the timed-text update hook; controls and spacers move this pen
/// as well as glyphs. Storage truncates to a signed halfword.
extern s16 gCapTextCursorX;

/// Final CAP text pen Y from the latest draw, in pixels from the screen centre.
///
/// Vertical shake is already removed for the timed-text update hook. Storage
/// truncates to a signed halfword.
extern s16 gCapTextCursorY;

/// CAP title and text texture-page origin X, in VRAM 16-bit pixels.
///
/// Drawing encodes this origin as a 4-bit GPU texture page. Texture data must
/// already be loaded there; CAP reset selects 384.
extern s16 gCapTexturePageVramX;

/// CAP title and text texture-page origin Y, in VRAM rows.
///
/// Shares the texture-page encoding and loaded-texture lifetime of
/// `gCapTexturePageVramX`; CAP reset selects zero.
extern s16 gCapTexturePageVramY;

/// Eligible continue-caret draw calls left before the caret appears.
///
/// Record advancement seeds thirty. Each attempted caret draw decrements and
/// returns while nonzero; updates that do not attempt a draw leave it intact.
/// Sequence startup does not reset this delay.
extern u8 gCapContinueCaretDelayCalls;

/// Visible-choice frames left before the current choices can be confirmed.
///
/// Startup and record advancement seed `CAP_CHOICE_CONFIRM_DELAY_FRAMES`.
/// Each marker draw with choices present decrements it before input confirmation.
extern u8 gCapChoiceConfirmFramesRemaining;

/// Selected playback's started latch (0 queued/unstarted or inactive, 1 dispatched).
///
/// Startup, finish and reset clear it; the first playback update sets it.
/// The queued-sequence guard uses zero to discard a selection that never ran.
extern u8 gCapPlaybackStarted;

/// Pre-playback one-image MDEC input mode retained for a return-view decode.
///
/// The resident decoder consumes 0 for RGB16 or 2 for RGB16 with pixel bit 15
/// set, then resets its mode. CAP restores this saved value only when queued
/// playback exits through a change back to the pre-playback view.
extern u16 gCapSavedImageMdecMode;

/// Optional timed-text update hook, cleared when a playable sequence starts.
///
/// Uses `CapTextUpdateCallback`'s cursor/reveal contract. The callback overlay
/// is borrowed and must remain loaded until the hook is cleared or replaced.
extern CapTextUpdateCallback gCapTextUpdateCallback;

/// Cleared at sequence startup and several text-processing boundaries; role unproven.
extern s16 D_80115664;

/// Selected CAP presentation mode, stored as a signed halfword.
///
/// `CAP_PLAYBACK_*` values select in-place, transition or action-capture behavior.
/// Startup mode 3 becomes mode 1 after queuing the unstarted guard. Other
/// nonzero values remain unchecked and follow the queued playback path.
extern s16 gCapPlaybackMode;

/// Active CAP variant key and retained result of the latest choice or declined action.
///
/// Startup accepts a signed halfword; only 0..255 can match record byte keys.
/// Choices and nonzero action fallback keys replace it. Finish and reset retain
/// the last value; the getter returns it sign-extended.
extern s16 gCapVariantKey;

/// Signed-halfword text-timing adjustment after a scene sync without a view change.
///
/// Zero is clear; 1 arms replacement on sync completion with elapsed control
/// ticks minus thirty. Instant timed text subtracts it from pause frames, or
/// from display frames when it exceeds the pause, then clears it. Truncation
/// and signed comparisons are retained, including a computed value of 1.
extern s16 gCapSceneSyncAdjustmentFrames;

/// Pre-playback saved view index used when CAP restores presentation.
///
/// Startup copies the live location's byte view. In-place exit restores it
/// when the event is idle; queued exit requests it only when the view changed.
extern u8 gCapSavedView;

/// View index to restore after a queued placed-object action prompt finishes.
///
/// Transition/capture playback saves the live view before switching to the
/// action view. The request must complete before playback reads this snapshot.
extern u8 gCapActionReturnView;

/// Playback scene-sync phase (0 idle, 1 deferred, 2 dispatch, 3 awaiting completion).
///
/// A scene-control record arms the deferred phase. Successive eligible updates
/// advance to dispatch and wait; consuming control completion returns to idle.
extern u8 gCapSceneSyncPhase;

/// Cleared only at sequence startup; no reader establishes its role.
extern u8 D_8011566F;

/// Current record's effective sound/text flags.
///
/// Bit 0 permits instant text; bits 1..7 retain the encoded room sound id.
/// A nonzero minimum-display interval clears bit 0 in this cached value; sound
/// dispatch still reads the original record byte.
extern u8 gCapRecordSoundAndTextFlags;

/// Last CAP startup's playback-task handle, usable only with a live selected sequence.
///
/// In-place spawning publishes its Task pointer or NULL on failure. Queued
/// playback publishes NULL even when accepted. Finish kills the task without
/// clearing this handle; reset and terminal selection also leave it retained.
/// This pointer alone does not establish task liveness.
extern Task* gCapPlaybackTask;

/// Eligible playback frames left in the current record's minimum-display interval.
///
/// Initialized from the record byte (0..255), decremented while positive after
/// transition/capture and post-confirmation gates. Confirmation copies the
/// leftover interval into `gCapAdvanceDelayFrames` before selecting the next record.
extern s16 gCapMinDisplayFramesRemaining;

/// Eligible playback frames to wait after confirmation before processing the next record.
///
/// Copies the old record's remaining minimum-display interval (0..255).
/// While positive, decrements and returns before the new record's minimum
/// interval or scene sync advances. Startup clears it.
extern s16 gCapAdvanceDelayFrames;

/// Borrowed read-only glyph/title cells from the last successfully relocated CAP file.
///
/// Text uses low-ten-bit indices (0..1023); a nonzero title byte indexes title
/// minus one (0..254). The file must cover every referenced cell and remain
/// loaded through drawing/measurement. No count is cached or checked. Failed
/// relocation leaves this pointer intact, and selection/reset need not clear it.
extern const TextGlyphCell* gCapGlyphCells;

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

/// Borrowed command-header base of the selected relocated CAP sequence, or NULL.
///
/// Slot zero is the command header; playback records start at slot one with
/// CapSequenceRecord stride and a signed-halfword index. Selection precedes
/// queued playback, so non-NULL does not prove a running playback task. The
/// containing CAP file must stay loaded until selection is released by finish,
/// reset, terminal selection or the unstarted guard. No storage is owned here.
extern CapSequenceRecord* gCapSelectedSequence;

/// Current record phase (0 revealing text or action unissued, 1 text complete or action issued).
///
/// Text drawing publishes 0/1; phase 1 waits for display/pause timing, user
/// confirmation or the singleton action request's completion. Starting playback
/// and advancing to a new ordinary record restore phase 0.
extern s16 gCapRecordPhase;

/// Active sequence slot cursor, stored as u16 and interpreted as s16 for addressing.
///
/// Slot zero is the command header; selected records and terminators must be
/// reachable within the borrowed sequence at indices 1..32767. Advancement wraps
/// the stored halfword before the next keyed scan. Keep the explicit signed view.
extern u16 gCapRecordIndex;

/// Inclusive visible-code cursor into the current u16 CAP text stream.
///
/// Starts at zero and advances one code after the reveal-delay gate opens.
/// Controls consume indices as well as glyphs. Addressing uses signed-halfword
/// indices, so the stream and any exposed one-past cursor must fit 0..32767.
/// Timed completion can advance it once past the terminator before the callback
/// observes it;
/// that extra element must be readable in the containing file.
extern s16 gCapRevealCodeIndex;

/// Cached left pen of the centered CAP text block, in screen pixels.
///
/// Used for both background edges and left-aligned lines; drawing subtracts
/// the screen-center X to obtain its pen coordinate. Playback recomputes this
/// from the current text before drawing; startup seeds provisional geometry.
extern s16 gCapTextBlockLeftX;

/// Cached first CAP text baseline, in screen pixels.
///
/// Measured for each selected text block. Drawing subtracts screen-center Y,
/// then removes vertical shake when emitting glyphs.
extern s16 gCapFirstBaselineY;

/// Cached CAP text-box height in pixels, measured from break-terminated lines.
///
/// Background and title drawing subtract it from the fixed box bottom. Empty
/// text measures zero. Selected nonterminal records refresh it before drawing.
extern s16 gCapTextBoxHeight;

/// Glyph-adjustable threshold for the elapsed CAP code-reveal update counter.
///
/// Startup seeds 7; each drawn glyph replaces it with an even value 0..14 from
/// its code attributes. A signed-byte comparison occurs before incrementing
/// the elapsed counter: from zero, a limit N advances after N + 1 eligible
/// updates. Transition, freeze and other playback gates do not count.
extern u8 gCapRevealDelayLimit;

/// Eligible CAP text-reveal updates elapsed since the last code-index advance.
///
/// Compared as a signed byte to `gCapRevealDelayLimit`'s signed-byte view before
/// incrementing. A limit N reached from zero advances after N + 1 updates and
/// resets this counter; the text-update callback sees true on that reset update.
/// Playback gates pause progression. Sequence initialization intentionally leaves
/// the counter intact. Incrementing uses the unsigned byte view then truncates.
extern s8 gCapRevealElapsedUpdates;

/// Playback ticks left before a mode-2 placed-action frame capture and prompt.
///
/// Zero is idle; dispatch arms four and eligible playback ticks count down to
/// zero before switching the saved view, capturing and spawning the prompt.
/// The task's presentation and transition gates can defer those ticks.
extern u8 gCapActionCaptureFramesRemaining;

/// Pending record-requested view phase (0 none, 1 view change, 2 view change with scene sync).
///
/// A changed saved view arms 1; a scene-control record advances it to 2 and
/// supplies that value to the synchronization receiver. View readiness or sync
/// completion clears it. Embedded text-view transitions use separate state.
extern u8 gCapPendingViewPhase;

/// Embedded text-view transition state (0 initial, 2 waiting, 1 completed).
///
/// A text control code requesting queued playback's view transition sets 2.
/// Playback waits until the stage transition finishes, then retains 1 until
/// the next sequence initializes the state. In-place view changes do not arm it.
extern s16 gCapTextViewTransitionState;

/// Number of choices laid out in the current CAP text block (zero means none).
///
/// Text drawing publishes this signed halfword for selection bounds, caret and
/// marker handling. A line break closes the final choice and advances its index
/// into a count; choice streams must close that final choice before ending.
/// Valid streams contain at most `CAP_CHOICE_CAPACITY` choices.
/// The value is refreshed by drawing, rather than reset on sequence selection.
extern s16 gCapChoiceCount;

/// Zero-based highlighted choice in the current laid-out CAP text block.
///
/// Stable selection is in [0, choice count); directional input tentatively
/// steps the signed halfword, then restores the prior index if out of range.
/// Startup and record advancement reset it to zero, also used with no choices.
/// Confirmation copies it to `gCapConfirmedChoiceIndex` before resetting it.
extern s16 gCapSelectedChoiceIndex;

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

/// Sequence view-change suppression choices; embedded text controls are independent.
enum {
    CAP_SEQUENCE_VIEW_CHANGES_ENABLED    = 0,
    CAP_SEQUENCE_VIEW_CHANGES_SUPPRESSED = 1
};

/// Suppresses view changes requested by sequence view-control records when nonzero.
///
/// Sequence startup clears this byte; an event-script skip sets it. Embedded
/// text-view controls have a separate path and do not consult this latch.
extern u8 gCapSuppressViewChanges;

/// Eligible playback frames left displaying completed timed-record text.
///
/// Loaded from the record's displayFrames byte, then adjusted for scene-sync
/// lateness and clamped at zero. Counts down before the pause begins; a resume
/// message clears both waits. Halfword storage retains truncating arithmetic.
extern s16 gCapDisplayFramesRemaining;

/// Eligible playback frames left in a timed record's following text-free pause.
///
/// Zero permits advancement; `CAP_SEQUENCE_PAUSE_UNTIL_RESUMED` (255) stays
/// pending until a resume message clears both waits. Initialized from the
/// record byte and adjusted for scene-sync lateness with halfword truncation.
extern s16 gCapPauseFramesRemaining;

/// CAP sequence view-selector interpretation; every nonzero byte selects direct.
enum { CAP_VIEW_SELECTOR_LOGICAL = 0,
       CAP_VIEW_SELECTOR_DIRECT  = 1 };

/// Uses direct sequence view indices when nonzero, logical view ids when zero.
///
/// EVS stores the low operand byte. Reset and interpreter startup/end select
/// logical ids; logical selector zero bypasses mapping. Direct mode preserves
/// the record's full view byte. Applies to sequence view-control records only.
extern u8 gCapUseDirectViewIndex;

/// Advances the selected CAP record's text, choice, action and view-control playback.
///
/// Called each unfrozen playback tick after the wrapper's state-0 setup. States
/// 0/1 process records; reaching a terminator starts states 2..4's three-tick
/// hold, and state 5 releases playback and the task. Neither spawn argument is
/// read. Scene synchronization, text transitions and minimum-frame gates can
/// defer processing. Timed records reveal text, display it, then pause; untimed
/// records accept Continue or a laid-out choice after its confirmation lockout.
///
/// Requires a selected relocated sequence with each scan and look-ahead inside
/// its live CAP file, at signed-halfword indices 1..32767. Text/glyph/choice and
/// GPU storage must satisfy the text drawer's contract; choices fit
/// CAP_CHOICE_CAPACITY and their current selection and row stride must be valid.
/// View selectors must resolve in the loaded room. Placed actions (1..63) must
/// index valid current-stage two-bit state storage before prompt lookup. Child
/// actions require the live scene task. The singleton action request is borrowed across
/// frames until its receiver sets done, so playback and those resources must
/// remain live. Action-capture mode delays its capture/prompt by four ticks.
void capUpdatePlaybackTask(Task* task);

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
