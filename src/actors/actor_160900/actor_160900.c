#include "common.h"

#include "main/fs.h"
#include "main/task.h"

#include <psyq/libgpu.h>

#include "actors/actor_160900.h"

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80131EB0);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801326EC);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132844);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132A14);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132C08);

INCLUDE_RODATA("actors/nonmatchings/actor_160900/actor_160900", D_actor_160900_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132E80);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133238);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_8013358C);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133758);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133880);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133A84);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133F90);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_8013418C);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801343E4);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801344D8);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801345D0);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80134624);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801346B0);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801346E0);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80134710);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80134790);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801347B0);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801347D0);

void func_actor_160900_801347F0(void)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    work->field_4C = 0;
    work->field_54 = 0;
    work->field_5C = 0;
    CdCmd_CancelReplaceAndActivate();
    SetDispMask(1);
}
