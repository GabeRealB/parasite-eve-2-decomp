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

/// Options-list label for restoring the live saved configuration defaults.
///
/// NUL-terminated UI text; word alignment and .data placement retain the overlay layout.
static const u8 _gOptionsRestoreDefaultsLabel[] __attribute__((section(".data"), aligned(4))) = "Restore default configuration";
/// Options-list label for the vibration setting.
static const u8 _gOptionsVibrationLabel[] __attribute__((section(".data"), aligned(4))) = "Vibration";
/// Vibration choice label for saved value 0 (enabled).
static const u8 _gOptionsVibrationOnLabel[] __attribute__((section(".data"), aligned(4))) = "On";
/// Vibration choice label for saved value 1 (disabled).
static const u8 _gOptionsVibrationOffLabel[] __attribute__((section(".data"), aligned(4))) = "Off";
/// Options-list label for the sound-output setting.
static const u8 _gOptionsSoundLabel[] __attribute__((section(".data"), aligned(4))) = "Sound";
/// Sound-output choice label for saved value 0 (stereo).
static const u8 _gOptionsSoundStereoLabel[] __attribute__((section(".data"), aligned(4))) = "Stereo";
/// Sound-output choice label for saved value 1 (mono).
static const u8 _gOptionsSoundMonoLabel[] __attribute__((section(".data"), aligned(4))) = "Mono";
/// Options-list label for the music-volume setting.
static const u8 _gOptionsMusicLabel[] __attribute__((section(".data"), aligned(4))) = "Music";
/// Music-volume choice label for saved value 0 (level 3, gain 100).
static const u8 _gOptionsMusicVolume3Label[] __attribute__((section(".data"), aligned(4))) = "3";
/// Music-volume choice label for saved value 1 (level 2, gain 64).
static const u8 _gOptionsMusicVolume2Label[] __attribute__((section(".data"), aligned(4))) = "2";
/// Music-volume choice label for saved value 2 (level 1, gain 32).
static const u8 _gOptionsMusicVolume1Label[] __attribute__((section(".data"), aligned(4))) = "1";
/// Music-volume choice label for saved value 3 (muted).
static const u8 _gOptionsMusicVolumeOffLabel[] __attribute__((section(".data"), aligned(4))) = "Off";
/// Options-list label for the menu-cursor preference.
static const u8 _gOptionsCursorLabel[] __attribute__((section(".data"), aligned(4))) = "Cursor";
/// Cursor choice label for saved value 1 (reset selection when opening a list).
static const u8 _gOptionsCursorStandardLabel[] __attribute__((section(".data"), aligned(4))) = "Standard";
/// Cursor choice label for saved value 0 (remember the previous selection).
static const u8 _gOptionsCursorMemoryLabel[] __attribute__((section(".data"), aligned(4))) = "Memory";
/// Options-list label for opening the controller-layout editor.
static const u8 _gOptionsKeyConfigurationLabel[] __attribute__((section(".data"), aligned(4))) = "Key Config";
/// Options-list label for Aya's default movement setting.
static const u8 _gOptionsMovementLabel[] __attribute__((section(".data"), aligned(4))) = "Movement";
/// Default-movement choice label for saved value 0 (walk).
static const u8 _gOptionsMovementWalkLabel[] __attribute__((section(".data"), aligned(4))) = "Walk";
/// Default-movement choice label for saved value 1 (run).
static const u8 _gOptionsMovementRunLabel[] __attribute__((section(".data"), aligned(4))) = "Run";
/// Menu-column label for the confirm action in the button-assignment chart.
static const u8 _gOptionsKeyConfirmLabel[] __attribute__((section(".data"), aligned(4))) = "OK";
/// Cancel-action label shared by the menu and battle assignment columns.
static const u8 _gOptionsKeyCancelLabel[] __attribute__((section(".data"), aligned(4))) = "Cancel";
/// Menu-column label for the help action.
static const u8 _gOptionsKeyHelpLabel[] __attribute__((section(".data"), aligned(4))) = "Help";
/// Menu-column label for paging a list downward.
static const u8 _gOptionsKeyScrollDownLabel[] __attribute__((section(".data"), aligned(4))) = "Scroll Down";
/// Menu-column label for paging a list upward.
static const u8 _gOptionsKeyScrollUpLabel[] __attribute__((section(".data"), aligned(4))) = "Scroll Up";
/// Normal-play assignment label for drawing a weapon.
static const u8 _gOptionsKeyDrawWeaponLabel[] __attribute__((section(".data"), aligned(4))) = "Draw Weapon";
/// Normal-play assignment label for examining an object.
static const u8 _gOptionsKeyExamineLabel[] __attribute__((section(".data"), aligned(4))) = "Examine";
/// Alternate-movement label when the saved default movement is walking.
static const u8 _gOptionsKeyRunLabel[] __attribute__((section(".data"), aligned(4))) = "Run";
/// Alternate-movement label when the saved default movement is running.
static const u8 _gOptionsKeyWalkLabel[] __attribute__((section(".data"), aligned(4))) = "Walk";
/// Battle assignment label for switching the selected target.
static const u8 _gOptionsKeySwitchTargetsLabel[] __attribute__((section(".data"), aligned(4))) = "Switch Targets";
/// Battle assignment label for opening the Parasite Energy menu.
static const u8 _gOptionsKeyPeMenuLabel[] __attribute__((section(".data"), aligned(4))) = "PE Menu";
/// Battle assignment label for the main-weapon action.
static const u8 _gOptionsKeyMainWeaponLabel[] __attribute__((section(".data"), aligned(4))) = "Main Weapon";
/// Battle assignment label for the sub-weapon action.
static const u8 _gOptionsKeySubWeaponLabel[] __attribute__((section(".data"), aligned(4))) = "Sub Weapon";
/// Heading for the menu assignments in the key configuration chart.
static const u8 _gOptionsKeyMenuHeading[] __attribute__((section(".data"), aligned(4))) = "MENU";
/// Heading for the normal-play assignments in the key configuration chart.
static const u8 _gOptionsKeyNormalHeading[] __attribute__((section(".data"), aligned(4))) = "NORMAL";
/// Heading for the battle assignments in the key configuration chart.
static const u8 _gOptionsKeyBattleHeading[] __attribute__((section(".data"), aligned(4))) = "BATTLE";
/// Controller-layout selector label for saved layout 0 (A).
static const u8 _gOptionsKeyLayoutALabel[] __attribute__((section(".data"), aligned(4))) = "Type A";
/// Controller-layout selector label for saved layout 1 (B).
static const u8 _gOptionsKeyLayoutBLabel[] __attribute__((section(".data"), aligned(4))) = "Type B";
/// Controller-layout selector label for saved layout 2 (C).
static const u8 _gOptionsKeyLayoutCLabel[] __attribute__((section(".data"), aligned(4))) = "Type C";
/// Help text for the selected sound-output row.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsSoundPrompt[] __attribute__((section(".data"), aligned(4))) = "Set sound output to match\nyour television.";
/// Help text for the selected music-volume row.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsMusicVolumePrompt[] __attribute__((section(".data"), aligned(4))) = "Set music volume level.\nSet low for clearer sound effects.";
/// Help text for the selected menu-cursor row.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsCursorPrompt[] __attribute__((section(".data"), aligned(4))) = "Set menu cursor. You can have it\nremember the last position used.";
/// Help text for the selected vibration-setting row.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsVibrationPrompt[] __attribute__((section(".data"), aligned(4))) = "Set vibration mode ON/OFF.\nSelect ON for more realistic play.";
/// Help text for the selected default-movement row.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsMovementPrompt[] __attribute__((section(".data"), aligned(4))) = "Set Aya's default movement.\nBeginners should use \"Walk.\"";
/// Help text for opening the controller-layout editor from the selected row.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsKeyConfigurationPrompt[] __attribute__((section(".data"), aligned(4))) = "Open key settings menu.\nPress the } button to proceed.";
/// Help text for restoring live option defaults from the selected row.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsRestoreDefaultsPrompt[] __attribute__((section(".data"), aligned(4))) = "Restore default settings.\nPress the } button.";
/// Help text for controller layout A, the default assignment.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsKeyLayoutAPrompt[] __attribute__((section(".data"), aligned(4))) = "Default setting.";
/// Help text for controller layout B, which exchanges the first two button assignments.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsKeyLayoutBPrompt[] __attribute__((section(".data"), aligned(4))) = "The function of the { button\nand ~ button is switched.";
/// Help text for controller layout C, which changes the combat assignments.
///
/// NUL-terminated UI text borrowed by the prompt panel while this overlay is loaded.
static const u8 _gOptionsKeyLayoutCPrompt[] __attribute__((section(".data"), aligned(4))) = "This mode uses the ~ button and\n | button for combat.";

