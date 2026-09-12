#include "common.h"
#include "actors/actor_110700.h"
#include "main/mem.h"

// Typed accesses change GCC 2.8.1's alias/CSE decisions in this initializer.
#define ACTOR_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8*)(expr) + (offset)))

void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    s32*  coordFlags;
    void* obj;
    void* work;

    obj        = ACTOR_FIELD(task, void**, 0x2C);
    coordFlags = ACTOR_FIELD(obj, s32**, 8);
    work       = Mem_Calloc(0x480, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    ACTOR_FIELD(task, void**, 0x1C) = work;
    ACTOR_FIELD(obj, void**, 0x1C)  = work + 0x45C;
    ACTOR_FIELD(obj, void**, 0x20)  = work + 0x43C;
    ACTOR_FIELD(obj, s16*, 0xC)     = 0;
    func_800B3F84(work, D_actor_110700_8013BFC0, obj, work + 0x30C, work + 0x14);
    ACTOR_FIELD(work, s32*, 0x47C)  = 0;
    ACTOR_FIELD(task, void**, 0x24) = D_actor_110700_8013BFA0;
    *coordFlags                     = 0;
    ACTOR_FIELD(task, s32*, 0x30)   = 1;
}

#undef ACTOR_FIELD

INCLUDE_ASM("actors/nonmatchings/actor_110700/actor_110700", ActorsShared80131e24Sub1);

INCLUDE_ASM("actors/nonmatchings/actor_110700/actor_110700", func_actor_110700_8013201C);

INCLUDE_RODATA("actors/nonmatchings/actor_110700/actor_110700", D_actor_110700_80131E20);
