#include "captions.h"

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/captions.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/task.h"

CapSequenceRecord* gCapSelectedSequence;

s16 gCapRecordPhase;

u16 gCapRecordIndex;

s16 gCapRevealCodeIndex;

s16 gCapTextBlockLeftX;

s16 gCapFirstBaselineY;

s16 gCapTextBoxHeight;

u8 gCapRevealDelayLimit;

s8 gCapRevealElapsedUpdates;

u8 gCapActionCaptureFramesRemaining;

u8 gCapPendingViewPhase;

s16 gCapTextViewTransitionState;

s16 gCapChoiceCount;

s16 gCapSelectedChoiceIndex;

void func_807245B8(void);

/// Selects a CAP sequence and seeds its text, choice and scene-control state.
///
/// `sequence` borrows the slot-zero command address of a relocated CAP sequence;
/// playback records start at slot one. It must be non-null and its containing
/// file must stay loaded until playback releases the selection. Previous
/// playback must have stopped: replacing the selection does not kill its task.
/// `variantKey` is stored as a signed halfword without validation; only 0..255
/// can match the records' byte keys. No record is read here.
///
/// Initial text geometry is in screen pixels and is recomputed before drawing.
/// Reveal advances when its counter reaches the delay limit, tested before
/// incrementing; the confirmation lockout counts frames with choices drawn.
static inline void _capInitializeSequencePlayback(CapSequenceRecord* sequence, s16 variantKey)
{
    enum {
        CAP_RECORD_INITIAL_PHASE            = 0,
        CAP_FIRST_PLAYBACK_RECORD_INDEX     = 1,
        CAP_TEXT_INITIAL_LEFT_X             = 48,
        CAP_TEXT_INITIAL_BASELINE_Y         = 192,
        CAP_TEXT_INITIAL_REVEAL_DELAY_LIMIT = 7, // Advancement after eight eligible updates from a zero counter.
        CAP_DEFAULT_CHOICE_ROW_STRIDE       = 1,
        CAP_VIEW_CHANGE_NONE                = 0,
        CAP_TEXT_VIEW_TRANSITION_NONE       = 0,
    };

    // Select the borrowed sequence before its first keyed record is scanned.
    gCapVariantKey       = variantKey;
    gCapSelectedSequence = sequence;
    gCapRecordPhase      = CAP_RECORD_INITIAL_PHASE;
    gCapRecordIndex      = CAP_FIRST_PLAYBACK_RECORD_INDEX;
    gCapRevealCodeIndex  = 0;
    // The initial X preset is overwritten before any text is drawn.
    gCapTextBlockLeftX   = CAP_TEXT_INITIAL_LEFT_X;
    gCapFirstBaselineY   = CAP_TEXT_INITIAL_BASELINE_Y;
    gCapRevealDelayLimit = CAP_TEXT_INITIAL_REVEAL_DELAY_LIMIT;
    gCapTextBlockLeftX   = DISPLAY_SCREEN_WIDTH;

    // Clear progress and pending transitions; record-specific timing follows.
    D_80115664                       = 0;
    gCapPauseFramesRemaining         = 0;
    gCapDisplayFramesRemaining       = 0;
    gCapAdvanceDelayFrames           = 0;
    gCapSelectedChoiceIndex          = 0;
    gCapConfirmedChoiceIndex         = 0;
    gCapTextViewTransitionState      = CAP_TEXT_VIEW_TRANSITION_NONE;
    gCapSceneSyncPhase               = CAP_SCENE_SYNC_IDLE;
    D_8011566F                       = 0;
    gCapActionCaptureFramesRemaining = 0;
    gCapPendingViewPhase             = CAP_VIEW_CHANGE_NONE;
    gCapRecordSoundIssued            = CAP_RECORD_SOUND_NOT_ISSUED;
    gCapSceneSyncAdjustmentFrames    = 0;
    gCapPlaybackStarted              = CAP_PLAYBACK_NOT_STARTED;
    gCapSuppressViewChanges          = CAP_SEQUENCE_VIEW_CHANGES_ENABLED;
    gCapPreserveActorControl         = CAP_ACTOR_CONTROL_RESTORE_ON_EXIT;
    gCapChoiceRowStride              = CAP_DEFAULT_CHOICE_ROW_STRIDE;
    gCapChoiceConfirmFramesRemaining = CAP_CHOICE_CONFIRM_DELAY_FRAMES;
}

s32 capStartSequence(CapSequenceRecord* sequence, s16 playbackMode, s16 variantKey)
{
    enum {
        CAP_PLAYBACK_TASK_BANK         = 2,
        CAP_PLAYBACK_TASK_TYPE         = 7,
        CAP_CLEAR_UNSTARTED_TASK_INDEX = 0
    };
    CdCmdQueue* cdQueue;
    TaskDesc*   playbackDescriptor;

    cdQueue = &gCdCmdQueue;
    if (sequence == NULL) {
        return 0;
    }

    _capInitializeSequencePlayback(sequence, variantKey);
    // Save the view and decode policy that playback exit restores.
    gCapSavedView          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;
    gCapSavedImageMdecMode = cdQueue->imageMdecMode;
    if (gDisplayState.debugMode != 0) {
        func_807245B8();
        gCapDebugPlaybackFrame = CAP_PLAYBACK_DEBUG_FRAME_DISABLED;
    }

    // Slot zero is the command header; find the first record for this key.
    gCapRecordIndex = capFindVariantRecord((s16)gCapRecordIndex);
    if (gCapSelectedSequence[(s16)gCapRecordIndex].textRef.offset == CAP_TEXT_REF_END) {
        gCapSelectedSequence = NULL;
        return 0;
    }

    capApplyRecordPlaybackSettings();
    gCapFirstBaselineY     = capGetTextFirstBaselineY(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text);
    gCapTextBoxHeight      = capGetTextBlockHeight(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text);
    gCapPlaybackMode       = playbackMode;
    gCapTextUpdateCallback = NULL;
    // Queued playback borrows the selection before its task begins running.
    if (playbackMode != CAP_PLAYBACK_IN_PLACE) {
        playbackDescriptor = taskGetDesc(CAP_PLAYBACK_TASK_BANK, CAP_PLAYBACK_TASK_TYPE);
        gCapPlaybackTask   = displayQueueModeTask(playbackDescriptor, 0, 0, STAGE_ENTRY_RELOAD);
        if (gCapPlaybackMode != CAP_PLAYBACK_CLEAR_IF_UNSTARTED) {
            return 0;
        }
        taskSpawnFromTable(gCapPlaybackTaskDescriptors, CAP_CLEAR_UNSTARTED_TASK_INDEX, 0, 0);
        gCapPlaybackMode = CAP_PLAYBACK_DISPLAY_TRANSITION;
    } else {
        gCapPlaybackTask = taskSpawn(CAP_PLAYBACK_TASK_BANK, CAP_PLAYBACK_TASK_TYPE, 0, 0);
    }
    return 0;
}
