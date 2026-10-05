#include "ui.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "mc.h"
#include "task.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

/// Answers published by memory-card prompt rows with a confirm outcome.
enum {
    MEMORY_CARD_MENU_ANSWER_SELECTED = 1, // Yes, OK or the single Cancel action
    MEMORY_CARD_MENU_ANSWER_NO       = -1
};

/// Initialization phases and the retained list selector for File Information.
enum {
    MEMORY_CARD_MENU_FILE_INFORMATION_INITIAL   = 0,
    MEMORY_CARD_MENU_FILE_INFORMATION_READY     = 1,
    MEMORY_CARD_MENU_FILE_INFORMATION_SAVE_LIST = 1
};

/// Retains File Information's list selector and borrows its parent's dialog work.
///
/// Invoke once as a standalone statement with state zero and the original
/// integer first spawn argument. The parent must borrow live `McWork` storage.
/// `owningTaskValue` must be a stable Task pointer and `dialogWorkValue` a writable
/// McWork* lvalue, both without side effects: each is evaluated repeatedly.
/// Captures no locals. Keeps the payload's low 16 bits in callback-owned
/// `killCountdown`, replaces that payload with the parent's work pointer and
/// advances to state one. The caller retains the borrowed pointer's lifetime.
#define MEMORY_CARD_MENU_INIT_FILE_INFORMATION(owningTaskValue, dialogWorkValue)                                                    \
    do {                                                                                                                            \
        (owningTaskValue)->killCountdown     = (u16)(owningTaskValue)->spawnArg1.value;                                             \
        (dialogWorkValue)                    = (owningTaskValue)->parent->spawnArg1.pointer;                                        \
        (owningTaskValue)->state            += MEMORY_CARD_MENU_FILE_INFORMATION_READY - MEMORY_CARD_MENU_FILE_INFORMATION_INITIAL; \
        (owningTaskValue)->spawnArg1.pointer = (dialogWorkValue);                                                                   \
    } while (0)

static const char McText_Select[];

static UiListRowCallback Mc_YesNoCallbacks[];

static UiList Mc_YesNoList;

static UiListRowCallback Mc_OkCallbacks[];

static UiList Mc_OkList;

static UiListRowCallback Mc_YesCallbacks[];

static UiList Mc_YesList;

static void _uiUpdateListWithCenteredCursor(UiList* list, UiPanel* panel);

static void _mcMenuYesRow(UiList* list, UiObject* object);

static void _mcMenuOkRow(UiList* list, UiObject* object);

static void _mcMenuCancelRow(UiList* list, UiObject* object);

static void _mcMenuNoRow(UiList* list, UiObject* object);

static void _mcMenuUpdatePromptChoices(Task* owningTask);

static const char McText_Select[]      = "Select";
const char        McText_Time[]        = "TIME";
const char        McText_Clear[]       = "CLEAR";
const char        McText_Nightmare[]   = "Nightmare";
const char        McText_Scavenger[]   = "Scavenger";
const char        McText_Bounty[]      = "Bounty";
const char        McText_Replay[]      = "Replay";
const char        McText_OpenParen[]   = " (";
const char        McText_Exp[]         = "EXP";
const char        McText_Unavailable[] = "---";
const char        McText_Bp[]          = "BP";

static UiListRowCallback Mc_YesNoCallbacks[] = { _mcMenuYesRow, _mcMenuNoRow };
static UiList            Mc_YesNoList        = { Mc_YesNoCallbacks, 2, 2, 0, 0x0F };
static UiListRowCallback Mc_OkCallbacks[]    = { _mcMenuOkRow };
static UiList            Mc_OkList           = { Mc_OkCallbacks, 1, 1, 0, 0x0F };
static UiListRowCallback Mc_YesCallbacks[]   = { _mcMenuCancelRow };
static UiList            Mc_YesList          = { Mc_YesCallbacks, 1, 1, 0, 0x0F };
UiObjectDesc             Mc_PromptDesc[]     = {
    { 0, { 0, 0, 0x4B, 0x20 }, 0x10, 0, TASK_BODY_NONE, 0xC0, _mcMenuUpdatePromptChoices, 0 },
};

