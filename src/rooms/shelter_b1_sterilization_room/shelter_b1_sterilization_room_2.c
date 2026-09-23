#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16 D_shelter_b1_sterilization_room_80184E80;
extern s16 D_shelter_b1_sterilization_room_80184E82;

void func_shelter_b1_sterilization_room_80180570(GsCOORDINATE2* coord, s16* arg1);

void func_shelter_b1_sterilization_room_80180340(void)
{
    Task* slot   = (Task*)Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = gameGetPtrSlot(3);
    }
    if (slot != NULL) {
        if (gGameSession->at4.loc.place == 5 && GameFlag_GetNibble(0xEA) == 1) {
            D_shelter_b1_sterilization_room_80184E82 = 0;
        } else {
            D_shelter_b1_sterilization_room_80184E82 = 0x2710;
        }
    } else {
        D_shelter_b1_sterilization_room_80184E82 = 0x2710;
    }
    func_shelter_b1_sterilization_room_80180570(((TmdObject*)task->extra)->coords, &D_shelter_b1_sterilization_room_80184E80);
}