/// Suspended-control encoding.
enum {
    OPTIONS_SUSPENDED_CONTROL_SHIFT = 16,
};

static void _optionsUpdateSoundRow(UiList* list, UiObject* object);
static void _optionsUpdateMusicVolumeRow(UiList* list, UiObject* object);
static void _optionsUpdateCursorRow(UiList* list, UiObject* object);
static void _optionsUpdateVibrationRow(UiList* list, UiObject* object);
static void _optionsUpdateMovementRow(UiList* list, UiObject* object);
static void _optionsUpdateKeyConfigurationRow(UiList* list, UiObject* object);
static void _optionsUpdateRestoreDefaultsRow(UiList* list, UiObject* object);
static void _optionsUpdateKeyConfigurationTask(Task* owningTask);

/// Empty NUL-terminated prompt for the key-layout fallback.
static const u8 _gOptionsKeyEmptyPrompt[] __attribute__((section(".data"), aligned(4))) = "";

/// Row callbacks for the full options list, indexed by absolute row 0..6.
///
/// Rows are Sound, Music, Cursor, Vibration, Movement, Key Config and Restore
/// Defaults, in that order. The list borrows this overlay's static storage;
/// callbacks use `UiListRowCallback`'s signature and input-permission contract.
static UiListRowCallback _gOptionsMenuRowCallbacks[] = {
    _optionsUpdateSoundRow,
    _optionsUpdateMusicVolumeRow,
    _optionsUpdateCursorRow,
    _optionsUpdateVibrationRow,
    _optionsUpdateMovementRow,
    _optionsUpdateKeyConfigurationRow,
    _optionsUpdateRestoreDefaultsRow,
};