void taskNoopBank0Slot12(Task* unusedTask)
{
    // Preserve the otherwise unused sixteen-byte stack frame.
    char unusedStackFrame[0x10];
}

/// Updates a list, then eases and draws an additional cursor at its content center Y.
///
/// Uses `uiUpdateList`'s live list, callback and embedded `UiObject.panel`
/// contract. With active control, the additional cursor target is two pixels
/// inside the content's left edge at content-relative Y zero. Both cursor calls
/// use the shared retained position and require `uiEaseAndDrawCursor`'s drawing
/// resources and signed coordinate bounds.
static void _uiUpdateListWithCenteredCursor(UiList* list, UiPanel* panel)
{
    uiUpdateList(list, panel);
    if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        uiEaseAndDrawCursor(panel, panel->contentLeft.signedValue + 2, 0);
    }
}

/// Updates an object's list and eases the shared cursor toward content-center Y.
///
/// Borrows a live `listObject` and mutable list under `uiUpdateList`'s callback,
/// index, row-height and drawing contract. Row callbacks receive this object
/// through its embedded panel; neither pointer is retained by this helper.
/// After row updates, exactly active control requests an additional cursor
/// update two pixels inside the content's left edge at content-relative Y zero,
/// including for an empty list. Uses `uiEaseAndDrawCursor`'s shared position,
/// signed coordinate bounds and drawing resources.
static inline void _uiUpdateListAndCenterCursor(UiList* list, UiObject* listObject)
{
    UiPanel* panel;

    panel = &listObject->panel;
    uiUpdateList(list, panel);
    if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        uiEaseAndDrawCursor(panel, panel->contentLeft.signedValue + 2, 0);
    }
}

void mcMenuUpdateLoadFileList(Task* owningTask)
{
    enum {
        MEMORY_CARD_MENU_LOAD_LIST_INITIAL    = 0,
        MEMORY_CARD_MENU_LOAD_LIST_READY      = 1,
        MEMORY_CARD_MENU_LOAD_LIST_FIRST_FILE = 0
    };
    UiObject* listObject;
    UiList*   fileList;

    listObject = owningTask->spawnArg2.pointer;
    fileList   = &Mc_SaveSlotList;
    uiDrawPanelLabel(&listObject->panel, McText_Select);
    if (owningTask->state == MEMORY_CARD_MENU_LOAD_LIST_INITIAL) {
        uiInitList(fileList, &listObject->panel);
        // Start each listing at the first file, overriding the saved cursor mode.
        fileList->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        fileList->selectedItemIndex                   = MEMORY_CARD_MENU_LOAD_LIST_FIRST_FILE;
        fileList->firstVisibleItemIndex.unsignedValue = MEMORY_CARD_MENU_LOAD_LIST_FIRST_FILE;
        uiSetListSystemCursorSound(fileList, true);
        owningTask->state += MEMORY_CARD_MENU_LOAD_LIST_READY - MEMORY_CARD_MENU_LOAD_LIST_INITIAL;
    } else {
        _uiUpdateListAndCenterCursor(fileList, listObject);
    }
}

