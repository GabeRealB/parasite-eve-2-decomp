#include "captions.h"

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/captions.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/task.h"

CapSequenceRecord* Gp_CapTable;

s16 D_801155AC;

u16 D_801155AE;

s16 D_801155B0;

s16 D_801155B2;

s16 D_801155B4;

s16 D_801155B6;

u8 D_801155B8;

s8 D_801155B9;

u8 D_801155BA;

u8 D_801155BB;

s16 D_801155BC;

s16 D_801155BE;

s16 D_801155C0;

void func_807245B8(void);

/// Selects the CAP sequence and resets its playback and initial text state.
static inline void _capInitializeSequencePlayback(CapSequenceRecord* sequence, s16 variantKey)
{
    enum {
        CAP_FIRST_PLAYBACK_RECORD_INDEX      = 1,
        CAP_TEXT_INITIAL_LEFT_X              = 48,
        CAP_TEXT_INITIAL_BASELINE_Y          = 192,
        CAP_TEXT_SCREEN_WIDTH                = 320,
        CAP_TEXT_INITIAL_REVEAL_DELAY_FRAMES = 7,
        CAP_DEFAULT_CHOICE_STEP              = 1,
        CAP_CHOICE_CONFIRM_DELAY_FRAMES      = 15
    };

    Gp_CapEventKey = variantKey;
    Gp_CapTable    = sequence;
    D_801155AC     = 0;
    D_801155AE     = CAP_FIRST_PLAYBACK_RECORD_INDEX;
    D_801155B0     = 0;
    D_801155B2     = CAP_TEXT_INITIAL_LEFT_X;
    D_801155B4     = CAP_TEXT_INITIAL_BASELINE_Y;
    D_801155B8     = CAP_TEXT_INITIAL_REVEAL_DELAY_FRAMES;
    D_801155B2     = CAP_TEXT_SCREEN_WIDTH;
    D_80115664     = 0;
    D_8011569A     = 0;
    D_80115698     = 0;
    D_8011567A     = 0;
    D_801155C0     = 0;
    D_801156A8     = 0;
    D_801155BC     = 0;
    D_8011566E     = 0;
    D_8011566F     = 0;
    D_801155BA     = 0;
    D_801155BB     = 0;
    D_80115648     = 0;
    D_8011566A     = 0;
    D_8011565A     = 0;
    D_80115688     = 0;
    D_80115690     = 0;
    D_80115680     = CAP_DEFAULT_CHOICE_STEP;
    D_80115659     = CAP_CHOICE_CONFIRM_DELAY_FRAMES;
}

s32 capStartSequence(CapSequenceRecord* sequence, s16 playbackMode, s16 variantKey)
{
    enum {
        CAP_PLAYBACK_IN_PLACE            = 0,
        CAP_PLAYBACK_DISPLAY_TRANSITION  = 1,
        CAP_PLAYBACK_CLEAR_IF_UNSTARTED  = 3,
        CAP_PLAYBACK_TASK_BANK           = 2,
        CAP_PLAYBACK_TASK_TYPE           = 7,
        CAP_CLEAR_UNSTARTED_TASK_INDEX   = 0,
        CAP_DEBUG_FRAME_COUNTER_DISABLED = -1
    };
    CdCmdQueue* cdQueue;
    TaskDesc*   playbackDescriptor;

    cdQueue = &gCdCmdQueue;
    if (sequence == NULL) {
        return 0;
    }

    _capInitializeSequencePlayback(sequence, variantKey);
    // Save the view and decode policy that playback exit restores.
    D_8011566C = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    D_8011565C = cdQueue->imageMdecMode;
    if (gDisplayState.debugMode != 0) {
        func_807245B8();
        D_8011564A = CAP_DEBUG_FRAME_COUNTER_DISABLED;
    }

    // Slot zero is the command header; find the first record for this key.
    D_801155AE = capFindVariantRecord((s16)D_801155AE);
    if (Gp_CapTable[(s16)D_801155AE].textRef.offset == CAP_TEXT_REF_END) {
        Gp_CapTable = NULL;
        return 0;
    }

    capApplyRecordPlaybackSettings();
    D_801155B4 = capGetTextFirstBaselineY(Gp_CapTable[(s16)D_801155AE].textRef.text);
    D_801155B6 = capGetTextBlockHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
    D_80115666 = playbackMode;
    D_80115660 = NULL;
    // Queued playback borrows the selection before its task begins running.
    if (playbackMode != CAP_PLAYBACK_IN_PLACE) {
        playbackDescriptor = taskGetDesc(CAP_PLAYBACK_TASK_BANK, CAP_PLAYBACK_TASK_TYPE);
        Gp_CapTask         = displayQueueModeTask(playbackDescriptor, 0, 0, STAGE_ENTRY_RELOAD);
        if (D_80115666 != CAP_PLAYBACK_CLEAR_IF_UNSTARTED) {
            return 0;
        }
        taskSpawnFromTable(D_8010FB4C, CAP_CLEAR_UNSTARTED_TASK_INDEX, 0, 0);
        D_80115666 = CAP_PLAYBACK_DISPLAY_TRANSITION;
    } else {
        Gp_CapTask = taskSpawn(CAP_PLAYBACK_TASK_BANK, CAP_PLAYBACK_TASK_TYPE, 0, 0);
    }
    return 0;
}
