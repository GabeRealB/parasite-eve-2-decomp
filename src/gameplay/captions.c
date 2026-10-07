#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/stdio.h>

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "evs.h"
#include "gameplay/evs_scripts.h"
#include "evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

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
u8 D_80115688;

CapFile* Gp_CapFile;

u8 D_80115690;

/// Rooms store a full word; gameplay consumes the low byte as the saved view id.
s32 D_80115694;

s16 D_80115698;

s16 D_8011569A;

u8 D_8011569C;

CapCommandRef* Gp_CapCmds;

u8 D_801156A4;

s32 D_801156A8;

#include "gameplay/captions.h"
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

/// Inclusive grey-pulse levels; the upper level also normalizes vertex colours.
enum { CAP_MARKER_PULSE_MIN = 8,
       CAP_MARKER_PULSE_MAX = 15 };

u16 func_800E5578(const u16* arg0, s32 arg1, u8 arg2, u16 arg3);

static void _capDrawChoiceMarker(void);

void Gp_CapExit(Task* arg0);

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
    enum { CAP_MARKER_PULSE_RISING  = 0,
           CAP_MARKER_PULSE_FALLING = 1 };

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

void func_800E44A0(Task* task)
{
    Task*      target;
    Task*      lookupTask;
    s16        eventIndex;
    s32        viewId;
    CapTextRef sceneText;
    CapTextRef dialogText;
    CapTextRef timedText;
    CapTextRef choiceText;
    s32        nextView;
    s32        taskState;
    s32        phase;
    s32        activeViewFlags;
    s32        confirmMask;
    u16        oldChoice;
    u8         viewPhase;
    s32        pauseFrames;
    s32        eventFlags;
    u8         choiceSound;
    u8         view;
    u8         nextPhase;
    s32        capFlags;
    s32        viewFlags;
    s32        activeFlags;
    s8         savedViewPhase;
    s8         spawnDelay;
    s8         viewPending;
    s32        nextChoiceIndex;
    s32        nextTextIndex;

    D_8011565A = 1;
    if (D_8011564A != -1) {
        D_8011564A = (u16)D_8011564A + 1;
    }
    taskState = task->state;
    if (taskState >= 2) {
        if (taskState >= 5) {
            Gp_CapExit(task);
            return;
        }
        task->state = taskState + 1;
        return;
    }
    if (D_801155BC == 2) {
        if (Stage_HasTransitionFlags() != 0) {
            return;
        }
        D_801155BC = 1;
    }
    if ((s8)D_801155BA > 0) {
        D_801155BA--;
        spawnDelay = D_801155BA;
        if (spawnDelay == 1) {
            return;
        }
        if (spawnDelay != 0) {
            return;
        }
        D_8011566D                                                 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_80115694;
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), 0xBB8, 0, 0);
        stageRequestFrameCapture();
        Task_SpawnPtr(1, 0x2C, 0, &D_801155A0);
    }
    eventIndex = capFindVariantRecord((s32)(s16)D_801155AE);
    D_801155AE = (u16)eventIndex;
    D_801155B2 = _capGetTextBlockLeftX(Gp_CapTable[eventIndex].textRef.text);
    eventFlags = Gp_CapTable[(s16)D_801155AE].control.text.flags;
    if (D_8011567A > 0) {
        D_8011567A = (u16)D_8011567A - 1;
        return;
    }
    if (D_80115678 > 0) {
        D_80115678 = (u16)D_80115678 - 1;
    }
    nextPhase = D_8011566E;
    phase     = nextPhase & 0xFF;
    if (phase != 0) {
        if (phase == 1) {
            D_8011566E = nextPhase + 1;
            return;
        }
        if (phase == 2) {
            D_8011566E = nextPhase + 1;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA7, (s32)(s8)D_801155BB, 0);
            return;
        }
        if (!(D_801156A4 & 0x20)) {
            return;
        }
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 5) {
            sndEvtRequestScriptStart(0, 0, 0);
        }
        D_801155BB  = 0;
        D_8011566E  = 0;
        D_801156A4 &= 0xDF;
        if (D_8011566A == 1) {
            D_8011566A = (u16)D_801156BC - 0x1E;
        }
    }
    savedViewPhase = D_801155BB;
    if (savedViewPhase != 0 && gGameSession->viewReady != 0) {
        D_801155BB = 0;
    }
    // View records reinterpret the timing bytes as a delayed message.
    if (eventFlags & CAP_SEQUENCE_VIEW_CONTROL) {
        capFlags  = D_801156A4;
        viewFlags = capFlags ^ 0x40;
        viewFlags = viewFlags & 0x40;
        if (eventFlags & viewFlags) {
            return;
        }
        viewFlags       = capFlags & 0x40;
        activeFlags     = eventFlags & viewFlags;
        activeViewFlags = activeFlags & 0xFF;
        if (activeViewFlags != (eventFlags & CAP_SEQUENCE_SCENE_PHASE)) {
            return;
        }
        if (activeViewFlags != 0) {
            D_801156A4 = capFlags & 0xBF;
        }
        if (D_8011569C == 0) {
            view   = Gp_CapTable[(s16)D_801155AE].control.scene.view;
            viewId = view & 0xFF;
            if (viewId != 0) {
                view = Gp_FindViewIndex(viewId);
            }
        } else {
            view = Gp_CapTable[(s16)D_801155AE].control.scene.view;
        }
        if (eventFlags & CAP_SEQUENCE_DELAYED_MESSAGE) {
            taskSpawnFromTable(D_8010FB4C, 1, Gp_CapTable[(s16)D_801155AE].control.scene.messageValue | (Gp_CapTable[(s16)D_801155AE].control.scene.messageDelayFrames << 8) | (Gp_CapTable[(s16)D_801155AE].trigger.messageRecipient << 0x10), 0);
        }
        _capAdvanceRecord();
        sceneText = Gp_CapTable[(s16)D_801155AE].textRef;
        if (sceneText.offset != CAP_TEXT_REF_END) {
            D_801155B4 = capGetTextFirstBaselineY(sceneText.text);
            D_801155B2 = _capGetTextBlockLeftX(Gp_CapTable[(s16)D_801155AE].textRef.text);
            D_801155B6 = capGetTextBlockHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
            nextView   = view & 0xFF;
            D_801155BB = 0;
            if ((nextView != 0) && (nextView != gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view)) {
                if (D_80115688 == 0) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = view;
                    D_801155BB                                                 = 1;
                    if (gDisplayState.debugMode != 0) {
                        if (D_8011564A == -1) {
                            D_8011564A = 0;
                        }
                        func_8072455C(D_8011564A, nextView);
                    }
                }
            }
            if (eventFlags & CAP_SEQUENCE_SCENE_CONTROL) {
                {
                    viewPending = D_801155BB;
                    if (viewPending != 0) {
                        viewPhase  = D_801155BB;
                        D_801155BB = viewPhase + 1;
                        if (D_801156F4.sceneKey != NULL) {
                            cdCmdSceneViewChangeNoOp();
                        }
                    } else if (!(eventFlags & CAP_SEQUENCE_SCENE_PHASE)) {
                        D_8011566A = 1;
                    }
                }
                D_8011566E = 1;
                return;
            }
        } else {
            task->state += 1;
            return;
        }
    } else {
        if (D_80115648 == 0) {
            if (Gp_CapTable[(s16)D_801155AE].trigger.soundAndTextFlags & CAP_SEQUENCE_SOUND_ID_MASK) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_SOUND, (Gp_CapTable[(s16)D_801155AE].trigger.soundAndTextFlags >> 1), 0);
                D_80115648 = 1;
            }
        }

        if (Gp_CapTable[(s16)D_801155AE].actionId != 0) {
            if (D_801155AC == 0) {
                D_801155A0.actionId = Gp_CapTable[(s16)D_801155AE].actionId;
                if (D_801155A0.actionId < (CAP_SEQUENCE_CHILD_ACTION_BASE + 1U)) {
                    if (areaGetCurrentObjectState(D_801155A0.actionId) == 2) {
                        D_801155AC          = 1;
                        D_801155A0.done     = 1;
                        D_801155A0.accepted = 1;
                        return;
                    }
                }
                if (D_801155A0.actionId >= (CAP_SEQUENCE_CHILD_ACTION_BASE + 1U)) {
                    lookupTask = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    target     = lookupTask;
                    TASK_MESSAGE_DISPATCH_SECOND_POINTER(lookupTask, SCENE_MESSAGE_FIND_OTHER_CHILD, D_801155A0.actionId - CAP_SEQUENCE_CHILD_ACTION_BASE, &target);
                    if (target != NULL) {
                        D_801155A0.done     = 0;
                        D_801155A0.accepted = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(target, CAP_ACTION_MESSAGE_REQUEST, &D_801155A0, 0);
                    } else {
                        D_801155A0.done     = 1;
                        D_801155A0.accepted = 1;
                    }
                } else {
                    D_801155A0.done = 0;
                    if (D_80115666 == 1) {
                        D_8011566D                                                 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_80115694;
                        Task_SpawnPtr(1, 0x2C, 0, &D_801155A0);
                    } else if (D_80115666 == 2) {
                        D_801155BA = 4;
                    } else {
                        displayQueueModeTask(taskGetDesc(9U, 0xBU), 0, &D_801155A0, STAGE_ENTRY_RELOAD);
                    }
                }
                D_801155AC = 1;
                return;
            }
            if (D_801155A0.done != 0) {
                if (D_80115666 != 0) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_8011566D;
                }
                D_801155AC = 0;
                if (D_801155A0.accepted == 0) {
                    if (Gp_CapTable[(s16)D_801155AE].control.action.fallbackKey != 0) {
                        Gp_CapEventKey = Gp_CapTable[(s16)D_801155AE].control.action.fallbackKey;
                    }
                }
                _capAdvanceRecord();
                D_801155AC = 0;
                D_801155B0 = 0;
                D_801155C0 = 0;
                dialogText = Gp_CapTable[(s16)D_801155AE].textRef;
                if (dialogText.offset == CAP_TEXT_REF_END) {
                    task->state += 1;
                    return;
                }
                D_801155B4 = capGetTextFirstBaselineY(dialogText.text);
                D_801155B6 = capGetTextBlockHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
                return;
            }
        } else if (D_801155AC == 1) {
            if (Gp_CapTable[(s16)D_801155AE].control.packed & CAP_SEQUENCE_TIMING_MASK) {
                if (D_80115698 != 0) {
                    func_800E5578(Gp_CapTable[(s16)D_801155AE].textRef.text, 0x80, 1, Gp_CapTable[(s16)D_801155AE].control.text.title | ((Gp_CapTable[(s16)D_801155AE].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << 8));
                    D_80115698 = (u16)D_80115698 - 1;
                    return;
                }
                if (D_8011569A == 0) {
                    _capAdvanceRecord();
                    timedText = Gp_CapTable[(s16)D_801155AE].textRef;
                    if (timedText.offset == CAP_TEXT_REF_END) {
                        task->state += 1;
                    } else {
                        D_801155B4 = capGetTextFirstBaselineY(timedText.text);
                        D_801155B6 = capGetTextBlockHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
                    }
                    D_801155AC = 0;
                    D_801155B0 = 0;
                    D_801155C0 = 0;
                    return;
                }
                if (D_8011569A != CAP_SEQUENCE_PAUSE_UNTIL_RESUMED) {
                    D_8011569A = (u16)D_8011569A - 1;
                    return;
                }
            } else {
                func_800E5578(Gp_CapTable[(s16)D_801155AE].textRef.text, 0x80, 1, Gp_CapTable[(s16)D_801155AE].control.text.title | ((Gp_CapTable[(s16)D_801155AE].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << 8));
                nextChoiceIndex = capFindVariantRecord((s16)D_801155AE + 1);
                if (((Gp_CapTable[nextChoiceIndex].textRef.offset != CAP_TEXT_REF_END) && (Gp_CapTable[nextChoiceIndex].actionId == 0) && ((Gp_CapTable[nextChoiceIndex].control.text.displayFrames != 0) || (Gp_CapTable[nextChoiceIndex].control.text.pauseFrames == 0)) && (D_801155BE == 0) && !(Gp_CapTable[nextChoiceIndex].control.text.flags & CAP_SEQUENCE_VIEW_CONTROL)) || (Gp_CapTable[(s16)D_801155AE].control.text.flags & CAP_SEQUENCE_FORCE_CARET)) {
                    _capDrawContinueCaret(0xA0, 0xDC);
                } else {
                    D_80115664 = 0;
                }
                oldChoice = (u16)D_801155C0;
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                    D_801155C0 = (u16)D_801155C0 - 1;
                }
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
                    D_801155C0 = (u16)D_801155C0 - D_80115680;
                }
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                    D_801155C0 = (u16)D_801155C0 + 1;
                }
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
                    D_801155C0 = (u16)D_801155C0 + D_80115680;
                }
                if (D_801155C0 < 0) {
                    D_801155C0 = (s16)oldChoice;
                }
                if (D_801155C0 >= D_801155BE) {
                    D_801155C0 = (s16)oldChoice;
                }
                if ((D_801155C0 != (s16)oldChoice) && (D_801155BE != 0)) {
                    sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
                }
                _capDrawChoiceMarker();
                confirmMask = Pad_MaskConfirm;
                if (D_801155BE == 0) {
                    confirmMask |= Pad_MaskCancel;
                }
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, confirmMask) != 0) {
                    if (D_801155BE != 0) {
                        if (D_80115659 != 0) {
                            return;
                        }
                        choiceSound = D_801155D0[D_801155C0].confirmSound;
                        if (choiceSound != CAP_CHOICE_SOUND_CONFIRM) {
                            if (choiceSound == CAP_CHOICE_SOUND_CURSOR) {
                                sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
                            }
                        } else {
                            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
                        }
                    }
                    D_801156A8 = (s32)D_801155C0;
                    if (D_801155BE != 0) {
                        Gp_CapEventKey = (s16)D_801155D0[D_801155C0].eventKey;
                    }
                    D_8011567A = (s16)(u16)D_80115678;
                    _capAdvanceRecord();
                    choiceText = Gp_CapTable[(s16)D_801155AE].textRef;
                    if (choiceText.offset == CAP_TEXT_REF_END) {
                        task->state += 1;
                    } else {
                        D_801155B4 = capGetTextFirstBaselineY(choiceText.text);
                        D_801155B6 = capGetTextBlockHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
                    }
                    D_801155AC = 0;
                    D_801155B0 = 0;
                    D_801155C0 = 0;
                    return;
                }
            }
        } else if ((Gp_CapTable[(s16)D_801155AE].control.text.displayFrames != 0) && !(D_80115670 & CAP_SEQUENCE_INSTANT_TEXT)) {
            D_801155AC = func_800E5578(Gp_CapTable[(s16)D_801155AE].textRef.text, 0x80, 0, Gp_CapTable[(s16)D_801155AE].control.text.title | ((Gp_CapTable[(s16)D_801155AE].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << 8));
            if ((s8)D_801155B8 > D_801155B9) {
                D_801155B9 = (u8)D_801155B9 + 1;
            } else {
                D_801155B9 = 0;
                D_801155B0 = (u16)D_801155B0 + 1;
            }
            if (D_80115660 != 0) {
                D_80115660(D_80115650, D_80115652, Gp_CapTable[(s16)D_801155AE].textRef.text, D_801155B0, D_801155B9 == 0);
            }
            if (D_801155AC != 0) {
                D_8011569A = Gp_CapTable[(s16)D_801155AE].control.text.pauseFrames;
                D_80115698 = Gp_CapTable[(s16)D_801155AE].control.text.displayFrames;
                D_80115664 = 0;
                return;
            }
        } else {
            if (Gp_CapTable[(s16)D_801155AE].control.packed & CAP_SEQUENCE_TIMING_MASK) {
                if (Gp_CapTable[(s16)D_801155AE].control.text.displayFrames != 0) {
                    func_800E5578(Gp_CapTable[(s16)D_801155AE].textRef.text, 0x80, 1, Gp_CapTable[(s16)D_801155AE].control.text.title | ((Gp_CapTable[(s16)D_801155AE].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << 8));
                }
                D_80115664 = 0;
                D_801155AC = 1;

                pauseFrames = Gp_CapTable[(s16)D_801155AE].control.text.pauseFrames;
                D_8011569A  = pauseFrames;
                D_80115698  = Gp_CapTable[(s16)D_801155AE].control.text.displayFrames;
                if (pauseFrames < D_8011566A) {
                    D_8011569A  = 0;
                    D_80115698 -= D_8011566A;
                    if (D_80115698 < 0) {
                        D_80115698 = 0;
                    }
                } else {
                    D_8011569A = pauseFrames - (u16)D_8011566A;
                }
                D_8011566A = 0;
                return;
            }
            if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) || (D_80115670 & CAP_SEQUENCE_INSTANT_TEXT)) {
                D_801155AC    = func_800E5578(Gp_CapTable[(s16)D_801155AE].textRef.text, 0x80, 1, Gp_CapTable[(s16)D_801155AE].control.text.title | ((Gp_CapTable[(s16)D_801155AE].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << 8));
                nextTextIndex = capFindVariantRecord((s16)D_801155AE + 1);
                if (((Gp_CapTable[nextTextIndex].textRef.offset != CAP_TEXT_REF_END) && (Gp_CapTable[nextTextIndex].actionId == 0) && ((Gp_CapTable[nextTextIndex].control.text.displayFrames != 0) || (Gp_CapTable[nextTextIndex].control.text.pauseFrames == 0))) || (Gp_CapTable[(s16)D_801155AE].control.text.flags & CAP_SEQUENCE_FORCE_CARET)) {
                    _capDrawContinueCaret(0xA0, 0xDC);
                    return;
                }
                D_80115664 = 0;
                return;
            }

            D_801155AC = func_800E5578(Gp_CapTable[(s16)D_801155AE].textRef.text, 0x80, 0, Gp_CapTable[(s16)D_801155AE].control.text.title | ((Gp_CapTable[(s16)D_801155AE].control.text.flags & (CAP_SEQUENCE_LEFT_ALIGN | CAP_SEQUENCE_TITLE_BANK)) << 8));
            if ((s8)D_801155B8 > D_801155B9) {
                D_801155B9 = (u8)D_801155B9 + 1;
                return;
            }
            D_801155B9 = 0;
            D_801155B0 = (u16)D_801155B0 + 1;
        }
    }
}

