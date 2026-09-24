#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b6_growth_room.h"

extern TaskDesc D_80135E78;
extern s32      D_80070F70;
extern u32      Gp_LcgState;
extern SVECTOR  D_shelter_b6_growth_room_8017F258[];
extern SVECTOR  D_shelter_b6_growth_room_8017F298[];
extern SVECTOR  D_shelter_b6_growth_room_8017F2C8[];
extern SVECTOR  D_shelter_b6_growth_room_8017F300[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_2", func_shelter_b6_growth_room_8017D82C);

void func_shelter_b6_growth_room_8017D9D8(Task* task)
{
    SVECTOR pos;
    s32     angle;
    s32     i;
    s32     z;

    if (Gp_State1C->eventState == 0) {
        if (task->spawnArg1 < 0x130 && !(D_80070F70 & 7)) {
            task->spawnArg1++;
        }
    }
    if (task->state < 6) {
        task->state = (task->spawnArg1 >> 4) + 1;
    }
    if ((u32)D_80070F70 % (task->state * 2 + 4) == 0) {
        for (i = 0; i < task->state; i++) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            angle       = ((Gp_LcgState >> 16) & 0x7FF) - 0x400;
            pos.vx      = D_shelter_b6_growth_room_8017F258[i + 30].vx + ((rcos(angle) * 1000) >> 12);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            pos.vy      = -((s32)(Gp_LcgState >> 16) % ((task->spawnArg1 + 1) * 8));
            pos.vz      = D_shelter_b6_growth_room_8017F258[i + 30].vz + ((rsin(angle) * 1000) >> 12);
            Gp_SpawnEff(0x601A1, NULL, 0x106500, &pos);
        }
    }
    if (!(D_80070F70 & 1)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vx      = -1000;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = -1200 - (Gp_LcgState >> 16) % 400;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        z           = (Gp_LcgState >> 16) % 400 + 0xDAC;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = z + ((Gp_LcgState >> 16) & 1) * 1000;
        Gp_SpawnEff(0x601A2, NULL, 0x183280, &pos);
    }
    func_shelter_b6_growth_room_8017E448(task->spawnArg1, (task->spawnArg1 >> 1) + 0x50);
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[0], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[1], 0x200, 0x400);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[6], 0x180, 0x400);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[7], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[8], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[9], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[10], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[11], 0x180, 0x440);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[12], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[13], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[14], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[15], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[20], 0x200, 0x444);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[21], 0x200, 0x444);
            break;
        case 3:
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[0], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[1], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[2], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[3], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[4], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[5], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[6], 0x200, 0x400);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[7], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[12], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[13], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[24], 0x200, 0x444);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[25], 0x200, 0x444);
            break;
        case 4:
            Room_Draw13(&D_shelter_b6_growth_room_8017F300[0], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F300[1], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F300[2], 0x100, 0x44);
            break;
        case 5:
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[0], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[1], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[2], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[3], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[4], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[5], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[6], 0x200, 0x400);
            Room_Draw13(&D_shelter_b6_growth_room_8017F258[7], 0x200, 0x44);
            break;
        case 6:
            Room_Draw13(&D_shelter_b6_growth_room_8017F2C8[0], 0x180, 0x400);
            Room_Draw13(&D_shelter_b6_growth_room_8017F2C8[1], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F2C8[2], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F2C8[7], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F2C8[8], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F2C8[9], 0x100, 0x44);
            break;
        case 7:
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[0], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[1], 0x200, 0x400);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[2], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[3], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[9], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[10], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[11], 0x180, 0x440);
            Room_Draw13(&D_shelter_b6_growth_room_8017F298[12], 0x200, 0x44);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_2", D_shelter_b6_growth_room_8017D5E8);
