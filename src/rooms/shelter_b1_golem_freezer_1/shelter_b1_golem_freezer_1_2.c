#include "common.h"

#include "gameplay/3CD8.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

extern s16 D_shelter_b1_golem_freezer_1_8017E6D0;
extern s16 D_shelter_b1_golem_freezer_1_8017E6D2;

void func_shelter_b1_golem_freezer_1_8017D7CC(GsCOORDINATE2* arg0, s16* arg1);

void func_shelter_b1_golem_freezer_1_8017D744(void)
{
    Task* slot   = (Task*)Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = Game_GetPtrSlot(3);
    }
    if (slot != NULL) {
        if (Game_Session->field_9 == 0x15) {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0;
        } else {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
        }
    } else {
        D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
    }
    func_shelter_b1_golem_freezer_1_8017D7CC(((TmdObject*)task->extra)->field_8, &D_shelter_b1_golem_freezer_1_8017E6D0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1_2", func_shelter_b1_golem_freezer_1_8017D7CC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1_2", func_shelter_b1_golem_freezer_1_8017DA7C);
