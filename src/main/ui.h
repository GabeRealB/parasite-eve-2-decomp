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

void McMenu_ConfirmWithRender(UiList* list, UiObject* object);

void McMenu_SelectList(Task* task);

void McMenu_SelectListAlt(Task* task);

void McMenu_FileInformation(Task* task);

#endif // MAIN_PRIVATE_UI_H