/// Requests a selection sound and publishes a save destination or cancellation.
///
/// Borrows a writable live object and the list dispatched for its save dialog.
/// True `acceptCurrentRow` reads `fileList->currentItemIndex` after the sound
/// request and sign-extends that byte into `resultValue`. Valid rows are 0..14:
/// indices below the directory count replace a file; the row at that count
/// creates New Block when space remains. False ignores the list and publishes
/// -1. Both outcomes publish `USER_INTERFACE_RESULT_CONFIRM`; the parent uses
/// the signed value to distinguish acceptance from cancellation and closes
/// the child. No pointer is retained.
///
/// `soundScriptId` uses `sndEvtRequestScriptStart`'s packed request-id format.
/// Zero pan offset and attenuation retain the sound's base pan and gain. The
/// selection is published even if sound admission or eventual playback fails.
static inline void _mcMenuPublishFileSelection(UiObject* object, const UiList* fileList, bool acceptCurrentRow, s32 soundScriptId)
{
    enum { MEMORY_CARD_MENU_FILE_SELECTION_CANCELLED = -1 };
    s16 selectionResult;

    sndEvtRequestScriptStart(soundScriptId, 0, 0);
    object->result = USER_INTERFACE_RESULT_CONFIRM;
    if (acceptCurrentRow) {
        selectionResult = (s8)(u8)fileList->currentItemIndex;
    } else {
        selectionResult = MEMORY_CARD_MENU_FILE_SELECTION_CANCELLED;
    }
    object->resultValue = selectionResult;
}

void mcMenuDrawSaveFileRow(UiList* list, UiObject* object)
{
    const McWork* work;
    s8            directoryIndex;

    directoryIndex = list->currentItemIndex;
    work           = object->owner->spawnArg1.pointer;
    mcDrawFilePreview(object, work, directoryIndex, 0, list->rowTextY.signedValue + 7);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        // Saving may replace a corrupt file; acceptance does not require a valid preview.
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            _mcMenuPublishFileSelection(object, list, true, SOUND_SYSTEM_CONFIRM);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            _mcMenuPublishFileSelection(object, list, false, SOUND_SYSTEM_CANCEL);
        }
    }
}

void mcMenuUpdateSaveFileList(Task* owningTask)
{
    enum {
        MEMORY_CARD_MENU_SAVE_LIST_INITIAL    = 0,
        MEMORY_CARD_MENU_SAVE_LIST_READY      = 1,
        MEMORY_CARD_MENU_SAVE_LIST_FIRST_FILE = 0
    };
    UiObject*     listObject;
    UiList*       fileList;
    const McWork* dialogWork;
    s32           firstVisibleRow;

    listObject = owningTask->spawnArg2.pointer;
    dialogWork = owningTask->spawnArg1.pointer;
    fileList   = &Mc_LoadSlotList;
    uiDrawPanelLabel(&listObject->panel, McText_Select);
    if (owningTask->state == MEMORY_CARD_MENU_SAVE_LIST_INITIAL) {
        uiInitList(fileList, &listObject->panel);
        // Restore the remembered destination at the viewport's bottom, or start at zero.
        fileList->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        fileList->selectedItemIndex                   = dialogWork->selectedSlot;
        firstVisibleRow                               = (u8)fileList->selectedItemIndex - fileList->visibleRowCount.unsignedValue + 1;
        fileList->firstVisibleItemIndex.unsignedValue = firstVisibleRow;
        if ((s8)firstVisibleRow < 0) {
            fileList->firstVisibleItemIndex.unsignedValue = MEMORY_CARD_MENU_SAVE_LIST_FIRST_FILE;
        }
        uiSetListSystemCursorSound(fileList, true);
        owningTask->state += MEMORY_CARD_MENU_SAVE_LIST_READY - MEMORY_CARD_MENU_SAVE_LIST_INITIAL;
    } else {
        _uiUpdateListAndCenterCursor(fileList, listObject);
    }
}

void mcMenuUpdateFileInformation(Task* owningTask)
{
    UiObject* informationObject;
    McWork*   dialogWork;
    UiList*   fileList;
    s32       directoryIndex;

    informationObject = owningTask->spawnArg2.pointer;
    if (owningTask->state == MEMORY_CARD_MENU_FILE_INFORMATION_INITIAL) {
        // Preserve the list selector before replacing its payload with borrowed dialog work.
        MEMORY_CARD_MENU_INIT_FILE_INFORMATION(owningTask, dialogWork);
    }
    dialogWork = owningTask->spawnArg1.pointer;
    uiDrawTitle(&informationObject->panel, "File Information");
    if (owningTask->killCountdown == MEMORY_CARD_MENU_FILE_INFORMATION_SAVE_LIST) {
        fileList = &Mc_LoadSlotList;
    } else {
        fileList = &Mc_SaveSlotList;
    }
    directoryIndex = fileList->selectedItemIndex;
    mcDrawFilePreview(informationObject, dialogWork, directoryIndex, 0, 0);
}

