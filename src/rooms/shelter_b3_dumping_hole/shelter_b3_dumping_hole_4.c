#include "common.h"

#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/task.h"

typedef struct {
    u8    pad_00[0x80];
    Task* field_80;
    u8    pad_84[0x8];
    s16   field_8C;
    s16   field_8E;
} DumpingHoleEntity;

typedef struct {
    u8                 pad_00[0x1C];
    DumpingHoleEntity* field_1C;
} DumpingHoleState;

extern DumpingHoleState* D_shelter_b3_dumping_hole_8018F4AC;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_4", func_shelter_b3_dumping_hole_801818E0);

void func_shelter_b3_dumping_hole_80181958(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4AC->field_1C;
    Gp_DispatchMsg(p->field_80, 0x3F3, arg0, 0);
}

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