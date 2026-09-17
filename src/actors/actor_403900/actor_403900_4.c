#include "common.h"

#include "main/fs.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_403900.h"

/// The spawn's tables: the task's next handler record, the `GpU16Pair`
/// `Gp_PackPair` packs into the third collision object, the `GpPairSrcE` whose
/// `field_4` seeds the enemy's HP, the stage / room box-table index run, the
/// box tables it selects, the per-stage cue-bank arrays and the animation data.
extern s32                D_actor_403900_801383A0[];
extern GpU16Pair          D_actor_403900_80153BF0;
extern GpPairSrcE         D_actor_403900_80153C00;
extern Actor403900Spot    D_actor_403900_80153C7C[];
extern Actor403900Region* D_actor_403900_80153F04[];
extern s16*               D_actor_403900_8015409C[];
extern u8                 D_actor_403900_801540EC[];

INCLUDE_RODATA("actors/nonmatchings/actor_403900/actor_403900_4", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80135A24);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80135BE0);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80135D5C);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80136184);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_801368E0);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80136D9C);

/// Spawn handler. Allocates the 0x71C-byte work block, points the model at its
/// light / colour matrices and loads the animation context, then branches on
/// the enemy's `field_4B` variant. Variant 0 is the full setup: it links the
/// enemy node, picks the box table and count for the current stage / room out
/// of `D_actor_403900_80153C7C`, requests the room's cue bank, and links the
/// work block's five collision objects with their `GpRec18` tables before
/// moving the task on (`field_30` 1). Variants 1 and 2 only seed the animation
/// and sequence state.
void func_actor_403900_80137444(GpEnemy* arg0, Actor403900* arg1)
{
    u8                param1[4];
    u8                param2[4];
    Actor403900Work*  work;
    Actor403900Obj2C* obj;
    Actor403900Coord* coord;
    s16*              cues;
    GpRec18*          records1;
    GpRec18*          records2;
    GpRec18*          records3;
    GpRec18*          records4;
    GpRec18*          records5;
    s32               i;
    s32               kind;

    obj   = arg1->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x71C, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->field_1C          = work;
    obj->field_C            = 0;
    coord->field_0.flg      = 0;
    obj->field_1C           = &work->field_45C;
    obj->field_20           = &work->field_43C;
    work->field_65C.field_0 = &arg1->field_2C->field_8->field_F0;
    work->field_65C.field_4 = 0x500;
    work->field_65C.field_6 = 2;
    func_800B3F84((GpAnimCtx*)work, D_actor_403900_801540EC, (GpAnimObj*)obj, work->field_30C, &work->field_14);
    work->field_6C0 = 0xB;
    work->field_6C2 = 0xB;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_6C0);
    }
    kind = arg0->field_4B;
    switch (kind) {
        case 0:
            work->field_6D8 = 0xFF;
            obj->field_2C   = 0;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = -1;
            arg0->field_4   = &coord->field_0.coord;
            arg0->field_48  = 0;
            Gp_LinkNode(&arg0->node);
            arg0->field_18    = &arg1->field_2C->field_8->field_F0;
            arg0->field_1C.vx = 0;
            arg0->field_1C.vy = 0;
            arg0->field_1C.vz = 0;
            arg0->field_50    = &D_actor_403900_80153C00;
            arg0->field_54    = (s32)work->field_49C;
            arg0->field_40    = D_actor_403900_80153C00.field_4;
            for (i = 0; D_actor_403900_80153C7C[i].field_0 != 0; i++) {
                if (Game_Session->field_7 == D_actor_403900_80153C7C[i].field_2 && Game_Session->field_6 == D_actor_403900_80153C7C[i].field_4) {
                    work->field_6B4 = D_actor_403900_80153F04[D_actor_403900_80153C7C[i].field_0];
                    work->field_6FA = D_actor_403900_80153C7C[i].field_6;
                }
            }
            work->field_6CC = 0xB;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            work->field_716 = 0x27;
            cues            = D_actor_403900_8015409C[Game_Session->field_7];
            if (cues != NULL) {
                work->field_712 = cues[Game_Session->field_6];
            }
            if (work->field_712 != 0) {
                param1[3] = 0;
                param1[2] = 0x28;
                param1[0] = work->field_712;
                param2[0] = 0x16;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }
            work->field_484 = &arg1->field_2C->field_8->field_F0;
            records1        = work->field_49C;
            work->field_488 = records1;
            work->field_48C = 0;
            work->field_48E = 0;
            work->field_490 = 0;
            work->field_494 = 0x30027;
            work->field_498 = 0x15E;
            work->field_49A = 1;
            Gp_LinkObj(2, (GpObj*)work->field_47C);
            Gp_InitRec18Table(records1, 3, 0);
            work->field_49A |= 0x8000;
            work->field_4EC  = arg1->field_2C->field_8;
            records2         = work->field_504;
            work->field_4F0  = records2;
            work->field_4F4  = 0;
            work->field_4F6  = -0x1F4;
            work->field_4F8  = 0;
            work->field_4FC  = 0x30027;
            work->field_500  = 0x1F4;
            work->field_502  = 1;
            Gp_LinkObj(2, (GpObj*)work->field_4E4);
            Gp_InitRec18Table(records2, 4, 0);
            work->field_502 |= 0x4200;
            work->field_56C  = &((GsCOORDINATE2*)arg1->field_2C->field_8)[8];
            records3         = &work->field_584;
            work->field_570  = records3;
            work->field_574  = 0;
            work->field_576  = 0;
            work->field_578  = 0;
            work->field_57C  = Gp_PackPair(&D_actor_403900_80153BF0, 1);
            work->field_580  = 0x12C;
            work->field_582  = 1;
            Gp_LinkObj(3, (GpObj*)work->field_564);
            Gp_InitRec18Table(records3, 1, 0);
            work->field_582 &= 0x7FFF;
            work->field_5DC  = 0;
            work->field_5DE  = -0x3E8;
            work->field_5E0  = -0x7D0;
            work->field_5E4  = 0;
            work->field_5E6  = -0x3E8;
            work->field_5E8  = 0;
            work->field_5EC  = 0x1F4;
            work->field_5EE  = 0x1F4;
            records4         = &work->field_5F4;
            work->field_5F0  = records4;
            work->field_5A4  = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            work->field_5A8  = &work->field_5DC;
            work->field_5AC  = 0;
            work->field_5AE  = 0;
            work->field_5B0  = 0;
            work->field_5B4  = 0;
            work->field_5B8  = 0;
            work->field_5BA  = 3;
            Gp_LinkObj(3, (GpObj*)work->field_59C);
            Gp_InitRec18Table(records4, 1, 0);
            work->field_5BA &= 0xBFFF;
            work->field_5C4  = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            work->field_5C8  = records4;
            work->field_5CC  = 0;
            work->field_5CE  = -0x320;
            work->field_5D0  = -0x5AA;
            work->field_5D4  = 0;
            work->field_5D8  = 0x1F4;
            work->field_5DA  = 1;
            Gp_LinkObj(3, (GpObj*)work->field_5BC);
            work->field_62C  = 0;
            work->field_62E  = -0x514;
            work->field_630  = 0x2710;
            work->field_634  = 0;
            work->field_636  = 0;
            work->field_638  = 0;
            work->field_63C  = 1;
            work->field_63E  = 1;
            records5         = &work->field_644;
            work->field_640  = records5;
            work->field_614  = coord;
            work->field_618  = &work->field_62C;
            work->field_61C  = 0;
            work->field_61E  = 0;
            work->field_620  = 0;
            work->field_624  = 0;
            work->field_628  = 0;
            work->field_62A  = 3;
            work->field_5DA &= 0xBFFF;
            Gp_LinkObj(3, (GpObj*)work->field_60C);
            Gp_InitRec18Table(records5, 1, 0);
            work->field_62A = (work->field_62A & 0x3FFF) | 0xC00;
            arg1->field_24  = D_actor_403900_801383A0;
            arg1->field_30  = 1;
            break;
        case 1:
            work->field_6C0 = 0x10;
            work->field_6CE = 2;
            arg1->field_30  = 2;
            work->field_6D8 = 0;
            obj->field_2C   = 0x1000;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
        case 2:
            work->field_6C0 = 0x14;
            work->field_6CE = kind;
            arg1->field_30  = kind;
            work->field_6D8 = 0;
            obj->field_2C   = 0x1000;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
    }
}
