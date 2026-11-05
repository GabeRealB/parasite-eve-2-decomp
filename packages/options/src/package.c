#include "options.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include <core/pe2.h>

#include <engine/display.h>
#include <engine/mc.h>
#include <engine/mc_types.h>
#include <engine/pad.h>
#include <engine/sound.h>
#include <engine/task_types.h>
#include <engine/text.h>
#include <engine/ui.h>
#include <engine/ui_types.h>

PE2_EDITOR_LABEL("Layout")
/// Pixels between the rows of the full options list, and the height of the vibration-only panel's one row. A list keeps it
/// in a signed byte, where zero asks for the UI's default spacing (asserted beside the lists).
PE2_OPTION_DEF(MENU_ROW_HEIGHT, int, 18)
PE2_OPTION_DEF(VIBRATION_ROW_HEIGHT, int, 15)
/// Pixels from the panel's content edge to each row's label, and to its first choice.
PE2_OPTION_DEF(LABEL_X, int, 6)
PE2_OPTION_DEF(CHOICES_X, int, 0x78)
/// Content width of the vibration-only panel, which is centered on the screen.
PE2_OPTION_DEF(VIBRATION_PANEL_WIDTH, int, 192)

PE2_EDITOR_LABEL("Key configuration")
/// Color of the key panel's labels and carets, and of the fill behind the selected layout: packed color words, red in
/// the low byte.
PE2_OPTION_DEF(KEY_LABEL_COLOR, int, 0x606060)
PE2_OPTION_DEF(KEY_SELECTOR_COLOR, int, 0x1741F)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(IS_COLOR(PE2_OPTION(KEY_LABEL_COLOR)), key_label_color_is_a_color);
STATIC_ASSERT(IS_COLOR(PE2_OPTION(KEY_SELECTOR_COLOR)), key_selector_color_is_a_color);

/* The options list: each setting's label, then its choices, each by the saved value it shows. */
static u8 _gRestoreDefaultsLabel[]  = "Restore default configuration";
static u8 _gVibrationLabel[]        = "Vibration";
static u8 _gVibrationOnLabel[]      = "On";       // 0
static u8 _gVibrationOffLabel[]     = "Off";      // 1
static u8 _gSoundLabel[]            = "Sound";
static u8 _gSoundStereoLabel[]      = "Stereo";   // 0
static u8 _gSoundMonoLabel[]        = "Mono";     // 1
static u8 _gMusicLabel[]            = "Music";
static u8 _gMusicVolume3Label[]     = "3";        // 0: gain 100
static u8 _gMusicVolume2Label[]     = "2";        // 1: gain 64
static u8 _gMusicVolume1Label[]     = "1";        // 2: gain 32
static u8 _gMusicVolumeOffLabel[]   = "Off";      // 3: muted
static u8 _gCursorLabel[]           = "Cursor";
static u8 _gCursorStandardLabel[]   = "Standard"; // 1: each list opens at its first row
static u8 _gCursorMemoryLabel[]     = "Memory";   // 0: each list opens where it was left
static u8 _gKeyConfigurationLabel[] = "Key Config";
static u8 _gMovementLabel[]         = "Movement";
static u8 _gMovementWalkLabel[]     = "Walk"; // 0
static u8 _gMovementRunLabel[]      = "Run";  // 1

/* The key configuration chart: the actions it assigns, its column headings and the layouts it offers. */
static u8 _gKeyConfirmLabel[]       = "OK";
static u8 _gKeyCancelLabel[]        = "Cancel";
static u8 _gKeyHelpLabel[]          = "Help";
static u8 _gKeyScrollDownLabel[]    = "Scroll Down";
static u8 _gKeyScrollUpLabel[]      = "Scroll Up";
static u8 _gKeyDrawWeaponLabel[]    = "Draw Weapon";
static u8 _gKeyExamineLabel[]       = "Examine";
static u8 _gKeyRunLabel[]           = "Run";  // the other movement, when the default is walking
static u8 _gKeyWalkLabel[]          = "Walk"; // and when it is running
static u8 _gKeySwitchTargetsLabel[] = "Switch Targets";
static u8 _gKeyPeMenuLabel[]        = "PE Menu";
static u8 _gKeyMainWeaponLabel[]    = "Main Weapon";
static u8 _gKeySubWeaponLabel[]     = "Sub Weapon";
static u8 _gKeyMenuHeading[]        = "MENU";
static u8 _gKeyNormalHeading[]      = "NORMAL";
static u8 _gKeyBattleHeading[]      = "BATTLE";
static u8 _gKeyLayoutALabel[]       = "Type A"; // 0
static u8 _gKeyLayoutBLabel[]       = "Type B"; // 1
static u8 _gKeyLayoutCLabel[]       = "Type C"; // 2