u16 func_800E5578(const u16* arg0, s32 arg1, u8 arg2, u16 arg3)
{
    u8             title;
    u8             flagA;
    const u16*     text;
    const u16*     body;
    s16            lineEnd;
    u16            ret;
    u16            inChoice;
    s16            lineIdx;
    u8             centered;
    u8             selected;
    s16            nChoice;
    s16            x;
    s32            y;
    s16            i;
    s16            sel;
    u16            code;
    s16            attr;
    s16            sc;
    s16            t;
    s16            glyphY;
    s32            palette;
    s32            titleWidth;
    const u16*     next;
    s32            g;
    s16            t2;
    s16            top;
    s32            base59;
    POLY_G4*       bg;
    POLY_G4*       bg2;
    DR_MODE*       dm;
    POLY_FT4*      ft;
    POLY_GT4*      gt;
    POLY_GT4*      gt2;
    TextGlyphCell* icon;
    CapChoice*     choices;
    CapChoice*     choice;

    const CapTextLayout* layout;

    text     = arg0;
    layout   = &D_80097518;
    nChoice  = 0;
    title    = arg3;
    inChoice = 0;
    lineIdx  = 0;
    selected = 0;
    centered = ((arg3 >> 9) ^ 1) & 1;
    flagA    = arg2;
    if (centered) {
        x = _capGetTextLineLeftX(arg0, 0) - 0xA0;
    } else {
        x = (u16)D_801155B2 - 0xA0;
    }
    y       = (u16)D_801155B4 - 0x78;
    body    = text;
    lineEnd = D_801155B0;
    code    = text[lineEnd];
    if (flagA == 0) {
        if ((s16)code == -1) {
            ret     = 1;
            lineEnd = lineEnd - 1;
        } else {
            ret = 0;
        }
    } else {
        ret = 1;
    }

    bg             = gGpuPrimCursor;
    gGpuPrimCursor = bg + 1;
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_801155B2 - 0xA7;
    bg->y0 = (0x59 - (u16)D_801155B6) - gDisplayState.vramYOffset;
    bg->x1 = (u16)D_801155B2 - D_801155B2 * 2 + 0xAE;
    bg->y1 = (0x59 - (u16)D_801155B6) - gDisplayState.vramYOffset;
    bg->x2 = (u16)D_801155B2 - 0xA7;
    bg->y2 = 0x59 - gDisplayState.vramYOffset;
    bg->x3 = (u16)D_801155B2 - D_801155B2 * 2 + 0xAE;
    bg->y3 = 0x59 - gDisplayState.vramYOffset;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = gGpuPrimCursor;
    gGpuPrimCursor = bg2 + 1;
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = gGpuPrimCursor;
    gGpuPrimCursor = dm + 1;
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    if (title) {
        ft             = gGpuPrimCursor;
        gGpuPrimCursor = ft + 1;
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        base59     = 0x59;
        top        = base59 - (u16)D_801155B6;
        ft->x0     = (u16)D_801155B2 - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - Gp_CapGlyphs[title].height;
        titleWidth = Gp_CapGlyphs[title].width - 0xA7;
        ft->x1     = (u16)D_801155B2 + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - Gp_CapGlyphs[title].height;
        ft->x2     = (u16)D_801155B2 - 0xA7;
        ft->y2     = (base59 - gDisplayState.vramYOffset) - (u16)D_801155B6;
        titleWidth = Gp_CapGlyphs[title].width - 0xA7;
        ft->x3     = (u16)D_801155B2 + titleWidth;
        ft->y3     = (base59 - gDisplayState.vramYOffset) - (u16)D_801155B6;
        ft->u0     = Gp_CapGlyphs[title].u;
        ft->v0     = Gp_CapGlyphs[title].v;
        ft->u1     = Gp_CapGlyphs[title].u + Gp_CapGlyphs[title].width;
        ft->v1     = Gp_CapGlyphs[title].v;
        ft->u2     = Gp_CapGlyphs[title].u;
        ft->v2     = Gp_CapGlyphs[title].v + Gp_CapGlyphs[title].height;
        ft->u3     = Gp_CapGlyphs[title].u + Gp_CapGlyphs[title].width;
        ft->v3     = Gp_CapGlyphs[title].v + Gp_CapGlyphs[title].height;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_80115654, D_80115656);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    i = 0;
    while (1) {
        code = body[i];
        if (flagA == 0) {
            if (lineEnd < i) {
                break;
            }
        } else {
            if ((s16)code == -1) {
                break;
            }
        }
        if ((s16)code == -2 && inChoice == 1) {
            nChoice++;
            inChoice = 0;
            selected = 0;
        }
        sc   = code;
        attr = code;
        if (sc == -2) {
            Gp_CapCaretY = y - 2;
            Gp_CapCaretX = x + 4;
            t2           = lineIdx + 1;
            next         = &body[i + 1];
            asm("" : "=r"(g), "+m"(*next) : "r"(lineIdx));
            lineIdx = t2;
            if (layout->vertical == 0) {
                y += _capGetTextLineAdvance(next);
                if (centered != 0) {
                    x = _capGetTextLineLeftX(arg0, (s16)lineIdx) - 0xA0;
                } else {
                    x = (u16)D_801155B2 - 0xA0;
                }
            } else {
                asm("" : "+r"(i) : "r"(g));
                y  = -0x58;
                x -= _capGetTextLineAdvance(next);
            }
            i++;
            continue;
        } else {
            if (sc == -3) {
                if (layout->vertical == 0) {
                    x += 3;
                } else {
                    y += 3;
                }
                i++;
                continue;
            } else if ((code & 0x9F00) == 0x8000) {
                if (code & 0x2000) {
                    sel = code & 0xFF;
                } else {
                    sel = Gp_FindViewIndex(code & 0xFF);
                }
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != sel) {
                    if (D_80115666 != 0) {
                        Stage_BeginTransition(sel, 1);
                        D_801155BC = 2;
                    } else {
                        if (attr & 0x4000) {
                            Gp_MsgPlayer3F3(0);
                            Gp_MsgAlly3F3(0);
                        }
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = sel;
                        gGameSession->hideHud                                      = 1;
                        gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
                    }
                }
                i++;
                continue;
            } else if ((code & 0xFF00) == CAP_TEXT_CHOICE_CONFIRM || (code & 0xFF00) == CAP_TEXT_CHOICE_SILENT || (code & 0xFF00) == CAP_TEXT_CHOICE_CURSOR) {
                if (flagA == 0) {
                    ret = 1;
                    break;
                }
                if (inChoice == 1) {
                    nChoice++;
                    selected = 0;
                }
                // Record the pen, variant key and confirm-sound class for this line.
                inChoice             = 1;
                choices              = D_801155D0;
                choice               = &choices[nChoice];
                choice->confirmSound = (attr & CAP_TEXT_CHOICE_SOUND_MASK) >> 8;
                choice->x            = x;
                choice->y            = y;
                choice->eventKey     = attr & 0xFF;
                if (nChoice == D_801155C0) {
                    selected = 1;
                }
                i++;
                continue;
            } else if ((code & 0xFF00) == 0x8400) {
                icon           = &D_8010FB70[code & 0xFF];
                ft             = gGpuPrimCursor;
                gGpuPrimCursor = ft + 1;
                setlen(ft, 9);
                setcode(ft, 0x2D);
                ft->clut  = 0x3C00;
                ft->tpage = 0x1E;
                t         = (y - gDisplayState.vramYOffset) + 1;
                ft->x0    = x;
                ft->y0    = t - icon->height;
                ft->x1    = x + icon->width;
                ft->y1    = t - icon->height;
                ft->x2    = x;
                ft->y2    = t;
                ft->x3    = x + icon->width;
                ft->y3    = t;
                ft->u0    = icon->u;
                ft->v0    = icon->v;
                ft->u1    = icon->u + icon->width;
                ft->v1    = icon->v;
                ft->u2    = icon->u;
                ft->v2    = icon->v + icon->height;
                ft->u3    = icon->u + icon->width;
                ft->v3    = icon->v + icon->height;
                addPrim(&gGpuCurrentOt[2], ft);
                x += icon->width;
                i++;
                continue;
            } else {
                D_801155B8     = ((s16)code >> 11) & 0xE;
                palette        = ((s16)code >> 10) & 3;
                code           = code & 0x3FF;
                glyphY         = y - gDisplayState.vramYOffset;
                gt             = gGpuPrimCursor;
                gGpuPrimCursor = gt + 1;
                setlen(gt, 12);
                setcode(gt, 0x3C);
                t = x;
                if (selected == 0) {
                    gt->clut = palette | 0x3D50;
                } else {
                    gt->clut = 0x3D52;
                }
                setShadeTex(gt, 1);
                setRGB0(gt, 0x70, 0x70, 0x70);
                setRGB1(gt, 0x70, 0x70, 0x70);
                setRGB2(gt, 0x70, 0x70, 0x70);
                setRGB3(gt, 0x70, 0x70, 0x70);
                setSemiTrans(gt, 1);
                gt->tpage = getTPage(0, 1, D_80115654, D_80115656);
                gt->x0    = t;
                gt->y0    = glyphY - Gp_CapGlyphs[(s16)code].height;
                gt->x1    = t + Gp_CapGlyphs[(s16)code].width;
                gt->y1    = glyphY - Gp_CapGlyphs[(s16)code].height;
                gt->x2    = t;
                gt->y2    = glyphY;
                gt->x3    = t + Gp_CapGlyphs[(s16)code].width;
                gt->y3    = glyphY;
                gt->u0    = Gp_CapGlyphs[(s16)code].u;
                gt->v0    = Gp_CapGlyphs[(s16)code].v;
                gt->u1    = Gp_CapGlyphs[(s16)code].u + Gp_CapGlyphs[(s16)code].width;
                gt->v1    = Gp_CapGlyphs[(s16)code].v;
                gt->u2    = Gp_CapGlyphs[(s16)code].u;
                gt->v2    = Gp_CapGlyphs[(s16)code].v + Gp_CapGlyphs[(s16)code].height;
                gt->u3    = Gp_CapGlyphs[(s16)code].u + Gp_CapGlyphs[(s16)code].width;
                gt->v3    = Gp_CapGlyphs[(s16)code].v + Gp_CapGlyphs[(s16)code].height;
                addPrim(&gGpuCurrentOt[2], gt);
                gt2            = gGpuPrimCursor;
                gGpuPrimCursor = gt2 + 1;
                *gt2           = *gt;
                gt2->tpage     = getTPage(0, GPU_BLEND_SUBTRACT, D_80115654, D_80115656);
                addPrim(&gGpuCurrentOt[2], gt2);
                if (layout->vertical == 0) {
                    x = Gp_CapGlyphs[(s16)code].width + x - 1;
                } else {
                    y = Gp_CapGlyphs[(s16)code].height + y - 1;
                }
            }
        }
        i++;
    }

    D_80115650 = x;
    D_801155BE = nChoice;
    D_80115652 = y - gDisplayState.vramYOffset;
    return ret;
}