/// Options-list navigation, pixel spacing and key-editor task seeds.
enum {
    OPTIONS_LIST_CLAMP_NAVIGATION       = 0,
    OPTIONS_MENU_ROW_HEIGHT_PIXELS      = 18,
    OPTIONS_VIBRATION_ROW_HEIGHT_PIXELS = 15,
    OPTIONS_KEY_CONFIGURATION_OT_INDEX  = 52,
};

/// Mutable list state for the full seven-setting options panel.
///
/// Starts at row zero, requests every row at 18-pixel spacing and clamps
/// navigation at either end. Fitting and drawing update its viewport, input
/// and selection state; reopening preserves selection in Memory cursor mode.
/// Only one full options panel may use this overlay-owned list at a time.
static UiList _gOptionsMenuList = {
    _gOptionsMenuRowCallbacks,
    ARRAY_SIZE(_gOptionsMenuRowCallbacks),
    ARRAY_SIZE(_gOptionsMenuRowCallbacks),
    OPTIONS_LIST_CLAMP_NAVIGATION,
    OPTIONS_MENU_ROW_HEIGHT_PIXELS
};

/// Callback table for the vibration-only panel's sole row.
///
/// Entry zero follows `UiListRowCallback`'s input and borrowing contract.
/// The list borrows this overlay's storage; its entries are never replaced.
static UiListRowCallback _gOptionsVibrationRowCallbacks[] = { _optionsUpdateVibrationRow };

