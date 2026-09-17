#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"
#include "main/mem.h"

/// Spawn handler. Allocates the 0x270-byte work block onto the task, points the
/// model at its light/colour matrices and links the enemy node. The model
/// variant (`Actor103700Kind::field_2`) picks the mode: tens digit 0 allocates
/// the model buffers and takes the units digit (0..2) as the pose, nudging the
/// root coordinate for poses 1 and 2; 1..3 set model flag 4 and mode 7 or 10.
/// The animation slots then get a shared random phase, and the collision object
/// is linked with its four records before the task moves to state 1.
void func_actor_103700_80131EC4(GpEnemy* arg0, Task* task)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor103700Work* work;
    s32              kind;
    s32              i;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0x270, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, task);
        return;
    }
    task->idMap    = (TaskIdMap*)work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->lightMtx;
    obj->field_20  = &work->colorMtx;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_50          = &D_actor_103700_80139D2C;
    arg0->field_18          = coord;
    arg0->node.field_4      = 0;
    arg0->field_1C.vx       = 0;
    arg0->field_1C.vy       = 0;
    arg0->field_1C.vz       = 0;
    arg0->field_54          = (s32)work->records;
    work->field_224.field_0 = &((TmdObject*)task->extra)->field_8[1];
    work->field_224.field_4 = 0x100;
    work->field_224.field_6 = 1;
    work->field_246         = ((Actor103700Kind*)arg0->field_3C)->field_A;
    kind                    = ((Actor103700Kind*)arg0->field_3C)->field_2;
    switch (kind / 10) {
        case 0:
            Tmd_AllocBuffers(obj);
            if (kind < 3) {
                work->field_24E = kind;
            } else {
                work->field_24E = 0;
            }
            work->field_248 = kind < 3 ? kind + 1 : 1;
            switch (work->field_24E) {
                case 1:
                    work->field_248    = 2;
                    coord->coord.t[1] += 0x50;
                    break;
                case 2:
                    work->field_248    = 3;
                    coord->coord.t[2] -= 0x55;
                    break;
                case 0:
                    work->field_248 = 1;
                    break;
            }
            break;
        case 1:
        case 2:
            obj->field_C   |= 4;
            work->field_24E = 7;
            work->field_248 = 1;
            if (kind == 10) {
                work->field_266 = 1;
            }
            break;
        case 3:
            obj->field_C   |= 4;
            work->field_24E = 10;
            work->field_248 = 1;
            break;
    }
    arg0->field_40  = D_actor_103700_80139D30;
    work->field_24A = work->field_248;
    task->field_24  = &D_actor_103700_80139F28;
    func_800B3F84(&work->anim, D_actor_103700_80139F04, (GpAnimObj*)obj, work->poses, work->slots);
    for (i = 1; i < 6; i++) {
        Gp_AnimResetSlot(&work->anim, i, work->field_248);
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    kind        = (Gp_LcgState >> 16) & 3;
    for (i = 1; i < 6; i++) {
        work->slots[i].field_9 += kind;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_234.vx = coord->coord.t[0];
    work->field_234.vy = coord->coord.t[1];
    work->field_234.vz = coord->coord.t[2];
    work->obj.field_1C = 0xC8;
    work->obj.field_8  = coord;
    work->obj.field_C  = work->records;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.field_18 = 0x30025;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(work->records, 4, 0);
    work->obj.flags |= 0xC000;
    task->state      = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700", func_actor_103700_8013224C);

INCLUDE_RODATA("actors/nonmatchings/actor_103700/actor_103700", D_actor_103700_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_103700/actor_103700", ActorsShared80135df4Table);
