#include "common.h"

#include "main/session.h"

#include "rooms/room_common.h"

extern void func_80131FBC(void);
extern void func_80132038(void);
extern void func_80132110(void);
extern void func_801322A0(void);
extern void func_80149E38(void);
extern void func_80149EBC(void);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_8017FF08);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_801800A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_801802AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_80180334);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_8018041C);

s32 func_shelter_1f_heliport_801804BC(s32 arg0, s32 arg1, RoomEventMsg* in)
{
    switch (in->field_2) {
        case 1:
            if (gGameSession->at4.loc.place == 1) {
                func_80149EBC();
            }
            if (gGameSession->at4.loc.place == 2) {
                func_80149E38();
            }
            break;
        case 2:
            func_80132038();
            break;
        case 3:
            func_80131FBC();
            break;
        case 4:
            func_80132110();
            break;
        case 5:
            func_801322A0();
            break;
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", RoomsShared8017eb5cIdList);
