#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

/// Tail every `NeoArkShrineFall` handler runs: clears the prop's root coordinate
/// flag, rebuilds its world matrix, and republishes the translation in
/// `func_800D7A9C`'s format, lowered by 0x320 so the prop draws on the floor.
void func_neo_ark_shrine_8017F86C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj        = task->extra;
    coord      = obj->field_8;
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_7", func_neo_ark_shrine_8017F8DC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_7", func_neo_ark_shrine_8017FC14);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_7", D_neo_ark_shrine_8017D6A4);
