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
    coord = obj->coords;
    work  = Mem_Calloc(0x270, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, task);
        return;
    }
    task->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->lightMtx;
    obj->colorMtx  = &work->colorMtx;
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
    work->field_224.field_0 = &((TmdObject*)task->extra)->coords[1];
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
            obj->flags     |= 4;
            work->field_24E = 7;
            work->field_248 = 1;
            if (kind == 10) {
                work->field_266 = 1;
            }
            break;
        case 3:
            obj->flags     |= 4;
            work->field_24E = 10;
            work->field_248 = 1;
            break;
    }
    arg0->field_40  = D_actor_103700_80139D30;
    work->field_24A = work->field_248;
    task->msgTable  = &D_actor_103700_80139F28;
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

/// Collision step. Applies the `func_800E0C10` push-back to the root coordinate
/// (a delta for 1, an absolute reset to `field_22C` for 2), then walks the four
/// records: kind 1 records push the actor out along the deepest overlap, kind 2
/// records are hits from a player slot, which take damage, spawn the hit spark
/// and move the actor into its flinch / knockdown modes.
void func_actor_103700_8013224C(Task* task, TmdObject* arg1, s32 arg2)
{
    Actor103700PushScratch* scratch;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          src;
    Actor103700Work*        work;
    s32                     push;
    s32                     reach;
    s32                     res;
    s32                     i;
    s32                     z;
    s32                     val;
    s32                     ex;
    s32                     ey;
    s32                     ez;
    s32                     broke;
    u32                     id;
    u32                     damage;

    push    = 0;
    broke   = 0;
    work    = (Actor103700Work*)task->work;
    scratch = (Actor103700PushScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x58);
    coord   = ((TmdObject*)task->extra)->coords;
    res     = func_800E0C10(work->records, &scratch->delta, 4, NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.vx.h.hi;
    coord->coord.t[1] += scratch->delta.vy.h.hi;
    z                  = coord->coord.t[2] + scratch->delta.vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_22C.vx;
    coord->coord.t[1] = work->field_22C.vy;
    z                 = work->field_22C.vz;
move_z:
    coord->coord.t[2] = z;
move_done:
    i               = 0;
    work->field_264 = 0;
    do {
        id = work->records[i].field_4;
        switch (id >> 16) {
            case 0:
                break;
            case 1:
                work->field_264     = id >> 16;
                scratch->delta.vx.w = coord->workm.t[0] - work->records[i].field_8;
                scratch->delta.vy.w = coord->workm.t[1] - work->records[i].field_A;
                scratch->delta.vz.w = coord->workm.t[2] - work->records[i].field_C;
                reach               = work->records[i].field_2 - SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w);
                val                 = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &scratch->normal, &scratch->dir);
                }
                break;
            case 2:
                src                 = Gp_ActorSlots[(id >> 7) & 1]->extra->coords;
                ex                  = src->coord.t[0] - coord->coord.t[0];
                scratch->delta.vx.w = ex;
                ey                  = src->coord.t[1] - coord->coord.t[1];
                scratch->delta.vy.w = ey;
                ez                  = src->coord.t[2] - coord->coord.t[2];
                scratch->delta.vz.w = ez;
                damage              = Gp_ComputeDamage(work->records[i].field_4, SquareRoot0(ex * ex + ey * ey + ez * ez), 0, 0);
                id                  = work->records[i].field_4;
                if (id & 0x8000) {
                    if (D_actor_103700_80139E94[id & 0x7F] == 3) {
                        broke  = 1;
                        damage = 0;
                    } else {
                        broke = Gp_GetIdParam1(id) & 0xFFFF;
                        if ((u32)(broke - 0xC) < 2) {
                            func_800FDB18(broke, coord, NULL, &work->field_224);
                        }
                        work->field_268 = D_actor_103700_80139E94[work->records[i].field_4 & 0x7F];
                        broke           = 0;
                    }
                } else {
                    work->field_268 = (Gp_GetIdParam1(id) & 0xFFFF) == 7;
                }
                func_800DA6E8(&((GpEnemy*)task->spawnArg2)->node, damage, 0);
                func_800E2C78((GpObj40*)task->spawnArg2, work->records[i].field_4, damage, 0);
                if ((s32)damage > 0) {
                    ((GpEnemy*)task->spawnArg2)->field_40 = 0;
                    work->field_24E                       = 6;
                    work->field_250                       = 0;
                    task->state                           = 2;
                } else if (((Gp_GetIdParam0(work->records[i].field_4) & 0xFFFF) == 8 || broke == 1) &&
                           (u16)(work->field_24E - 1) >= 2) {
                    if (work->field_262 == 0) {
                        work->field_24E = 5;
                        work->field_250 = 3;
                    } else {
                        work->field_24E = 11;
                        work->field_250 = 0;
                    }
                }
                break;
        }
    } while (++i < 4);
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->dir.vx) >> 12;
        coord->coord.t[2] += (push * scratch->dir.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->records);
    *(u8**)G_SCRATCH_HEAD += 0x58;
}

INCLUDE_RODATA("actors/nonmatchings/actor_103700/actor_103700", D_actor_103700_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_103700/actor_103700", ActorsShared80135df4Table);
