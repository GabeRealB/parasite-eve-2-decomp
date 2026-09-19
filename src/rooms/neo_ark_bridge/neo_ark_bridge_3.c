#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/task.h"

extern s32 D_8011572C;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115758;
extern u32 Gp_LcgState;

extern SVECTOR D_neo_ark_bridge_80181F58;
extern SVECTOR D_neo_ark_bridge_80181F60;
extern SVECTOR D_neo_ark_bridge_80181F68;

void func_neo_ark_bridge_8017EB08(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Bridge effect task tick. State 0 installs the five bridge effect ids
/// (0x601E1, then 0x601FD / 0x60219 / 0x6017A / 0x6017B) and advances. State 1
/// only acts while `Gp_GetViewIndex()` reports the two side views 5 or 6 and no
/// state-1C flag is set: two LCG rolls each spawn effect 0x60070 at
/// `D_neo_ark_bridge_80181F60` / `D_neo_ark_bridge_80181F68` on a 1-in-4, then
/// the sprite at `D_neo_ark_bridge_80181F58` is drawn for 0x600 frames.
void func_neo_ark_bridge_8017E954(Task* arg0)
{
    s32 view;
    u32 rnd;
    u32 rndSpawn;
    u32 rndSpawn2;

    switch (arg0->state) {
        case 0:
            D_80115758  = 0x601E1;
            D_8011572C  = 0x601FD;
            D_80115750  = 0x60219;
            D_8011574C  = 0x6017A;
            D_80115738  = 0x6017B;
            arg0->state = 1;
            /* fallthrough */
        case 1:
            view = Gp_GetViewIndex() & 0xFF;
            if (view < 7) {
                if (view >= 5) {
                    if (Gp_State1C->eventState == 0) {
                        rnd         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rnd;
                        if (((rnd >> 16) & 3) == 0) {
                            rndSpawn    = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rndSpawn;
                            Gp_SpawnEff(0x60070, 0, ((rndSpawn >> 16) & 0x11FF) | 0x22200,
                                        &D_neo_ark_bridge_80181F60);
                        }
                        rnd         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rnd;
                        if (((rnd >> 16) & 3) == 0) {
                            rndSpawn2   = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rndSpawn2;
                            Gp_SpawnEff(0x60070, 0, ((rndSpawn2 >> 16) & 0x11FF) | 0x22200,
                                        &D_neo_ark_bridge_80181F68);
                        }
                    }
                    func_neo_ark_bridge_8017EB08(&D_neo_ark_bridge_80181F58, 0x600, 0xC0);
                }
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_bridge/neo_ark_bridge_3", func_neo_ark_bridge_8017EB08);
