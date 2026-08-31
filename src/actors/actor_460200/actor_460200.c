#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

extern s32 D_actor_460200_80137AA0;
extern s32 D_actor_460200_80137BA8;
extern s32 D_actor_460200_80137CB0;
extern s32 D_actor_460200_80137DA0;
extern s32 D_actor_460200_80137F98;
extern s32 D_actor_460200_80137FE0;
extern s32 D_actor_460200_80138028;
extern s32 D_actor_460200_80138070;

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80131E2C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80131FB0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132090);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801320E0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132124);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132204);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132210);

void func_actor_460200_801322B8(void)
{
    switch (GameFlag_GetNibble(0x114)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137AA0, 0);
            GameFlag_SetNibble(0x114, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137BA8, 0);
            GameFlag_SetNibble(0x114, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80137CB0, 0);
            GameFlag_SetNibble(0x114, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80137DA0, 0);
            break;
    }
}

void func_actor_460200_80132390(void)
{
    switch (GameFlag_GetNibble(0x115)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137F98, 0);
            GameFlag_SetNibble(0x115, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137FE0, 0);
            GameFlag_SetNibble(0x115, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80138028, 0);
            GameFlag_SetNibble(0x115, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80138070, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132468);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801325FC);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801327B4);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132808);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132950);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132978);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132A04);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132A50);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132AC8);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132B2C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132B98);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132C14);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132C8C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132CAC);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132F0C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801330C8);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_8013311C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_8013322C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133254);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801332E0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_8013332C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801333A4);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133408);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133474);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801334F0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133568);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133580);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_8013364C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801336B4);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_8013386C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801338C0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133A04);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133A88);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133AB0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133B3C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133B88);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133C00);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133C64);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133CD0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133D4C);

s32 func_actor_460200_80133DC4(void)
{
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133DCC);

INCLUDE_RODATA("actors/nonmatchings/actor_460200/actor_460200", D_actor_460200_80131E20);
