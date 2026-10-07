#include "options/options.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/display.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

/// Menu labels and help text for the options screen.
static u8 D_options_801D5B2C[32] = "Restore default configuration";
static u8 D_options_801D5B4C[12] = "Vibration";
static u8 D_options_801D5B58[4]  = "On";
static u8 D_options_801D5B5C[4]  = "Off";
static u8 D_options_801D5B60[8]  = "Sound";
static u8 D_options_801D5B68[8]  = "Stereo";
static u8 D_options_801D5B70[8]  = "Mono";
static u8 D_options_801D5B78[8]  = "Music";
static u8 D_options_801D5B80[4]  = "3";
static u8 D_options_801D5B84[4]  = "2";
static u8 D_options_801D5B88[4]  = "1";
static u8 D_options_801D5B8C[4]  = "Off";
static u8 D_options_801D5B90[8]  = "Cursor";
static u8 D_options_801D5B98[12] = "Standard";
static u8 D_options_801D5BA4[8]  = "Memory";
static u8 D_options_801D5BAC[12] = "Key Config";
static u8 D_options_801D5BB8[12] = "Movement";
static u8 D_options_801D5BC4[8]  = "Walk";
static u8 D_options_801D5BCC[4]  = "Run";
static u8 D_options_801D5BD0[4]  = "OK";
static u8 D_options_801D5BD4[8]  = "Cancel";
static u8 D_options_801D5BDC[8]  = "Help";
static u8 D_options_801D5BE4[12] = "Scroll Down";
static u8 D_options_801D5BF0[12] = "Scroll Up";
static u8 D_options_801D5BFC[12] = "Draw Weapon";
static u8 D_options_801D5C08[8]  = "Examine";
static u8 D_options_801D5C10[4]  = "Run";
static u8 D_options_801D5C14[8]  = "Walk";
static u8 D_options_801D5C1C[16] = "Switch Targets";
static u8 D_options_801D5C2C[8]  = "PE Menu";
static u8 D_options_801D5C34[12] = "Main Weapon";
static u8 D_options_801D5C40[12] = "Sub Weapon";
static u8 D_options_801D5C4C[8]  = "MENU";
static u8 D_options_801D5C54[8]  = "NORMAL";
static u8 D_options_801D5C5C[8]  = "BATTLE";
static u8 D_options_801D5C64[8]  = "Type A";
static u8 D_options_801D5C6C[8]  = "Type B";
static u8 D_options_801D5C74[8]  = "Type C";
static u8 D_options_801D5C7C[44] = "Set sound output to match\nyour television.";
static u8 D_options_801D5CA8[60] = "Set music volume level.\nSet low for clearer sound effects.";
static u8 D_options_801D5CE4[68] = "Set menu cursor. You can have it\nremember the last position used.";
static u8 D_options_801D5D28[64] = "Set vibration mode ON/OFF.\nSelect ON for more realistic play.";
static u8 D_options_801D5D68[60] = "Set Aya's default movement.\nBeginners should use \"Walk.\"";
static u8 D_options_801D5DA4[56] = "Open key settings menu.\nPress the } button to proceed.";
static u8 D_options_801D5DDC[48] = "Restore default settings.\nPress the } button.";
static u8 D_options_801D5E0C[20] = "Default setting.";
static u8 D_options_801D5E20[56] = "The function of the { button\nand ~ button is switched.";
static u8 D_options_801D5E58[56] = "This mode uses the ~ button and\n | button for combat.";

/// Suspended-control encoding and saved default-movement selectors.
enum {
    OPTIONS_SUSPENDED_CONTROL_SHIFT = 16,
    OPTIONS_MOVEMENT_WALK           = 0,
    OPTIONS_MOVEMENT_RUN            = 1
};

static void _optionsUpdateSoundRow(UiList* list, UiObject* object);
static void _optionsUpdateMusicVolumeRow(UiList* list, UiObject* object);
static void _optionsUpdateCursorRow(UiList* list, UiObject* object);
static void _optionsUpdateVibrationRow(UiList* list, UiObject* object);
static void _optionsUpdateMovementRow(UiList* list, UiObject* object);
static void _optionsUpdateKeyConfigurationRow(UiList* list, UiObject* object);
static void _optionsUpdateRestoreDefaultsRow(UiList* list, UiObject* object);
static void _optionsUpdateKeyConfigurationTask(Task* owningTask);

/// Sits immediately before the list tables; zero on disc.
static u8 D_options_801D5E90[4] = { 0, 0, 0, 0 };

