#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/stdio.h>

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "gameplay/evs_scripts.h"
#include "evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/item_menu.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"

/* Define BSS before API headers to preserve first-declaration order. */
u8 gCapSuppressViewChanges;

CapFile* gCapFile;

u8 gCapPreserveActorControl;

/// Rooms store a full word; gameplay consumes the low byte as the saved view id.
s32 D_80115694;

s16 gCapDisplayFramesRemaining;

s16 gCapPauseFramesRemaining;

u8 gCapUseDirectViewIndex;

CapCommandRef* gCapCommandEntries;

u8 D_801156A4;

s32 gCapConfirmedChoiceIndex;

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "captions.h"

/// Signed views of the u16 CAP stream's terminator, line break and spacer.
///
/// These are text codes, separate from the sequence-table reference sentinel.
enum {
    CAP_TEXT_CODE_END         = -1,
    CAP_TEXT_CODE_LINE_BREAK  = -2,
    CAP_TEXT_CODE_SPACER      = -3,
    CAP_TEXT_CODE_FAMILY_MASK = 0xFF00,
    CAP_TEXT_CODE_ICON        = 0x8400,
    CAP_TEXT_GLYPH_INDEX_MASK = 0x3FF
};

/// Pixel extents used by CAP text measurement and baseline placement.
enum {
    CAP_TEXT_SPACER_WIDTH      = 3,
    CAP_TEXT_ICON_WIDTH        = 16,
    CAP_TEXT_LINE_GAP          = 2,
    CAP_TEXT_END_LINE_ADVANCE  = 13,
    CAP_TEXT_SCREEN_WIDTH      = 320,
    CAP_TEXT_CENTER_LEFT_BIAS  = 5,
    CAP_TEXT_BOTTOM_BASELINE_Y = 208
};

/// Screen-centre and box-edge pixel offsets shared by CAP text drawing.
enum {
    CAP_TEXT_SCREEN_CENTER_X    = CAP_TEXT_SCREEN_WIDTH / 2,
    CAP_TEXT_SCREEN_CENTER_Y    = 120,
    CAP_TEXT_BOX_LEFT_X_OFFSET  = CAP_TEXT_SCREEN_CENTER_X + 7,
    CAP_TEXT_BOX_RIGHT_X_OFFSET = CAP_TEXT_SCREEN_CENTER_X + 14,
    CAP_TEXT_BOX_BOTTOM_Y       = CAP_TEXT_BOTTOM_BASELINE_Y - CAP_TEXT_SCREEN_CENTER_Y + 1,
};

static u16 _capDrawTextStream(const u16* textStream, s32 unusedDrawArg, u8 revealAll, u16 titleAndFlags);

static void _capDrawChoiceMarker(void);

static void _capFinishPlayback(Task* task);

static void _capDrawContinueCaret(s32 unusedX, s32 unusedY);

static s16 _capGetTextBlockLeftX(const u16* text);

static s16 _capGetTextLineLeftX(const u16* text, s32 selectedLineIndex);

static s32 _capGetTextLineAdvance(const u16* text);

static void _capAdvanceRecord(void);

void func_8072455C(s16 arg0, s32 arg1);

void func_807244CC(char* arg0);

void func_80724714(void);

/// Advances a marker's 8..15 grey pulse and reverses at either endpoint.
///
/// The pointers address distinct writable s32 words: a level in 8..15 and
/// direction (0 rising, 1 falling). At 8 the direction must be rising; at 15
/// it must be falling. Each call changes the level by one before reversing.
static inline void _capStepMarkerPulse(s32* greyLevel, s32* falling)
{
    if (*falling == CAP_MARKER_PULSE_RISING) {
        (*greyLevel)++;
        if (*greyLevel >= CAP_MARKER_PULSE_MAX) {
            *falling = CAP_MARKER_PULSE_FALLING;
        }
    } else {
        (*greyLevel)--;
        if (*greyLevel <= CAP_MARKER_PULSE_MIN) {
            *falling = CAP_MARKER_PULSE_RISING;
        }
    }
}

/// Applies directional input to the laid-out CAP choices and draws their marker.
///
/// The current index must select a live choice when the count is nonzero;
/// count must fit CAP_CHOICE_CAPACITY and the row stride must describe the
/// current layout. Moves accumulate with halfword wrap; an out-of-range result
/// restores the old index after all four directions have been checked.
/// A changed nonempty selection plays the cursor sound. Marker drawing also
/// advances the confirmation lockout, requiring its GPU packet/OT storage.
/// Port 0 queries run in left/up/right/down order; simultaneous presses combine.
/// With no choices the marker draws nothing and the old selection is retained.
static inline void _capUpdateChoiceSelection(void)
{
    u16 previousChoiceIndex;

    previousChoiceIndex = (u16)gCapSelectedChoiceIndex;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
        gCapSelectedChoiceIndex = (u16)gCapSelectedChoiceIndex - 1;
    }
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
        gCapSelectedChoiceIndex = (u16)gCapSelectedChoiceIndex - gCapChoiceRowStride;
    }
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
        gCapSelectedChoiceIndex = (u16)gCapSelectedChoiceIndex + 1;
    }
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
        gCapSelectedChoiceIndex = (u16)gCapSelectedChoiceIndex + gCapChoiceRowStride;
    }
    if (gCapSelectedChoiceIndex < 0) {
        gCapSelectedChoiceIndex = (s16)previousChoiceIndex;
    }
    if (gCapSelectedChoiceIndex >= D_801155BE) {
        gCapSelectedChoiceIndex = (s16)previousChoiceIndex;
    }
    if ((gCapSelectedChoiceIndex != (s16)previousChoiceIndex) && (D_801155BE != 0)) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
    }
    _capDrawChoiceMarker();
}

