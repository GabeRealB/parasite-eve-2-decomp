#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

typedef struct {
    u8    pad_00[0x80];
    Task* field_80;
    u8    pad_84[0x8];
    s16   field_8C;
    s16   field_8E;
    u8    pad_90[0xC];
    u16   field_9C;
} DumpingHoleEntity;

typedef struct {
    u8                 pad_00[0x1C];
    DumpingHoleEntity* field_1C;
} DumpingHoleState;

extern DumpingHoleState* D_shelter_b3_dumping_hole_8018F4AC;
extern u16               D_801153F6;

void func_shelter_b3_dumping_hole_801818E0(void)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4AC->field_1C;
    if (p->field_9C == 0) {
        Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(0), 0x20);
        D_801153F6              = 0;
        Game_Session->field_69 |= 0x80;
        p->field_9C             = 1;
    }
}

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