#undef MEMORY_CARD_MENU_INIT_FILE_INFORMATION

/// Requests a memory-card prompt's selection sound and publishes its answer.
///
/// Borrows a live prompt object. `answer` is `MEMORY_CARD_MENU_ANSWER_SELECTED`
/// (1) for Yes, OK or the single Cancel action, or `MEMORY_CARD_MENU_ANSWER_NO`
/// (-1) for No. Both answers publish `USER_INTERFACE_RESULT_CONFIRM`; the
/// parent reads the signed answer and starts closing the child.
///
/// `soundScriptId` uses `sndEvtRequestScriptStart`'s packed request-id format.
/// Zero pan offset and attenuation preserve the sound's base pan and gain.
/// The answer is published even if the sound request fails.
static inline void _mcMenuPublishAnswer(UiObject* object, s16 answer, s32 soundScriptId)
{
    sndEvtRequestScriptStart(soundScriptId, 0, 0);
    object->result      = USER_INTERFACE_RESULT_CONFIRM;
    object->resultValue = answer;
}

/// Draws Yes and handles its selected row in the memory-card Yes/No prompt.
///
/// Uses the borrowed `UiListRowCallback` row coordinates, color and lifetime
/// contract. Active-row Confirm publishes answer 1 with the confirm sound;
/// Cancel plays the cursor sound and requests a step to the following No row.
/// Controller port zero is queried for newly pressed buttons; Confirm wins
/// when both buttons are pressed. The parent closes the prompt after an answer.
static void _mcMenuYesRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_Yes, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_SELECTED, SOUND_SYSTEM_CONFIRM);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            // Cancel moves focus to No; a later input on that row publishes the answer.
            sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
            list->navigationStep = USER_INTERFACE_LIST_STEP_NEXT;
            list->actionResult   = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
        }
    }
}

/// Draws OK and acknowledges the selected memory-card notice with answer 1.
///
/// Uses `UiListRowCallback`'s borrowed row coordinates, color and lifetime.
/// Only active-row Confirm on controller port zero publishes a confirm outcome
/// and plays the confirm sound. Cancel is ignored; the parent closes the notice.
static void _mcMenuOkRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_Ok, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_SELECTED, SOUND_SYSTEM_CONFIRM);
        }
    }
}

/// Draws the single Cancel action in a memory-card prompt and selects it with answer 1.
///
/// Uses `UiListRowCallback`'s borrowed row coordinates, color and lifetime.
/// Only active-row Confirm on controller port zero publishes a confirm outcome
/// and plays the cancel sound. The Cancel button is ignored; the parent handles
/// the selected action and closes the prompt.
static void _mcMenuCancelRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_Cancel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_SELECTED, SOUND_SYSTEM_CANCEL);
        }
    }
}

/// Draws No and rejects the selected memory-card Yes/No prompt with answer -1.
///
/// Uses `UiListRowCallback`'s borrowed row coordinates, color and lifetime.
/// Active-row Confirm or Cancel on controller port zero plays the cancel sound
/// and publishes a confirm outcome carrying -1. The parent closes the prompt.
static void _mcMenuNoRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_No, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_NO, SOUND_SYSTEM_CANCEL);
        }
    }
}