void capUpdatePlaybackTask(Task* task)
{
    enum {
        CAP_RECORD_BEGIN                    = 0,
        CAP_RECORD_WAIT                     = 1,
        CAP_PLAYBACK_EXIT_FIRST_STATE       = 2,
        CAP_PLAYBACK_EXIT_FINAL_STATE       = 5,
        CAP_TEXT_TRANSITION_WAIT            = 2,
        CAP_TEXT_TRANSITION_COMPLETE        = 1,
        CAP_ACTION_CAPTURE_DELAY_FRAMES     = 4,
        CAP_SCENE_SYNC_ADJUSTMENT_PENDING   = 1, // Replace with elapsed control ticks minus thirty at sync completion.
        CAP_VIEW_CHANGE_NONE                = 0,
        CAP_VIEW_CHANGE_PENDING             = 1,
        CAP_EVENT_SCRIPT_WAITING_FOR_CUE    = 0x40,
        CAP_PLAYBACK_BYTE_MASK              = 0xFF,
        CAP_PLAYBACK_ROOM_EFFECT_MESSAGE    = 3000,
        CAP_TEXTURE_MESSAGE_TASK_SLOT       = 1,
        CAP_TEXTURE_MESSAGE_DELAY_SHIFT     = 8,
        CAP_TEXTURE_MESSAGE_RECIPIENT_SHIFT = 16,
        CAP_TEXT_TITLE_FLAGS_SHIFT          = 8,
        CAP_PLAYBACK_UNUSED_TEXT_DRAW_ARG   = 128,
        CAP_PLAYBACK_UNUSED_CARET_X         = 160,
        CAP_PLAYBACK_UNUSED_CARET_Y         = 220,
        CAP_PLACED_ACTION_ALREADY_COMPLETE  = 2,
        CAP_SCENE_SYNC_SOUND_DEMO_SCENE     = 5
    };
    Task*      actionTask;
    Task*      sceneTask;
    s16        recordIndex;
    s32        logicalViewId;
    CapTextRef nextViewText;
    CapTextRef nextActionText;
    CapTextRef nextTimedText;
    CapTextRef nextConfirmedText;
    s32        nextViewId;
    s32        playbackState;
    s32        sceneSyncPhase;
    s32        matchingScenePhase;
    s32        confirmButtons;
    u8         pendingViewPhase;
    s32        recordPauseFrames;
    s32        recordFlags;
    u8         confirmSound;
    u8         recordViewIndex;
    u8         storedSceneSyncPhase;
    s32        controlFlags;
    s32        cueGateMask;
    s32        requiredCueBits;
    s8         pendingViewState;
    s8         actionCaptureFramesLeft;
    s8         hasPendingViewChange;
    s32        nextRecordForChoice;
    s32        nextRecordForReveal;

    // Playback exit spans three held callbacks before resources are released.
    gCapPlaybackStarted = CAP_PLAYBACK_STARTED;
    if (gCapDebugPlaybackFrame != CAP_PLAYBACK_DEBUG_FRAME_DISABLED) {
        gCapDebugPlaybackFrame = (u16)gCapDebugPlaybackFrame + 1;
    }
    playbackState = task->state;
    if (playbackState >= CAP_PLAYBACK_EXIT_FIRST_STATE) {
        if (playbackState >= CAP_PLAYBACK_EXIT_FINAL_STATE) {
            _capFinishPlayback(task);
            return;
        }
        task->state = playbackState + 1;
        return;
    }
    if (gCapTextViewTransitionState == CAP_TEXT_TRANSITION_WAIT) {
        if (stageIsTransitionPending() != 0) {
            return;
        }
        gCapTextViewTransitionState = CAP_TEXT_TRANSITION_COMPLETE;
    }
    // Capture the action view only after its four-tick presentation delay.
    if ((s8)gCapActionCaptureFramesRemaining > 0) {
        gCapActionCaptureFramesRemaining--;
        actionCaptureFramesLeft = gCapActionCaptureFramesRemaining;
        if (actionCaptureFramesLeft == 1) {
            return;
        }
        if (actionCaptureFramesLeft != 0) {
            return;
        }
        gCapActionReturnView                                       = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_80115694;
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), CAP_PLAYBACK_ROOM_EFFECT_MESSAGE, 0, 0);
        stageRequestFrameCapture();
        taskSpawn(ITEM_PICKUP_ACTION_TASK_BANK, ITEM_PICKUP_ACTION_TASK_TYPE, 0, &gCapActionRequest);
    }
    recordIndex        = capFindVariantRecord((s16)gCapRecordIndex);
    gCapRecordIndex    = (u16)recordIndex;
    gCapTextBlockLeftX = _capGetTextBlockLeftX(gCapSelectedSequence[recordIndex].textRef.text);
    recordFlags        = gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags;
    if (gCapAdvanceDelayFrames > 0) {
        gCapAdvanceDelayFrames = (u16)gCapAdvanceDelayFrames - 1;
        return;
    }
    if (gCapMinDisplayFramesRemaining > 0) {
        gCapMinDisplayFramesRemaining = (u16)gCapMinDisplayFramesRemaining - 1;
    }
    // Deferred scene synchronization blocks record processing until control replies.
    storedSceneSyncPhase = gCapSceneSyncPhase;
    sceneSyncPhase       = storedSceneSyncPhase & CAP_PLAYBACK_BYTE_MASK;
    if (sceneSyncPhase != CAP_SCENE_SYNC_IDLE) {
        if (sceneSyncPhase == CAP_SCENE_SYNC_DEFERRED) {
            gCapSceneSyncPhase = storedSceneSyncPhase + 1;
            return;
        }
        if (sceneSyncPhase == CAP_SCENE_SYNC_DISPATCH) {
            gCapSceneSyncPhase = storedSceneSyncPhase + 1;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_BEGIN_SCENE_SYNC, (s32)(s8)gCapPendingViewPhase, 0);
            return;
        }
        if (!(D_801156A4 & CAP_CONTROL_SCENE_SYNC_COMPLETE)) {
            return;
        }
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == CAP_SCENE_SYNC_SOUND_DEMO_SCENE) {
            sndEvtRequestScriptStart(0, 0, 0);
        }
        gCapPendingViewPhase = CAP_VIEW_CHANGE_NONE;
        gCapSceneSyncPhase   = CAP_SCENE_SYNC_IDLE;
        D_801156A4          &= CAP_PLAYBACK_BYTE_MASK ^ CAP_CONTROL_SCENE_SYNC_COMPLETE;
        // The no-view sync path compensates text timing for control ticks beyond thirty.
        if (gCapSceneSyncAdjustmentFrames == CAP_SCENE_SYNC_ADJUSTMENT_PENDING) {
            gCapSceneSyncAdjustmentFrames = (u16)D_801156BC - CAP_CONTROL_SCENE_SYNC_DELAY_FRAMES;
        }
    }
    pendingViewState = gCapPendingViewPhase;
    if (pendingViewState != 0 && gGameSession->viewReady != 0) {
        gCapPendingViewPhase = CAP_VIEW_CHANGE_NONE;
    }
    // View records reinterpret the timing bytes as a delayed message.
    if (recordFlags & CAP_SEQUENCE_VIEW_CONTROL) {
        controlFlags = D_801156A4;
        cueGateMask  = controlFlags ^ CAP_EVENT_SCRIPT_WAITING_FOR_CUE;
        cueGateMask  = cueGateMask & CAP_EVENT_SCRIPT_WAITING_FOR_CUE;
        if (recordFlags & cueGateMask) {
            return;
        }
        cueGateMask        = controlFlags & CAP_EVENT_SCRIPT_WAITING_FOR_CUE;
        requiredCueBits    = recordFlags & cueGateMask;
        matchingScenePhase = requiredCueBits & CAP_PLAYBACK_BYTE_MASK;
        if (matchingScenePhase != (recordFlags & CAP_SEQUENCE_SCENE_PHASE)) {
            return;
        }
        if (matchingScenePhase != 0) {
            D_801156A4 = controlFlags & (CAP_PLAYBACK_BYTE_MASK ^ CAP_EVENT_SCRIPT_WAITING_FOR_CUE);
        }
        if (gCapUseDirectViewIndex == CAP_VIEW_SELECTOR_LOGICAL) {
            recordViewIndex = gCapSelectedSequence[(s16)gCapRecordIndex].control.scene.view;
            logicalViewId   = recordViewIndex & CAP_PLAYBACK_BYTE_MASK;
            if (logicalViewId != 0) {
                recordViewIndex = viewFindLogicalIndex(logicalViewId);
            }
        } else {
            recordViewIndex = gCapSelectedSequence[(s16)gCapRecordIndex].control.scene.view;
        }
        if (recordFlags & CAP_SEQUENCE_DELAYED_MESSAGE) {
            taskSpawnFromTable(gCapPlaybackTaskDescriptors, CAP_TEXTURE_MESSAGE_TASK_SLOT, gCapSelectedSequence[(s16)gCapRecordIndex].control.scene.messageValue | (gCapSelectedSequence[(s16)gCapRecordIndex].control.scene.messageDelayFrames << CAP_TEXTURE_MESSAGE_DELAY_SHIFT) | (gCapSelectedSequence[(s16)gCapRecordIndex].trigger.messageRecipient << CAP_TEXTURE_MESSAGE_RECIPIENT_SHIFT), 0);
        }
        _capAdvanceRecord();
        nextViewText = gCapSelectedSequence[(s16)gCapRecordIndex].textRef;
        if (nextViewText.offset != CAP_TEXT_REF_END) {
            gCapFirstBaselineY   = capGetTextFirstBaselineY(nextViewText.text);
            gCapTextBlockLeftX   = _capGetTextBlockLeftX(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text);
            gCapTextBoxHeight    = capGetTextBlockHeight(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text);
            nextViewId           = recordViewIndex & CAP_PLAYBACK_BYTE_MASK;
            gCapPendingViewPhase = CAP_VIEW_CHANGE_NONE;
            if ((nextViewId != 0) && (nextViewId != gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view)) {
                if (gCapSuppressViewChanges == CAP_SEQUENCE_VIEW_CHANGES_ENABLED) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = recordViewIndex;
                    gCapPendingViewPhase                                       = CAP_VIEW_CHANGE_PENDING;
                    if (gDisplayState.debugMode != 0) {
                        if (gCapDebugPlaybackFrame == CAP_PLAYBACK_DEBUG_FRAME_DISABLED) {
                            gCapDebugPlaybackFrame = 0;
                        }
                        func_8072455C(gCapDebugPlaybackFrame, nextViewId);
                    }
                }
            }
            if (recordFlags & CAP_SEQUENCE_SCENE_CONTROL) {
                {
                    hasPendingViewChange = gCapPendingViewPhase;
                    if (hasPendingViewChange != 0) {
                        pendingViewPhase     = gCapPendingViewPhase;
                        gCapPendingViewPhase = pendingViewPhase + 1;
                        if (gEvsSelectedSceneKey != NULL) {
                            cdCmdSceneViewChangeNoOp();
                        }
                    } else if (!(recordFlags & CAP_SEQUENCE_SCENE_PHASE)) {
                        gCapSceneSyncAdjustmentFrames = CAP_SCENE_SYNC_ADJUSTMENT_PENDING;
                    }
                }
                gCapSceneSyncPhase = CAP_SCENE_SYNC_DEFERRED;
                return;
            }
        } else {
            task->state += 1;
            return;
        }
    } else {
        if (gCapRecordSoundIssued == CAP_RECORD_SOUND_NOT_ISSUED) {
            if (gCapSelectedSequence[(s16)gCapRecordIndex].trigger.soundAndTextFlags & CAP_SEQUENCE_SOUND_ID_MASK) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_SOUND, (gCapSelectedSequence[(s16)gCapRecordIndex].trigger.soundAndTextFlags >> 1), 0);
                gCapRecordSoundIssued = CAP_RECORD_SOUND_ISSUED;
            }
        }

        // Keep the singleton request live until its prompt or scene child finishes.
        if (gCapSelectedSequence[(s16)gCapRecordIndex].actionId != 0) {
            if (gCapRecordPhase == CAP_RECORD_BEGIN) {
                gCapActionRequest.actionId = gCapSelectedSequence[(s16)gCapRecordIndex].actionId;
                if (gCapActionRequest.actionId < (CAP_SEQUENCE_CHILD_ACTION_BASE + 1U)) {
                    if (areaGetCurrentObjectState(gCapActionRequest.actionId) == CAP_PLACED_ACTION_ALREADY_COMPLETE) {
                        gCapRecordPhase            = CAP_RECORD_WAIT;
                        gCapActionRequest.done     = 1;
                        gCapActionRequest.accepted = 1;
                        return;
                    }
                }
                if (gCapActionRequest.actionId >= (CAP_SEQUENCE_CHILD_ACTION_BASE + 1U)) {
                    sceneTask  = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    actionTask = sceneTask;
                    TASK_MESSAGE_DISPATCH_SECOND_POINTER(sceneTask, SCENE_MESSAGE_FIND_OTHER_CHILD, gCapActionRequest.actionId - CAP_SEQUENCE_CHILD_ACTION_BASE, &actionTask);
                    if (actionTask != NULL) {
                        gCapActionRequest.done     = 0;
                        gCapActionRequest.accepted = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(actionTask, CAP_ACTION_MESSAGE_REQUEST, &gCapActionRequest, 0);
                    } else {
                        gCapActionRequest.done     = 1;
                        gCapActionRequest.accepted = 1;
                    }
                } else {
                    gCapActionRequest.done = 0;
                    if (gCapPlaybackMode == CAP_PLAYBACK_DISPLAY_TRANSITION) {
                        gCapActionReturnView                                       = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_80115694;
                        taskSpawn(ITEM_PICKUP_ACTION_TASK_BANK, ITEM_PICKUP_ACTION_TASK_TYPE, 0, &gCapActionRequest);
                    } else if (gCapPlaybackMode == CAP_PLAYBACK_ACTION_CAPTURE) {
                        gCapActionCaptureFramesRemaining = CAP_ACTION_CAPTURE_DELAY_FRAMES;
                    } else {
                        displayQueueModeTask(taskGetDesc(CAP_ACTION_PROMPT_EXIT_TASK_BANK, CAP_ACTION_PROMPT_EXIT_TASK_TYPE), 0, &gCapActionRequest, STAGE_ENTRY_RELOAD);
                    }
                }
                gCapRecordPhase = CAP_RECORD_WAIT;
                return;
            }
            if (gCapActionRequest.done != 0) {
                if (gCapPlaybackMode != CAP_PLAYBACK_IN_PLACE) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = gCapActionReturnView;
                }
                gCapRecordPhase = CAP_RECORD_BEGIN;
                if (gCapActionRequest.accepted == 0) {
                    if (gCapSelectedSequence[(s16)gCapRecordIndex].control.action.fallbackKey != 0) {
                        gCapVariantKey = gCapSelectedSequence[(s16)gCapRecordIndex].control.action.fallbackKey;
                    }
                }
                _capAdvanceRecord();
                gCapRecordPhase         = CAP_RECORD_BEGIN;
                gCapRevealCodeIndex     = 0;
                gCapSelectedChoiceIndex = 0;
                nextActionText          = gCapSelectedSequence[(s16)gCapRecordIndex].textRef;
                if (nextActionText.offset == CAP_TEXT_REF_END) {
                    task->state += 1;
                    return;
                }
                gCapFirstBaselineY = capGetTextFirstBaselineY(nextActionText.text);
                gCapTextBoxHeight  = capGetTextBlockHeight(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text);
                return;
            }
        } else if (gCapRecordPhase == CAP_RECORD_WAIT) {
            // Completed text either observes frame waits or accepts a choice/continue.
            if (gCapSelectedSequence[(s16)gCapRecordIndex].control.packed & CAP_SEQUENCE_TIMING_MASK) {
                if (gCapDisplayFramesRemaining != 0) {
                    _capDrawTextStream(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text, CAP_PLAYBACK_UNUSED_TEXT_DRAW_ARG, 1, gCapSelectedSequence[(s16)gCapRecordIndex].control.text.title | ((gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << CAP_TEXT_TITLE_FLAGS_SHIFT));
                    gCapDisplayFramesRemaining = (u16)gCapDisplayFramesRemaining - 1;
                    return;
                }
                if (gCapPauseFramesRemaining == 0) {
                    _capAdvanceRecord();
                    nextTimedText = gCapSelectedSequence[(s16)gCapRecordIndex].textRef;
                    if (nextTimedText.offset == CAP_TEXT_REF_END) {
                        task->state += 1;
                    } else {
                        gCapFirstBaselineY = capGetTextFirstBaselineY(nextTimedText.text);
                        gCapTextBoxHeight  = capGetTextBlockHeight(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text);
                    }
                    gCapRecordPhase         = CAP_RECORD_BEGIN;
                    gCapRevealCodeIndex     = 0;
                    gCapSelectedChoiceIndex = 0;
                    return;
                }
                if (gCapPauseFramesRemaining != CAP_SEQUENCE_PAUSE_UNTIL_RESUMED) {
                    gCapPauseFramesRemaining = (u16)gCapPauseFramesRemaining - 1;
                    return;
                }
            } else {
                _capDrawTextStream(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text, CAP_PLAYBACK_UNUSED_TEXT_DRAW_ARG, 1, gCapSelectedSequence[(s16)gCapRecordIndex].control.text.title | ((gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << CAP_TEXT_TITLE_FLAGS_SHIFT));
                nextRecordForChoice = capFindVariantRecord((s16)gCapRecordIndex + 1);
                if (((gCapSelectedSequence[nextRecordForChoice].textRef.offset != CAP_TEXT_REF_END) && (gCapSelectedSequence[nextRecordForChoice].actionId == 0) && ((gCapSelectedSequence[nextRecordForChoice].control.text.displayFrames != 0) || (gCapSelectedSequence[nextRecordForChoice].control.text.pauseFrames == 0)) && (D_801155BE == 0) && !(gCapSelectedSequence[nextRecordForChoice].control.text.flags & CAP_SEQUENCE_VIEW_CONTROL)) || (gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & CAP_SEQUENCE_FORCE_CARET)) {
                    _capDrawContinueCaret(CAP_PLAYBACK_UNUSED_CARET_X, CAP_PLAYBACK_UNUSED_CARET_Y);
                } else {
                    D_80115664 = 0;
                }
                _capUpdateChoiceSelection();
                confirmButtons = gPadConfirmButtonMask;
                if (D_801155BE == 0) {
                    confirmButtons |= gPadCancelButtonMask;
                }
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, confirmButtons) != 0) {
                    if (D_801155BE != 0) {
                        if (gCapChoiceConfirmFramesRemaining != 0) {
                            return;
                        }
                        confirmSound = gCapChoices[gCapSelectedChoiceIndex].confirmSound;
                        if (confirmSound != CAP_CHOICE_SOUND_CONFIRM) {
                            if (confirmSound == CAP_CHOICE_SOUND_CURSOR) {
                                sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
                            }
                        } else {
                            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
                        }
                    }
                    gCapConfirmedChoiceIndex = gCapSelectedChoiceIndex;
                    if (D_801155BE != 0) {
                        gCapVariantKey = gCapChoices[gCapSelectedChoiceIndex].eventKey;
                    }
                    // Select the next record now, but hold processing until the old minimum expires.
                    gCapAdvanceDelayFrames = gCapMinDisplayFramesRemaining;
                    _capAdvanceRecord();
                    nextConfirmedText = gCapSelectedSequence[(s16)gCapRecordIndex].textRef;
                    if (nextConfirmedText.offset == CAP_TEXT_REF_END) {
                        task->state += 1;
                    } else {
                        gCapFirstBaselineY = capGetTextFirstBaselineY(nextConfirmedText.text);
                        gCapTextBoxHeight  = capGetTextBlockHeight(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text);
                    }
                    gCapRecordPhase         = CAP_RECORD_BEGIN;
                    gCapRevealCodeIndex     = 0;
                    gCapSelectedChoiceIndex = 0;
                    return;
                }
            }
        } else if ((gCapSelectedSequence[(s16)gCapRecordIndex].control.text.displayFrames != 0) && !(gCapRecordSoundAndTextFlags & CAP_SEQUENCE_INSTANT_TEXT)) {
            gCapRecordPhase = _capDrawTextStream(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text, CAP_PLAYBACK_UNUSED_TEXT_DRAW_ARG, 0, gCapSelectedSequence[(s16)gCapRecordIndex].control.text.title | ((gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << CAP_TEXT_TITLE_FLAGS_SHIFT));
            if ((s8)gCapRevealDelayLimit > D_801155B9) {
                D_801155B9 = (u8)D_801155B9 + 1;
            } else {
                D_801155B9          = 0;
                gCapRevealCodeIndex = (u16)gCapRevealCodeIndex + 1;
            }
            if (gCapTextUpdateCallback != NULL) {
                gCapTextUpdateCallback(gCapTextCursorX, gCapTextCursorY, gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text, gCapRevealCodeIndex, D_801155B9 == 0);
            }
            if (gCapRecordPhase != CAP_RECORD_BEGIN) {
                gCapPauseFramesRemaining   = gCapSelectedSequence[(s16)gCapRecordIndex].control.text.pauseFrames;
                gCapDisplayFramesRemaining = gCapSelectedSequence[(s16)gCapRecordIndex].control.text.displayFrames;
                D_80115664                 = 0;
                return;
            }
        } else {
            if (gCapSelectedSequence[(s16)gCapRecordIndex].control.packed & CAP_SEQUENCE_TIMING_MASK) {
                if (gCapSelectedSequence[(s16)gCapRecordIndex].control.text.displayFrames != 0) {
                    _capDrawTextStream(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text, CAP_PLAYBACK_UNUSED_TEXT_DRAW_ARG, 1, gCapSelectedSequence[(s16)gCapRecordIndex].control.text.title | ((gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << CAP_TEXT_TITLE_FLAGS_SHIFT));
                }
                D_80115664      = 0;
                gCapRecordPhase = CAP_RECORD_WAIT;

                recordPauseFrames          = gCapSelectedSequence[(s16)gCapRecordIndex].control.text.pauseFrames;
                gCapPauseFramesRemaining   = recordPauseFrames;
                gCapDisplayFramesRemaining = gCapSelectedSequence[(s16)gCapRecordIndex].control.text.displayFrames;
                if (recordPauseFrames < gCapSceneSyncAdjustmentFrames) {
                    gCapPauseFramesRemaining    = 0;
                    gCapDisplayFramesRemaining -= gCapSceneSyncAdjustmentFrames;
                    if (gCapDisplayFramesRemaining < 0) {
                        gCapDisplayFramesRemaining = 0;
                    }
                } else {
                    gCapPauseFramesRemaining = recordPauseFrames - (u16)gCapSceneSyncAdjustmentFrames;
                }
                gCapSceneSyncAdjustmentFrames = 0;
                return;
            }
            if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0) || (gCapRecordSoundAndTextFlags & CAP_SEQUENCE_INSTANT_TEXT)) {
                gCapRecordPhase     = _capDrawTextStream(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text, CAP_PLAYBACK_UNUSED_TEXT_DRAW_ARG, 1, gCapSelectedSequence[(s16)gCapRecordIndex].control.text.title | ((gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << CAP_TEXT_TITLE_FLAGS_SHIFT));
                nextRecordForReveal = capFindVariantRecord((s16)gCapRecordIndex + 1);
                if (((gCapSelectedSequence[nextRecordForReveal].textRef.offset != CAP_TEXT_REF_END) && (gCapSelectedSequence[nextRecordForReveal].actionId == 0) && ((gCapSelectedSequence[nextRecordForReveal].control.text.displayFrames != 0) || (gCapSelectedSequence[nextRecordForReveal].control.text.pauseFrames == 0))) || (gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & CAP_SEQUENCE_FORCE_CARET)) {
                    _capDrawContinueCaret(CAP_PLAYBACK_UNUSED_CARET_X, CAP_PLAYBACK_UNUSED_CARET_Y);
                    return;
                }
                D_80115664 = 0;
                return;
            }

            gCapRecordPhase = _capDrawTextStream(gCapSelectedSequence[(s16)gCapRecordIndex].textRef.text, CAP_PLAYBACK_UNUSED_TEXT_DRAW_ARG, 0, gCapSelectedSequence[(s16)gCapRecordIndex].control.text.title | ((gCapSelectedSequence[(s16)gCapRecordIndex].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << CAP_TEXT_TITLE_FLAGS_SHIFT));
            if ((s8)gCapRevealDelayLimit > D_801155B9) {
                D_801155B9 = (u8)D_801155B9 + 1;
                return;
            }
            D_801155B9          = 0;
            gCapRevealCodeIndex = (u16)gCapRevealCodeIndex + 1;
        }
    }
}

