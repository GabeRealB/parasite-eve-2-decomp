#include "common.h"

#include "main/fs.h"

typedef struct {
    u8  pad_00[0x8C];
    s16 field_8C;
    s16 field_8E;
} DumpingHoleEntity;

typedef struct {
    u8                 pad_00[0x1C];
    DumpingHoleEntity* field_1C;
} DumpingHoleState;

extern DumpingHoleState* D_shelter_b3_dumping_hole_8018F4AC;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_4", func_shelter_b3_dumping_hole_801818E0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_4", func_shelter_b3_dumping_hole_80181958);

void func_shelter_b3_dumping_hole_80181990(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4AC->field_1C;
    p->field_8C          = arg0;
    p->field_8E          = 0;
}

void func_shelter_b3_dumping_hole_801819B0(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_shelter_b3_dumping_hole_801819D0(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_shelter_b3_dumping_hole_801819F0(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}