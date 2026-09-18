#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

void func_mine_mesa_8017EAAC(void)
{
    gGameSession->viewDirty = 1;
}

void func_mine_mesa_8017EAC0(void)
{
    Task* slot;

    slot = (Task*)Game_GetPtrSlot(0xA);
    if (slot != NULL) {
        Gp_SpawnEff(0x6002B, &((TmdObject*)slot->extra)->field_8[8], 0x21, NULL);
        SndEvt_EnqueueType6(0x40650001, 0, 0);
    }
}