/// Mutable list state for the vibration-only options panel.
///
/// Requests its sole row at 15-pixel spacing with clamped navigation. Fitting
/// and drawing update its viewport, input and selection state. Only one
/// vibration-only panel may use this overlay-owned list at a time.
static UiList _gOptionsVibrationList = {
    _gOptionsVibrationRowCallbacks,
    ARRAY_SIZE(_gOptionsVibrationRowCallbacks),
    ARRAY_SIZE(_gOptionsVibrationRowCallbacks),
    OPTIONS_LIST_CLAMP_NAVIGATION,
    OPTIONS_VIBRATION_ROW_HEIGHT_PIXELS
};

/// Read-only spawn recipe for the options panel's key-configuration child.
///
/// Initial bounds are (-140, -96, 280, 144) screen-centered pixels; the editor
/// fits its content height on its first update. Uses OT tag base 52 and a
/// bodyless task at priority 192. `uiSpawnObject` copies the recipe; the
/// callback code must remain loaded until the parent closes the child.
static const UiObjectDesc _gOptionsKeyConfigurationDesc __attribute__((section(".data"), aligned(4))) = {
    USER_INTERFACE_PANEL_TITLE_STYLE,
    { -140,
      -96,
      280,
      144 },
    OPTIONS_KEY_CONFIGURATION_OT_INDEX,
    0x0000,
    TASK_BODY_NONE,
    TASK_PRIORITY_CONTROL,
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

static const _OptionsKeyIconUv _gOptionsKeyIconUvs[8];

/// One 16-byte stack slot of the key configuration draw routine, used for two
/// unrelated things in turn.
///
/// The routine's final label is drawn from `labelRequest`; once that draw has
/// returned, each pass of the icon loop copies the glyph table into the same
/// bytes as `iconUvs` and reads the row's entry back. Neither member is ever
/// read through the other: the union states only that the two occupy one slot,
/// which is what holds the routine's frame at its original size.
typedef union {
    TextDrawReq       labelRequest;
    _OptionsKeyIconUv iconUvs[ARRAY_SIZE(_gOptionsKeyIconUvs)];
} _OptionsKeyConfigStackSlot;
STATIC_ASSERT_SIZEOF(_OptionsKeyConfigStackSlot, 0x10);

/// Draws the Sound row and applies stereo/mono changes immediately.
///
/// Borrows the list and object under `UiListRowCallback`'s contract. The live
/// sound option is 0 stereo or 1 mono. Active row input wraps Left/Right through
/// the labels; drawing uses the option from before input. The selected row
/// publishes help while panel control is active or suspended active.
static void _optionsUpdateSoundRow(UiList* list, UiObject* object)
{
    const u8*  labels[] = { _gOptionsSoundStereoLabel, _gOptionsSoundMonoLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, _gOptionsSoundLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
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
                selectedMode = MEMORY_CARD_OPTION_SOUND_STEREO;
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
            case MEMORY_CARD_OPTION_SOUND_STEREO:
                sndOutputSetStereo(SOUND_OUTPUT_STEREO);
                break;
            case MEMORY_CARD_OPTION_SOUND_MONO:
                sndOutputSetStereo(SOUND_OUTPUT_MONO);
                break;
            default:
                sndOutputSetStereo(SOUND_OUTPUT_STEREO);
                break;
        }
    }
    controlMode = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gOptionsSoundPrompt, 0, 0);
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
        _gOptionsMusicVolume3Label,
        _gOptionsMusicVolume2Label,
        _gOptionsMusicVolume1Label,
        _gOptionsMusicVolumeOffLabel,
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, _gOptionsMusicLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
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
        uiSetPromptText(_gOptionsMusicVolumePrompt, 0, 0);
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
    const u8*  labels[] = { _gOptionsCursorMemoryLabel, _gOptionsCursorStandardLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, _gOptionsCursorLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
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
        uiSetPromptText(_gOptionsCursorPrompt, 0, 0);
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
    const u8*  labels[] = { _gOptionsVibrationOnLabel, _gOptionsVibrationOffLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, _gOptionsVibrationLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
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
        uiSetPromptText(_gOptionsVibrationPrompt, 0, 0);
    }
}

