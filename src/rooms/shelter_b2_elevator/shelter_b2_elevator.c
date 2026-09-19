#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"
#include "rooms/rooms_shared_8017f470.h"

typedef struct {
    /* 0x0 */ s32 field_0;
} ShelterElevatorCar;

extern s8  D_8007218B;
extern s32 D_801378D0;
extern s32 D_801380F8;

extern GpMsgEntry D_shelter_b2_elevator_8017DFA0[];
extern TaskDesc   D_shelter_b2_elevator_8017DF70;

void func_shelter_b2_elevator_8017D5E8(Task* task)
{
    task->msgTable = D_shelter_b2_elevator_8017DFA0;
    Game_SetPtrSlot(task, 7);
    RoomsShared8017f470Cars[0] = Task_SpawnFromTable(&D_shelter_b2_elevator_8017DF70, 0, 0, -1);
    RoomsShared8017f470Cars[1] = Task_SpawnFromTable(&D_shelter_b2_elevator_8017DF70, 1, 0, 1);
    if (D_8007218B != 9) {
        if (GameFlag_GetNibble(0xCF) == 0) {
            GameFlag_SetNibble(0xCF, 1);
            func_800E8634((s32)&D_801378D0, 0, (s32)&D_801380F8);
            func_800E3FAC(0xA2, 0x24);
        } else {
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Task_SpawnFromTable(&D_shelter_b2_elevator_8017DF70, 2, 0, 0);
            Gp_RunCapCmd(3, 0);
        }
    }
    task->state++;
}

void func_shelter_b2_elevator_8017D70C(Task* task)
{
    TmdObject*          obj;
    GsCOORDINATE2*      coord;
    ShelterElevatorCar* car;
    VECTOR              vec;

    obj   = task->extra;
    coord = obj->coords;
    switch (task->state) {
        case 0:
            car = memCalloc(4, 0);
            if (car == NULL) {
                taskKill(task);
                return;
            }
            task->work        = (TaskIdMap*)car;
            car->field_0      = 0;
            obj->otOffset     = 0x64;
            obj->flags        = 0;
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = 0x2A94;
            coord->coord.t[1] = 0;
            coord->coord.t[2] = -0x1F4;
            coord->flg        = 0;
            task->state++;
            break;
        case 1:
            car          = (ShelterElevatorCar*)task->work;
            car->field_0 = car->field_0 + task->spawnArg1 * 10;
            if (car->field_0 < 0) {
                car->field_0 = 0;
            }
            if (car->field_0 >= 0x1F5) {
                car->field_0 = 0x1F4;
            }
            coord->coord.t[2] = car->field_0 * (s32)task->spawnArg2 - 0x1F4;
            if (gGameSession->at4.loc.view == 2) {
                obj->flags = 0;
            } else {
                obj->flags = 0x80;
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            func_800D7A9C(obj, &vec, 0, 3);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_elevator/shelter_b2_elevator", D_shelter_b2_elevator_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_elevator/shelter_b2_elevator", RoomsShared8017d878Table);
