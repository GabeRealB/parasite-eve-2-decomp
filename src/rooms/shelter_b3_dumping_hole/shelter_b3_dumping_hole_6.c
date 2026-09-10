#include "common.h"

typedef struct {
    u8  _pad0[0x4];
    s16 field_4;
} DumpingHoleEntity;

typedef struct {
    u8                 _pad0[0x1C];
    DumpingHoleEntity* field_1C;
} DumpingHoleState;

typedef struct {
    u8  _pad0[0x2];
    u16 field_2;
} DumpingHoleMsg;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183144);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183198);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183218);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183298);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_801833EC);

void func_shelter_b3_dumping_hole_80183530(DumpingHoleState* arg0, s32 arg1, DumpingHoleMsg* arg2)
{
    DumpingHoleEntity* ent = arg0->field_1C;
    if (arg2->field_2 == 4) {
        ent->field_4 = arg2->field_2;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183550);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_801835C8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183620);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183678);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_801836E0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_8018378C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183824);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_801838A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183950);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183A00);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183A98);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183AEC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183B9C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183C38);
