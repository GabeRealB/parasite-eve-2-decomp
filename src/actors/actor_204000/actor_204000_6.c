#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"

/// Task-like caller whose `field_30` counts the frames the slot flags were cleared.
typedef struct Actor204000Task {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} Actor204000Task;

extern u8           D_8007216C;
extern Actor104000* D_actor_204000_80156538[6];

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_6", func_actor_204000_80150CC8);

void func_actor_204000_80150D74(Actor204000Task* arg0)
{
    s16 i;

    if (Gp_StateF0.field_0 == 1) {
        for (i = 0; i < 6; i++) {
            if (D_actor_204000_80156538[i] != NULL) {
                D_actor_204000_80156538[i]->field_20->field_14 = 0;
            }
        }
        arg0->field_30++;
    }
    if (D_8007216C == 5) {
        for (i = 0; i < 6; i++) {
            if (D_actor_204000_80156538[i] != NULL) {
                Gp_ArmStateF0(1);
                return;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_6", func_actor_204000_80150E5C);
