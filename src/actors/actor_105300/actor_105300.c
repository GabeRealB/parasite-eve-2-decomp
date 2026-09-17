#include "common.h"

#include "actors/actor_105300.h"

#include "actors/actors_shared_80133610.h"
#include "actors/actors_shared_80136574.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"

extern GpPairSrcE         D_actor_105300_8013D3A0;
extern Actor05300SpawnPos D_actor_105300_80133A20[2];
extern Actor05300Clip     D_actor_105300_8013D3E0[];
extern Actor05300Clip     D_actor_105300_8013D3EC[];
extern Actor05300SndRow   D_actor_105300_8013D3C4[];
extern u32                D_actor_105300_8013D3BC;

void func_8017E524(s32 arg0);
void func_8017FD88(s32 arg0);

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300", func_actor_105300_80131E3C);

/// Per-frame animation schedule of the enemy, one of the three handlers the
/// shared dispatcher `ActorsShared80133468` runs each frame. The sub-state
/// (`field_32C`) picks the clip table: state 0 walks `D_actor_105300_8013D3E0`
/// once the countdown `field_32A` has run out, and on that table's terminator
/// row resets the row index, reseeds the countdown from the gameplay LCG and
/// plays the work block's sound id with the actor id in the high nibble of
/// `GpEnemy::field_8`; state 1 walks `D_actor_105300_8013D3EC` and moves to
/// state 2 on its terminator; state 2 hands the pose back to 0 once
/// `ActorsShared80133610` has ticked `field_324` frames past the pose's own
/// length. The row's `field_2` is the scale `ActorsShared80136574` applies to
/// the work block's coordinate matrix, 0x1000 when no row was read, and while
/// the session is in the `field_4D` state the per-area row of
/// `D_actor_105300_8013D3C4` is enqueued as the enemy's sound.
void func_actor_105300_8013222C(Actor05300* arg0)
{
    Actor05300Work* work;
    GsCOORDINATE2*  coord;
    u16             scale;
    s32             pan;
    s32             sndId;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    scale = 0x1000;
    switch ((s16)work->field_32C) {
        case 0:
            if ((s16)work->field_32A <= 0) {
                scale = D_actor_105300_8013D3E0[(s16)work->field_328].field_2;
                if (D_actor_105300_8013D3E0[(s16)work->field_328].field_0 != 0) {
                    work->field_328 = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
                    sndId           = D_actor_105300_8013D3BC |
                            (((u16)arg0->field_20->field_8 >> 12) << 8);
                    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(sndId, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else {
                    work->field_328 = work->field_328 + 1;
                }
            } else {
                work->field_32A = work->field_32A - 1;
            }
            break;
        case 1:
            scale = D_actor_105300_8013D3EC[(s16)work->field_328].field_2;
            if (D_actor_105300_8013D3EC[(s16)work->field_328].field_0 != 0) {
                work->field_328 = 0;
                work->field_32C = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            } else {
                work->field_328 = work->field_328 + 1;
            }
            break;
        case 2:
            if ((s16)work->field_324 >= ActorsShared80133610Table[(s16)work->field_320] + 0x23) {
                work->field_320 = 1;
                work->field_32C = 0;
            }
            break;
    }
    ActorsShared80136574((ActorShared80136574*)arg0, &work->field_2FC, scale, 1);
    if (Game_Session->field_4D == 1) {
        SndEvt_EnqueueTypeA(work->field_31C, D_actor_105300_8013D3C4[Game_Session->field_4].field_0,
                            D_actor_105300_8013D3C4[Game_Session->field_4].field_2);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300", func_actor_105300_8013246C);

/// Spawn state of the enemy: allocates the 0x48-byte part object, seeds its
/// coordinate's translation from the sub-state's entry in
/// `D_actor_105300_80133A20`, links it into `Gp_ObjLists[2]`, and raises one of
/// the two per-enemy death flags. A failed allocation tears the enemy down
/// instead and leaves the task on this handler; otherwise the task moves to the
/// tick handler (`state` 1).
void func_actor_105300_80132BAC(GpEnemy* arg0, Task* arg1)
{
    Actor05300Obj2C* obj;
    Actor05300Work*  work;
    Actor05300Part*  part;
    GsCOORDINATE2*   coord;
    GpRec18*         rec18;
    s32              flag;
    u16              type;

    obj   = arg1->extra;
    coord = obj->field_8;
    work  = (Actor05300Work*)arg1->parent->idMap;
    part  = Mem_Calloc(0x48, 0);
    if (part == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap       = (TaskIdMap*)part;
    coord->sub        = &Gfx_ViewCoord;
    coord->coord.t[0] = D_actor_105300_80133A20[work->field_334].x;
    coord->coord.t[1] = D_actor_105300_80133A20[work->field_334].y;
    coord->coord.t[2] = D_actor_105300_80133A20[work->field_334].z;
    coord->flg        = 0;
    arg0->field_4     = &coord->coord;
    arg0->field_48    = 0;
    Gp_LinkNode(&arg0->node);
    rec18              = part->rec18;
    arg0->field_18     = coord;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_105300_8013D3A0;
    arg0->field_54     = (s32)rec18;
    arg0->field_40     = D_actor_105300_8013D3A0.field_4;
    part->field_3C     = 0x500;
    part->field_38     = coord;
    part->field_3E     = 2;
    part->obj.field_8  = coord;
    part->obj.field_C  = rec18;
    part->obj.field_10 = 0;
    part->obj.field_12 = 0;
    part->obj.field_14 = 0;
    part->obj.field_18 = ((Actor05300Work*)arg1->parent->idMap)->field_29C;
    part->obj.field_1C = 0xC8;
    part->obj.flags    = 1;
    Gp_LinkObj(2, &part->obj);
    Gp_InitRec18Table(rec18, 1, 0);
    part->obj.flags |= 0x8000;
    type             = (u16)work->field_334;
    part->field_46   = type;
    if ((type << 0x10) == 0) {
        func_8017FD88(1);
        flag = 0x147;
    } else {
        func_8017E524(1);
        flag = 0x148;
    }
    GameFlag_SetNibble(flag, 0);
    arg1->state = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300", func_actor_105300_80132DAC);

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300", func_actor_105300_8013310C);

INCLUDE_RODATA("actors/nonmatchings/actor_105300/actor_105300", D_actor_105300_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_105300/actor_105300", D_actor_105300_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_105300/actor_105300", D_actor_105300_80131E30);