/* The help line of the row selected, and of each controller layout; the font draws `{`, `}`, `~` and `|` as pad buttons. */
static u8 _gSoundPrompt[]            = "Set sound output to match\nyour television.";
static u8 _gMusicVolumePrompt[]      = "Set music volume level.\nSet low for clearer sound effects.";
static u8 _gCursorPrompt[]           = "Set menu cursor. You can have it\nremember the last position used.";
static u8 _gVibrationPrompt[]        = "Set vibration mode ON/OFF.\nSelect ON for more realistic play.";
static u8 _gMovementPrompt[]         = "Set Aya's default movement.\nBeginners should use \"Walk.\"";
static u8 _gKeyConfigurationPrompt[] = "Open key settings menu.\nPress the } button to proceed.";
static u8 _gRestoreDefaultsPrompt[]  = "Restore default settings.\nPress the } button.";
static u8 _gKeyLayoutAPrompt[]       = "Default setting.";
static u8 _gKeyLayoutBPrompt[]       = "The function of the { button\nand ~ button is switched.";
static u8 _gKeyLayoutCPrompt[]       = "This mode uses the ~ button and\n | button for combat.";

/// Suspended-control encoding.
enum {
    SUSPENDED_CONTROL_SHIFT = 16,
};

static void _updateSoundRow(UiList* list, UiObject* object);
static void _updateMusicVolumeRow(UiList* list, UiObject* object);
static void _updateCursorRow(UiList* list, UiObject* object);
static void _updateVibrationRow(UiList* list, UiObject* object);
static void _updateMovementRow(UiList* list, UiObject* object);
static void _updateKeyConfigurationRow(UiList* list, UiObject* object);
static void _updateRestoreDefaultsRow(UiList* list, UiObject* object);
static void _updateKeyConfigurationTask(Task* owningTask);

/// The help line of a controller layout out of range.
static u8 _gKeyEmptyPrompt[] = "";

/// The full list's rows, top to bottom.
static UiListRowCallback _gMenuRowCallbacks[] = {
    _updateSoundRow,
    _updateMusicVolumeRow,
    _updateCursorRow,
    _updateVibrationRow,
    _updateMovementRow,
    _updateKeyConfigurationRow,
    _updateRestoreDefaultsRow,
};

/// The lists' navigation, and the key chart's ordering-table entry.
enum {
    LIST_CLAMP_NAVIGATION      = 0,
    KEY_CONFIGURATION_OT_INDEX = 52,
};

STATIC_ASSERT(PE2_OPTION(MENU_ROW_HEIGHT) > 0 && PE2_OPTION(MENU_ROW_HEIGHT) <= 0x7F, menu_row_height_fits_a_list);
STATIC_ASSERT(PE2_OPTION(VIBRATION_ROW_HEIGHT) > 0 && PE2_OPTION(VIBRATION_ROW_HEIGHT) <= 0x7F, vibration_row_height_fits_a_list);

/// The full list, navigation clamped at either end. It holds its selection, so one full panel uses it at a time.
static UiList _gMenuList = {
    _gMenuRowCallbacks,
    ARRAY_SIZE(_gMenuRowCallbacks),
    ARRAY_SIZE(_gMenuRowCallbacks),
    LIST_CLAMP_NAVIGATION,
    PE2_OPTION(MENU_ROW_HEIGHT)
};

