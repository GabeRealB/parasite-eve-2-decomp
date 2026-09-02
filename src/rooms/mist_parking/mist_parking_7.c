#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_mist_parking_8018F374;
extern s32 D_mist_parking_8018F4AC;
extern s32 D_mist_parking_8018F5E4;
extern s32 D_mist_parking_8018F824;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_7", func_mist_parking_80182A44);

void func_mist_parking_80182F60(Task* task)
{
    s32 key;

    switch (task->state) {
        case 0:
            func_800E8614((s32)&D_mist_parking_8018F374, 1);
            task->state++;
            break;
        case 1:
        case 3:
            if (Game_Session->field_1 != 0) {
                return;
            }
            task->state++;
            break;
        case 2:
            key             = Gp_GetCapEventKey();
            task->spawnArg1 = key;
            switch (key) {
                case 4:
                    func_800E8614((s32)&D_mist_parking_8018F4AC, 1);
                    break;
                case 5:
                    func_800E8614((s32)&D_mist_parking_8018F5E4, 1);
                    break;
                case 6:
                    func_800E8614((s32)&D_mist_parking_8018F824, 1);
                    break;
            }
            task->state++;
            break;
        case 4:
            if (task->spawnArg1 == 4) {
                Gp_MsgPlayerWeapon(1);
            }
            Task_Kill(task);
            break;
    }
}
