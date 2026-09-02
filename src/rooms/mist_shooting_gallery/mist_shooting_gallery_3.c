#include "common.h"

#include "rooms/mist_shooting_gallery.h"

#include "gameplay/3A34.h"
#include "main/display.h"
#include "main/pad.h"
#include "main/stage.h"
#include "main/task.h"

extern void func_8014B0D4(void);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182064);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182294);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801826C4);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182B1C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182C58);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801831B0);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_8018341C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801838FC);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80183E78);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801842D0);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184470);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_8018458C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801846F4);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801847D4);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801848B4);

void func_mist_shooting_gallery_80184954(void)
{
    MistShootingGalleryWork* work = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->idMap;

    work->field_1F = 1;
}

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184970);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801849BC);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184A14);

void func_mist_shooting_gallery_80184A80(Task* arg0)
{
    GameActor* actor;

    actor                             = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    actor->field_97B                  = 0;
    actor->field_983                  = 7;
    ((GpObj*)actor->field_AC)->flags |= 0x2000;
    Display_ReleaseRef();
    ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184AE0);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184B10);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184BB8);

void func_mist_shooting_gallery_80184C0C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0x10;
        case 1:
            if (arg0->killCountdown != 0) {
                arg0->killCountdown--;
                goto call_func;
            }
        pad_check:
            asm volatile("" : : "i"(&&pad_check));
            if (Pad_CheckButtons(0, 1, arg0->spawnArg1) != 0) {
                arg0->state = arg0->state + 1;
                return;
            }
        call_func:
            func_8014B0D4();
            return;
        case 2:
            Task_Kill(arg0);
            Stage_SetEndingFlag();
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184CD0);