/// Draws the pulsing marker at the highlighted CAP dialogue choice.
///
/// With choices present, decrements the confirmation lockout once per call.
/// The selected index must be within the laid-out choice count and table.
/// Choice coordinates are centre-relative pixels; drawing removes vertical shake.
static void _capDrawChoiceMarker(void)
{
    POLY_G3*   marker;
    CapChoice* choices;
    s32        choiceIndex;
    s32        x;
    s32        y;
    s32        markerY;
    s32        grey;

    if (D_801155BE != 0) {
        // Confirmation stays locked while the initial choice frames elapse.
        if (D_80115659 != 0) {
            D_80115659--;
        }
        marker         = gGpuPrimCursor;
        gGpuPrimCursor = marker + 1;
        choiceIndex    = D_801155C0;
        choices        = D_801155D0;
        x              = choices[choiceIndex].x;
        y              = choices[choiceIndex].y;
        markerY        = -(gDisplayState.vramYOffset + 2) + y;
        setPolyG3(marker);
        grey = (D_8010FB80 << 7) / CAP_MARKER_PULSE_MAX;
        setRGB0(marker, grey, grey, grey);
        grey       = (D_8010FB80 * 0xC0) / CAP_MARKER_PULSE_MAX;
        marker->x0 = x;
        marker->y0 = markerY - 5;
        marker->x1 = x - 10;
        marker->y1 = markerY - 10;
        marker->x2 = x - 10;
        marker->y2 = markerY;
        setRGB1(marker, grey, grey, grey);
        setRGB2(marker, grey, grey, grey);
        addPrim(&gGpuCurrentOt[2], marker);
        _capStepMarkerPulse(&D_8010FB80, &D_8010FB84);
    }
}

