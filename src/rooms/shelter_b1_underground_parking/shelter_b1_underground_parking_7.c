#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/shelter_b1_underground_parking.h"

extern GpMsgEntry D_shelter_b1_underground_parking_80187230[];

void func_shelter_b1_underground_parking_80183810(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_underground_parking_80187230;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_b1_underground_parking_801848A4();
    if (gGameSession->at4.loc.place == 0x15) {
        Gp_MsgSlot4Chain(0, 1);
    }
    if ((GameFlag_GetNibble(0x7A) >= 6) && (GameFlag_GetNibble(0x123) == 0)) {
        GameFlag_SetNibble(0x123, 1);
        func_shelter_b1_underground_parking_8018390C();
    }
    arg0->state = arg0->state + 1;
}