/// The vibration-only panel's one row.
static UiListRowCallback _gVibrationRowCallbacks[] = { _updateVibrationRow };

/// The vibration-only list, used by one such panel at a time.
static UiList _gVibrationList = {
    _gVibrationRowCallbacks,
    ARRAY_SIZE(_gVibrationRowCallbacks),
    ARRAY_SIZE(_gVibrationRowCallbacks),
    LIST_CLAMP_NAVIGATION,
    PE2_OPTION(VIBRATION_ROW_HEIGHT)
};

/// The key configuration panel the Key Config row opens, a child of the options panel; its first update fits its height.
static UiObjectDesc _gKeyConfigurationDesc = {
    USER_INTERFACE_PANEL_TITLE_STYLE,
    { -140,
      -96,
      280,
      144 },
    KEY_CONFIGURATION_OT_INDEX,
    0x0000,
    TASK_BODY_NONE,
    TASK_PRIORITY_CONTROL,
    _updateKeyConfigurationTask,
    0,
};

/// Top-left texel of one pad-button glyph in the user-interface texture page.
///
/// Only the origin is stored. A glyph's extent comes from the row that draws
/// it: 15x15 texels for a face button, 15x8 for a shoulder button.
typedef struct {
    u8 u; // Texel column of the glyph's left edge
    u8 v; // Texel row of the glyph's top edge
} _KeyIconUv;

static const _KeyIconUv _gKeyIconUvs[8];

/// One 16-byte stack slot of the key configuration draw routine, used for two
/// unrelated things in turn.
///
/// The routine's final label is drawn from `labelRequest`; once that draw has
/// returned, each pass of the icon loop copies the glyph table into the same
/// bytes as `iconUvs` and reads the row's entry back. Neither member is ever
/// read through the other: the union states only that the two occupy one slot,
/// which is what holds the routine's frame at its original size.
typedef union {
    TextDrawReq labelRequest;
    _KeyIconUv  iconUvs[ARRAY_SIZE(_gKeyIconUvs)];
} _KeyConfigStackSlot;

/* The rows. Each draws its label and its choices, the saved one bright, takes Left and Right through them while its input
 * is on, applying a change at once, and shows its help line while selected and the panel is active or suspended active. */