/// The seven list-item renderers the main options list dispatches through.
static UiListRowCallback D_options_801D5E94[7] = {
    _optionsUpdateSoundRow,
    _optionsUpdateMusicVolumeRow,
    _optionsUpdateCursorRow,
    _optionsUpdateVibrationRow,
    _optionsUpdateMovementRow,
    _optionsUpdateKeyConfigurationRow,
    _optionsUpdateRestoreDefaultsRow,
};

/// The main options list: seven rows of 0x12 pixels.
static UiList D_options_801D5EB0 = { D_options_801D5E94, 0x07, 0x07, 0x00, 0x12 };

/// The key-config sub-list renders every row with the same function.
static UiListRowCallback D_options_801D5ED4[1] = { _optionsUpdateVibrationRow };

/// That sub-list: one row of 0x0F pixels.
static UiList D_options_801D5ED8 = { D_options_801D5ED4, 0x01, 0x01, 0x00, 0x0F };

/// The options screen's own UI object, spawned by `uiSpawnObject`.
static UiObjectDesc D_options_801D5EFC = {
    USER_INTERFACE_PANEL_TITLE_STYLE,
    { -140,
      -96,
      0x0118,
      0x0090 },
    0x0034,
    0x0000,
    TASK_BODY_NONE,
    0x00C0,
    _optionsUpdateKeyConfigurationTask,
    0,
};

/// Top-left texel of one pad-button glyph in the user-interface texture page.
///
/// Only the origin is stored. A glyph's extent comes from the row that draws
/// it: 15x15 texels for a face button, 15x8 for a shoulder button.
typedef struct {
    u8 u; // Texel column of the glyph's left edge
    u8 v; // Texel row of the glyph's top edge
} _OptionsKeyIconUv;
STATIC_ASSERT_SIZEOF(_OptionsKeyIconUv, 0x2);

/// Pad-button glyph origins for the key configuration screen, in row order.
///
/// The screen draws one glyph beside each of its seven rows: entries 0..3 are
/// the 15x15 glyphs of the first four rows and entries 4..6 the 15x8 glyphs of
/// the last three. Entry 7 is stored but never drawn. The array is wrapped in a
/// struct so the whole table can be copied by assignment.
typedef struct {
    _OptionsKeyIconUv icons[8];
} _OptionsKeyIconUvs;
STATIC_ASSERT_SIZEOF(_OptionsKeyIconUvs, 0x10);

/// One 16-byte stack slot of the key configuration draw routine, used for two
/// unrelated things in turn.
///
/// The routine's final label is drawn from `labelRequest`; once that draw has
/// returned, each pass of the icon loop copies the glyph table into the same
/// bytes as `iconUvs` and reads the row's entry back. Neither member is ever
/// read through the other: the union states only that the two occupy one slot,
/// which is what holds the routine's frame at its original size.
typedef union {
    TextDrawReq        labelRequest;
    _OptionsKeyIconUvs iconUvs;
} _OptionsKeyConfigStackSlot;
STATIC_ASSERT_SIZEOF(_OptionsKeyConfigStackSlot, 0x10);

static const _OptionsKeyIconUvs Options_KeyIconUvs;

/// Draws the Sound row and applies stereo/mono changes immediately.
///
/// Borrows the list and object under `UiListRowCallback`'s contract. The live
/// sound option is 0 stereo or 1 mono. Active row input wraps Left/Right through
/// the labels; drawing uses the option from before input. The selected row
/// publishes help while panel control is active or suspended active.
static void _optionsUpdateSoundRow(UiList* list, UiObject* object)
{
    enum { OPTIONS_SOUND_STEREO = 0,
           OPTIONS_SOUND_MONO   = 1 };
    const u8*  labels[] = { D_options_801D5B68, D_options_801D5B70 };
    const u8** labelCursor;
    s32        choiceIndex;
    s32        choiceOffsetNumerator;
    s32        choicesLeftX;
    s32        choicesSpanPixels;
    s32        selectedMode;
    s32        choiceStep;
    u32        textColorRgb;
    s32        controlMode;
    s32        previousMode;
    s32        choiceCount;

    choiceCount = ARRAY_SIZE(labels);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, D_options_801D5B60, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    previousMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode;
    choiceOffsetNumerator = 0;
    selectedMode          = previousMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + 0x78;
    choicesSpanPixels     = object->panel.contentRight.signedValue - choicesLeftX;
    // Space the saved choices across the row before processing this frame's input.
    do {
        if (choiceIndex != selectedMode) {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
        } else {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
        }

        choiceStep = 1;
        textDrawUiLine(object, choicesLeftX + choiceOffsetNumerator / choiceCount, list->rowTextY.signedValue, *labelCursor, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        labelCursor++;
        choiceOffsetNumerator += choicesSpanPixels;
        choiceIndex           += choiceStep;
    } while (choiceIndex < ARRAY_SIZE(labels));
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode += choiceStep;
            if (selectedMode >= ARRAY_SIZE(labels)) {
                selectedMode = OPTIONS_SOUND_STEREO;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode -= 1;
            if (selectedMode < 0) {
                selectedMode += ARRAY_SIZE(labels);
            }
        }
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode = selectedMode;
    if (previousMode != gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode) {
        switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode) {
            case OPTIONS_SOUND_STEREO:
                sndOutputSetStereo(SOUND_OUTPUT_STEREO);
                break;
            case OPTIONS_SOUND_MONO:
                sndOutputSetStereo(SOUND_OUTPUT_MONO);
                break;
            default:
                sndOutputSetStereo(SOUND_OUTPUT_STEREO);
                break;
        }
    }
    controlMode = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(D_options_801D5C7C, 0, 0);
    }
}

