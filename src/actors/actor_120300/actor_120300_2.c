#include "common.h"
#include "main/fs.h"
#include "main/session.h"

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133D04);

void func_actor_120300_80133DA4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
    Game_Session->field_52 = 1;
}
