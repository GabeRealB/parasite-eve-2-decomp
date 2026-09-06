#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

void func_neo_ark_shrine_8017F448(void);

/// Message table installed at `Task::field_24` by the room task's state 0.
extern GpMsgEntry D_neo_ark_shrine_80181E34[];

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D6AC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D740);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D7F0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D84C);

void func_neo_ark_shrine_8017D8F4(Task* task)
{
    task->field_24 = D_neo_ark_shrine_80181E34;
    Game_SetPtrSlot(task, 7);
    func_neo_ark_shrine_8017F448();
    task->state++;
}

void func_neo_ark_shrine_8017D940(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", D_neo_ark_shrine_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D948);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D9A0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017DB10);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80180294States);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80181e70Table);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", D_neo_ark_shrine_8017D620);