/// Draws the Music row and applies saved music-volume changes immediately.
///
/// Borrows the list and object under `UiListRowCallback`'s contract. Saved
/// indices 0..3 select levels 3, 2, 1 and Off. Active row input wraps Left/Right
/// through the labels. The changed signed save byte triggers volume application;
/// the selected row publishes help with active or suspended active control.
static void _optionsUpdateMusicVolumeRow(UiList* list, UiObject* object)
{
    enum { OPTIONS_MUSIC_FULL_VOLUME = 0 };
    const u8* labels[] = {
        D_options_801D5B80,
        D_options_801D5B84,
        D_options_801D5B88,
        D_options_801D5B8C,
    };
    const u8** labelCursor;
    s32        choiceIndex;
    s32        choiceOffsetNumerator;
    s32        choicesLeftX;
    s32        choicesSpanPixels;
    s32        selectedMode;
    s32        choiceStep;
    s32        previousMode;
    u32        textColorRgb;
    s32        choiceCount;
    s32        controlMode;

    choiceCount = ARRAY_SIZE(labels);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, D_options_801D5B78, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    previousMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume;
    choiceOffsetNumerator = 0;
    selectedMode          = previousMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + 0x78;
    choicesSpanPixels     = object->panel.contentRight.signedValue - choicesLeftX;
    // Space the saved choices across the row before processing this frame's input.
    do {
        if (choiceIndex != selectedMode) {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
        } else {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
        }

        choiceStep = 1;
        textDrawUiLine(object, choicesLeftX + choiceOffsetNumerator / choiceCount, list->rowTextY.signedValue, *labelCursor, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        labelCursor++;
        choiceOffsetNumerator += choicesSpanPixels;
        choiceIndex           += choiceStep;
    } while (choiceIndex < ARRAY_SIZE(labels));
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode += choiceStep;
            if (selectedMode >= choiceCount) {
                selectedMode = OPTIONS_MUSIC_FULL_VOLUME;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode -= 1;
            if (selectedMode < 0) {
                selectedMode += choiceCount;
            }
        }
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume = selectedMode;
    if (previousMode != (s8)selectedMode) {
        midiApplyMusicVolume(MIDI_MUSIC_VOLUME_SAVED);
    }
    controlMode = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(D_options_801D5CA8, 0, 0);
    }
}

/// Draws the Cursor row and selects remembered or reset menu selection.
///
/// Borrows the list and object under `UiListRowCallback`'s contract. Saved
/// index 0 selects Memory and 1 Standard. Active row input wraps Left/Right;
/// the selected row publishes help with active or suspended active control.
static void _optionsUpdateCursorRow(UiList* list, UiObject* object)
{
    enum { OPTIONS_CURSOR_MEMORY = 0 };
    const u8*  labels[] = { D_options_801D5BA4, D_options_801D5B98 };
    const u8** labelCursor;
    s32        choiceIndex;
    s32        choiceOffsetNumerator;
    s32        choicesLeftX;
    s32        choicesSpanPixels;
    s32        selectedMode;
    s32        choiceStep;
    u32        textColorRgb;
    s32        choiceCount;
    s32        controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, D_options_801D5B90, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    choiceOffsetNumerator = 0;
    selectedMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cursorMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + 0x78;
    choicesSpanPixels     = object->panel.contentRight.signedValue - choicesLeftX;
    choiceCount           = ARRAY_SIZE(labels);
    // Space the saved choices across the row before processing this frame's input.
    do {
        if (choiceIndex != selectedMode) {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
        } else {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
        }

        choiceStep = 1;
        textDrawUiLine(object, choicesLeftX + choiceOffsetNumerator / choiceCount, list->rowTextY.signedValue, *labelCursor, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        labelCursor++;
        choiceOffsetNumerator += choicesSpanPixels;
        choiceIndex           += choiceStep;
    } while (choiceIndex < ARRAY_SIZE(labels));
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode += choiceStep;
            if (selectedMode >= choiceCount) {
                selectedMode = OPTIONS_CURSOR_MEMORY;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode -= 1;
            if (selectedMode < 0) {
                selectedMode += choiceCount;
            }
        }
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cursorMode = selectedMode;
    controlMode                                         = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(D_options_801D5CE4, 0, 0);
    }
}

