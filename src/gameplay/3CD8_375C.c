#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"

extern GpRec14 D_8010FB10;
extern GpRec14 D_8010FB24;
extern GpRec14 Gp_WeaponMsgRec;
extern u16     Gp_WeaponIdBase[];

void Gp_EvtCapWeaponTask(Task* arg0)
{
    s32        flags;
    GameActor* actor;
    s32        mode;
    GpRec14    recB;
    GpRec14    recA;

    flags = (s32)arg0->spawnArg2;
    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    switch (arg0->state) {
        case 0:
            if ((flags & 1) && (flags != 0xFF)) {
                recA         = Gp_WeaponMsgRec;
                recA.field_0 = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&recA, 0);
            }
            recB         = D_8010FB10;
            recB.field_0 = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FA, 0, 0);
            arg0->state++;
            break;
        case 1:
            arg0->state++;
            break;
        case 2:
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->field_954 != 2) {
                Task_Kill(arg0);
            }
            break;
        case 3:
            if ((flags & 1) && (flags != 0xFF)) {
                Gp_StateF0.field_4 = 1;
            }
            if ((flags & 2) && (flags != 0xFF)) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 0, 0);
            }
            if ((flags & 4) && (flags != 0xFF)) {
                mode = 2;
            } else if ((flags & 1) == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            if (flags == 0xFF) {
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F0, arg0->spawnArg1, mode);
            } else {
                Gp_RunCapCmd(arg0->spawnArg1, mode);
            }
            arg0->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 1, 0);
                arg0->state++;
            }
            break;
        case 5:
            if (D_80115598 != 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F2, (s32)arg0->spawnArg2 + 0x64, 0);
            }
            recB         = D_8010FB24;
            recB.field_0 = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FA, 1, 0);
            arg0->state++;
            break;
        case 6:
            arg0->state++;
            break;
        case 7:
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->field_954 != 2) {
                Task_Kill(arg0);
                Gp_StateF0.field_4 = 0;
            }
            break;
        case 8:
            Task_Kill(arg0);
            Gp_StateF0.field_4 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            break;
    }
}
