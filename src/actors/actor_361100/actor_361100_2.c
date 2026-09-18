#include "common.h"

#include "actors/actor_361100.h"

#include "main/display.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

extern Task* D_actor_361100_80171BE0;

extern TaskDesc D_actor_361100_80165C58;

/// Script pair handed to `Gp_SpawnScript18` on every even frame of the blink.
extern s32 D_actor_361100_80166AD0;
extern s32 D_actor_361100_80166AD8;

void func_actor_361100_8016299C(void)
{
    D_actor_361100_80171BE0 = Task_SpawnFromTable(&D_actor_361100_80165C58, 0, 0, 0);
}

void func_actor_361100_801629D0(s32 arg0)
{
    if (D_actor_361100_80171BE0 != NULL) {
        if (arg0 < 2) {
            if (arg0 >= 0) {
                D_actor_361100_80171BE0->spawnArg1 = arg0;
                return;
            }
        }
        Task_Kill(D_actor_361100_80171BE0);
        D_actor_361100_80171BE0 = NULL;
    }
}

void func_actor_361100_80162A24(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_361100_80165C58, 1, arg0, 0);
}

void func_actor_361100_80162A54(Task* arg0)
{
    s32 countdown;

    countdown       = arg0->spawnArg1 - 1;
    arg0->spawnArg1 = countdown;
    if (countdown > 0) {
        Display_ClampField126((countdown & 1) ? 0 : -1);
        Gp_SpawnScript18((s32)&D_actor_361100_80166AD0, (s32)&D_actor_361100_80166AD8);
    }
    if ((arg0->spawnArg1 <= 0) || (gGameSession->evtSkipped != 0)) {
        Display_ClampField126(0);
        Task_Kill(arg0);
    }
}
