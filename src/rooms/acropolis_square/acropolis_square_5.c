#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/acropolis_square.h"

extern u8                      D_8007216C;
extern s8                      D_80072310;
extern TaskDesc                RoomsShared80181228Desc;
extern s32                     D_acropolis_square_8018382C;
extern AcropolisSquareCutscene D_acropolis_square_801888AC;
extern s32                     D_acropolis_square_80183834;
extern s32                     D_acropolis_square_801838DC;
extern s32                     D_acropolis_square_80188898;
extern s32                     D_acropolis_square_801888A0;
extern s32                     D_acropolis_square_801888A4;
extern GsCOORDINATE2           D_acropolis_square_801888CC;

s32 func_acropolis_square_801820D8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0) {
        Gp_SpawnIfCapIdle(5, 0);
    }
    return 0;
}
