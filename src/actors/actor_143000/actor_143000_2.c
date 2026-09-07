#include "common.h"

#include "actors/actor_143000.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/strings.h"
extern TaskDesc D_actor_143000_801350C8;

extern s32              D_80070F6C;
extern s8               D_8007218B;
extern u8               D_801153F4;
extern GpAreaApplyRec   D_80186488;
extern GpAreaApplyRec   D_8018649C;
extern char             D_actor_143000_80131EB0[];
extern char             D_actor_143000_80131EBC[];
extern s32              D_actor_143000_801351B0;
extern s32              D_actor_143000_80135870;
extern s32              D_actor_143000_80135A20;
extern s32              D_actor_143000_80135AE0;
extern Actor143000Spawn D_actor_143000_80135C08;
extern s32              D_actor_143000_80135C14;
extern s32              D_actor_143000_80135C18;
extern s32              D_actor_143000_80135C1C;
extern char             D_actor_143000_80135C20[];
extern u8               D_actor_143000_80135C38[];

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_801335C8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_80133664);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_80133698);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_801336E8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_80133800);

void func_actor_143000_801338C8(Actor143000* arg0)
{
    arg0->field_1C->field_4 = 0;
    arg0->field_30          = 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_801338E0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_801339CC);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_80133AC0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_80133AE8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_80133C2C);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", ActorsShared8013845cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_2", func_actor_143000_80133CF0);