/// Queues two translucent black-to-green gradients behind the CAP text.
///
/// Uses cached screen-pixel left/height metrics and the live vertical shake;
/// coordinates narrow to signed halfword pixels. The draw-mode command precedes
/// both gradients in the GPU chain, selecting dithering and average blending.
/// Requires writable primitive storage for two G4s and one DR_MODE, plus OT 3;
/// those packets remain live through GPU completion. The final command has one
/// payload word even though DR_MODE reserves space for two.
static inline void _capDrawTextBackground(void)
{
    enum { CAP_TEXT_BACKGROUND_DRAW_MODE    = 0xE100020A,
           CAP_TEXT_BACKGROUND_OT_INDEX     = 3,
           CAP_TEXT_BACKGROUND_BOTTOM_GREEN = 0x40,
           CAP_TEXT_BACKGROUND_BOTTOM_BLUE  = 0x20,
           CAP_TEXT_BACKGROUND_MODE_WORDS   = 1 };
    POLY_G4* background;
    POLY_G4* backgroundCopy;
    DR_MODE* backgroundDrawMode;

    background     = gGpuPrimCursor;
    gGpuPrimCursor = background + 1;
    setPolyG4(background);
    setSemiTrans(background, 1);
    setRGB0(background, 0, 0, 0);
    setRGB1(background, 0, 0, 0);
    setRGB2(background, 0, CAP_TEXT_BACKGROUND_BOTTOM_GREEN, CAP_TEXT_BACKGROUND_BOTTOM_BLUE);
    setRGB3(background, 0, CAP_TEXT_BACKGROUND_BOTTOM_GREEN, CAP_TEXT_BACKGROUND_BOTTOM_BLUE);
    background->x0 = (u16)gCapTextBlockLeftX - CAP_TEXT_BOX_LEFT_X_OFFSET;
    background->y0 = (CAP_TEXT_BOX_BOTTOM_Y - (u16)gCapTextBoxHeight) - gDisplayState.vramYOffset;
    background->x1 = (u16)gCapTextBlockLeftX - gCapTextBlockLeftX * 2 + CAP_TEXT_BOX_RIGHT_X_OFFSET;
    background->y1 = (CAP_TEXT_BOX_BOTTOM_Y - (u16)gCapTextBoxHeight) - gDisplayState.vramYOffset;
    background->x2 = (u16)gCapTextBlockLeftX - CAP_TEXT_BOX_LEFT_X_OFFSET;
    background->y2 = CAP_TEXT_BOX_BOTTOM_Y - gDisplayState.vramYOffset;
    background->x3 = (u16)gCapTextBlockLeftX - gCapTextBlockLeftX * 2 + CAP_TEXT_BOX_RIGHT_X_OFFSET;
    background->y3 = CAP_TEXT_BOX_BOTTOM_Y - gDisplayState.vramYOffset;
    addPrim(&gGpuCurrentOt[CAP_TEXT_BACKGROUND_OT_INDEX], background);
    backgroundCopy  = gGpuPrimCursor;
    gGpuPrimCursor  = backgroundCopy + 1;
    *backgroundCopy = *background;
    addPrim(&gGpuCurrentOt[CAP_TEXT_BACKGROUND_OT_INDEX], backgroundCopy);
    backgroundDrawMode = gGpuPrimCursor;
    gGpuPrimCursor     = backgroundDrawMode + 1;
    setlen(backgroundDrawMode, CAP_TEXT_BACKGROUND_MODE_WORDS);
    backgroundDrawMode->code[0] = CAP_TEXT_BACKGROUND_DRAW_MODE;
    addPrim(&gGpuCurrentOt[CAP_TEXT_BACKGROUND_OT_INDEX], backgroundDrawMode);
}

