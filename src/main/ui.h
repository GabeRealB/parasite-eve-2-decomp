#ifndef MAIN_PRIVATE_UI_H
#define MAIN_PRIVATE_UI_H

#include "main/task_types.h"
#include "main/ui_types.h"

extern UiList Mc_SaveSlotList;

extern UiList Mc_LoadSlotList;

extern UiObjectDesc Mc_TaskDescriptors[];

extern UiObjectDesc Mc_PromptDesc[];

void McMenu_ConfirmWithRender(UiList* list, UiObject* object);

void McMenu_SelectList(Task* task);

void McMenu_SelectListAlt(Task* task);

void McMenu_FileInformation(Task* task);

#endif // MAIN_PRIVATE_UI_H
