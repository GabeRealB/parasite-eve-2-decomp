#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s32 D_dryfield_general_store_8017E55C;
extern s32 D_dryfield_general_store_8017E560;
extern s32 D_dryfield_general_store_8017E564;

void func_dryfield_general_store_8017DFB4(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E55C, 0x7DB);
            SndEvt_EnqueueType6(0x5203000F, 0, 0);
            arg0->killCountdown = 0x5A;
            arg0->state++;
            return;
        case 1:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if (temp_v0 < 0) {
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_general_store_8017E064(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E560, 0x7DB);
            arg0->killCountdown = 0x5A;
            arg0->state++;
            return;
        case 1:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if (temp_v0 < 0) {
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E564, 0x7DB);
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
            }
            return;
    }
}