/// Draws the current CAP text box and replays its visible text/control codes.
///
/// textStream borrows a live, halfword-aligned, 0xFFFF-terminated u16 stream.
/// revealAll == 0 draws through the inclusive global reveal index; a terminator there or an encountered
/// choice code returns 1. Other nonzero revealAll values draw through the stream,
/// lay out choices and return 1. Otherwise returns 0. unusedDrawArg is ignored.
/// titleAndFlags uses its low byte as title glyph + 1 (0 none) and bit 9 for left
/// alignment; all other bits, including the published title-bank bit, are ignored.
///
/// Requires cached screen-pixel text metrics, a valid reveal index and active
/// glyph/texture storage. Code and line indices must fit signed 16 bits; glyph
/// indices use the low ten bits, icons the low byte (0..3 in the icon table).
/// Streams must fit CAP_CHOICE_CAPACITY and close their final choice with a line
/// break to publish its count. Embedded view selectors must resolve successfully.
/// Pen X narrows to signed 16 bits; drawing removes vertical shake from pen Y.
///
/// Publishes the final pen, choice count and glyph delay; embedded view codes
/// can queue transitions or hide scene actors and the HUD. Requires live session,
/// save and stage state. OT entries 2/3 and primitive storage must stay writable
/// through GPU completion: two G4s and a DR_MODE, one FT4 per title/icon, and two
/// GT4s per glyph. The borrowed stream and glyph/icon tables are not modified.
static u16 _capDrawTextStream(const u16* textStream, s32 unusedDrawArg, u8 revealAll, u16 titleAndFlags)
{
    enum {
        CAP_TEXT_COLUMN_TOP_Y                    = -88,
        CAP_TEXT_CARET_X_OFFSET                  = 4,
        CAP_TEXT_CARET_Y_OFFSET                  = 2,
        CAP_TEXT_LEFT_ALIGN_BIT                  = 9,
        CAP_TEXT_CODE_VALUE_MASK                 = 0xFF,
        CAP_TEXT_VIEW_FAMILY_MASK                = 0x9F00,
        CAP_TEXT_CODE_VIEW                       = 0x8000,
        CAP_TEXT_VIEW_DIRECT_INDEX               = 0x2000,
        CAP_TEXT_VIEW_HIDE_ACTORS                = 0x4000,
        CAP_TEXT_VIEW_TRANSITION_WAIT            = 2,
        CAP_TEXT_VIEW_TRANSITION_FILTERED_ACTORS = 1,
        CAP_TEXT_GLYPH_PALETTE_SHIFT             = 10,
        CAP_TEXT_GLYPH_PALETTE_MASK              = 3,
        CAP_TEXT_GLYPH_DELAY_SHIFT               = 11,
        CAP_TEXT_GLYPH_DELAY_MASK                = 0xE,
        CAP_TEXT_GLYPH_GREY                      = 0x70,
        CAP_TEXT_TITLE_CLUT                      = 0x3D93,
        CAP_TEXT_ICON_CLUT                       = 0x3C00,
        CAP_TEXT_ICON_TPAGE                      = 0x1E,
        CAP_TEXT_GLYPH_CLUT_BASE                 = 0x3D50,
        CAP_TEXT_HIGHLIGHT_CLUT                  = 0x3D52,
    };
    u8                   title;
    const u16*           text;
    const u16*           codes;
    s16                  revealEndIndex;
    u16                  textComplete;
    u16                  choiceOpen;
    s16                  lineIndex;
    u8                   centerLines;
    u8                   choiceHighlighted;
    s16                  choiceIndex;
    s16                  penX;
    s32                  penY;
    s16                  codeIndex;
    s16                  viewIndex;
    u16                  code;
    s16                  controlCode;
    s16                  iconBaselineY;
    s16                  glyphLeftX;
    s16                  glyphBaselineY;
    s32                  glyphPalette;
    s32                  titleRightXOffset;
    const u16*           nextLine;
    s32                  lineBreakDependency;
    s16                  nextLineIndex;
    s16                  boxTopY;
    s32                  boxBottomY;
    POLY_FT4*            spriteQuad;
    POLY_GT4*            glyphQuad;
    POLY_GT4*            subtractGlyphQuad;
    const TextGlyphCell* icon;
    CapChoice*           choices;
    CapChoice*           choice;

    const CapTextLayout* layout;

    // Convert cached screen-space metrics to centre-relative pen coordinates.
    text              = textStream;
    layout            = &gCapTextLayout;
    choiceIndex       = 0;
    title             = titleAndFlags;
    choiceOpen        = 0;
    lineIndex         = 0;
    choiceHighlighted = 0;
    centerLines       = ((titleAndFlags >> CAP_TEXT_LEFT_ALIGN_BIT) ^ 1) & 1;
    if (centerLines) {
        penX = _capGetTextLineLeftX(textStream, 0) - CAP_TEXT_SCREEN_CENTER_X;
    } else {
        penX = (u16)gCapTextBlockLeftX - CAP_TEXT_SCREEN_CENTER_X;
    }
    penY           = (u16)gCapFirstBaselineY - CAP_TEXT_SCREEN_CENTER_Y;
    codes          = text;
    revealEndIndex = gCapRevealCodeIndex;
    code           = text[revealEndIndex];
    if (revealAll == 0) {
        if ((s16)code == CAP_TEXT_CODE_END) {
            textComplete   = 1;
            revealEndIndex = revealEndIndex - 1;
        } else {
            textComplete = 0;
        }
    } else {
        textComplete = 1;
    }

    _capDrawTextBackground();

    // The title selector uses only its low byte; the published bank bit is ignored.
    if (title) {
        spriteQuad     = gGpuPrimCursor;
        gGpuPrimCursor = spriteQuad + 1;
        setPolyFT4(spriteQuad);
        setShadeTex(spriteQuad, 1);
        title             = title - 1;
        boxBottomY        = CAP_TEXT_BOX_BOTTOM_Y;
        boxTopY           = boxBottomY - (u16)gCapTextBoxHeight;
        spriteQuad->x0    = (u16)gCapTextBlockLeftX - CAP_TEXT_BOX_LEFT_X_OFFSET;
        spriteQuad->y0    = (boxTopY - gDisplayState.vramYOffset) - gCapGlyphCells[title].height;
        titleRightXOffset = gCapGlyphCells[title].width - CAP_TEXT_BOX_LEFT_X_OFFSET;
        spriteQuad->x1    = (u16)gCapTextBlockLeftX + titleRightXOffset;
        spriteQuad->y1    = (boxTopY - gDisplayState.vramYOffset) - gCapGlyphCells[title].height;
        spriteQuad->x2    = (u16)gCapTextBlockLeftX - CAP_TEXT_BOX_LEFT_X_OFFSET;
        spriteQuad->y2    = (boxBottomY - gDisplayState.vramYOffset) - (u16)gCapTextBoxHeight;
        titleRightXOffset = gCapGlyphCells[title].width - CAP_TEXT_BOX_LEFT_X_OFFSET;
        spriteQuad->x3    = (u16)gCapTextBlockLeftX + titleRightXOffset;
        spriteQuad->y3    = (boxBottomY - gDisplayState.vramYOffset) - (u16)gCapTextBoxHeight;
        spriteQuad->u0    = gCapGlyphCells[title].u;
        spriteQuad->v0    = gCapGlyphCells[title].v;
        spriteQuad->u1    = gCapGlyphCells[title].u + gCapGlyphCells[title].width;
        spriteQuad->v1    = gCapGlyphCells[title].v;
        spriteQuad->u2    = gCapGlyphCells[title].u;
        spriteQuad->v2    = gCapGlyphCells[title].v + gCapGlyphCells[title].height;
        spriteQuad->u3    = gCapGlyphCells[title].u + gCapGlyphCells[title].width;
        spriteQuad->v3    = gCapGlyphCells[title].v + gCapGlyphCells[title].height;
        spriteQuad->clut  = CAP_TEXT_TITLE_CLUT;
        spriteQuad->tpage = getTPage(0, GPU_BLEND_ADD, gCapTexturePageVramX, gCapTexturePageVramY);
        addPrim(&gGpuCurrentOt[2], spriteQuad);
    }

    // Replay visible codes each frame, applying control codes as well as drawing.
    codeIndex = 0;
    while (1) {
        code = codes[codeIndex];
        if (revealAll == 0) {
            if (revealEndIndex < codeIndex) {
                break;
            }
        } else {
            if ((s16)code == CAP_TEXT_CODE_END) {
                break;
            }
        }
        if ((s16)code == CAP_TEXT_CODE_LINE_BREAK && choiceOpen == 1) {
            choiceIndex++;
            choiceOpen        = 0;
            choiceHighlighted = 0;
        }
        controlCode = code;
        if (controlCode == CAP_TEXT_CODE_LINE_BREAK) {
            gCapContinueCaretY = penY - CAP_TEXT_CARET_Y_OFFSET;
            gCapContinueCaretX = penX + CAP_TEXT_CARET_X_OFFSET;
            nextLineIndex      = lineIndex + 1;
            nextLine           = &codes[codeIndex + 1];
            // Retained scheduling dependencies preserve the spilled line-index update.
            asm("" : "=r"(lineBreakDependency), "+m"(*nextLine) : "r"(lineIndex));
            lineIndex = nextLineIndex;
            if (layout->vertical == 0) {
                penY += _capGetTextLineAdvance(nextLine);
                if (centerLines != 0) {
                    penX = _capGetTextLineLeftX(textStream, (s16)lineIndex) - CAP_TEXT_SCREEN_CENTER_X;
                } else {
                    penX = (u16)gCapTextBlockLeftX - CAP_TEXT_SCREEN_CENTER_X;
                }
            } else {
                asm("" : "+r"(codeIndex) : "r"(lineBreakDependency));
                penY  = CAP_TEXT_COLUMN_TOP_Y;
                penX -= _capGetTextLineAdvance(nextLine);
            }
            codeIndex++;
            continue;
        } else {
            if (controlCode == CAP_TEXT_CODE_SPACER) {
                if (layout->vertical == 0) {
                    penX += CAP_TEXT_SPACER_WIDTH;
                } else {
                    penY += CAP_TEXT_SPACER_WIDTH;
                }
                codeIndex++;
                continue;
            } else if ((code & CAP_TEXT_VIEW_FAMILY_MASK) == CAP_TEXT_CODE_VIEW) {
                // View commands are idempotent once the saved view matches their selector.
                if (code & CAP_TEXT_VIEW_DIRECT_INDEX) {
                    viewIndex = code & CAP_TEXT_CODE_VALUE_MASK;
                } else {
                    viewIndex = viewFindLogicalIndex(code & CAP_TEXT_CODE_VALUE_MASK);
                }
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != viewIndex) {
                    if (gCapPlaybackMode != CAP_PLAYBACK_IN_PLACE) {
                        stageRequestViewTransition(viewIndex, CAP_TEXT_VIEW_TRANSITION_FILTERED_ACTORS);
                        gCapTextViewTransitionState = CAP_TEXT_VIEW_TRANSITION_WAIT;
                    } else {
                        if (controlCode & CAP_TEXT_VIEW_HIDE_ACTORS) {
                            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                        }
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewIndex;
                        gGameSession->hideHud                                      = 1;
                        gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
                    }
                }
                codeIndex++;
                continue;
            } else if ((code & CAP_TEXT_CODE_FAMILY_MASK) == CAP_TEXT_CHOICE_CONFIRM || (code & CAP_TEXT_CODE_FAMILY_MASK) == CAP_TEXT_CHOICE_SILENT || (code & CAP_TEXT_CODE_FAMILY_MASK) == CAP_TEXT_CHOICE_CURSOR) {
                if (revealAll == 0) {
                    textComplete = 1;
                    break;
                }
                if (choiceOpen == 1) {
                    choiceIndex++;
                    choiceHighlighted = 0;
                }
                // Record the pen, variant key and confirm-sound class for this line.
                choiceOpen           = 1;
                choices              = gCapChoices;
                choice               = &choices[choiceIndex];
                choice->confirmSound = (controlCode & CAP_TEXT_CHOICE_SOUND_MASK) >> 8;
                choice->x            = penX;
                choice->y            = penY;
                choice->eventKey     = controlCode & CAP_TEXT_CODE_VALUE_MASK;
                if (choiceIndex == gCapSelectedChoiceIndex) {
                    choiceHighlighted = 1;
                }
                codeIndex++;
                continue;
            } else if ((code & CAP_TEXT_CODE_FAMILY_MASK) == CAP_TEXT_CODE_ICON) {
                icon           = &gCapInlineIconCells[code & CAP_TEXT_CODE_VALUE_MASK];
                spriteQuad     = gGpuPrimCursor;
                gGpuPrimCursor = spriteQuad + 1;
                setPolyFT4(spriteQuad);
                setShadeTex(spriteQuad, 1);
                spriteQuad->clut  = CAP_TEXT_ICON_CLUT;
                spriteQuad->tpage = CAP_TEXT_ICON_TPAGE;
                iconBaselineY     = (penY - gDisplayState.vramYOffset) + 1;
                spriteQuad->x0    = penX;
                spriteQuad->y0    = iconBaselineY - icon->height;
                spriteQuad->x1    = penX + icon->width;
                spriteQuad->y1    = iconBaselineY - icon->height;
                spriteQuad->x2    = penX;
                spriteQuad->y2    = iconBaselineY;
                spriteQuad->x3    = penX + icon->width;
                spriteQuad->y3    = iconBaselineY;
                spriteQuad->u0    = icon->u;
                spriteQuad->v0    = icon->v;
                spriteQuad->u1    = icon->u + icon->width;
                spriteQuad->v1    = icon->v;
                spriteQuad->u2    = icon->u;
                spriteQuad->v2    = icon->v + icon->height;
                spriteQuad->u3    = icon->u + icon->width;
                spriteQuad->v3    = icon->v + icon->height;
                addPrim(&gGpuCurrentOt[2], spriteQuad);
                penX += icon->width;
                codeIndex++;
                continue;
            } else {
                // Glyph attributes select the palette and an even reveal-delay threshold.
                gCapRevealDelayLimit = ((s16)code >> CAP_TEXT_GLYPH_DELAY_SHIFT) & CAP_TEXT_GLYPH_DELAY_MASK;
                glyphPalette         = ((s16)code >> CAP_TEXT_GLYPH_PALETTE_SHIFT) & CAP_TEXT_GLYPH_PALETTE_MASK;
                code                 = code & CAP_TEXT_GLYPH_INDEX_MASK;
                glyphBaselineY       = penY - gDisplayState.vramYOffset;
                glyphQuad            = gGpuPrimCursor;
                gGpuPrimCursor       = glyphQuad + 1;
                setPolyGT4(glyphQuad);
                glyphLeftX = penX;
                if (choiceHighlighted == 0) {
                    glyphQuad->clut = glyphPalette | CAP_TEXT_GLYPH_CLUT_BASE;
                } else {
                    glyphQuad->clut = CAP_TEXT_HIGHLIGHT_CLUT;
                }
                setShadeTex(glyphQuad, 1);
                setRGB0(glyphQuad, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY);
                setRGB1(glyphQuad, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY);
                setRGB2(glyphQuad, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY);
                setRGB3(glyphQuad, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY, CAP_TEXT_GLYPH_GREY);
                setSemiTrans(glyphQuad, 1);
                glyphQuad->tpage = getTPage(0, GPU_BLEND_ADD, gCapTexturePageVramX, gCapTexturePageVramY);
                glyphQuad->x0    = glyphLeftX;
                glyphQuad->y0    = glyphBaselineY - gCapGlyphCells[(s16)code].height;
                glyphQuad->x1    = glyphLeftX + gCapGlyphCells[(s16)code].width;
                glyphQuad->y1    = glyphBaselineY - gCapGlyphCells[(s16)code].height;
                glyphQuad->x2    = glyphLeftX;
                glyphQuad->y2    = glyphBaselineY;
                glyphQuad->x3    = glyphLeftX + gCapGlyphCells[(s16)code].width;
                glyphQuad->y3    = glyphBaselineY;
                glyphQuad->u0    = gCapGlyphCells[(s16)code].u;
                glyphQuad->v0    = gCapGlyphCells[(s16)code].v;
                glyphQuad->u1    = gCapGlyphCells[(s16)code].u + gCapGlyphCells[(s16)code].width;
                glyphQuad->v1    = gCapGlyphCells[(s16)code].v;
                glyphQuad->u2    = gCapGlyphCells[(s16)code].u;
                glyphQuad->v2    = gCapGlyphCells[(s16)code].v + gCapGlyphCells[(s16)code].height;
                glyphQuad->u3    = gCapGlyphCells[(s16)code].u + gCapGlyphCells[(s16)code].width;
                glyphQuad->v3    = gCapGlyphCells[(s16)code].v + gCapGlyphCells[(s16)code].height;
                addPrim(&gGpuCurrentOt[2], glyphQuad);
                // The second textured pass subtracts the same glyph.
                subtractGlyphQuad        = gGpuPrimCursor;
                gGpuPrimCursor           = subtractGlyphQuad + 1;
                *subtractGlyphQuad       = *glyphQuad;
                subtractGlyphQuad->tpage = getTPage(0, GPU_BLEND_SUBTRACT, gCapTexturePageVramX, gCapTexturePageVramY);
                addPrim(&gGpuCurrentOt[2], subtractGlyphQuad);
                if (layout->vertical == 0) {
                    penX = gCapGlyphCells[(s16)code].width + penX - 1;
                } else {
                    penY = gCapGlyphCells[(s16)code].height + penY - 1;
                }
            }
        }
        codeIndex++;
    }

    // A break closes an open choice, turning its index into the published count.
    gCapTextCursorX = penX;
    D_801155BE      = choiceIndex;
    gCapTextCursorY = penY - gDisplayState.vramYOffset;
    return textComplete;
}