void Gp_CapExit(Task* arg0)
{
    CdCmdQueue* queue;
    char        buf[0x20];

    queue = &gCdCmdQueue;
    if (D_80115666 == 2) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), 0xBB8, 0, 0);
    }
    if (D_80115666 != 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == D_8011566C) {
            stageRequestModeTaskExit();
        } else {
            queue->imageMdecMode = D_8011565C;
            Stage_BeginTransitionKind7(D_8011566C);
        }
    } else {
        if (D_80115690 == 0) {
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
        }
        if (gGameSession->eventState == 0) {
            gGameSession->hideHud                                      = 0;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_8011566C;
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            if (gDisplayState.debugMode != 0) {
                func_8072455C(D_8011564A, D_8011566C);
            }
        }
    }
    if (gDisplayState.debugMode != 0 && D_801156F4.sceneKey != 0) {
        sprintf(
            buf, Gp_StrEvsFmt, D_801156F4.sceneKey->group, D_801156F4.sceneKey->streamId,
            D_801156F4.sceneKey->subId);
        func_807244CC(buf);
    }
    Gp_CapTable = 0;
    D_8011565A  = 0;
    D_801156A4  = 0;
    taskKill(arg0);
}

/// Draws the pulsing continue caret at the current CAP text pen.
///
/// Counts down the frame delay before drawing. Coordinates come from the text
/// pen in centre-relative pixels, with vertical shake removed when emitted.
/// Both arguments are ignored. The grey pulse runs inclusively from 8 to 15.
static void _capDrawContinueCaret(s32 unusedX, s32 unusedY)
{
    POLY_G3* caret;
    s32      grey;
    u16      x;
    u16      y;

    if (Gp_CapCaretDelay != 0) {
        Gp_CapCaretDelay--;
        return;
    }

    caret          = gGpuPrimCursor;
    gGpuPrimCursor = caret + 1;
    setPolyG3(caret);

    grey = (Gp_CapCaretGrey << 7) / CAP_MARKER_PULSE_MAX;
    setRGB0(caret, grey, grey, grey);

    grey = (Gp_CapCaretGrey * 0xC0) / CAP_MARKER_PULSE_MAX;
    setRGB1(caret, grey, grey, grey);
    setRGB2(caret, grey, grey, grey);

    x         = Gp_CapCaretX;
    y         = Gp_CapCaretY;
    caret->x0 = x + 3;
    caret->y0 = y - gDisplayState.vramYOffset;
    caret->x1 = x;
    caret->y1 = -(gDisplayState.vramYOffset + 7) + y;
    caret->x2 = x + 7;
    caret->y2 = -(gDisplayState.vramYOffset + 7) + y;
    addPrim(&gGpuCurrentOt[2], caret);

    _capStepMarkerPulse(&Gp_CapCaretGrey, &Gp_CapCaretDir);
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
            lineWidth += Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].width - 1;
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
            lineWidth += Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].width - 1;
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
                if (lineHeight < Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP) {
                    lineHeight = Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP;
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
                if (lineHeight < Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP) {
                    lineHeight = Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP;
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
            if (lineAdvance < Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP) {
                lineAdvance = Gp_CapGlyphs[code & CAP_TEXT_GLYPH_INDEX_MASK].height + CAP_TEXT_LINE_GAP;
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

s32 Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2)
{
    CapSequenceRecord* entry;

    if (Gp_CapTable != 0) {
        return 0;
    }

    entry = Gp_CapCmds[arg0].sequence;
    if (entry == 0) {
        return 1;
    }
    return (s16)Gp_StartCap(entry, arg1, arg2);
}

s32 capIsBusy(void)
{
    return Gp_CapTable != 0;
}

s32 Gp_AbortCap(void)
{
    if (Gp_CapTable != 0) {
        if (Gp_CapTask != NULL) {
            Gp_CapExit(Gp_CapTask);
            return 0;
        }
        return -1;
    }
    return -1;
}

s32 capGetVariantKey(void)
{
    return Gp_CapEventKey;
}

void capSetTexturePage(s16 vramX, s16 vramY)
{
    D_80115654 = vramX;
    D_80115656 = vramY;
}

void capSelectLoadedFile(s32 dataResourceOrdinal)
{
    s32 resourceSlotIndex;
    s32 dataResourceIndex;

    dataResourceIndex = 0;
    for (resourceSlotIndex = 0; resourceSlotIndex < ARRAY_SIZE(D_8006C338); resourceSlotIndex++) {
        if (D_8006C338[resourceSlotIndex].kind == FILE_SYSTEM_RESOURCE_DATA) {
            if (dataResourceIndex == dataResourceOrdinal) {
                if (gDisplayState.debugMode != 0) {
                    func_80724714();
                }
                Gp_CapFile = D_8006C338[resourceSlotIndex].data;
                capRelocateFile(Gp_CapFile);
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
    Gp_CapTable = NULL;
    D_801156A8  = 0;
    D_8011565A  = 0;
    capSetTexturePage(CAP_DEFAULT_TEXTURE_VRAM_X, CAP_DEFAULT_TEXTURE_VRAM_Y);
    Gp_CapFile = NULL;
    capSelectLoadedFile(CAP_DEFAULT_FILE_ORDINAL);
    D_8011569C = 0;
}

void capSetTextUpdateCallback(CapTextUpdateCallback callback)
{
    D_80115660 = callback;
}

void capApplyRecordPlaybackSettings(void)
{
    CapSequenceRecord* record;
    u8                 soundAndTextFlags;

    record            = _capSequenceRecordAt(Gp_CapTable, (s16)D_801155AE);
    soundAndTextFlags = record->trigger.soundAndTextFlags;
    D_80115670        = soundAndTextFlags;
    // A minimum playback interval disables instant reveal.
    if (record->minDisplayFrames != 0) {
        D_80115670 = soundAndTextFlags & CAP_SEQUENCE_SOUND_ID_MASK;
    }
    D_80115678 = record->minDisplayFrames;
}

s32 capFindVariantRecord(s32 recordIndex)
{
    CapSequenceRecord* record;

    for (;;) {
        record = _capSequenceRecordAt(Gp_CapTable, recordIndex);
        if (record->textRef.offset != CAP_TEXT_REF_END && record->key != Gp_CapEventKey) {
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
        if (Gp_CapTable != 0 && D_8011565A == 0) {
            Gp_CapTable = 0;
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
    enum {
        CAP_CONTINUE_CARET_DELAY_CALLS  = 30,
        CAP_CHOICE_CONFIRM_DELAY_FRAMES = 15
    };

    D_801155AE++;
    D_801155AE       = capFindVariantRecord((s16)D_801155AE);
    D_80115648       = 0;
    Gp_CapCaretDelay = CAP_CONTINUE_CARET_DELAY_CALLS;
    D_80115659       = CAP_CHOICE_CONFIRM_DELAY_FRAMES;
    capApplyRecordPlaybackSettings();
}