/// Draws the Vibration row and updates the live on/off setting.
///
/// Serves both the full settings list and the vibration-only list, borrowing
/// the arguments under `UiListRowCallback`'s contract. Saved index 0 means On
/// and 1 Off. Active row input wraps Left/Right; the selected row publishes
/// help with active or suspended active control.
static void _optionsUpdateVibrationRow(UiList* list, UiObject* object)
{
    enum { OPTIONS_VIBRATION_ON = 0 };
    const u8*  labels[] = { D_options_801D5B58, D_options_801D5B5C };
    const u8** labelCursor;
    s32        choiceIndex;
    s32        choiceOffsetNumerator;
    s32        choicesLeftX;
    s32        choicesSpanPixels;
    s32        selectedMode;
    s32        choiceStep;
    u32        textColorRgb;
    s32        choiceCount;
    s32        controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, D_options_801D5B4C, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    choiceOffsetNumerator = 0;
    selectedMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
    choicesLeftX          = object->panel.contentLeft.signedValue + 0x78;
    choicesSpanPixels     = object->panel.contentRight.signedValue - choicesLeftX;
    choiceCount           = ARRAY_SIZE(labels);
    // Space the saved choices across the row before processing this frame's input.
    do {
        if (choiceIndex != selectedMode) {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
        } else {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
        }

        choiceStep = 1;
        textDrawUiLine(object, choicesLeftX + choiceOffsetNumerator / choiceCount, list->rowTextY.signedValue, *labelCursor, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        labelCursor++;
        choiceOffsetNumerator += choicesSpanPixels;
        choiceIndex           += choiceStep;
    } while (choiceIndex < ARRAY_SIZE(labels));
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode += choiceStep;
            if (selectedMode >= choiceCount) {
                selectedMode = OPTIONS_VIBRATION_ON;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode -= 1;
            if (selectedMode < 0) {
                selectedMode += choiceCount;
            }
        }
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration = selectedMode;
    controlMode                                        = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(D_options_801D5D28, 0, 0);
    }
}

/// Draws the Movement row and selects walking or running as the default.
///
/// Borrows the list and object under `UiListRowCallback`'s contract. Saved
/// index 0 selects Walk and 1 Run. Active row input wraps Left/Right; the
/// selected row publishes help with active or suspended active control.
static void _optionsUpdateMovementRow(UiList* list, UiObject* object)
{
    const u8*  labels[] = { D_options_801D5BC4, D_options_801D5BCC };
    const u8** labelCursor;
    s32        choiceIndex;
    s32        choiceOffsetNumerator;
    s32        choicesLeftX;
    s32        choicesSpanPixels;
    s32        selectedMode;
    s32        choiceStep;
    u32        textColorRgb;
    s32        choiceCount;
    s32        controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, D_options_801D5BB8, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    choiceOffsetNumerator = 0;
    selectedMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + 0x78;
    choicesSpanPixels     = object->panel.contentRight.signedValue - choicesLeftX;
    choiceCount           = ARRAY_SIZE(labels);
    // Space the saved choices across the row before processing this frame's input.
    do {
        if (choiceIndex != selectedMode) {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
        } else {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
        }

        choiceStep = 1;
        textDrawUiLine(object, choicesLeftX + choiceOffsetNumerator / choiceCount, list->rowTextY.signedValue, *labelCursor, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        labelCursor++;
        choiceOffsetNumerator += choicesSpanPixels;
        choiceIndex           += choiceStep;
    } while (choiceIndex < ARRAY_SIZE(labels));
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode += choiceStep;
            if (selectedMode >= choiceCount) {
                selectedMode = OPTIONS_MOVEMENT_WALK;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            selectedMode -= 1;
            if (selectedMode < 0) {
                selectedMode += choiceCount;
            }
        }
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode = selectedMode;
    controlMode                                       = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(D_options_801D5D68, 0, 0);
    }
}