/// Draws the Movement row and selects walking or running as the default.
///
/// Borrows the list and object under `UiListRowCallback`'s contract. Saved
/// index 0 selects Walk and 1 Run. Active row input wraps Left/Right; the
/// selected row publishes help with active or suspended active control.
static void _optionsUpdateMovementRow(UiList* list, UiObject* object)
{
    const u8*  labels[] = { _gOptionsMovementWalkLabel, _gOptionsMovementRunLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, _gOptionsMovementLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
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
                selectedMode = MEMORY_CARD_OPTION_MOVEMENT_WALK;
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
        uiSetPromptText(_gOptionsMovementPrompt, 0, 0);
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

    list   = &_gOptionsMenuList;
    object = owningTask->spawnArg2.pointer;
    if (owningTask->spawnArg1.value == OPTIONS_MENU_VIBRATION_ONLY) {
        list = &_gOptionsVibrationList;
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
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->resultValue = controlMode;
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
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
/// Draws an action label in the key-configuration chart with medium outlined text.
///
/// `requestValue` is a writable `TextDrawReq` lvalue whose X anchor and baseline Y
/// are already in draw-environment pixels; no panel-origin or baseline offset is
/// added. `colorValue` packs R/G/B in bits 0..23 (R low byte); `alignmentValue`
/// selects `TEXT_ALIGNMENT_*` and narrows to a signed byte.
///
/// Borrows the live `UiObject` and NUL-terminated UI text under `textDrawString`'s
/// contract. The panel's signed OT base + 1 (fill) and + 2 (outline) must be valid,
/// with fonts, palettes and sufficient primitive storage available. Drawing
/// advances the request's X/Y pen and initializes its texture V bias; restore
/// placement before drawing another label. The object and text are not retained.
///
/// Expands to one void expression with assignments sequenced before drawing.
/// `requestValue` is evaluated six times and must identify stable storage without
/// side effects; every other argument is evaluated once. Captures no caller locals.
#define OPTIONS_KEY_DRAW_ASSIGNMENT(requestValue, objectValue, colorValue, alignmentValue, textValue) \
    ((requestValue).otIndex    = (objectValue)->panel.otIndex.signedValue + 1,                        \
     (requestValue).colorRgb   = (colorValue),                                                        \
     (requestValue).glyphTable = TEXT_GLYPH_TABLE_MEDIUM,                                             \
     (requestValue).alignment  = (alignmentValue),                                                    \
     (requestValue).drawMode   = TEXT_DRAW_OUTLINED,                                                  \
     textDrawString(&(requestValue), (textValue)))
    enum {
        OPTIONS_KEY_TASK_INITIAL           = 0,
        OPTIONS_KEY_TASK_READY             = 1,
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
    const u8*                  labels[] = { _gOptionsKeyLayoutALabel, _gOptionsKeyLayoutBLabel, _gOptionsKeyLayoutCLabel };
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

    alternateMovementLabel = _gOptionsKeyRunLabel;
    contentTopY            = object->panel.contentTop.signedValue;

    defaultMovement = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode;
    layout          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
    penY            = contentTopY + 0xF;
    if (defaultMovement == MEMORY_CARD_OPTION_MOVEMENT_RUN) {
        alternateMovementLabel = _gOptionsKeyWalkLabel;
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
    textDrawUiLine(object, penX, penY, _gOptionsKeyMenuHeading, labelColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    headerY = penY;
    penY   += 0xB;

    // Show menu actions, including the two Cancel rows and the scroll controls.
    menuTopFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuTopFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuTopFaceRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gOptionsKeyCancelLabel);

    menuSecondFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuSecondFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                   += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuSecondFaceRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gOptionsKeyCancelLabel);

    menuConfirmRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuConfirmRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuConfirmRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gOptionsKeyConfirmLabel);

    menuHelpRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuHelpRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY             += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuHelpRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gOptionsKeyHelpLabel);

    menuScrollDownRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuScrollDownRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                   += 0x1E;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuScrollDownRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gOptionsKeyScrollDownLabel);

    menuScrollUpRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuScrollUpRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penX                  = contentLeftX + 0x6A;
    penY                  = headerY;
    OPTIONS_KEY_DRAW_ASSIGNMENT(menuScrollUpRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gOptionsKeyScrollUpLabel);

    // Show normal-play actions; the movement button uses the non-default gait.
    textDrawUiLine(object, penX, penY, _gOptionsKeyNormalHeading, labelColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    penY += 0xB;
    if (layout != MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gOptionsKeyDrawWeaponLabel);
    } else {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, alternateMovementLabel);
    }

    penY += 0xF;
    if (layout != MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, alternateMovementLabel);
    } else {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, _gOptionsKeyDrawWeaponLabel);
    }

    penY += 0xF;

    normalActionRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
    normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                 += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_CENTER, _gOptionsKeyExamineLabel);

    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_C) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, _gOptionsKeyDrawWeaponLabel);

        penY                              += 0xF;
        normalAlternateDrawWeaponRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalAlternateDrawWeaponRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(normalAlternateDrawWeaponRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_LEFT, _gOptionsKeyDrawWeaponLabel);
    }

    // Show battle actions; the PE Menu entry is centered when it spans both columns.
    penY = headerY;
    penX = object->panel.contentRight.signedValue - 4;
    textDrawUiLine(object, penX, penY, _gOptionsKeyBattleHeading, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    penY           += 0xB;
    battleAlignment = TEXT_ALIGNMENT_RIGHT;
    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_A) {
        battleActionLabel = _gOptionsKeySwitchTargetsLabel;
    } else if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        battleActionLabel = _gOptionsKeyCancelLabel;
    } else {
        battleActionLabel = _gOptionsKeyMainWeaponLabel;
    }
    battleTopFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleTopFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                  += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(battleTopFaceRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, battleActionLabel);

    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_A) {
        battleActionLabel = _gOptionsKeyCancelLabel;
    } else if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        battleActionLabel = _gOptionsKeySwitchTargetsLabel;
    } else {
        battleActionLabel = _gOptionsKeyCancelLabel;
    }
    battleSecondFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleSecondFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                     += 0x1E;
    OPTIONS_KEY_DRAW_ASSIGNMENT(battleSecondFaceRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, battleActionLabel);

    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_C) {
        battleActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(battleActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, _gOptionsKeySubWeaponLabel);
    } else {
        battleActionRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
        battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(battleActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_CENTER, _gOptionsKeyPeMenuLabel);
    }

    penY += 0xF;
    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_A) {
        battleActionLabel = _gOptionsKeyMainWeaponLabel;
    } else if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        battleActionLabel = _gOptionsKeyMainWeaponLabel;
    } else {
        battleActionLabel = _gOptionsKeySwitchTargetsLabel;
    }
    battleActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                 += 0xF;
    OPTIONS_KEY_DRAW_ASSIGNMENT(battleActionRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, battleActionLabel);

    if (layout != MEMORY_CARD_OPTION_BUTTON_LAYOUT_C) {
        finalBattleLabelAndIconSlot.labelRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        finalBattleLabelAndIconSlot.labelRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(finalBattleLabelAndIconSlot.labelRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, battleAlignment, _gOptionsKeySubWeaponLabel);
    } else {
        finalBattleLabelAndIconSlot.labelRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
        finalBattleLabelAndIconSlot.labelRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        OPTIONS_KEY_DRAW_ASSIGNMENT(finalBattleLabelAndIconSlot.labelRequest, object, OPTIONS_KEY_LABEL_COLOR_RGB, TEXT_ALIGNMENT_CENTER, _gOptionsKeyPeMenuLabel);
    }

    // Draw seven button glyphs; the stored eighth origin has no displayed row.
    penY         = headerY;
    penX         = object->panel.contentLeft.signedValue + 2;
    iconRowIndex = 0;
    do {
        __builtin_memcpy(&finalBattleLabelAndIconSlot.iconUvs, _gOptionsKeyIconUvs, sizeof(_gOptionsKeyIconUvs));
        iconWidth      = OPTIONS_KEY_ICON_EDGE_PIXELS;
        iconHeight     = OPTIONS_KEY_ICON_EDGE_PIXELS;
        iconSprite     = gGpuPrimCursor;
        gGpuPrimCursor = iconSprite + 1;
        iconSprite->y0 = penY - OPTIONS_KEY_ICON_EDGE_PIXELS;
        iconSprite->x0 = penX;
        if (iconRowIndex >= OPTIONS_KEY_FACE_BUTTON_ROWS) {
            iconHeight     = OPTIONS_KEY_SHOULDER_HEIGHT_PIXELS;
            iconSprite->y0 = penY - 0xB;
        }
        penY            += 0xF;
        iconSprite->w    = iconWidth;
        iconSprite->h    = iconHeight;
        iconSprite->u0   = finalBattleLabelAndIconSlot.iconUvs[iconRowIndex].u;
        iconSprite->v0   = finalBattleLabelAndIconSlot.iconUvs[iconRowIndex].v;
        iconSprite->clut = OPTIONS_KEY_ICON_CLUT;
        setSprt(iconSprite);
        setShadeTex(iconSprite, OPTIONS_KEY_ICON_RAW_TEXTURE);
        addPrim(&gGpuCurrentOt[object->panel.otIndex.signedValue + 1], iconSprite);
        iconRowIndex++;
    } while (iconRowIndex < OPTIONS_KEY_ASSIGNMENT_ROWS);

    promptControlMode = object->panel.control.word;
    if (((promptControlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (promptControlMode == USER_INTERFACE_PANEL_ACTIVE)) {
        switch (layout) {
            case MEMORY_CARD_OPTION_BUTTON_LAYOUT_A:
                uiSetPromptText(_gOptionsKeyLayoutAPrompt, 0, 0);
                break;
            case MEMORY_CARD_OPTION_BUTTON_LAYOUT_B:
                uiSetPromptText(_gOptionsKeyLayoutBPrompt, 0, 0);
                break;
            case MEMORY_CARD_OPTION_BUTTON_LAYOUT_C:
                uiSetPromptText(_gOptionsKeyLayoutCPrompt, 0, 0);
                break;
            default:
                uiSetPromptText(_gOptionsKeyEmptyPrompt, 0, 0);
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
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
#undef OPTIONS_KEY_DRAW_ASSIGNMENT
}

// Keep these bytes after the chart's label/title constants in .rodata.
/// Texture-page origins of the key chart's pad-button glyphs, in row order.
///
/// U/V are unsigned texel coordinates on texture page zero with CLUT 0x3C00.
/// Each draw copies all eight origins into temporary storage, then reads one
/// of rows 0..6. The first four glyphs are 15x15 texels; the last three drawn
/// glyphs are 15x8. Entry 7 is retained in the copy but has no displayed row;
/// its button identity is unproven. The table belongs to this loaded overlay.
static const _OptionsKeyIconUv _gOptionsKeyIconUvs[8] = {
    { 0x10, 0x60 }, // Square
    { 0x10, 0x70 }, // Circle
    { 0x20, 0x60 }, // Cross
    { 0x20, 0x70 }, // Triangle
    { 0x90, 0x58 }, // R1
    { 0xB0, 0x58 }, // R2
    { 0x80, 0x58 }, // L1
    { 0xA0, 0x58 }, // Stored origin with no displayed row
};

/// Draws Key Config and opens its child editor when Confirm is pressed.
///
/// Borrows the arguments under `UiListRowCallback`'s contract. The selected
/// row publishes help with active or suspended active control. Active input
/// spawns a child with active control and a one-tick opening delay, then
/// suspends the parent even if allocation fails. The parent handles child exit.
static void _optionsUpdateKeyConfigurationRow(UiList* list, UiObject* object)
{
    s32 controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, _gOptionsKeyConfigurationLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    controlMode = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gOptionsKeyConfigurationPrompt, 0, 0);
    }
    if ((list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        uiSpawnObject(&_gOptionsKeyConfigurationDesc, 0, USER_INTERFACE_PANEL_ACTIVE, 1, object);
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 6, list->rowTextY.signedValue, _gOptionsRestoreDefaultsLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    controlMode = object->panel.control.word;
    if ((((controlMode >> OPTIONS_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gOptionsRestoreDefaultsPrompt, 0, 0);
    }
    if ((list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        mcResetOptions();
    }
}
