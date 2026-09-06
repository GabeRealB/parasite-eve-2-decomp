#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"

extern s32 D_mist_parking_8018F4AC;
extern s32 D_mist_parking_8018F5E4;
extern s32 D_mist_parking_8018FA4C;
extern s32 D_mist_parking_8018FB3C;
extern s32 D_mist_parking_8019531C;

void func_mist_parking_8018345C(Task* arg0)
{
    if (Game_Session->field_1 == 0 && Gp_CapBusy() == 0) {
        if (D_mist_parking_8019531C == 2) {
            func_800E8614((s32)&D_mist_parking_8018F5E4, 1);
        } else {
            func_800E8614((s32)&D_mist_parking_8018F4AC, 1);
        }
        Task_Kill(arg0);
    }
}

void func_mist_parking_801834D4(Task* arg0)
{
    if (Game_Session->field_1 == 0 && Gp_CapBusy() == 0) {
        if (D_mist_parking_8019531C == 2) {
            func_800E8614((s32)&D_mist_parking_8018FB3C, 1);
        } else {
            func_800E8614((s32)&D_mist_parking_8018FA4C, 1);
        }
        Task_Kill(arg0);
    }
}