/// Gives the vibration-only panel its fixed content width and centered bounds.
///
/// Borrows a laid-out panel; height stays unchanged. Width/height bits are
/// interpreted as signed halfwords before halving, with stores retaining 16 bits.
static inline void _optionsCenterVibrationPanel(UiPanel* panel)
{
    enum { OPTIONS_VIBRATION_CONTENT_WIDTH_PIXELS = 192 };

    uiSetPanelContentSize(panel, OPTIONS_VIBRATION_CONTENT_WIDTH_PIXELS, 0);
    panel->bounds.rect.y = -(panel->bounds.rect.h / 2);
    panel->bounds.rect.x = -(panel->bounds.rect.w / 2);
}

void optionsUpdateMenuTask(Task* owningTask)
{
    enum {
        OPTIONS_MENU_TASK_INITIAL = 0,
        OPTIONS_MENU_TASK_READY   = 1
    };
    UiList*   list;
    UiObject* object;
    UiObject* childObject;
    s32       controlMode;
    s32       childResult;

    list   = &D_options_801D5EB0;
    object = owningTask->spawnArg2.pointer;
    if (owningTask->spawnArg1.value == OPTIONS_MENU_VIBRATION_ONLY) {
        list = &D_options_801D5ED8;
    }
    // Fit once; only the vibration layout explicitly centers the outer bounds.
    if (owningTask->state == OPTIONS_MENU_TASK_INITIAL) {
        uiFitPanelToList(list, &object->panel);
        owningTask->state += OPTIONS_MENU_TASK_READY - OPTIONS_MENU_TASK_INITIAL;
        if (owningTask->spawnArg1.value == OPTIONS_MENU_VIBRATION_ONLY) {
            _optionsCenterVibrationPanel(&object->panel);
        }
    }
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "Option");
    uiUpdateList(list, &object->panel);
    controlMode = object->panel.control.word;
    // Cancel acknowledges this panel; Menu requests cancellation of its parent flow.
    if (controlMode == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->resultValue = controlMode;
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
    // Returning from key configuration restores focus; its cancellation propagates.
    if (owningTask->firstChild != NULL) {
        childObject = owningTask->firstChild->spawnArg2.pointer;
        childResult = childObject->result;
        switch (childResult) {
            case USER_INTERFACE_RESULT_CONFIRM:
                uiStartTreeClosing(childObject, childObject->owner);
                object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                break;
            case USER_INTERFACE_RESULT_CANCEL:
                object->result = childResult;
                break;
        }
    }
}

