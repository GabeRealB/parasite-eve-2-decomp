#ifndef GAMEPLAY_ITEM_MENU_H
#define GAMEPLAY_ITEM_MENU_H

#include "types.h"

#include "gameplay/action_prompt.h"

#include "main/task_types.h"
#include "main/ui_types.h"

// Inventory menu tasks, item panels, prompts and their shared descriptors.

/// Item-move `UiObjectDesc` table. `Gp_ItemMoveTask` spawns `[0]` / `[1]`
/// and, when `spawnArg1 == 1`, `[9]`.
extern UiObjectDesc D_8010D6F4[];

/// Extra `UiObjectDesc` spawned after the `D_8010D6F4` pair.
#define D_8010D80C D_8010D6F4[10]

/// Named as a task entry by the enemy descriptor tables in the map UI overlays.
void Gp_ItemPickupTilt(Task* arg0);

void func_800B65B0(Task* task);

void Gp_DrawPromptLines(UiObject* arg0, Task* arg1);

void func_800CE5D0(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_800CE22C(Task* arg0);

void Gp_DrawItemLabel(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

void Gp_DrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_SetPreviewItem(s32 arg0, s32 arg1);

void Gp_ClearPreviewItems(void);

UiObject* func_800CD89C(UiObject* arg0);

void func_800C5F70(Task* arg0);

void func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);

extern ActionPrompt D_80114D28[2];

void Gp_SetHolderItemText(s32 arg0);

UiObject* Gp_SpawnItemPrompt(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_800D4D2C(s32 arg0);

s32 func_800D4EC0(void);

s32 func_800D4E78(s32 arg0, s32 arg1, s32 arg2);

s32 Gp_GetPreviewItem(void);

extern char Gp_StrEmpty[];

void func_800C0E20(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, u32 arg6);

/// Command-indexed menu descriptors. Zero rows reserve unused command IDs.
extern UiObjectDesc D_8010EAB4[50];

void Gp_MenuRootTask(Task* arg0);

#define D_8010EFA0 D_8010EAB4[45]

#endif // GAMEPLAY_ITEM_MENU_H
