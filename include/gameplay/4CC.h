#ifndef GAMEPLAY_4CC_H
#define GAMEPLAY_4CC_H

#include "common.h"

#include "main/mc.h"
#include "main/task.h"
#include "main/ui.h"

/// Object at `Task::spawnArg2` for `func_800BF5CC` / `func_800BF624` /
/// `func_800CE094`. `field_8` is the packed item id passed to `func_800BB470`.
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

/// 8-byte item descriptor in `D_8010D838`, indexed by item id.
/// `field_3` bit 0 gates the `arg1 == 1` result in `func_800BF334`.
/// `field_4` is a name/string pointer walked by `func_800B8EB0`.
typedef struct _GpItemDesc {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ u8    field_1;
    /* 0x02 */ u8    field_2;
    /* 0x03 */ u8    field_3;
    /* 0x04 */ void* field_4;
} GpItemDesc;
STATIC_ASSERT_SIZEOF(GpItemDesc, 0x8);

extern GpItemDesc D_8010D838[];
/// Packed item/location halfword. High byte selects a UI path in
/// `func_800CDFA8` (0/1, 8, or default); low byte is written to
/// `Mc_SaveData.field_12` on the case-8 path. Copied into `D_80114D7C`
/// by `func_800BCC44`.
extern u16 D_80114DDC;
/// Item/location halfword copied from `D_80114DDC` by `func_800BCC44`.
/// `func_800BF334` special-cases the value `0x703`.
extern u16 D_80114D7C;
/// UiList used by `func_800BF464`. `field_10` is 1 when `spawnArg1` is 0.
extern UiList D_8010D6B4;
void func_800BF2C8(UiObject* arg0, void (*arg1)(UiObject*, Task*));
s32  func_800BF334(s32 arg0, s32 arg1);
void func_800BF464(Task* arg0);
/// Task callback. `spawnArg2` is the `UiObject`; on first run it is published
/// as `Wip_UiHolder`. `spawnArg1` is a text pointer; when non-zero, two prompt
/// lines are drawn at `field_18 + 0xF` / `+ 0x1E` in color `0x606060`.
void func_800BF4FC(Task* arg0);
s32  func_800BF5CC(Task* arg0, s32 arg1, GpItemObj2* arg2);

#endif // GAMEPLAY_4CC_H
