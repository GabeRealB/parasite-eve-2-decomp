#ifndef GAMEPLAY_4CC_H
#define GAMEPLAY_4CC_H

#include "common.h"

#include "main/mc.h"
#include "main/task.h"
#include "main/ui.h"

/// Object at `Task::spawnArg2` for `func_800BF5CC` / `func_800BF624` /
/// `func_800CE094` / `func_800BBC10`. `field_8` is the packed item id passed
/// to `func_800BB470` (and inlined by `func_800BBC10`).
/// `field_A` is the item/location halfword copied into `D_80114DDC` by
/// `func_800BF624` and cleared by `func_800CE094` on the cancel path.
typedef struct _GpItemObj8 {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u8   field_8;
    /* 0x09 */ byte pad_9;
    /* 0x0A */ u16  field_A;
} GpItemObj8;

/// Object stored in `Task::extraState` by `func_800BF5CC`. `field_2` is set
/// when `func_800BB470` returns 2.
typedef struct _GpItemObj2 {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ u8   field_2;
} GpItemObj2;

/// 8-byte item descriptor in `D_8010D838` / `D_8010D638`, indexed by item id.
/// `func_800BBF84` / `func_800B8EB0` use `D_8010D838[id]` when `id < 0x100`
/// and `D_8010D638[id]` otherwise (same raw index, different base).
/// `field_3` bit 0 gates the `arg1 == 1` result in `func_800BF334`.
/// `field_4` is a name/string pointer walked by `func_800B8EB0` /
/// `func_800BBF84` (fields separated by NUL or `'\n'`).
typedef struct _GpItemDesc {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ u8    field_1;
    /* 0x02 */ u8    field_2;
    /* 0x03 */ u8    field_3;
    /* 0x04 */ void* field_4;
} GpItemDesc;
STATIC_ASSERT_SIZEOF(GpItemDesc, 0x8);

/// 4 prompt strings copied onto the stack by `func_800BE808` and indexed
/// by `DialogPrompt::field_8`: All / Select / Discard / End
/// (`D_8010D610` / `D_8010D614` / `D_8010D620` / `D_8010D61C`).
typedef struct {
    u8* texts[4];
} GpPromptTexts;
STATIC_ASSERT_SIZEOF(GpPromptTexts, 0x10);

extern GpItemDesc D_8010D838[];
/// Second item-descriptor base. Indexed as `D_8010D638[id]` for `id >= 0x100`.
extern GpItemDesc D_8010D638[];
/// Packed item/location halfword. High byte selects a UI path in
/// `func_800CDFA8` (0/1, 8, or default); low byte is written to
/// `Mc_SaveData.field_12` on the case-8 path. Copied into `D_80114D7C`
/// by `func_800BCC44`.
extern u16 D_80114DDC;
/// Set to 1 by `func_800BF624` after it publishes `D_80114DDC` /
/// `D_80114DD0`.
extern u16 D_80114DC8;
/// Stack/count halfword published by `func_800BF624`: 1 for item ids
/// below 0xA0, else `D_8010E3B8[id - 0xA0].field_0`.
extern u16 D_80114DD0;
/// Packed item id copied from `GpItemObj8.field_8` by `func_800BF624`.
extern u16 D_80114DEC;
/// Item/location halfword copied from `D_80114DDC` by `func_800BCC44`.
/// `func_800BF334` special-cases the value `0x703`.
extern u16 D_80114D7C;
/// Pair of inventory UiLists indexed by `Task::spawnArg1` (source / dest).
/// `field_10` is the selected row passed to `func_800BB5BC`.
extern UiList D_8010D634[];
/// Action-button callbacks for `D_8010D68C`, filled by `func_800BDC80`.
extern UiListItemFunc D_8010D67C[];
/// UiList used by `func_800BDDC4`.
extern UiList D_8010D68C;
/// UiList used by `func_800BF464`. `field_10` is 1 when `spawnArg1` is 0.
extern UiList D_8010D6B4;
extern GpPromptTexts D_80093DA0;
void func_800BD6DC(DialogPrompt* arg0, UiObject* arg1);
/// List-item confirm for `D_8010D67C`. Draws `D_8010D588`, then on confirm
/// looks up the selected inventory row and inlines `func_800BF334` against
/// `owner->parent->flags`. A true result opens prompt `0x1E`; dest inventory
/// (`spawnArg1 == 1`) plus an equipped weapon/armor (`field_21+0x7F` /
/// `field_23+0x5F`) opens prompt `7`; otherwise `field_2E = 0x23`.
void func_800BDAA8(DialogPrompt* arg0, UiObject* arg1);
/// Fills `D_8010D67C` and `arg0->field_4` / `field_5` from the selected
/// inventory row (`D_8010D628[spawnArg1]` / `D_8010D634[spawnArg1].field_10`).
void func_800BDC80(UiList* arg0, UiObject* arg1);
/// Task callback for the `D_8010D68C` item list. On first run it copies
/// `parent->flags`, clamps `field_E + field_12` to 0x64, then calls
/// `func_800BDC80` and `Ui_LayoutListPanel`. Confirm (`D_8005ED78`) is
/// cancel (`field_2E = -1`) when `owner->flags` is 0, else 6; cancel
/// (`D_8005ED74`) is 6. Child `field_2E` -1 / 9 / 6 closes, remaps to
/// 6, or teardowns.
void func_800BDDC4(Task* arg0);
/// List-item callback for All / Select / Discard / End. Draws
/// `D_80093DA0[field_8]`. Confirm: All → `field_2E = 0x26`, Select → 6,
/// Discard strips 0x80–0x9F attachments missing from
/// `Mc_SaveData.field_5BC` and sets `field_2E = 0x27`. Cancel once sets
/// `field_10 = 2` / `field_22 = 0x21`; a second cancel does the discard
/// strip.
void func_800BE808(DialogPrompt* arg0, UiObject* arg1);
void func_800BF2C8(UiObject* arg0, void (*arg1)(UiObject*, Task*));
s32  func_800BF334(s32 arg0, s32 arg1);
void func_800BF464(Task* arg0);
/// Task callback. `spawnArg2` is the `UiObject`; on first run it is published
/// as `Wip_UiHolder`. `spawnArg1` is a text pointer; when non-zero, two prompt
/// lines are drawn at `field_18 + 0xF` / `+ 0x1E` in color `0x606060`.
void func_800BF4FC(Task* arg0);
s32  func_800BF5CC(Task* arg0, s32 arg1, GpItemObj2* arg2);
/// First state of the `D_80096E70` dispatcher. Copies `field_8` /
/// `field_A` into `D_80114DEC` / `D_80114DDC`, remaps owned 0x60–0x7F
/// items to 0xD and 0x80–0x9F items to 0x3D, then publishes a stack
/// count in `D_80114DD0`.
void func_800BF624(Task* arg0);

#endif // GAMEPLAY_4CC_H
