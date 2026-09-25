#include "common.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_450800.h"
#include "actors/actor_461800_move.h"
#include "actors/actors_shared_80132404.h"
#include "actors/actors_shared_80132514.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/gfx.h"

/// Message table `func_actor_450800_80132160` hangs off `Task::msgTable`, and
/// the `TaskDesc` table its three helper tasks come from - the same two roles
/// `D_actor_461800_80139F5C` / `D_actor_461800_80139F8C` play for that overlay.
extern GpMsgEntry D_actor_450800_8014AC58[];
extern TaskDesc   D_actor_450800_8014AC88[];

/// Animation data `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_450800_8014ACC4[];

extern s32  D_actor_450800_8013930C;
extern s32  D_actor_450800_801397A4;
extern s32  D_actor_450800_801398EC;
extern s32  D_actor_450800_8013A564;
extern s32  D_actor_450800_8013A684;
extern s32  D_actor_450800_8013A774;
extern s32  D_actor_450800_8013A984;
extern s32  D_actor_450800_8013AB7C;
extern s32  D_actor_450800_8013ACFC;
extern s16  D_80071076;
extern void func_80180038(s32);
extern void func_80182D14(s32, s32);

void func_actor_450800_80132448(Task* task);
void func_actor_450800_80132868(Task* task);
void func_actor_450800_80132AE0(Task* task);

void func_actor_450800_80131E2C(void)
{
    s32 temp_v0;
    s32 n;

    if (gGameSession->at4.loc.view == 4) {
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
