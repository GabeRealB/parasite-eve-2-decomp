#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_observatory.h"

extern SVECTOR D_neo_ark_observatory_80181368;
extern s16     D_neo_ark_observatory_8018136A; // D_neo_ark_observatory_80181368.vy

void func_neo_ark_observatory_8017FE34(GsCOORDINATE2* coord, SVECTOR* offset);

/// Fills in the marker state of one room's spot on the map. Most rooms have no
/// marker; the six that do read a GameFlag nibble, either straight (plus one -
/// rooms 5 / 41 / 45) or folded into a fixed state (rooms 2, 16, 20). The
/// caller passes one buffer as both records, so the state lands on top of the
/// room id it was derived from.
s32 func_neo_ark_observatory_8017F44C(MapMarkerRec* arg0, MapMarkerOut* arg1)
{
    if (arg0->field_5 == 0) {
        switch (arg0->field_0) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    arg1->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    arg1->field_3 = 3;
                }
                break;
            case 5:
                arg1->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    arg1->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        arg1->field_3 = 1;
                        break;
                    case 1:
                        arg1->field_3 = 6;
                        break;
                    case 2:
                        arg1->field_3 = 7;
                        break;
                    case 3:
                        arg1->field_3 = 8;
                        break;
                    default:
                        arg1->field_3 = 1;
                        break;
                }
                break;
            case 45:
                arg1->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                arg1->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_2", func_neo_ark_observatory_8017F588);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_2", func_neo_ark_observatory_8017F6F8);

/// Aims the observatory's model at slot 0xA when one is resident, otherwise at
/// the slot-3 game pointer, and publishes the y of the vertex offset
/// `func_neo_ark_observatory_8017FE34` adds to the transformed model: 0 while
/// flag 0xD7 is set, 10000 while it is clear or no slot-0xA model is resident.
void func_neo_ark_observatory_8017FA98(void)
{
    Task* task;
    Task* slotA;

    task  = Game_GetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = Game_GetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xD7) != 0) {
        D_neo_ark_observatory_8018136A = 0;
    } else {
        D_neo_ark_observatory_8018136A = 0x2710;
    }
    func_neo_ark_observatory_8017FE34(((TmdObject*)task->extra)->field_8, &D_neo_ark_observatory_80181368);
}

void func_neo_ark_observatory_8017FB1C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x300, 0);
            Gp_SpawnIfCapIdle(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            Task_Kill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_2", RoomsShared8017d878Table);