/// Draws the pulsing marker at the highlighted CAP dialogue choice.
///
/// With choices present, decrements the confirmation lockout once per call.
/// The selected index must be within the laid-out choice count and table.
/// Choice coordinates are centre-relative pixels; drawing removes vertical shake.
static void _capDrawChoiceMarker(void)
{
    POLY_G3*         marker;
    const CapChoice* choices;
    s32              choiceIndex;
    s32              x;
    s32              y;
    s32              markerY;
    s32              grey;

    if (D_801155BE != 0) {
        // Confirmation stays locked while the initial choice frames elapse.
        if (gCapChoiceConfirmFramesRemaining != 0) {
            gCapChoiceConfirmFramesRemaining--;
        }
        marker         = gGpuPrimCursor;
        gGpuPrimCursor = marker + 1;
        choiceIndex    = gCapSelectedChoiceIndex;
        choices        = gCapChoices;
        x              = choices[choiceIndex].x;
        y              = choices[choiceIndex].y;
        markerY        = -(gDisplayState.vramYOffset + 2) + y;
        setPolyG3(marker);
        grey = (gCapChoiceMarkerPulseLevel << 7) / CAP_MARKER_PULSE_MAX;
        setRGB0(marker, grey, grey, grey);
        grey       = (gCapChoiceMarkerPulseLevel * 0xC0) / CAP_MARKER_PULSE_MAX;
        marker->x0 = x;
        marker->y0 = markerY - 5;
        marker->x1 = x - 10;
        marker->y1 = markerY - 10;
        marker->x2 = x - 10;
        marker->y2 = markerY;
        setRGB1(marker, grey, grey, grey);
        setRGB2(marker, grey, grey, grey);
        addPrim(&gGpuCurrentOt[2], marker);
        _capStepMarkerPulse(&gCapChoiceMarkerPulseLevel, &gCapChoiceMarkerPulseFalling);
    }
}

