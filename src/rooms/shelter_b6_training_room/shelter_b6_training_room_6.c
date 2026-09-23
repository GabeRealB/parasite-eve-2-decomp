#include "common.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern SVECTOR D_shelter_b6_training_room_80184334[];
extern u8      D_shelter_b6_training_room_80185C60[][16];
extern u16     D_shelter_b6_training_room_80185C98;
extern u32     Gp_LcgState;

void func_shelter_b6_training_room_8017DDE8(Task* task)
{
    s32 i;
    s32 j;

    if (task->state == 0) {
        D_shelter_b6_training_room_80185C98 = 0;
        for (j = 0; j < 3; j++) {
            for (i = 0; i < 6; i++) {
                D_shelter_b6_training_room_80185C60[task->spawnArg1][i] = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            }
        }
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            Room_Draw01(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            break;
        case 3:
            Room_Draw01(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            Room_Draw01(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            Room_Draw13(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[15], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[16], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[21], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[22], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            Room_Draw13(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 4:
            Room_Draw01(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            Room_Draw13(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            Room_Draw13(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 5:
            Room_Draw01(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            Room_Draw13(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            Room_Draw13(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 6:
            Room_Draw13(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            Room_Draw13(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 7:
            Room_Draw01(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            Room_Draw13(&D_shelter_b6_training_room_80184334[17], 0x280, 0x444);
            Room_Draw13(&D_shelter_b6_training_room_80184334[23], 0x280, 0x444);
            break;
        case 8:
            Room_Draw13(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            Room_Draw13(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            Room_Draw13(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
    }
}

/// Closes this unit's `.rodata` after the 7-entry jump table above, so the next
/// unit's rodata starts 8-byte aligned. Nothing reads it.
const u32 D_shelter_b6_training_room_8017D608 = 0;