/// Draws the button-assignment chart and edits the live controller layout.
///
/// `owningTask` borrows its live `UiObject` from spawnArg2. The options overlay
/// and UI drawing resources must remain live; layout must be 0..2 (A/B/C), and
/// default movement 0 Walk or 1 Run. The first update fits the panel and saves
/// the initial layout in spawnArg1. Coordinates are content pixels until panel
/// origins are added; icon storage and OT entries are supplied by the UI.
///
/// Active input cycles Right/Down forward or Up/Left backward. Confirm and
/// Cancel both publish confirm; Menu publishes cancel. Edits apply immediately
/// and neither exit restores the initial layout. Drawing reflects the layout
/// read before this update's input. The parent owns closing and release.
static void _optionsUpdateKeyConfigurationTask(Task* owningTask)
{
/// Sets a positioned chart request's medium outlined style and draws its label.
///
/// `requestValue` is a live TextDrawReq lvalue with X/Y already set. It is
/// evaluated repeatedly and must have no side effects; `objectValue` is a
/// borrowed live UI object. Color is packed RGB and alignment is TEXT_ALIGNMENT_*.
/// Other arguments are evaluated once. The drawer mutates the request but
/// retains neither argument. Use as a standalone statement in a compound block.
#define OPTIONS_KEY_DRAW_ASSIGNMENT(requestValue, objectValue, colorValue, alignmentValue, textValue) \
    (requestValue).otIndex    = (objectValue)->panel.otIndex.signedValue + 1;                         \
    (requestValue).colorRgb   = (colorValue);                                                         \
    (requestValue).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                              \
    (requestValue).alignment  = (alignmentValue);                                                     \
    (requestValue).drawMode   = TEXT_DRAW_OUTLINED;                                                   \
    textDrawString(&(requestValue), (textValue))
    enum {
        OPTIONS_KEY_TASK_INITIAL           = 0,
        OPTIONS_KEY_TASK_READY             = 1,
        OPTIONS_KEY_LAYOUT_A               = 0,
        OPTIONS_KEY_LAYOUT_B               = 1,
        OPTIONS_KEY_LAYOUT_C               = 2,
        OPTIONS_KEY_ASSIGNMENT_ROWS        = 7,
        OPTIONS_KEY_FACE_BUTTON_ROWS       = 4,
        OPTIONS_KEY_PANEL_TEXT_ROWS        = 9,
        OPTIONS_KEY_ICON_EDGE_PIXELS       = 15,
        OPTIONS_KEY_SHOULDER_HEIGHT_PIXELS = 8,
        OPTIONS_KEY_ICON_CLUT              = 0x3C00, // VRAM palette at (0, 240)
        OPTIONS_KEY_ICON_RAW_TEXTURE       = 1,
        OPTIONS_KEY_LABEL_COLOR_RGB        = 0x606060,
        OPTIONS_KEY_SELECTOR_COLOR_RGB     = 0x1741F
    };
    UiObject*                  object   = owningTask->spawnArg2.pointer;
    const u8*                  labels[] = { D_options_801D5C64, D_options_801D5C6C, D_options_801D5C74 };
    const u8*                  alternateMovementLabel;
    TextDrawReq                menuTopFaceRequest;
    TextDrawReq                menuSecondFaceRequest;
    TextDrawReq                menuConfirmRequest;
    TextDrawReq                menuHelpRequest;
    TextDrawReq                menuScrollDownRequest;
    TextDrawReq                menuScrollUpRequest;
    TextDrawReq                normalActionRequest;
    TextDrawReq                normalAlternateDrawWeaponRequest;
    TextDrawReq                battleTopFaceRequest;
    TextDrawReq                battleSecondFaceRequest;
    TextDrawReq                battleActionRequest;
    _OptionsKeyConfigStackSlot finalBattleLabelAndIconSlot;
    s32                        contentTopY;
    s32                        penX;
    s32                        penY;
    s32                        headerY;
    s32                        selectorTopY;
    s32                        contentRightX;
    s32                        contentLeftX;
    s32                        layoutLabelX;
    s32                        selectorControlMode;
    s32                        promptControlMode;
    s32                        iconRowIndex;
    s32                        iconHeight;
    s32                        iconWidth;
    s32                        upCaretOuterRightX;
    s32                        upCaretOriginInsetX;
    s32                        downCaretOuterRightX;
    s32                        downCaretOriginInsetX;
    s32                        layout;
    u32                        labelColorRgb;
    s32                        battleAlignment;
    s32                        defaultMovement;
    s32                        selectorSeparatorY;
    const u8*                  battleActionLabel;
    SPRT*                      iconSprite;

    alternateMovementLabel = D_options_801D5C10;
    contentTopY            = object->panel.contentTop.signedValue;

    defaultMovement = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode;
    layout          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
    penY            = contentTopY + 0xF;
    if (defaultMovement == OPTIONS_MOVEMENT_RUN) {
        alternateMovementLabel = D_options_801D5C14;
    }
    // Fit once, retaining the initial layout in the task payload.
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "Key Configuration");
    if (owningTask->state == OPTIONS_KEY_TASK_INITIAL) {
        uiSetPanelContentSize(&object->panel, 0, uiGetTextRowsHeight(OPTIONS_KEY_PANEL_TEXT_ROWS) + 6);
        owningTask->spawnArg1.value = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
        owningTask->state          += OPTIONS_KEY_TASK_READY - OPTIONS_KEY_TASK_INITIAL;
    }
    selectorTopY        = contentTopY + 1;
    contentRightX       = object->panel.contentRight.signedValue;
    layoutLabelX        = contentRightX - 0x3B;
    upCaretOuterRightX  = object->panel.bounds.rect.x + object->panel.bounds.rect.w;
    upCaretOriginInsetX = object->panel.contentOriginX.signedValue + 5;
    uiDrawFlatCaret(&object->panel, upCaretOuterRightX - upCaretOriginInsetX, selectorTopY, OPTIONS_KEY_LABEL_COLOR_RGB, USER_INTERFACE_CARET_UP);
    downCaretOuterRightX  = object->panel.bounds.rect.x + object->panel.bounds.rect.w;
    downCaretOriginInsetX = object->panel.contentOriginX.signedValue + 5;
    uiDrawFlatCaret(&object->panel, downCaretOuterRightX - downCaretOriginInsetX, penY, OPTIONS_KEY_LABEL_COLOR_RGB, USER_INTERFACE_CARET_DOWN);
    textDrawUiLine(object, layoutLabelX, penY, labels[layout], OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    selectorControlMode = object->panel.control.word;
    if (((selectorControlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (selectorControlMode == USER_INTERFACE_PANEL_ACTIVE)) {
        uiEaseAndDrawCursor(&object->panel, layoutLabelX, contentTopY + 7);
        if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            object->panel.otIndex.signedValue = object->panel.otIndex.signedValue + 1;
            uiFillRectInterior(&object->panel, contentRightX - 0x40, selectorTopY, 0x3A, 0xE, OPTIONS_KEY_SELECTOR_COLOR_RGB);
            object->panel.otIndex.signedValue = object->panel.otIndex.signedValue - 1;
        }
    }
    selectorSeparatorY = penY + 2;
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, selectorSeparatorY);
    labelColorRgb = OPTIONS_KEY_LABEL_COLOR_RGB;
    uiDrawVerticalSeparator(&object->panel, penY + 5, object->panel.contentBottom.signedValue, object->panel.contentLeft.signedValue + 0x5F);
    penY        += 0x13;
    contentLeftX = object->panel.contentLeft.signedValue;

    penX = contentLeftX + 0x1E;
    textDrawUiLine(object, penX, penY, D_options_801D5C4C, labelColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    headerY = penY;
    penY   += 0xB;

    // Show menu actions, including the two Cancel rows and the scroll controls.
    menuTopFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuTopFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuTopFaceRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, D_options_801D5BD4);

    menuSecondFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuSecondFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                   += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuSecondFaceRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, D_options_801D5BD4);

    menuConfirmRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuConfirmRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuConfirmRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, D_options_801D5BD0);

    menuHelpRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuHelpRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY             += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuHelpRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, D_options_801D5BDC);

    menuScrollDownRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuScrollDownRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                   += 0x1E;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuScrollDownRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, D_options_801D5BE4);

    menuScrollUpRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuScrollUpRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penX                  = contentLeftX + 0x6A;
    penY                  = headerY;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuScrollUpRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, D_options_801D5BF0);

    // Show normal-play actions; the movement button uses the non-default gait.
    textDrawUiLine(object, penX, penY, D_options_801D5C54, labelColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    penY += 0xB;
    if (layout != OPTIONS_KEY_LAYOUT_B) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, D_options_801D5BFC);
    } else {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, alternateMovementLabel);
    }

    penY += 0xF;
    if (layout != OPTIONS_KEY_LAYOUT_B) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, alternateMovementLabel);
    } else {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, D_options_801D5BFC);
    }

    penY += 0xF;

    normalActionRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
    normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                 += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_CENTER, D_options_801D5C08);

    if (layout == OPTIONS_KEY_LAYOUT_C) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, D_options_801D5BFC);

        penY                              += 0xF;
        normalAlternateDrawWeaponRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalAlternateDrawWeaponRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalAlternateDrawWeaponRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, D_options_801D5BFC);
    }

    // Show battle actions; the PE Menu entry is centered when it spans both columns.
    penY = headerY;
    penX = object->panel.contentRight.signedValue - 4;
    textDrawUiLine(object, penX, penY, D_options_801D5C5C, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    penY           += 0xB;
    battleAlignment = TEXT_ALIGNMENT_RIGHT;
    if (layout == OPTIONS_KEY_LAYOUT_A) {
        battleActionLabel = D_options_801D5C1C;
    } else if (layout == OPTIONS_KEY_LAYOUT_B) {
        battleActionLabel = D_options_801D5BD4;
    } else {
        battleActionLabel = D_options_801D5C34;
    }
    battleTopFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleTopFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                  += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(battleTopFaceRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, battleActionLabel);

    if (layout == OPTIONS_KEY_LAYOUT_A) {
        battleActionLabel = D_options_801D5BD4;
    } else if (layout == OPTIONS_KEY_LAYOUT_B) {
        battleActionLabel = D_options_801D5C1C;
    } else {
        battleActionLabel = D_options_801D5BD4;
    }
    battleSecondFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleSecondFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                     += 0x1E;
    OPTIONS_KEY_DRAW_ASSIGNMENT(battleSecondFaceRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, battleActionLabel);

    if (layout == OPTIONS_KEY_LAYOUT_C) {
        battleActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(battleActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, D_options_801D5C40);
    } else {
        battleActionRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
        battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(battleActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_CENTER, D_options_801D5C2C);
    }

    penY += 0xF;
    if (layout == OPTIONS_KEY_LAYOUT_A) {
        battleActionLabel = D_options_801D5C34;
    } else if (layout == OPTIONS_KEY_LAYOUT_B) {
        battleActionLabel = D_options_801D5C34;
    } else {
        battleActionLabel = D_options_801D5C1C;
    }
    battleActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                 += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(battleActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, battleActionLabel);

    if (layout != OPTIONS_KEY_LAYOUT_C) {
        finalBattleLabelAndIconSlot.labelRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        finalBattleLabelAndIconSlot.labelRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(finalBattleLabelAndIconSlot.labelRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, D_options_801D5C40);
    } else {
        finalBattleLabelAndIconSlot.labelRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
        finalBattleLabelAndIconSlot.labelRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(finalBattleLabelAndIconSlot.labelRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_CENTER, D_options_801D5C2C);
    }

    // Draw seven button glyphs; the stored eighth origin has no displayed row.
    penY         = headerY;
    penX         = object->panel.contentLeft.signedValue + 2;
    iconRowIndex = 0;
    do {
        finalBattleLabelAndIconSlot.iconUvs = Options_KeyIconUvs;
        iconWidth                           = OPTIONS_KEY_ICON_EDGE_PIXELS;
        iconHeight                          = OPTIONS_KEY_ICON_EDGE_PIXELS;
        iconSprite                          = gGpuPrimCursor;
        gGpuPrimCursor                      = iconSprite + 1;
        iconSprite->y0                      = penY - OPTIONS_KEY_ICON_EDGE_PIXELS;
        iconSprite->x0                      = penX;
        if (iconRowIndex >= OPTIONS_KEY_FACE_BUTTON_ROWS) {
            iconHeight     = OPTIONS_KEY_SHOULDER_HEIGHT_PIXELS;
            iconSprite->y0 = penY - 0xB;
        }
        penY            += 0xF;
        iconSprite->w    = iconWidth;
        iconSprite->h    = iconHeight;
        iconSprite->u0   = finalBattleLabelAndIconSlot.iconUvs.icons[iconRowIndex].u;
        iconSprite->v0   = finalBattleLabelAndIconSlot.iconUvs.icons[iconRowIndex].v;
        iconSprite->clut = OPTIONS_KEY_ICON_CLUT;
        setSprt(iconSprite);
        setShadeTex(iconSprite, OPTIONS_KEY_ICON_RAW_TEXTURE);
        addPrim(&gGpuCurrentOt[object->panel.otIndex.signedValue + 1], iconSprite);
        iconRowIndex++;
    } while (iconRowIndex < OPTIONS_KEY_ASSIGNMENT_ROWS);

    promptControlMode = object->panel.control.word;
    if (((promptControlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (promptControlMode == USER_INTERFACE_PANEL_ACTIVE)) {
        switch (layout) {
            case OPTIONS_KEY_LAYOUT_A:
                uiSetPromptText(D_options_801D5E0C, 0, 0);
                break;
            case OPTIONS_KEY_LAYOUT_B:
                uiSetPromptText(D_options_801D5E20, 0, 0);
                break;
            case OPTIONS_KEY_LAYOUT_C:
                uiSetPromptText(D_options_801D5E58, 0, 0);
                break;
            default:
                uiSetPromptText(D_options_801D5E90, 0, 0);
                break;
        }
    }
    // Queue the glyph page before its sprites in the same ordering-table entry.
    uiQueueTexturePage(object->panel.otIndex.signedValue + 1, 0);
    // Layout edits are live; both confirmation buttons acknowledge without rollback.
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout = ((s8)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout + 1)) % ARRAY_SIZE(labels);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP | PAD_BUTTON_LEFT) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout = ((s8)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout + (ARRAY_SIZE(labels) - 1))) % ARRAY_SIZE(labels);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
#undef OPTIONS_KEY_DRAW_ASSIGNMENT
}

