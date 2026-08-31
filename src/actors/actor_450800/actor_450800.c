#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"

extern s32 D_actor_450800_8013930C;
extern s32 D_actor_450800_8013A684;
extern s32 D_actor_450800_8013A774;
extern s32 D_actor_450800_8013A984;
extern s32 D_actor_450800_8013AB7C;
extern s32 D_actor_450800_8013ACFC;

void func_actor_450800_80131E34(void)
{
    GameSession* session;
    s32          temp_v0;
    s32          n;

    if ((u8)session->field_4 == 4) {
        if (GameFlag_GetNibble(0xC7) == 1) {
            temp_v0                 = D_actor_450800_8013930C + 1;
            D_actor_450800_8013930C = temp_v0;
            if (temp_v0 >= 3) {
                D_actor_450800_8013930C = 3;
                func_800E8614((s32)&D_actor_450800_8013A774, 0);
            } else {
                func_800E8614((s32)&D_actor_450800_8013A684, 0);
            }
        } else {
            n = GameFlag_GetNibble(0xC8) + 1;
            if (n >= 4) {
                n = 3;
            }
            GameFlag_SetNibble(0xC8, n);
            if (n == 1) {
                if (GameFlag_GetNibble(0x83) == n) {
                    func_800E8614((s32)&D_actor_450800_8013A984, 0);
                } else {
                    func_800E8614((s32)&D_actor_450800_8013AB7C, 0);
                }
                func_800E3FAC(0xA2, 0x32);
            } else {
                func_800E8614((s32)&D_actor_450800_8013ACFC, 0);
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80131F28);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80131F70);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80131F98);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132000);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132028);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132080);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801320E8);

INCLUDE_RODATA("actors/nonmatchings/actor_450800/actor_450800", D_actor_450800_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132108);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132160);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132448);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132790);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801327E4);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132868);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801328BC);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132958);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132A1C);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132A68);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132AE0);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132B44);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132BB0);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132C68);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132CE0);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132E9C);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801330AC);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133264);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801332B8);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_8013333C);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133364);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133400);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_8013344C);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801334C4);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133528);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133594);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801335F8);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133670);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133678);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80133740);
