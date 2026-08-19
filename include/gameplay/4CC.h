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

/// 0x1C work block allocated by `func_800BCC44` (`Mem_Calloc(0x1C, 0)`)
/// and stored at `Task::idMap` / `D_80114D78`. `field_0` / `field_4` are
/// the first two `Ui_SpawnFromDesc` results; `field_8` is a write offset
/// into this same block (`func_800BC634`).
typedef struct _GpItemMoveState {
    /* 0x00 */ UiObject* field_0;
    /* 0x04 */ UiObject* field_4;
    /* 0x08 */ s32       field_8;
    /* 0x0C */ s32       field_C;
    /* 0x10 */ s32       field_10;
    /* 0x14 */ s32       field_14;
    /* 0x18 */ s32       field_18;
} GpItemMoveState;
STATIC_ASSERT_SIZEOF(GpItemMoveState, 0x1C);

extern GpItemDesc D_8010D838[];
/// Second item-descriptor base. Indexed as `D_8010D638[id]` for `id >= 0x100`.
extern GpItemDesc D_8010D638[];
/// Packed item/location halfword. High byte selects a UI path in
/// `func_800CDFA8` (0/1, 8, or default); low byte is written to
/// `Mc_SaveData.field_12` on the case-8 path. Copied into `D_80114D7C`
/// by `func_800BCC44`.
extern u16 D_80114DDC;
/// Extra halfword copied from `GpBit2Rec.field_6` by `func_800B63B8`
/// (always written on a match; delay slot of the item-id `< 0x100` check).
extern u16 D_80114DDE;
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
/// Item-move UI work block published by `func_800BCC44`.
extern GpItemMoveState* D_80114D78;
/// Item-move `UiObjectDesc` table. `func_800BCC44` spawns `[0]` / `[1]`
/// and, when `spawnArg1 == 1`, `[9]`.
extern UiObjectDesc D_8010D6F4[];
/// Extra `UiObjectDesc` spawned after the `D_8010D6F4` pair.
extern UiObjectDesc D_8010D80C;
/// Pair of inventory UiLists indexed by `Task::spawnArg1` (source / dest).
/// `field_10` is the selected row passed to `func_800BB5BC`.
extern UiList D_8010D634[];
/// Action-button callbacks for `D_8010D68C`, filled by `func_800BDC80`.
extern UiListItemFunc D_8010D67C[];
/// UiList used by `func_800BDDC4`.
extern UiList D_8010D68C;
/// UiList used by `func_800BF464`. `field_10` is 1 when `spawnArg1` is 0.
extern UiList D_8010D6B4;
extern char          D_80093D70[]; // "Battle Field"
extern char          D_80093D80[]; // "Item Box"
extern char          D_80093D8C[]; // "Player Item"
extern GpPromptTexts D_80093DA0;
/// Per-child item-move handler. Walked by `func_800BCC44` over
/// `obj->owner`'s children as `func_800BC634(child->spawnArg2, child)`.
void func_800BC634(UiObject* arg0, Task* arg1);
/// Task callback for the item-move UI. `spawnArg2` is the `UiObject`.
/// First run copies `D_8010D550[D_80114DDC]` / `Mc_SaveData.field_5BC`
/// into `D_8010D628` / `D_8010D62C`, spawns the `D_8010D6F4` pair
/// (plus `[9]` when `spawnArg1 == 1`), then walks children through
/// `func_800BC634`. Always writes `field_2C = 0x34`.
void func_800BCC44(Task* arg0);
/// Task callback for one `D_8010D634` inventory pane. `spawnArg1 >= 0x100`
/// is masked to the low byte and `flags` is set so the title is
/// `D_80093D70` ("Battle Field") instead of `D_80093D80` ("Item Box");
/// dest (`spawnArg1 != 0`) uses `D_80093D8C` ("Player Item"). Seeds the
/// list from `D_8010D628[spawnArg1].field_1` (visible rows capped at 10).
/// First-state confirm/cancel is `field_2E = -1`; later states write
/// `0x24`. Circle (src) / Square (dest) / mask 3 switch panes (`0xA`)
/// and play type-6 sound 2. Walks children through `func_800BF398`.
void func_800BD2FC(Task* arg0);
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
/// Child closer for the `D_8010D634` inventory panes. `-1` tears down and
/// either restores parent status or sets parent `field_2E = -1` when the
/// parent owner has no flags; `6` / `0x23` / `38` / `39` copy those codes
/// onto the parent (`6` also restores status).
void func_800BF398(UiObject* arg0, Task* arg1);
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
