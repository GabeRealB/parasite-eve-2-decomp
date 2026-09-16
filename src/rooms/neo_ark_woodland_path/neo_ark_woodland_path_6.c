#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_woodland_path.h"

#include <psyq/libgs.h>

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_8018046C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_80180568);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_801806D8);

s32 func_neo_ark_woodland_path_80180B18(Task* task, s32 arg1, NeoArkWoodlandPathMsg* msg)
{
    s32                    result;
    u16                    cmd;
    NeoArkWoodlandPathObj* obj;

    result = 0;
    if (msg->field_0 == 0xB05) {
        cmd = msg->field_2;
        switch (cmd) {
            case 0:
                D_neo_ark_woodland_path_8018498E.s = -1;
                result                             = 0;
                return result;
            case 2:
                D_neo_ark_woodland_path_80184A5C.field_0 = 5;
                D_neo_ark_woodland_path_80184A5C.field_1 = 0xB;
                D_neo_ark_woodland_path_80184A5C.field_2 = 0xC;
                result                                   = 1;
                if (Gp_LookupSlot4(0) != 0) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB,
                                   (s32)&D_neo_ark_woodland_path_80184A5C, 0);
                    obj                                                                  = ((Task*)Gp_LookupSlot4(0))->spawnArg2;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord.t[0] = 5;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord.t[1] = 0;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord.t[2] = -0x320;
                    if (obj != 0) {
                        *(u16*)((u8*)obj + 0x40)         = D_neo_ark_woodland_path_80184A60;
                        D_neo_ark_woodland_path_80184A60 = 0;
                        obj->field_4C                    = 0;
                    }
                    Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord,
                                   0x400, 1);
                    D_neo_ark_woodland_path_8018498E.s = 0x5A;
                }
                return result;
            default:
                return 0;
        }
    } else {
        return result;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_80180C6C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_80180DDC);

s32 func_neo_ark_woodland_path_80181474(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_8018147C);

void func_neo_ark_woodland_path_801814D4(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_801814E8);

s32 func_neo_ark_woodland_path_8018154C(void)
{
    D_neo_ark_woodland_path_8018498E.u += 0x5A;
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_80181568);

void func_neo_ark_woodland_path_801815C0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_801815D4);