/// Restores the HUD, pre-CAP view and automatic drawing of the player and companion.
///
/// In-place playback calls this only when the session event state is idle.
/// Requires the saved view and debug frame counter from the live playback;
/// debug mode also passes that frame/view pair to the tooling hook.
static inline void _capRestoreInPlacePresentation(void)
{
    gGameSession->hideHud                                      = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = gCapSavedView;
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    if (gDisplayState.debugMode != 0) {
        func_8072455C(gCapDebugPlaybackFrame, gCapSavedView);
    }
}

/// Restores the pre-CAP presentation and releases the active playback task.
///
/// Requires the live playback globals initialized by `capStartSequence` and
/// its still-live task. Queued playback exits stage mode, requesting the saved
/// view when needed and restoring the decoder policy only on that transition.
/// In-place playback conditionally resumes actors. An idle event state also
/// restores actor visibility and the HUD. Clears the selected sequence,
/// active-playback marker and control flags before killing the task.
static void _capFinishPlayback(Task* task)
{
    enum { CAP_ROOM_EFFECT_MESSAGE_3000 = 3000,
           CAP_PLAYBACK_INACTIVE        = 0,
           CAP_CONTROL_FLAGS_NONE       = 0 };
    CdCmdQueue* cdQueue;
    char        sceneFilename[0x20];

    // Send the action-capture exit message before restoring the display mode.
    cdQueue = &gCdCmdQueue;
    if (gCapPlaybackMode == CAP_PLAYBACK_ACTION_CAPTURE) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), CAP_ROOM_EFFECT_MESSAGE_3000, 0, 0);
    }
    if (gCapPlaybackMode != CAP_PLAYBACK_IN_PLACE) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == gCapSavedView) {
            stageRequestModeTaskExit();
        } else {
            cdQueue->imageMdecMode = gCapSavedImageMdecMode;
            stageRequestViewTransitionAndModeExit(gCapSavedView);
        }
    } else {
        if (gCapPreserveActorControl == CAP_ACTOR_CONTROL_RESTORE_ON_EXIT) {
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
        }
        if (gGameSession->eventState == 0) {
            _capRestoreInPlacePresentation();
        }
    }
    // Three unsigned-halfword decimal fields fit this 32-byte debug filename.
    if (gDisplayState.debugMode != 0 && gEvsSelectedSceneKey != NULL) {
        sprintf(
            sceneFilename, gCapSceneKeyFilenameFormat, gEvsSelectedSceneKey->group, gEvsSelectedSceneKey->streamId,
            gEvsSelectedSceneKey->subId);
        func_807244CC(sceneFilename);
    }
    gCapSelectedSequence = NULL;
    gCapPlaybackStarted  = CAP_PLAYBACK_INACTIVE;
    D_801156A4           = CAP_CONTROL_FLAGS_NONE;
    taskKill(task);
}

