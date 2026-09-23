#include "common.h"

#include "main/session.h"
#include "rooms/shelter_b1_underground_parking.h"

extern u8 D_8007216D;
extern u8 D_shelter_b1_underground_parking_801876C4[];
extern u8 D_shelter_b1_underground_parking_8018D788;

void func_shelter_b1_underground_parking_8018491C(void)
{
    D_8007216D                  = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->at4.loc.room  = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->roomObjsDirty = 1;
}
