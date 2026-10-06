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

#include "captions.h"

u16 func_800E5578(const u16* arg0, s32 arg1, u8 arg2, u16 arg3);

void func_800E62C0(void);

void Gp_CapExit(Task* arg0);

/// Blinking POLY_G3 continue caret. `Gp_CapCaretDelay` is a frame delay before the
/// first draw; `Gp_CapCaretX` / `Gp_CapCaretY` are base XY; `Gp_CapCaretGrey` /
/// `Gp_CapCaretDir` pulse the vertex greys between 8 and 15.
void Gp_DrawCapCaret(s32 unusedX, s32 unusedY);

s16 Gp_CapCenterX(const u16* text);

s16 Gp_CapCenterXLine(const u16* arg0, s32 arg1);

s32 func_800E6BB8(const u16* arg0);

void func_800E704C(void);

void func_8072455C(s16 arg0, s32 arg1);

void func_807244CC(char* arg0);

void func_80724714(void);

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
        Stage_RequestImageCapture();
        Task_SpawnPtr(1, 0x2C, 0, &D_801155A0);
    }
    eventIndex = Gp_FindCapEvt((s32)(s16)D_801155AE);
    D_801155AE = (u16)eventIndex;
    D_801155B2 = Gp_CapCenterX(Gp_CapTable[eventIndex].textRef.text);
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
        func_800E704C();
        sceneText = Gp_CapTable[(s16)D_801155AE].textRef;
        if (sceneText.offset != CAP_TEXT_REF_END) {
            D_801155B4 = Gp_CapTextTopY(sceneText.text);
            D_801155B2 = Gp_CapCenterX(Gp_CapTable[(s16)D_801155AE].textRef.text);
            D_801155B6 = Gp_CapTextHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
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
                    if (Gp_GetCurBit2Flag(D_801155A0.actionId) == 2) {
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
                        Display_InitModeObj(Task_GetDesc(9U, 0xBU), 0, &D_801155A0, 0);
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
                func_800E704C();
                D_801155AC = 0;
                D_801155B0 = 0;
                D_801155C0 = 0;
                dialogText = Gp_CapTable[(s16)D_801155AE].textRef;
                if (dialogText.offset == CAP_TEXT_REF_END) {
                    task->state += 1;
                    return;
                }
                D_801155B4 = Gp_CapTextTopY(dialogText.text);
                D_801155B6 = Gp_CapTextHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
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
                    func_800E704C();
                    timedText = Gp_CapTable[(s16)D_801155AE].textRef;
                    if (timedText.offset == CAP_TEXT_REF_END) {
                        task->state += 1;
                    } else {
                        D_801155B4 = Gp_CapTextTopY(timedText.text);
                        D_801155B6 = Gp_CapTextHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
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
                nextChoiceIndex = Gp_FindCapEvt((s16)D_801155AE + 1);
                if (((Gp_CapTable[nextChoiceIndex].textRef.offset != CAP_TEXT_REF_END) && (Gp_CapTable[nextChoiceIndex].actionId == 0) && ((Gp_CapTable[nextChoiceIndex].control.text.displayFrames != 0) || (Gp_CapTable[nextChoiceIndex].control.text.pauseFrames == 0)) && (D_801155BE == 0) && !(Gp_CapTable[nextChoiceIndex].control.text.flags & CAP_SEQUENCE_VIEW_CONTROL)) || (Gp_CapTable[(s16)D_801155AE].control.text.flags & CAP_SEQUENCE_FORCE_CARET)) {
                    Gp_DrawCapCaret(0xA0, 0xDC);
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
                func_800E62C0();
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
                    func_800E704C();
                    choiceText = Gp_CapTable[(s16)D_801155AE].textRef;
                    if (choiceText.offset == CAP_TEXT_REF_END) {
                        task->state += 1;
                    } else {
                        D_801155B4 = Gp_CapTextTopY(choiceText.text);
                        D_801155B6 = Gp_CapTextHeight(Gp_CapTable[(s16)D_801155AE].textRef.text);
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
                nextTextIndex = Gp_FindCapEvt((s16)D_801155AE + 1);
                if (((Gp_CapTable[nextTextIndex].textRef.offset != CAP_TEXT_REF_END) && (Gp_CapTable[nextTextIndex].actionId == 0) && ((Gp_CapTable[nextTextIndex].control.text.displayFrames != 0) || (Gp_CapTable[nextTextIndex].control.text.pauseFrames == 0))) || (Gp_CapTable[(s16)D_801155AE].control.text.flags & CAP_SEQUENCE_FORCE_CARET)) {
                    Gp_DrawCapCaret(0xA0, 0xDC);
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
        x = Gp_CapCenterXLine(arg0, 0) - 0xA0;
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
                y += func_800E6BB8(next);
                if (centered != 0) {
                    x = Gp_CapCenterXLine(arg0, (s16)lineIdx) - 0xA0;
                } else {
                    x = (u16)D_801155B2 - 0xA0;
                }
            } else {
                asm("" : "+r"(i) : "r"(g));
                y  = -0x58;
                x -= func_800E6BB8(next);
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

void func_800E62C0(void)
{
    POLY_G3*   p;
    CapChoice* choices;
    s32        i;
    s32        x;
    s32        y;
    s32        top;
    s32        color;

    if (D_801155BE != 0) {
        if (D_80115659 != 0) {
            D_80115659--;
        }
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        i              = D_801155C0;
        choices        = D_801155D0;
        x              = choices[i].x;
        y              = choices[i].y;
        top            = -(gDisplayState.vramYOffset + 2) + y;
        setPolyG3(p);
        color = (D_8010FB80 << 7) / 15;
        setRGB0(p, color, color, color);
        color = (D_8010FB80 * 0xC0) / 15;
        p->x0 = x;
        p->y0 = top - 5;
        p->x1 = x - 10;
        p->y1 = top - 10;
        p->x2 = x - 10;
        p->y2 = top;
        setRGB1(p, color, color, color);
        setRGB2(p, color, color, color);
        addPrim(&gGpuCurrentOt[2], p);
        if (D_8010FB84 == 0) {
            D_8010FB80++;
            if (D_8010FB80 >= 15) {
                D_8010FB84 = 1;
            }
        } else {
            D_8010FB80--;
            if (D_8010FB80 < 9) {
                D_8010FB84 = 0;
            }
        }
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
            Stage_SetEndingFlag();
        } else {
            queue->imageMdecMode = D_8011565C;
            Stage_BeginTransitionKind7(D_8011566C);
        }
        goto block_11;
    }
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
            goto block_11;
        }
    } else {
    block_11:
        if (gDisplayState.debugMode != 0 && D_801156F4.sceneKey != 0) {
            sprintf(
                buf, Gp_StrEvsFmt, D_801156F4.sceneKey->group, D_801156F4.sceneKey->streamId,
                D_801156F4.sceneKey->subId);
            func_807244CC(buf);
        }
    }
    Gp_CapTable = 0;
    D_8011565A  = 0;
    D_801156A4  = 0;
    taskKill(arg0);
}

/// Blinking POLY_G3 continue caret. `Gp_CapCaretDelay` is a frame delay before the
/// first draw; `Gp_CapCaretX` / `Gp_CapCaretY` are base XY; `Gp_CapCaretGrey` /
/// `Gp_CapCaretDir` pulse the vertex greys between 8 and 15.
void Gp_DrawCapCaret(s32 unusedX, s32 unusedY)
{
    POLY_G3* p;
    s32      color;
    u16      x;
    u16      y;

    if (Gp_CapCaretDelay != 0) {
        Gp_CapCaretDelay--;
        return;
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyG3(p);

    color = (Gp_CapCaretGrey << 7) / 15;
    setRGB0(p, color, color, color);

    color = (Gp_CapCaretGrey * 0xC0) / 15;
    setRGB1(p, color, color, color);
    setRGB2(p, color, color, color);

    x     = Gp_CapCaretX;
    y     = Gp_CapCaretY;
    p->x0 = x + 3;
    p->y0 = y - gDisplayState.vramYOffset;
    p->x1 = x;
    p->y1 = -(gDisplayState.vramYOffset + 7) + y;
    p->x2 = x + 7;
    p->y2 = -(gDisplayState.vramYOffset + 7) + y;
    addPrim(&gGpuCurrentOt[2], p);

    if (Gp_CapCaretDir == 0) {
        Gp_CapCaretGrey++;
        if (Gp_CapCaretGrey >= 0xF) {
            Gp_CapCaretDir = 1;
        }
    } else {
        Gp_CapCaretGrey--;
        if (Gp_CapCaretGrey < 9) {
            Gp_CapCaretDir = 0;
        }
    }
}

s16 Gp_CapCenterX(const u16* text)
{
    s16 lineW = 0;
    s16 maxW  = 0;
    s16 i     = 0;
    s16 code  = text[0];

    while (code != -1) {
        if (code == -2) {
            if (lineW > maxW) {
                maxW = lineW;
            }
            lineW = 0;
            code  = text[++i];
        } else if (code == -3) {
            lineW += 3;
            code   = text[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = text[++i];
        } else if (code >= 0) {
            lineW += Gp_CapGlyphs[code & 0x3FF].width - 1;
            code   = text[++i];
        } else {
            code = text[++i];
        }
    }
    return (0x140 - maxW) / 2 - 5;
}

s16 Gp_CapCenterXLine(const u16* arg0, s32 arg1)
{
    s16 lineW;
    s16 selectedW;
    s16 i;
    s16 lineIndex;
    s16 code;

    lineW     = 0;
    selectedW = 0;
    i         = 0;
    lineIndex = 0;
    code      = arg0[0];
    while (code != -1) {
        if (code == -2) {
            if (lineIndex == arg1) {
                selectedW = lineW;
            }
            lineW = 0;
            i++;
            lineIndex++;
            code = arg0[i];
        } else if (code == -3) {
            lineW += 3;
            code   = arg0[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = arg0[++i];
        } else if (code >= 0) {
            lineW += Gp_CapGlyphs[code & 0x3FF].width - 1;
            code   = arg0[++i];
        } else {
            code = arg0[++i];
        }
    }
    return (0x140 - selectedW) / 2 - 5;
}

s16 Gp_CapTextHeight(const u16* arg0)
{
    s16 lineH = 0;
    s16 total = 0;
    s16 i     = 0;
    s16 code  = arg0[0];

    while (code != -1) {
        if (code == -2) {
            if (lineH == 0) {
                lineH = 2;
            }
            total += lineH;
            lineH  = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < Gp_CapGlyphs[code & 0x3FF].height + 2) {
                    lineH = Gp_CapGlyphs[code & 0x3FF].height + 2;
                }
            }
        }
        code = arg0[++i];
    }
    if (total == 2) {
        total = 0;
    }
    return total;
}

s16 Gp_CapTextTopY(const u16* arg0)
{
    s16        lineH     = 0;
    s16        total     = 0;
    s16        i         = 0;
    s16        seenBreak = 0;
    const u16* text      = arg0;
    s16        code      = text[0];

    while (code != -1) {
        if (code == -2) {
            if (seenBreak) {
                if (lineH == 0) {
                    lineH = 2;
                }
                total += lineH;
            } else {
                seenBreak = 1;
            }
            lineH = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < Gp_CapGlyphs[code & 0x3FF].height + 2) {
                    lineH = Gp_CapGlyphs[code & 0x3FF].height + 2;
                }
            }
        }
        code = text[++i];
    }
    return 0xD0 - total;
}

s32 func_800E6BB8(const u16* arg0)
{
    s16 height = 0;
    s16 i      = 0;
    s16 cont   = 1;
    s16 code   = arg0[0];

    do {
        if (code == -2) {
            cont = 0;
        } else if (code == -1) {
            cont   = 0;
            height = 0xD;
        } else if (code >= 0) {
            if (height < Gp_CapGlyphs[code & 0x3FF].height + 2) {
                height = Gp_CapGlyphs[code & 0x3FF].height + 2;
            }
            code = arg0[++i];
        } else {
            code = arg0[++i];
        }
    } while (cont);
    if (height == 0) {
        height = 2;
    }
    return height;
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

s32 Gp_CapBusy(void)
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

s32 Gp_GetCapEventKey(void)
{
    return Gp_CapEventKey;
}

void func_800E6D4C(s16 arg0, s16 arg1)
{
    D_80115654 = arg0;
    D_80115656 = arg1;
}

void Gp_LoadCapFile(s32 arg0)
{
    s32 i;
    s32 count;

    count = 0;
    for (i = 0; i < ARRAY_SIZE(D_8006C338); i++) {
        if (D_8006C338[i].kind == FILE_SYSTEM_RESOURCE_DATA) {
            if (count == arg0) {
                if (gDisplayState.debugMode != 0) {
                    func_80724714();
                }
                Gp_CapFile = D_8006C338[i].data;
                Gp_RelocCapFile(Gp_CapFile);
                break;
            }
            count++;
        }
    }
}

void Gp_ResetCap(void)
{
    Gp_CapTable = 0;
    D_801156A8  = 0;
    D_8011565A  = 0;
    func_800E6D4C(0x180, 0);
    Gp_CapFile = 0;
    Gp_LoadCapFile(0);
    D_8011569C = 0;
}

void func_800E6E44(CapTextUpdateCallback callback)
{
    D_80115660 = callback;
}

void Gp_ApplyCapEvtFlags(void)
{
    CapSequenceRecord* p;
    u8                 soundAndTextFlags;
    CapSequenceRecord* base;
    s32                idx;

    idx               = (s16)D_801155AE;
    base              = Gp_CapTable;
    p                 = Gp_CapEventAt(base, idx);
    soundAndTextFlags = p->trigger.soundAndTextFlags;
    D_80115670        = soundAndTextFlags;
    // A minimum playback interval disables instant reveal.
    if (p->minDisplayFrames != 0) {
        D_80115670 = soundAndTextFlags & CAP_SEQUENCE_SOUND_ID_MASK;
    }
    D_80115678 = p->minDisplayFrames;
}

s32 Gp_FindCapEvt(s32 arg0)
{
    s32                flag;
    s32                id;
    CapSequenceRecord* base;
    CapSequenceRecord* p;

    flag = CAP_TEXT_REF_END;
    id   = Gp_CapEventKey;
    base = Gp_CapTable;
    p    = Gp_CapEventAt(base, arg0);
loop:
    if (p->textRef.offset == flag) {
        goto done;
    }
    if (p->key == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_800E6EF4(Task* task)
{
    if (task->state > 0) {
        if (Gp_CapTable != 0 && D_8011565A == 0) {
            Gp_CapTable = 0;
        }
        taskKill(task);
    }
    task->state++;
}

/// `spawnArg1` packs three bytes: bits 0-7 are the message argument, bits
/// 8-15 the delay in frames, and bits 16-23 the recipient - 0 for slot 3, 1
/// for slot 0xA, otherwise `Gp_LookupSlot4(n - 2)`.
void Gp_DelayedMsgTask(Task* task)
{
    s32   val;
    s32   mode;
    Task* slot;

    switch (task->state) {
        case 0:
            task->killCountdown = (task->spawnArg1.value >> 8) & 0xFF;
            task->state++;
            break;
        case 1:
            if (task->killCountdown == 0) {
                mode = (task->spawnArg1.value >> 16) & 0xFF;
                val  = task->spawnArg1.value & 0xFF;
                if (mode == 0) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, val, 0);
                } else if (mode == 1) {
                    slot = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
                    if (slot != NULL) {
                        taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, val, 0);
                    }
                } else {
                    slot = Gp_LookupSlot4(mode - 2);
                    if (slot != NULL) {
                        taskMessageDispatch(slot, 0x7E0, val, 0);
                    }
                }
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

void func_800E704C(void)
{
    D_801155AE++;
    D_801155AE       = Gp_FindCapEvt((s16)D_801155AE);
    D_80115648       = 0;
    Gp_CapCaretDelay = 0x1E;
    D_80115659       = 0xF;
    Gp_ApplyCapEvtFlags();
}
