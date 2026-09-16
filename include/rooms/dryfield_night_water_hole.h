#ifndef ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H
#define ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Parameter block of `func_dryfield_night_water_hole_8017D6AC`, the room-local
/// resolver `func_dryfield_night_water_hole_8017DC28` calls with one pointer as
/// both its input and its output.
///
/// `field_0` is the code the resolver switches on: its jump table spans 2..0x2D
/// and anything outside that range falls through untouched. `field_2` passes
/// through unchanged, `field_3` is the byte the resolver writes, and `field_5`
/// is a busy flag - non-zero makes the resolver return immediately without
/// reading or writing anything else. The caller stages the block from the
/// `DnwhEventDesc` it is about to publish and copies `field_3` back into it.
typedef struct DnwhUtilParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
} DnwhUtilParam;
STATIC_ASSERT_SIZEOF(DnwhUtilParam, 0x6);

/// The twelve bytes `func_dryfield_night_water_hole_8017DC28` stages in
/// `D_dryfield_night_water_hole_80183630` before spawning the room's event task
/// from `D_dryfield_night_water_hole_801805EC`.
///
/// The task that consumes it is still assembly
/// (`func_dryfield_night_water_hole_8017D7E8`): it forwards `field_4` as message
/// 0x3EE to the slot-3 game pointer - the halfword is staged on its own frame,
/// and 0xFFFF makes it abort the event instead - while `field_8` is the
/// sound-event id it plays through `SndEvt_EnqueueType6` and then polls with
/// `SndVoice_HasActiveId`. Once that id goes quiet it copies the four bytes at
/// 0x0 into `Mc_SaveData`'s 0x5..0x8, the save-location block, and re-spawns to
/// task type 0x11.
///
/// `field_6` is never read or written by either side, so it is padding.
typedef struct DnwhEventDesc {
    /* 0x0 */ u8   field_0;
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ u8   field_3;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  field_8;
} DnwhEventDesc;
STATIC_ASSERT_SIZEOF(DnwhEventDesc, 0xC);

/// Descriptor the room's event task is spawned from, index 0 of the table
/// `func_dryfield_night_water_hole_8017DC28` hands `Task_SpawnFromTable`. Its
/// callback is that same task, `func_dryfield_night_water_hole_8017D7E8`.
extern TaskDesc D_dryfield_night_water_hole_801805EC;

/// The staged event descriptor, read by the task spawned above.
extern DnwhEventDesc D_dryfield_night_water_hole_80183630;

/// One entry of the NULL-terminated override list
/// `func_dryfield_night_water_hole_8017DE88` walks: the record the entry
/// installs in the room's parameter table, `Gp_RoomParamTables[stage][room]`,
/// and the slot it goes in.
typedef struct DnwhParamOverride {
    /* 0x0 */ GpRoomParamRec* rec;
    /* 0x4 */ s32             index;
} DnwhParamOverride;
STATIC_ASSERT_SIZEOF(DnwhParamOverride, 0x8);

/// Replaces the room's parameter slots with the overrides in `list`, both the
/// `GpRoomParamRec` pointer and the byte `Gp_LoadRoomParams` would have copied
/// out of it. `func_dryfield_night_water_hole_8017D958` applies it on the visit
/// that has already latched progress nibble 0xB8.
void func_dryfield_night_water_hole_8017DE88(DnwhParamOverride* list);

/// Room entry task tick: publishes the message table and claims game pointer
/// slot 7, then takes whichever opening move progress nibble 0xB8 leaves -
/// the room's event task while the nibble is clear, the parameter overrides
/// above once it is set - announces the room to the slot-4 task with message
/// 0x7DB on the visit whose sub-id is 1, and spawns the ending task on sub-id
/// 0xA with nibble 0xCF still clear. Then advances state.
void func_dryfield_night_water_hole_8017D958(Task* arg0);

/// Resolves the code in `in->field_0` and writes the resulting byte to
/// `out->field_3`. `func_dryfield_night_water_hole_8017DC28` is its only caller
/// and passes one block as both sides.
void func_dryfield_night_water_hole_8017D6AC(DnwhUtilParam* in, DnwhUtilParam* out);

#endif // ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H