/// Draws the pulsing continue caret at the current CAP text pen.
///
/// Counts down eligible drawing calls before drawing. Coordinates come from the text
/// pen in centre-relative pixels, with vertical shake removed when emitted.
/// Both arguments are ignored. The grey pulse runs inclusively from 8 to 15.
static void _capDrawContinueCaret(s32 unusedX, s32 unusedY)
{
    POLY_G3* caret;
    s32      grey;
    u16      x;
    u16      y;

    if (gCapContinueCaretDelayCalls != 0) {
        gCapContinueCaretDelayCalls--;
        return;
    }

    caret          = gGpuPrimCursor;
    gGpuPrimCursor = caret + 1;
    setPolyG3(caret);

    grey = (gCapContinueCaretPulseLevel << 7) / CAP_MARKER_PULSE_MAX;
    setRGB0(caret, grey, grey, grey);

    grey = (gCapContinueCaretPulseLevel * 0xC0) / CAP_MARKER_PULSE_MAX;
    setRGB1(caret, grey, grey, grey);
    setRGB2(caret, grey, grey, grey);

    x         = gCapContinueCaretX;
    y         = gCapContinueCaretY;
    caret->x0 = x + 3;
    caret->y0 = y - gDisplayState.vramYOffset;
    caret->x1 = x;
    caret->y1 = -(gDisplayState.vramYOffset + 7) + y;
    caret->x2 = x + 7;
    caret->y2 = -(gDisplayState.vramYOffset + 7) + y;
    addPrim(&gGpuCurrentOt[2], caret);

    _capStepMarkerPulse(&gCapContinueCaretPulseLevel, &gCapContinueCaretPulseFalling);
}

/// Returns the screen X of a centred CAP block's left pen, in pixels.
///
/// Uses the widest break-terminated line, with a five-pixel left bias on a
/// 320-pixel screen. An unfinished final line contributes no width. Glyphs
/// advance by width minus one, spacers by three, and icons by sixteen pixels.
/// The borrowed text must end with 0xFFFF before its signed-16 element index
/// overflows, and the active glyph table must cover every low-ten-bit glyph index.
static s16 _capGetTextBlockLeftX(const u16* text)
{
    s16 lineWidth = 0;
    s16 maxWidth  = 0;
    s16 codeIndex = 0;
    s16 code      = text[0];

    while (code != CAP_TEXT_CODE_END) {
        if (code == CAP_TEXT_CODE_LINE_BREAK) {
            if (lineWidth > maxWidth) {
                maxWidth = lineWidth;
            }
            lineWidth = 0;
            code      = text[++codeIndex];
        } else if (code == CAP_TEXT_CODE_SPACER) {
            lineWidth += CAP_TEXT_SPACER_WIDTH;
            code       = text[++codeIndex];
        } else if ((code & CAP_TEXT_CODE_FAMILY_MASK) == CAP_TEXT_CODE_ICON) {
            lineWidth += CAP_TEXT_ICON_WIDTH;
            code       = text[++codeIndex];
        } else if (code >= 0) {
            lineWidth += gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].width - 1;
            code       = text[++codeIndex];
        } else {
            code = text[++codeIndex];
        }
    }
    return (CAP_TEXT_SCREEN_WIDTH - maxWidth) / 2 - CAP_TEXT_CENTER_LEFT_BIAS;
}

/// Returns the screen X of a centred CAP line's left pen, in pixels.
///
/// `selectedLineIndex` is zero-based. Only a line closed by a break supplies its
/// width; an absent or unfinished line uses width zero. Uses the same five-pixel
/// left bias, glyph advances and borrowed-stream bounds as `_capGetTextBlockLeftX`.
static s16 _capGetTextLineLeftX(const u16* text, s32 selectedLineIndex)
{
    s16 lineWidth;
    s16 selectedLineWidth;
    s16 codeIndex;
    s16 lineIndex;
    s16 code;

    lineWidth         = 0;
    selectedLineWidth = 0;
    codeIndex         = 0;
    lineIndex         = 0;
    code              = text[0];
    while (code != CAP_TEXT_CODE_END) {
        if (code == CAP_TEXT_CODE_LINE_BREAK) {
            if (lineIndex == selectedLineIndex) {
                selectedLineWidth = lineWidth;
            }
            lineWidth = 0;
            codeIndex++;
            lineIndex++;
            code = text[codeIndex];
        } else if (code == CAP_TEXT_CODE_SPACER) {
            lineWidth += CAP_TEXT_SPACER_WIDTH;
            code       = text[++codeIndex];
        } else if ((code & CAP_TEXT_CODE_FAMILY_MASK) == CAP_TEXT_CODE_ICON) {
            lineWidth += CAP_TEXT_ICON_WIDTH;
            code       = text[++codeIndex];
        } else if (code >= 0) {
            lineWidth += gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].width - 1;
            code       = text[++codeIndex];
        } else {
            code = text[++codeIndex];
        }
    }
    return (CAP_TEXT_SCREEN_WIDTH - selectedLineWidth) / 2 - CAP_TEXT_CENTER_LEFT_BIAS;
}

s16 capGetTextBlockHeight(const u16* text)
{
    s16 lineHeight  = 0;
    s16 blockHeight = 0;
    s16 codeIndex   = 0;
    s16 code        = text[0];

    while (code != CAP_TEXT_CODE_END) {
        if (code == CAP_TEXT_CODE_LINE_BREAK) {
            if (lineHeight == 0) {
                lineHeight = CAP_TEXT_LINE_GAP;
            }
            blockHeight += lineHeight;
            lineHeight   = 0;
        } else if (code != CAP_TEXT_CODE_SPACER) {
            if (code >= 0) {
                if (lineHeight < gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP) {
                    lineHeight = gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP;
                }
            }
        }
        code = text[++codeIndex];
    }
    if (blockHeight == CAP_TEXT_LINE_GAP) {
        blockHeight = 0;
    }
    return blockHeight;
}

s16 capGetTextFirstBaselineY(const u16* codes)
{
    s16 lineHeight      = 0;
    s16 remainingHeight = 0;
    s16 codeIndex       = 0;
    s16 firstLineEnded  = 0;
    s16 code            = codes[0];

    while (code != CAP_TEXT_CODE_END) {
        if (code == CAP_TEXT_CODE_LINE_BREAK) {
            if (firstLineEnded) {
                if (lineHeight == 0) {
                    lineHeight = CAP_TEXT_LINE_GAP;
                }
                remainingHeight += lineHeight;
            } else {
                firstLineEnded = 1;
            }
            lineHeight = 0;
        } else if (code != CAP_TEXT_CODE_SPACER) {
            if (code >= 0) {
                if (lineHeight < gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP) {
                    lineHeight = gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP;
                }
            }
        }
        code = codes[++codeIndex];
    }
    return CAP_TEXT_BOTTOM_BASELINE_Y - remainingHeight;
}