/* Defined after the function so its rodata follows the function's own
   constants (the label table and title string), as in the original layout. */
static const _OptionsKeyIconUvs Options_KeyIconUvs = { {
    { 0x10, 0x60 },
    { 0x10, 0x70 },
    { 0x20, 0x60 },
    { 0x20, 0x70 },
    { 0x90, 0x58 },
    { 0xB0, 0x58 },
    { 0x80, 0x58 },
    { 0xA0, 0x58 },
} };

/// Draws Key Config and opens its child editor when Confirm is pressed.
///
/// Borrows the arguments under `UiListRowCallback`'s contract. The selected
/// row publishes help with active or suspended active control. Active input
/// spawns a child with active control and a one-tick opening delay, then
/// suspends the parent even if allocation fails. The parent handles child exit.
static void _optionsUpdateKeyConfigurationRow(UiList* list, UiObject* object)
{
    s32 controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, D_options_801D5BAC, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    controlMode = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(D_options_801D5DA4, 0, 0);
    }
    if ((list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        uiSpawnObject(&D_options_801D5EFC, 0, USER_INTERFACE_PANEL_ACTIVE, 1, object);
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

/// Draws Restore Defaults and immediately resets live options on Confirm.
///
/// Borrows the arguments under `UiListRowCallback`'s contract. The selected
/// row publishes help with active or suspended active control. Active input
/// restores saved option defaults and applies sound/music settings; no child
/// confirmation dialog is opened and the options panel remains active.
static void _optionsUpdateRestoreDefaultsRow(UiList* list, UiObject* object)
{
    s32 controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, D_options_801D5B2C, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    controlMode = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(D_options_801D5DDC, 0, 0);
    }
    if ((list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        mcResetOptions();
    }
}
