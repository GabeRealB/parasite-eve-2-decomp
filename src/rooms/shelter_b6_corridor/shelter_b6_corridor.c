#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s16 D_800691CA;

extern s32 D_shelter_b6_corridor_8017F354;
extern s32 D_shelter_b6_corridor_8017F684;
extern s32 D_shelter_b6_corridor_8017EF24[];
extern s16 D_shelter_b6_corridor_801851B0;

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

s32 func_shelter_b6_corridor_8017DEA8(void)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017DEB0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 id;
    s32 k;

    *out = *in;
    func_80179B14(in, out);
    k  = in->msgId;
    id = k;
    k  = 0x19;
    if (id == 9) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (id == k) {
        return Gp_StateF0.field_0 != 1;
    }
    return 1;
}

s32 func_shelter_b6_corridor_8017DF48(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0x144) != 0) {
                Gp_RunCapCmd1(5);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(2);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 3:
            if (GameFlag_GetNibble(0x145) != 0) {
                Gp_RunCapCmd1(6);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(3);
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x146) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.field_0 != 1) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

s32 func_shelter_b6_corridor_8017E020(void)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017E028(void)
{
    func_800E8634((s32)&D_shelter_b6_corridor_8017F354, 0, (s32)&D_shelter_b6_corridor_8017F684);
    func_800E3FAC(0xA2, 0x2F);
    return 0;
}

void func_shelter_b6_corridor_8017E064(Task* arg0)
{
    u16* ptr;
    s32  i;

    arg0->msgTable = D_shelter_b6_corridor_8017EF24;
    Game_SetPtrSlot(arg0, 7);
    ptr = (u16*)Fs_ImgBuffers;
    i   = 0;
    do {
        *ptr = (u16)(*ptr | 0x8000);
        i   += 1;
        ptr += 1;
    } while (i <= 0x12BFF);
    D_shelter_b6_corridor_801851B0 = 2;
    if (gGameSession->at4.loc.place == 1) {
        D_80062735               = 2;
        gGameSession->flowFlags |= 1;
        gGameSession->flowFlags |= 2;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b6_corridor_8017E12C(void)
{
    char pad[0x10];

    D_800691CA = 2;
}
INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", RoomsShared8017d878Table);