/// Fits a memory-card prompt's choice panel and sets its initial answer row.
///
/// Call once after laying out the live `promptPanel`. Borrows its task read-only;
/// `spawnArg1.value` retains the `MEMORY_CARD_MENU_PROMPT_*` layout. `choiceList`
/// must be the corresponding shared list, with positive pixel rowHeight and no
/// concurrent prompt using it: two rows (Yes, No), or one row (OK or Cancel).
/// Only `MEMORY_CARD_MENU_PROMPT_YES_NO_INITIAL_NO` selects No; every other mode
/// selects the first row. The viewport starts at row zero, overriding saved
/// cursor mode.
///
/// Fitting resets scrolling, flags and row input. The outer Y then moves up by
/// half the fitted height in signed screen-centered pixels, truncating toward
/// zero and narrowing to a halfword. The UI lifecycle refreshes content layout
/// on its next update. Enables the system cursor sound without drawing rows.
static inline void _mcMenuInitPromptChoices(const Task* owningTask, UiPanel* promptPanel, UiList* choiceList)
{
    enum {
        MEMORY_CARD_MENU_PROMPT_FIRST_ROW = 0,
        MEMORY_CARD_MENU_PROMPT_NO_ROW    = 1
    };

    uiFitPanelToList(choiceList, promptPanel);
    promptPanel->bounds.rect.y -= promptPanel->bounds.rect.h / 2;

    // Seed the answer after fitting so saved cursor mode cannot override it.
    if (owningTask->spawnArg1.value != MEMORY_CARD_MENU_PROMPT_YES_NO_INITIAL_NO) {
        choiceList->selectedItemIndex = MEMORY_CARD_MENU_PROMPT_FIRST_ROW;
    } else {
        choiceList->selectedItemIndex = MEMORY_CARD_MENU_PROMPT_NO_ROW;
    }
    choiceList->firstVisibleItemIndex.unsignedValue = MEMORY_CARD_MENU_PROMPT_FIRST_ROW;
    uiSetListSystemCursorSound(choiceList, true);
}

/// Initializes and updates the choices of a memory-card prompt.
///
/// `owningTask` owns the live `UiObject` in its second spawn argument. Its first
/// argument must retain its `MEMORY_CARD_MENU_PROMPT_*` layout: OK, Cancel or Yes/No,
/// with mode 3 initially selecting No. Other values initially select Yes.
/// State zero fits the rows and centers the panel vertically in signed pixels;
/// later calls draw and handle controller-port-zero input through `uiUpdateList`.
///
/// The three lists are shared mutable state: prompts using the same layout must
/// run sequentially. The UI lifecycle supplies panel layout and drawing resources
/// and suspends input during animation. Row callbacks publish the answer on the
/// object; the parent closes it. This callback retains no additional allocation.
static void _mcMenuUpdatePromptChoices(Task* owningTask)
{
    enum {
        MEMORY_CARD_MENU_PROMPT_INITIAL = 0,
        MEMORY_CARD_MENU_PROMPT_READY   = 1
    };
    UiObject* promptObject;
    UiList*   choiceList;
    s32       promptMode;

    promptMode   = owningTask->spawnArg1.value;
    promptObject = owningTask->spawnArg2.pointer;
    if (promptMode == MEMORY_CARD_MENU_PROMPT_CANCEL) {
        goto cancelChoices;
    }
    if (promptMode >= MEMORY_CARD_MENU_PROMPT_YES_NO_INITIAL_NO) {
        goto yesNoChoices;
    }
    if (promptMode != MEMORY_CARD_MENU_PROMPT_OK) {
        goto yesNoChoices;
    }
    choiceList = &Mc_OkList;
    goto choicesSelected;
cancelChoices:
    choiceList = &Mc_YesList;
    goto choicesSelected;
yesNoChoices:
    choiceList = &Mc_YesNoList;
choicesSelected:
    if (owningTask->state == MEMORY_CARD_MENU_PROMPT_INITIAL) {
        // Override the saved cursor preference only after fitting the viewport.
        _mcMenuInitPromptChoices(owningTask, &promptObject->panel, choiceList);
        owningTask->state += MEMORY_CARD_MENU_PROMPT_READY - MEMORY_CARD_MENU_PROMPT_INITIAL;
    } else {
        uiUpdateList(choiceList, &promptObject->panel);
    }
}
