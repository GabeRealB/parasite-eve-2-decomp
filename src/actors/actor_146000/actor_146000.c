#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s16            D_80071076;
extern s8             D_80072170;
extern GpAreaApplyRec D_80183618;
extern s32            D_actor_146000_80135428;
extern s32            D_actor_146000_80135980;
extern s32            D_actor_146000_80135BD8;

void func_actor_146000_80131E24(Task* arg0)
{
    s32 state;
    s8  session;

    state = arg0->state;
    switch (state) {
        case 0:
            if (GameFlag_GetNibble(0x73) != 0) {
                func_800E8634((s32)&D_actor_146000_80135980, 0, (s32)&D_actor_146000_80135BD8);
                GameFlag_SetNibble(0x4B, 7);
                D_80072170 = 4;
            } else {
                func_800E8634((s32)&D_actor_146000_80135428, 1, (s32)&D_actor_146000_80135BD8);
                D_80072170 = 2;
            }
            arg0->state++;
            return;
        case 1:
            session = Game_Session->field_1;
            if (session == 2) {
                arg0->state = session;
            }
            return;
        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            GameFlag_SetNibble(0x4C, 0);
            Gp_ApplyAreaRecs(&D_80183618);
            Mc_SaveData.field_6 = 0x19;
            Mc_SaveData.field_5 = state;
            D_80071076          = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Task_Kill(arg0);
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_146000/actor_146000", D_actor_146000_80131E20);
