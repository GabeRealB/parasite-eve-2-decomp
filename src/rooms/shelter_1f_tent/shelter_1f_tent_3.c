#include "common.h"

#include "rooms/room_common.h"

extern void func_801322B8(void);
extern void func_80132390(void);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_tent/shelter_1f_tent_3", func_shelter_1f_tent_8017FCA0);

s32 func_shelter_1f_tent_8017FD54(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    if (arg2->field_2 == 1) {
        func_801322B8();
    }
    if (arg2->field_2 == 2) {
        func_80132390();
    }
    return 0;
}
