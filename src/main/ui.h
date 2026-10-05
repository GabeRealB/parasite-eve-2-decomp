#ifndef MAIN_PRIVATE_UI_H
#define MAIN_PRIVATE_UI_H

#include "main/task_types.h"
#include "main/ui_types.h"

extern UiList Mc_SaveSlotList;

extern UiList Mc_LoadSlotList;

extern UiObjectDesc Mc_TaskDescriptors[];

/// Choice layouts passed as the first spawn argument with `Mc_PromptDesc`.
///
/// Other values use Yes/No with Yes initially selected. Mode 3 selects No
/// after viewport initialization, overriding the saved cursor preference.
enum {
    MEMORY_CARD_MENU_PROMPT_YES_NO            = 0,
    MEMORY_CARD_MENU_PROMPT_OK                = 1,
    MEMORY_CARD_MENU_PROMPT_CANCEL            = 2,
    MEMORY_CARD_MENU_PROMPT_YES_NO_INITIAL_NO = 3
};

extern UiObjectDesc Mc_PromptDesc[];

/// Draws a save destination and accepts the active row or cancellation.
///
/// Borrows the list and object under `UiListRowCallback`'s contract. The owner's
/// first spawn argument must borrow the live `McWork` for this save dialog.
/// Row indices 0..entryCount-1 (at most 0..14) draw cached files, including corrupt
/// ones; an extra row at entryCount draws New Block when a free block exists.
/// Uses panel-relative pixel origin (0, rowTextY + 7) for the preview.
/// Cached previews and drawing resources must satisfy `mcDrawFilePreview`'s contract.
///
/// Newly pressed Confirm on port zero accepts any active row, including a corrupt
/// file. It wins over Cancel. Both publish `USER_INTERFACE_RESULT_CONFIRM`, with
/// the signed-byte row index or -1 in resultValue, and request the corresponding
/// system sound. The parent interprets the answer and closes the child.
void mcMenuDrawSaveFileRow(UiList* list, UiObject* object);

/// Initializes and updates the memory-card load-file selection panel.
///
/// `owningTask` must own the live `UiObject` in its second spawn argument and
/// borrow the dialog's `McWork` in its first. The parent supplies the shared
/// list's cached-file count (0..15); only one such listing may be live at a time.
/// State zero initializes the viewport, selects file zero and enables the system
/// cursor sound. Later calls draw rows and handle port-zero input; active control
/// also eases and draws the shared cursor two pixels inside the content's left
/// edge at content-relative Y zero. The UI lifecycle supplies layout and drawing
/// resources. The Select label is drawn on every call; no allocation is retained.
void mcMenuUpdateLoadFileList(Task* owningTask);

void McMenu_SelectListAlt(Task* task);

void McMenu_FileInformation(Task* task);

#endif // MAIN_PRIVATE_UI_H
