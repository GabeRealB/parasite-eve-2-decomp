#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s16 D_800691CA;

extern GpMsgEntry D_shelter_b6_training_room_80182AF4[];
extern s32        D_shelter_b6_training_room_80182B24;
extern s32        D_shelter_b6_training_room_80183BB4;
extern s32        D_shelter_b6_training_room_80184124;
extern s32        D_shelter_b6_training_room_80184274;
extern s32        D_shelter_b6_training_room_80185C58;

void func_shelter_b6_training_room_8017DBB0(s32 arg0);

s32 func_shelter_b6_training_room_8017D764(void)
{
    gGameSession->flowFlags |= 0x80;
    func_800E8634((s32)&D_shelter_b6_training_room_80183BB4, 0, (s32)&D_shelter_b6_training_room_80184124);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(3), 0x7DB, (s32)&D_shelter_b6_training_room_80182B24, 0);
    D_shelter_b6_training_room_80185C58 = 1;
    return 0;
}

void func_shelter_b6_training_room_8017D7D4(Task* arg0)
{
    u16* ptr;
    s32  i;

    arg0->msgTable = D_shelter_b6_training_room_80182AF4;
    Game_SetPtrSlot(arg0, 7);
    ptr = (u16*)Fs_ImgBuffers;
    i   = 0;
    do {
        *ptr = (u16)(*ptr | 0x8000);
        i   += 1;
        ptr += 1;
    } while (i <= 0x12BFF);
    GameFlag_SetNibble(0x4D, 1);
    D_80062735 = 0xA;
    func_shelter_b6_training_room_8017DBB0(0);
    arg0->state                         = (s32)(arg0->state + 1);
    D_shelter_b6_training_room_80185C58 = 0;
}

void func_shelter_b6_training_room_8017D874(void)
{
    u8 place;

    D_800691CA = 2;
    place      = gGameSession->at4.loc.place;
    if (place == 1 && gGameSession->eventState == 0 && D_shelter_b6_training_room_80185C58 == place) {
        func_800E8614((s32)&D_shelter_b6_training_room_80184274, 0);
        D_shelter_b6_training_room_80185C58 = 2;
    }
}