/// Returns the baseline advance in pixels for the line starting at `text`.
///
/// Takes the greatest nonnegative glyph height plus two up to a line break,
/// returning two for an empty line. Reaching 0xFFFF instead forces thirteen,
/// even after glyphs. Other negative codes, including icons, add no height.
/// The borrowed stream must reach a break or terminator before its signed-16
/// element index overflows; the active glyph table must cover the glyph indices.
static s32 _capGetTextLineAdvance(const u16* text)
{
    s16 lineAdvance = 0;
    s16 codeIndex   = 0;
    s16 scanActive  = 1;
    s16 code        = text[0];

    do {
        if (code == CAP_TEXT_CODE_LINE_BREAK) {
            scanActive = 0;
        } else if (code == CAP_TEXT_CODE_END) {
            scanActive  = 0;
            lineAdvance = CAP_TEXT_END_LINE_ADVANCE;
        } else if (code >= 0) {
            if (lineAdvance < gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP) {
                lineAdvance = gCapGlyphCells[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP;
            }
            code = text[++codeIndex];
        } else {
            code = text[++codeIndex];
        }
    } while (scanActive);
    if (lineAdvance == 0) {
        lineAdvance = CAP_TEXT_LINE_GAP;
    }
    return lineAdvance;
}

s16 capStartSequenceSlot(s16 commandIndex, s16 playbackMode, s16 variantKey)
{
    enum { CAP_SEQUENCE_SLOT_MISSING = 1 };
    CapSequenceRecord* sequence;

    if (gCapSelectedSequence != NULL) {
        return 0;
    }

    sequence = gCapCommandEntries[commandIndex].sequence;
    if (sequence == NULL) {
        return CAP_SEQUENCE_SLOT_MISSING;
    }
    return capStartSequence(sequence, playbackMode, variantKey);
}

s32 capIsBusy(void)
{
    return gCapSelectedSequence != 0;
}

s32 capAbortPlayback(void)
{
    enum { CAP_ABORT_SUCCESS     = 0,
           CAP_ABORT_UNAVAILABLE = -1 };

    if (gCapSelectedSequence != NULL) {
        if (gCapPlaybackTask != NULL) {
            _capFinishPlayback(gCapPlaybackTask);
            return CAP_ABORT_SUCCESS;
        }
        return CAP_ABORT_UNAVAILABLE;
    }
    return CAP_ABORT_UNAVAILABLE;
}

s32 capGetVariantKey(void)
{
    return gCapVariantKey;
}

void capSetTexturePage(s16 vramX, s16 vramY)
{
    gCapTexturePageVramX = vramX;
    gCapTexturePageVramY = vramY;
}

void capSelectLoadedFile(s32 dataResourceOrdinal)
{
    s32 resourceSlotIndex;
    s32 dataResourceIndex;

    dataResourceIndex = 0;
    for (resourceSlotIndex = 0; resourceSlotIndex < ARRAY_SIZE(gFsResourceSlots); resourceSlotIndex++) {
        if (gFsResourceSlots[resourceSlotIndex].kind == FILE_SYSTEM_RESOURCE_DATA) {
            if (dataResourceIndex == dataResourceOrdinal) {
                if (gDisplayState.debugMode != 0) {
                    func_80724714();
                }
                gCapFile = gFsResourceSlots[resourceSlotIndex].data;
                capRelocateFile(gCapFile);
                break;
            }
            dataResourceIndex++;
        }
    }
}

void capReset(void)
{
    enum {
        CAP_DEFAULT_FILE_ORDINAL   = 0,
        CAP_DEFAULT_TEXTURE_VRAM_X = 384,
        CAP_DEFAULT_TEXTURE_VRAM_Y = 0
    };

    // Release the selection before restoring the bundle's default CAP resource.
    gCapSelectedSequence     = NULL;
    gCapConfirmedChoiceIndex = 0;
    gCapPlaybackStarted      = CAP_PLAYBACK_NOT_STARTED;
    capSetTexturePage(CAP_DEFAULT_TEXTURE_VRAM_X, CAP_DEFAULT_TEXTURE_VRAM_Y);
    gCapFile = NULL;
    capSelectLoadedFile(CAP_DEFAULT_FILE_ORDINAL);
    gCapUseDirectViewIndex = CAP_VIEW_SELECTOR_LOGICAL;
}

void capSetTextUpdateCallback(CapTextUpdateCallback callback)
{
    gCapTextUpdateCallback = callback;
}

void capApplyRecordPlaybackSettings(void)
{
    CapSequenceRecord* record;
    u8                 soundAndTextFlags;

    record                      = _capSequenceRecordAt(gCapSelectedSequence, (s16)gCapRecordIndex);
    soundAndTextFlags           = record->trigger.soundAndTextFlags;
    gCapRecordSoundAndTextFlags = soundAndTextFlags;
    // A minimum playback interval disables instant reveal.
    if (record->minDisplayFrames != 0) {
        gCapRecordSoundAndTextFlags = soundAndTextFlags & CAP_SEQUENCE_SOUND_ID_MASK;
    }
    gCapMinDisplayFramesRemaining = record->minDisplayFrames;
}

s32 capFindVariantRecord(s32 recordIndex)
{
    CapSequenceRecord* record;

    for (;;) {
        record = _capSequenceRecordAt(gCapSelectedSequence, recordIndex);
        if (record->textRef.offset != CAP_TEXT_REF_END && record->key != gCapVariantKey) {
            recordIndex++;
        } else {
            break;
        }
    }
    return recordIndex;
}

void capClearUnstartedSequenceTask(Task* task)
{
    enum { CAP_UNSTARTED_SEQUENCE_GRACE_STATE = 0 };

    // Give queued playback one dispatch to begin before clearing its selection.
    if (task->state > CAP_UNSTARTED_SEQUENCE_GRACE_STATE) {
        if (gCapSelectedSequence != 0 && gCapPlaybackStarted == CAP_PLAYBACK_NOT_STARTED) {
            gCapSelectedSequence = 0;
        }
        taskKill(task);
    }
    task->state++;
}

/// Sends a CAP texture-mode request using the selected actor's message protocol.
///
/// `recipient` and `textureMode` are zero-extended CAP bytes carried as message
/// words. Recipient 0 requires a live player; 1 selects the optional companion;
/// 2..17 select placed actors 0..15 in the current stage and area. Every other
/// recipient byte also reaches the placed-actor lookup after subtracting 2,
/// without range validation. That path requires an initialized scene manager.
/// An absent companion or placed actor drops the request.
///
/// Player and companion modes select texture-upload sequences. Placed actors
/// interpret the mode themselves: image selection, blinking or queued texture
/// uploads. The mode must be valid for the receiver's loaded resources. Dispatch
/// is synchronous, sends zero as the second payload and discards the result;
/// receiver tasks, work and handler code must remain live through the call.
static inline void _capDispatchTextureMessage(s32 recipient, s32 textureMode)
{
    enum {
        CAP_TEXTURE_RECIPIENT_PLAYER              = 0,
        CAP_TEXTURE_RECIPIENT_COMPANION           = 1,
        CAP_TEXTURE_RECIPIENT_PLACED_ACTOR_BASE   = 2,
        CAP_TEXTURE_MESSAGE_SET_PLACED_ACTOR_MODE = 0x7E0
    };
    Task* targetTask;

    if (recipient == CAP_TEXTURE_RECIPIENT_PLAYER) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, textureMode, 0);
    } else if (recipient == CAP_TEXTURE_RECIPIENT_COMPANION) {
        targetTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
        if (targetTask != NULL) {
            taskMessageDispatch(targetTask, GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, textureMode, 0);
        }
    } else {
        targetTask = sceneFindPlacedActor(recipient - CAP_TEXTURE_RECIPIENT_PLACED_ACTOR_BASE);
        if (targetTask != NULL) {
            taskMessageDispatch(targetTask, CAP_TEXTURE_MESSAGE_SET_PLACED_ACTOR_MODE, textureMode, 0);
        }
    }
}

void capDelayedTextureMessageTask(Task* task)
{
    enum {
        CAP_TEXTURE_MESSAGE_STATE_INIT      = 0,
        CAP_TEXTURE_MESSAGE_STATE_WAIT      = 1,
        CAP_TEXTURE_MESSAGE_BYTE_MASK       = 0xFF,
        CAP_TEXTURE_MESSAGE_DELAY_SHIFT     = 8,
        CAP_TEXTURE_MESSAGE_RECIPIENT_SHIFT = 16
    };
    s32 textureMode;
    s32 recipient;

    switch (task->state) {
        case CAP_TEXTURE_MESSAGE_STATE_INIT:
            task->killCountdown = (task->spawnArg1.value >> CAP_TEXTURE_MESSAGE_DELAY_SHIFT) & CAP_TEXTURE_MESSAGE_BYTE_MASK;
            task->state++;
            break;
        case CAP_TEXTURE_MESSAGE_STATE_WAIT:
            if (task->killCountdown == 0) {
                recipient   = (task->spawnArg1.value >> CAP_TEXTURE_MESSAGE_RECIPIENT_SHIFT) & CAP_TEXTURE_MESSAGE_BYTE_MASK;
                textureMode = task->spawnArg1.value & CAP_TEXTURE_MESSAGE_BYTE_MASK;
                _capDispatchTextureMessage(recipient, textureMode);
                taskKill(task);
            }
            // Teardown also rewrites this counter; retain the decrement afterward.
            task->killCountdown--;
            break;
    }
}

/// Selects the next record for the current CAP variant and resets its playback gates.
///
/// Requires a selected, relocated sequence and a current nonterminal index.
/// The next matching record or terminator must be within the loaded file and
/// fit a nonnegative signed halfword. The stored index increments as u16 and
/// is sign-extended for the scan. Callers handle the returned terminal selection.
/// Caret delay counts eligible draw calls; choice lockout counts visible-choice frames.
static void _capAdvanceRecord(void)
{
    enum { CAP_CONTINUE_CARET_DELAY_CALLS = 30 };

    gCapRecordIndex++;
    gCapRecordIndex                  = capFindVariantRecord((s16)gCapRecordIndex);
    gCapRecordSoundIssued            = CAP_RECORD_SOUND_NOT_ISSUED;
    gCapContinueCaretDelayCalls      = CAP_CONTINUE_CARET_DELAY_CALLS;
    gCapChoiceConfirmFramesRemaining = CAP_CHOICE_CONFIRM_DELAY_FRAMES;
    capApplyRecordPlaybackSettings();
}