/// Sound: stereo or mono.
static void _updateSoundRow(UiList* list, UiObject* object)
{
    const u8*  labels[] = { _gSoundStereoLabel, _gSoundMonoLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + PE2_OPTION(LABEL_X), list->rowTextY.signedValue, _gSoundLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    previousMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode;
    choiceOffsetNumerator = 0;
    selectedMode          = previousMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + PE2_OPTION(CHOICES_X);
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
    if ((((controlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gSoundPrompt, 0, 0);
    }
}

/// Music: the volume, from 3 to off.
static void _updateMusicVolumeRow(UiList* list, UiObject* object)
{
    enum { MUSIC_FULL_VOLUME = 0 };
    const u8* labels[] = {
        _gMusicVolume3Label,
        _gMusicVolume2Label,
        _gMusicVolume1Label,
        _gMusicVolumeOffLabel,
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + PE2_OPTION(LABEL_X), list->rowTextY.signedValue, _gMusicLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    previousMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume;
    choiceOffsetNumerator = 0;
    selectedMode          = previousMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + PE2_OPTION(CHOICES_X);
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
                selectedMode = MUSIC_FULL_VOLUME;
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
    if ((((controlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gMusicVolumePrompt, 0, 0);
    }
}

/// Cursor: whether lists remember their selection.
static void _updateCursorRow(UiList* list, UiObject* object)
{
    enum { CURSOR_MEMORY = 0 };
    const u8*  labels[] = { _gCursorMemoryLabel, _gCursorStandardLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + PE2_OPTION(LABEL_X), list->rowTextY.signedValue, _gCursorLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    choiceOffsetNumerator = 0;
    selectedMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cursorMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + PE2_OPTION(CHOICES_X);
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
                selectedMode = CURSOR_MEMORY;
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
    if ((((controlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gCursorPrompt, 0, 0);
    }
}

/// Vibration: on or off, in both lists.
static void _updateVibrationRow(UiList* list, UiObject* object)
{
    enum { VIBRATION_ON = 0 };
    const u8*  labels[] = { _gVibrationOnLabel, _gVibrationOffLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + PE2_OPTION(LABEL_X), list->rowTextY.signedValue, _gVibrationLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    choiceOffsetNumerator = 0;
    selectedMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
    choicesLeftX          = object->panel.contentLeft.signedValue + PE2_OPTION(CHOICES_X);
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
                selectedMode = VIBRATION_ON;
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
    if ((((controlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gVibrationPrompt, 0, 0);
    }
}

/// Movement: whether Aya walks or runs by default.
static void _updateMovementRow(UiList* list, UiObject* object)
{
    const u8*  labels[] = { _gMovementWalkLabel, _gMovementRunLabel };
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
    textDrawUiLine(object, object->panel.contentLeft.signedValue + PE2_OPTION(LABEL_X), list->rowTextY.signedValue, _gMovementLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    choiceIndex           = 0;
    labelCursor           = labels;
    choiceOffsetNumerator = 0;
    selectedMode          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode;
    choicesLeftX          = object->panel.contentLeft.signedValue + PE2_OPTION(CHOICES_X);
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
    if ((((controlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gMovementPrompt, 0, 0);
    }
}

/// Gives the vibration-only panel its width and centers it, its height kept.
static inline void _centerVibrationPanel(UiPanel* panel)
{
    uiSetPanelContentSize(panel, PE2_OPTION(VIBRATION_PANEL_WIDTH), 0);
    panel->bounds.rect.y = -(panel->bounds.rect.h / 2);
    panel->bounds.rect.x = -(panel->bounds.rect.w / 2);
}

void optionsUpdateMenuTask(Task* owningTask)
{
    enum {
        MENU_TASK_INITIAL = 0,
        MENU_TASK_READY   = 1
    };
    UiList*   list;
    UiObject* object;
    UiObject* childObject;
    s32       controlMode;
    s32       childResult;

    list   = &_gMenuList;
    object = owningTask->spawnArg2.pointer;
    if (owningTask->spawnArg1.value == OPTIONS_MENU_VIBRATION_ONLY) {
        list = &_gVibrationList;
    }
    // Fit once; only the vibration layout explicitly centers the outer bounds.
    if (owningTask->state == MENU_TASK_INITIAL) {
        uiFitPanelToList(list, &object->panel);
        owningTask->state += MENU_TASK_READY - MENU_TASK_INITIAL;
        if (owningTask->spawnArg1.value == OPTIONS_MENU_VIBRATION_ONLY) {
            _centerVibrationPanel(&object->panel);
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

/// The key configuration panel: the chart of what each button does in the layout chosen, and the choice of layout.
///
/// Right or Down picks the next layout and Up or Left the one before, applied at once; Confirm or Cancel closes the panel,
/// keeping the layout, and Menu cancels the whole options flow. Its first update fits the panel and keeps the layout it
/// opened with in spawnArg1.
static void _updateKeyConfigurationTask(Task* owningTask)
{
/// Draws an action label of the chart: medium outlined text at the request's pen, which it advances. `requestValue` is
/// evaluated six times, so it names storage.
#define KEY_DRAW_ASSIGNMENT(requestValue, objectValue, colorValue, alignmentValue, textValue) \
    ((requestValue).otIndex    = (objectValue)->panel.otIndex.signedValue + 1,                \
     (requestValue).colorRgb   = (colorValue),                                                \
     (requestValue).glyphTable = TEXT_GLYPH_TABLE_MEDIUM,                                     \
     (requestValue).alignment  = (alignmentValue),                                            \
     (requestValue).drawMode   = TEXT_DRAW_OUTLINED,                                          \
     textDrawString(&(requestValue), (textValue)))
    enum {
        KEY_TASK_INITIAL           = 0,
        KEY_TASK_READY             = 1,
        KEY_ASSIGNMENT_ROWS        = 7,
        KEY_FACE_BUTTON_ROWS       = 4,
        KEY_PANEL_TEXT_ROWS        = 9,
        KEY_ICON_EDGE_PIXELS       = 15,
        KEY_SHOULDER_HEIGHT_PIXELS = 8,
        KEY_ICON_CLUT              = 0x3C00, // VRAM palette at (0, 240)
        KEY_ICON_RAW_TEXTURE       = 1,
    };
    UiObject*           object   = owningTask->spawnArg2.pointer;
    const u8*           labels[] = { _gKeyLayoutALabel, _gKeyLayoutBLabel, _gKeyLayoutCLabel };
    const u8*           alternateMovementLabel;
    TextDrawReq         menuTopFaceRequest;
    TextDrawReq         menuSecondFaceRequest;
    TextDrawReq         menuConfirmRequest;
    TextDrawReq         menuHelpRequest;
    TextDrawReq         menuScrollDownRequest;
    TextDrawReq         menuScrollUpRequest;
    TextDrawReq         normalActionRequest;
    TextDrawReq         normalAlternateDrawWeaponRequest;
    TextDrawReq         battleTopFaceRequest;
    TextDrawReq         battleSecondFaceRequest;
    TextDrawReq         battleActionRequest;
    _KeyConfigStackSlot finalBattleLabelAndIconSlot;
    s32                 contentTopY;
    s32                 penX;
    s32                 penY;
    s32                 headerY;
    s32                 selectorTopY;
    s32                 contentRightX;
    s32                 contentLeftX;
    s32                 layoutLabelX;
    s32                 selectorControlMode;
    s32                 promptControlMode;
    s32                 iconRowIndex;
    s32                 iconHeight;
    s32                 iconWidth;
    s32                 upCaretOuterRightX;
    s32                 upCaretOriginInsetX;
    s32                 downCaretOuterRightX;
    s32                 downCaretOriginInsetX;
    s32                 layout;
    u32                 labelColorRgb;
    s32                 battleAlignment;
    s32                 defaultMovement;
    s32                 selectorSeparatorY;
    const u8*           battleActionLabel;
    SPRT*               iconSprite;

    alternateMovementLabel = _gKeyRunLabel;
    contentTopY            = object->panel.contentTop.signedValue;

    defaultMovement = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode;
    layout          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
    penY            = contentTopY + 0xF;
    if (defaultMovement == MEMORY_CARD_OPTION_MOVEMENT_RUN) {
        alternateMovementLabel = _gKeyWalkLabel;
    }
    // Fit once, retaining the initial layout in the task payload.
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "Key Configuration");
    if (owningTask->state == KEY_TASK_INITIAL) {
        uiSetPanelContentSize(&object->panel, 0, uiGetTextRowsHeight(KEY_PANEL_TEXT_ROWS) + 6);
        owningTask->spawnArg1.value = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
        owningTask->state          += KEY_TASK_READY - KEY_TASK_INITIAL;
    }
    selectorTopY        = contentTopY + 1;
    contentRightX       = object->panel.contentRight.signedValue;
    layoutLabelX        = contentRightX - 0x3B;
    upCaretOuterRightX  = object->panel.bounds.rect.x + object->panel.bounds.rect.w;
    upCaretOriginInsetX = object->panel.contentOriginX.signedValue + 5;
    uiDrawFlatCaret(&object->panel, upCaretOuterRightX - upCaretOriginInsetX, selectorTopY, PE2_OPTION(KEY_LABEL_COLOR), USER_INTERFACE_CARET_UP);
    downCaretOuterRightX  = object->panel.bounds.rect.x + object->panel.bounds.rect.w;
    downCaretOriginInsetX = object->panel.contentOriginX.signedValue + 5;
    uiDrawFlatCaret(&object->panel, downCaretOuterRightX - downCaretOriginInsetX, penY, PE2_OPTION(KEY_LABEL_COLOR), USER_INTERFACE_CARET_DOWN);
    textDrawUiLine(object, layoutLabelX, penY, labels[layout], PE2_OPTION(KEY_LABEL_COLOR), TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    selectorControlMode = object->panel.control.word;
    if (((selectorControlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (selectorControlMode == USER_INTERFACE_PANEL_ACTIVE)) {
        uiEaseAndDrawCursor(&object->panel, layoutLabelX, contentTopY + 7);
        if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            object->panel.otIndex.signedValue = object->panel.otIndex.signedValue + 1;
            uiFillRectInterior(&object->panel, contentRightX - 0x40, selectorTopY, 0x3A, 0xE, PE2_OPTION(KEY_SELECTOR_COLOR));
            object->panel.otIndex.signedValue = object->panel.otIndex.signedValue - 1;
        }
    }
    selectorSeparatorY = penY + 2;
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, selectorSeparatorY);
    labelColorRgb = PE2_OPTION(KEY_LABEL_COLOR);
    uiDrawVerticalSeparator(&object->panel, penY + 5, object->panel.contentBottom.signedValue, object->panel.contentLeft.signedValue + 0x5F);
    penY        += 0x13;
    contentLeftX = object->panel.contentLeft.signedValue;

    penX = contentLeftX + 0x1E;
    textDrawUiLine(object, penX, penY, _gKeyMenuHeading, labelColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    headerY = penY;
    penY   += 0xB;

    // Show menu actions, including the two Cancel rows and the scroll controls.
    menuTopFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuTopFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                += 0xF;
    KEY_DRAW_ASSIGNMENT(menuTopFaceRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gKeyCancelLabel);

    menuSecondFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuSecondFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                   += 0xF;
    KEY_DRAW_ASSIGNMENT(menuSecondFaceRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gKeyCancelLabel);

    menuConfirmRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuConfirmRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                += 0xF;
    KEY_DRAW_ASSIGNMENT(menuConfirmRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gKeyConfirmLabel);

    menuHelpRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuHelpRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY             += 0xF;
    KEY_DRAW_ASSIGNMENT(menuHelpRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gKeyHelpLabel);

    menuScrollDownRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuScrollDownRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                   += 0x1E;
    KEY_DRAW_ASSIGNMENT(menuScrollDownRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gKeyScrollDownLabel);

    menuScrollUpRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    menuScrollUpRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penX                  = contentLeftX + 0x6A;
    penY                  = headerY;
    KEY_DRAW_ASSIGNMENT(menuScrollUpRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gKeyScrollUpLabel);

    // Show normal-play actions; the movement button uses the non-default gait.
    textDrawUiLine(object, penX, penY, _gKeyNormalHeading, labelColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    penY += 0xB;
    if (layout != MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(normalActionRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, _gKeyDrawWeaponLabel);
    } else {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(normalActionRequest, object, labelColorRgb, TEXT_ALIGNMENT_LEFT, alternateMovementLabel);
    }

    penY += 0xF;
    if (layout != MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(normalActionRequest, object, PE2_OPTION(KEY_LABEL_COLOR), TEXT_ALIGNMENT_LEFT, alternateMovementLabel);
    } else {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(normalActionRequest, object, PE2_OPTION(KEY_LABEL_COLOR), TEXT_ALIGNMENT_LEFT, _gKeyDrawWeaponLabel);
    }

    penY += 0xF;

    normalActionRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
    normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                 += 0xF;
    KEY_DRAW_ASSIGNMENT(normalActionRequest, object, PE2_OPTION(KEY_LABEL_COLOR), TEXT_ALIGNMENT_CENTER, _gKeyExamineLabel);

    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_C) {
        normalActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(normalActionRequest, object, PE2_OPTION(KEY_LABEL_COLOR), TEXT_ALIGNMENT_LEFT, _gKeyDrawWeaponLabel);

        penY                              += 0xF;
        normalAlternateDrawWeaponRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        normalAlternateDrawWeaponRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(normalAlternateDrawWeaponRequest, object, PE2_OPTION(KEY_LABEL_COLOR), TEXT_ALIGNMENT_LEFT, _gKeyDrawWeaponLabel);
    }

    // Show battle actions; the PE Menu entry is centered when it spans both columns.
    penY = headerY;
    penX = object->panel.contentRight.signedValue - 4;
    textDrawUiLine(object, penX, penY, _gKeyBattleHeading, PE2_OPTION(KEY_LABEL_COLOR), TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    penY           += 0xB;
    battleAlignment = TEXT_ALIGNMENT_RIGHT;
    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_A) {
        battleActionLabel = _gKeySwitchTargetsLabel;
    } else if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        battleActionLabel = _gKeyCancelLabel;
    } else {
        battleActionLabel = _gKeyMainWeaponLabel;
    }
    battleTopFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleTopFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                  += 0xF;
    KEY_DRAW_ASSIGNMENT(battleTopFaceRequest, object, PE2_OPTION(KEY_LABEL_COLOR), battleAlignment, battleActionLabel);

    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_A) {
        battleActionLabel = _gKeyCancelLabel;
    } else if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        battleActionLabel = _gKeySwitchTargetsLabel;
    } else {
        battleActionLabel = _gKeyCancelLabel;
    }
    battleSecondFaceRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleSecondFaceRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                     += 0x1E;
    KEY_DRAW_ASSIGNMENT(battleSecondFaceRequest, object, PE2_OPTION(KEY_LABEL_COLOR), battleAlignment, battleActionLabel);

    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_C) {
        battleActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(battleActionRequest, object, PE2_OPTION(KEY_LABEL_COLOR), battleAlignment, _gKeySubWeaponLabel);
    } else {
        battleActionRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
        battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(battleActionRequest, object, PE2_OPTION(KEY_LABEL_COLOR), TEXT_ALIGNMENT_CENTER, _gKeyPeMenuLabel);
    }

    penY += 0xF;
    if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_A) {
        battleActionLabel = _gKeyMainWeaponLabel;
    } else if (layout == MEMORY_CARD_OPTION_BUTTON_LAYOUT_B) {
        battleActionLabel = _gKeyMainWeaponLabel;
    } else {
        battleActionLabel = _gKeySwitchTargetsLabel;
    }
    battleActionRequest.x = object->panel.contentOriginX.unsignedValue + penX;
    battleActionRequest.y = object->panel.contentOriginY.unsignedValue + penY;
    penY                 += 0xF;
    KEY_DRAW_ASSIGNMENT(battleActionRequest, object, PE2_OPTION(KEY_LABEL_COLOR), battleAlignment, battleActionLabel);

    if (layout != MEMORY_CARD_OPTION_BUTTON_LAYOUT_C) {
        finalBattleLabelAndIconSlot.labelRequest.x = object->panel.contentOriginX.unsignedValue + penX;
        finalBattleLabelAndIconSlot.labelRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(finalBattleLabelAndIconSlot.labelRequest, object, PE2_OPTION(KEY_LABEL_COLOR), battleAlignment, _gKeySubWeaponLabel);
    } else {
        finalBattleLabelAndIconSlot.labelRequest.x = object->panel.contentOriginX.unsignedValue + ((object->panel.contentRight.signedValue + 0x60 + object->panel.contentLeft.signedValue) / 2);
        finalBattleLabelAndIconSlot.labelRequest.y = object->panel.contentOriginY.unsignedValue + penY;
        KEY_DRAW_ASSIGNMENT(finalBattleLabelAndIconSlot.labelRequest, object, PE2_OPTION(KEY_LABEL_COLOR), TEXT_ALIGNMENT_CENTER, _gKeyPeMenuLabel);
    }

    // Draw seven button glyphs; the stored eighth origin has no displayed row.
    penY         = headerY;
    penX         = object->panel.contentLeft.signedValue + 2;
    iconRowIndex = 0;
    do {
        __builtin_memcpy(&finalBattleLabelAndIconSlot.iconUvs, _gKeyIconUvs, sizeof(_gKeyIconUvs));
        iconWidth      = KEY_ICON_EDGE_PIXELS;
        iconHeight     = KEY_ICON_EDGE_PIXELS;
        iconSprite     = gGpuPrimCursor;
        gGpuPrimCursor = iconSprite + 1;
        iconSprite->y0 = penY - KEY_ICON_EDGE_PIXELS;
        iconSprite->x0 = penX;
        if (iconRowIndex >= KEY_FACE_BUTTON_ROWS) {
            iconHeight     = KEY_SHOULDER_HEIGHT_PIXELS;
            iconSprite->y0 = penY - 0xB;
        }
        penY            += 0xF;
        iconSprite->w    = iconWidth;
        iconSprite->h    = iconHeight;
        iconSprite->u0   = finalBattleLabelAndIconSlot.iconUvs[iconRowIndex].u;
        iconSprite->v0   = finalBattleLabelAndIconSlot.iconUvs[iconRowIndex].v;
        iconSprite->clut = KEY_ICON_CLUT;
        setSprt(iconSprite);
        setShadeTex(iconSprite, KEY_ICON_RAW_TEXTURE);
        addPrim(&gGpuCurrentOt[object->panel.otIndex.signedValue + 1], iconSprite);
        iconRowIndex++;
    } while (iconRowIndex < KEY_ASSIGNMENT_ROWS);

    promptControlMode = object->panel.control.word;
    if (((promptControlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (promptControlMode == USER_INTERFACE_PANEL_ACTIVE)) {
        switch (layout) {
            case MEMORY_CARD_OPTION_BUTTON_LAYOUT_A:
                uiSetPromptText(_gKeyLayoutAPrompt, 0, 0);
                break;
            case MEMORY_CARD_OPTION_BUTTON_LAYOUT_B:
                uiSetPromptText(_gKeyLayoutBPrompt, 0, 0);
                break;
            case MEMORY_CARD_OPTION_BUTTON_LAYOUT_C:
                uiSetPromptText(_gKeyLayoutCPrompt, 0, 0);
                break;
            default:
                uiSetPromptText(_gKeyEmptyPrompt, 0, 0);
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
#undef KEY_DRAW_ASSIGNMENT
}

/// Where the chart's pad-button glyphs are on the UI texture page, by chart row: four 15x15 face buttons, then 15x8
/// shoulder buttons. Its place after the chart's strings is the original's. No row draws the last.
static const _KeyIconUv _gKeyIconUvs[8] = {
    { 0x10, 0x60 }, // Square
    { 0x10, 0x70 }, // Circle
    { 0x20, 0x60 }, // Cross
    { 0x20, 0x70 }, // Triangle
    { 0x90, 0x58 }, // R1
    { 0xB0, 0x58 }, // R2
    { 0x80, 0x58 }, // L1
    { 0xA0, 0x58 }, // Stored origin with no displayed row
};

/// Key Config: Confirm opens the key configuration panel and suspends this one, whether or not the child could be made.
static void _updateKeyConfigurationRow(UiList* list, UiObject* object)
{
    s32 controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + PE2_OPTION(LABEL_X), list->rowTextY.signedValue, _gKeyConfigurationLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    controlMode = object->panel.control.word;
    if ((((controlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gKeyConfigurationPrompt, 0, 0);
    }
    if ((list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        uiSpawnObject(&_gKeyConfigurationDesc, 0, USER_INTERFACE_PANEL_ACTIVE, 1, object);
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

/// Restore Defaults: Confirm resets every setting at once, asking nothing.
static void _updateRestoreDefaultsRow(UiList* list, UiObject* object)
{
    s32 controlMode;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + PE2_OPTION(LABEL_X), list->rowTextY.signedValue, _gRestoreDefaultsLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    controlMode = object->panel.control.word;
    if ((((controlMode >> SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (controlMode == USER_INTERFACE_PANEL_ACTIVE)) && (list->selectedItemIndex == list->currentItemIndex)) {
        uiSetPromptText(_gRestoreDefaultsPrompt, 0, 0);
    }
    if ((list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        mcResetOptions();
    }
}
