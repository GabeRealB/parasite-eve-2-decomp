#include "common.h"

#include "actors/actors_shared_80132724.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

void ActorsShared80132724(s16 arg0)
{
    ActorsShared80132724Msg msg;

    msg.field_0 = gGameSession->at4.loc.stage;
    msg.field_1 = gGameSession->at4.loc.area;
    msg.field_2 = arg0;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}
