#include "common.h"

extern u8 D_actor_548100_80135884;
extern u8 D_actor_548100_80135885;
extern u8 D_actor_548100_80135886;

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134778);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_801347F8);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_801348A4);

void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_80135884 * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_80135885 * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_80135886 * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_801349E0);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134A60);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134AE0);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134BA8);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134BF0);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134CB8);
