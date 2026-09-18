#include "common.h"

#include "gameplay/3A34.h"

#include "actors/actor_402200.h"
#include "actors/actors_shared_80132d78.h"

/// Per-roll wait lengths state 0 of `func_actor_402200_801329A4` scales by
/// `16 - field_70C`, indexed by a 4-bit `Gp_LcgState` draw.
extern s16 D_actor_402200_80153C38[];

/// Per-roll state offsets state 0 adds to 2 when `field_6E8` is set.
extern u16 D_actor_402200_80153C58[];

/// State machine on `field_6CE`: 0 rolls a `field_6D4` wait, 1 counts it
/// down, 2 picks state 3 or 4 from `field_70E` and an LCG draw offset by
/// `field_710` (or 5 when `ActorsShared80132d78` reports a box hit), and 3-5
/// settle the result, walking `field_70C` up to 12.
void func_actor_402200_801329A4(Actor402200* arg0)
{
    Actor402200Work* work;

    work = arg0->field_1C;
    switch (work->field_6CE) {
        case 0:
            work->field_6C8 = 0;
            work->field_6EC = 0;
            work->field_6EE = 0;
            if (work->field_6E8 != 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6CE = D_actor_402200_80153C58[(Gp_LcgState >> 16) & 0xF] + 2;
                work->field_6EE = 1;
                func_actor_402200_80132E34(arg0);
                work->field_6E4 = 0;
            } else if (work->field_6E4 == 0) {
                work->field_6D4 = (D_actor_402200_80153C38[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF] * (0x10 - work->field_70C)) / 16;
                work->field_6CE = 1;
            } else {
                work->field_6D4 = 0;
                work->field_6CE = 2;
                work->field_6E4 = 0;
            }
            break;
        case 1:
            work->field_6D4--;
            if ((s16)work->field_6D4 <= 0) {
                work->field_6CE = 2;
                work->field_6D4 = 0;
            }
            break;
        case 2:
            if (work->field_70E != 3 && ActorsShared80132d78((ActorShared80132d78*)arg0) != 0) {
                work->field_6CE = 5;
                work->field_710 = 0;
                break;
            }
            if (work->field_70E == 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((s32)((Gp_LcgState >> 16) & 0xF) < work->field_710 + 10) {
                    work->field_6CE = 4;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 3;
                    work->field_710++;
                }
            } else if (work->field_70E == 2) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((s32)((Gp_LcgState >> 16) & 0xF) < work->field_710 + 8) {
                    work->field_6CE = 3;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 4;
                    work->field_710++;
                }
            } else {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6CE = ((Gp_LcgState >> 16) & 0xF) < 8 ? 3 : 4;
                work->field_710 = 0;
            }
            func_actor_402200_80132E34(arg0);
            break;
        case 3:
            if (work->field_5F4.key == 0) {
                work->field_6CC = 1;
                work->field_6CE = 0;
                work->field_70E = 1;
                if (work->field_70C < 12) {
                    work->field_70C++;
                }
            } else {
                work->field_6CE = 2;
                if (work->field_710 != 0) {
                    work->field_710--;
                }
            }
            work->field_5BA &= ~0x4000;
            work->field_5DA &= ~0x4000;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
        case 4:
            if (work->field_5F4.key == 0) {
                work->field_6CC = 2;
                work->field_6CE = 0;
                work->field_70E = 2;
                if (work->field_70C < 12) {
                    work->field_70C++;
                }
            } else {
                work->field_6CE = 2;
                if (work->field_710 != 0) {
                    work->field_710--;
                }
            }
            work->field_5BA &= ~0x4000;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
        case 5:
            work->field_6CC = 3;
            work->field_6CE = 0;
            work->field_70E = 3;
            Gp_ClearRec18Occupied(&work->field_644);
            if (work->field_70C < 12) {
                work->field_70C++;
            }
            break;
    }
}
