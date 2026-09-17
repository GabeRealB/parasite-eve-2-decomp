#include "common.h"

#include "actors/actor_105400.h"
#include "main/mem.h"
#include "main/sound.h"

/// The spawn's offset pair; `field_8` is the vector the enemy's local
/// position and the second list node are both seeded from.
extern Actor05400Pose D_actor_105400_80133A30;

/// The pair source `GpEnemy::field_50` points at; its `field_4` is the HP the
/// context's `field_40` is seeded with.
extern GpPairSrcE D_actor_105400_8013CE30;

/// HP/pose words read straight out of the overlay data: `[0]` is the value
/// stored in `Actor05400Work::field_33C`, which the per-frame handler reads as
/// `Actor05400Work::field_338`.
extern u16 D_actor_105400_8013CE34[];

/// Sound-event base the spawn ORs `(enemy id >> 12) << 8` into.
extern s32 D_actor_105400_8013CE60;

/// One pan/volume row per `Game_Session::field_4`, played at spawn.
extern Actor05400SndRow D_actor_105400_8013CE64[];

/// Task descriptors the spawn hands `Gp_SpawnEnemyFromTable` (entry 1 is the
/// per-frame dispatcher `func_actor_105400_801337DC`).
extern TaskDesc D_actor_105400_8013CEA0[];

/// Animation bank `func_800B3F84` builds the work block's clip context from.
extern u8 D_actor_105400_8013CEB8[];

/// The message table the task is put on (`Task::field_24`).
extern u8 D_actor_105400_80133A00[];

INCLUDE_ASM("actors/nonmatchings/actor_105400/actor_105400", func_actor_105400_80131E3C);

INCLUDE_ASM("actors/nonmatchings/actor_105400/actor_105400", func_actor_105400_8013222C);

INCLUDE_ASM("actors/nonmatchings/actor_105400/actor_105400", func_actor_105400_8013246C);

INCLUDE_ASM("actors/nonmatchings/actor_105400/actor_105400", func_actor_105400_80132BAC);

INCLUDE_ASM("actors/nonmatchings/actor_105400/actor_105400", func_actor_105400_80132DAC);

/// Spawn/setup handler. It allocates the 0x340-byte work block and hangs it on
/// the task, points the model's coordinate and its two matrices (0x244 colour,
/// 0x264 light) at the block, and seeds the enemy's local position and the
/// second `GpObj` from the spawn offsets.
///
/// The block's 0x14 prefix becomes the `GpAnimCtx`: `func_800B3F84` loads the
/// animation bank into it over the ten `GpAnimSlot`s and slots 1..9 are reset.
/// The two `GpObj` nodes at 0x284 / 0x2A4 are linked onto list 2 with their two
/// `GpRec18` records (`Gp_InitRec18Table`), each carrying the "last element"
/// flag 0x8000. A child enemy is spawned from `D_actor_105400_8013CEA0` and its
/// model pointed at the placement record's texture page and CLUT row, then the
/// task moves to the tick handler (`state` 1).
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler.
void func_actor_105400_8013310C(GpEnemy* arg0, Task* arg1)
{
    TmdObject*      obj;
    TmdObject*      model;
    GsCOORDINATE2*  coord;
    Actor05400Work* work;
    GpAreaKey       key;
    GpAreaRec*      rec;
    GpAreaPlace*    place;
    GpAreaKey*      sessionKey;
    Actor05400Pose* pose;
    Actor05400Pose* pose2;
    s32             idx;
    s32             sound;
    s32             i;

    obj   = arg1->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0x340, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap    = (TaskIdMap*)work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->field_264;
    obj->field_20  = &work->field_244;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18    = coord;
    pose              = &D_actor_105400_80133A30;
    arg0->field_1C.vx = pose->field_8.vx;
    arg0->field_1C.vy = pose->field_8.vy;
    arg0->field_1C.vz = pose->field_8.vz;
    arg0->field_50    = &D_actor_105400_8013CE30;
    arg0->field_54    = (s32)work->recs;
    arg0->field_40    = D_actor_105400_8013CE30.field_4;
    work->coord       = coord;
    work->field_2F8   = 0x500;
    work->field_2FA   = 3;
    func_800B3F84((GpAnimCtx*)work, D_actor_105400_8013CEB8, (GpAnimObj*)obj, work->poses,
                  work->slots);
    for (i = 1; i < 0xA; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_334      = 1;
    work->field_326      = 0x1000;
    work->field_2FC      = coord->coord;
    work->field_338      = 1;
    work->field_33C      = D_actor_105400_8013CE34[0];
    work->node0.field_8  = coord;
    work->node0.field_C  = work->recs;
    work->node0.field_10 = 0;
    work->node0.field_12 = 0;
    work->node0.field_14 = 0;
    work->node0.field_18 = 0x30036;
    work->node0.field_1C = 0x5DC;
    work->node0.flags    = 1;
    Gp_LinkObj(2, &work->node0);
    Gp_InitRec18Table(work->recs, 2, 0);
    work->node1.field_8  = coord;
    work->node1.field_C  = work->recs;
    work->node0.flags    = (u16)(work->node0.flags | 0x8000);
    pose2                = &D_actor_105400_80133A30;
    work->node1.field_10 = pose2->field_8.vx;
    work->node1.field_12 = pose2->field_8.vy;
    work->node1.field_14 = pose2->field_8.vz;
    work->node1.field_18 = 0x30036;
    work->node1.field_1C = 0x12C;
    work->node1.flags    = 1;
    Gp_LinkObj(2, &work->node1);
    work->node1.flags = (u16)(work->node1.flags | 0x8000);
    model             = Gp_SpawnEnemyFromTable(&D_actor_105400_8013CEA0, 1, 0, arg0)->task->extra;
    idx               = arg0->field_8 >> 12;
    sessionKey        = (GpAreaKey*)&Game_Session->field_4;
    key.field_3       = sessionKey->field_3;
    key.field_2       = sessionKey->field_2;
    key.field_1       = sessionKey->field_1;
    key.field_0       = sessionKey->field_0;
    Gp_SyncAreaKeyIndex(&key);
    rec             = Gp_GetNestedAreaRec(&key);
    place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = place->field_D;
    model->field_25 = place->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    sound           = D_actor_105400_8013CE60 | ((((GpEnemy*)arg1->spawnArg2)->field_8 >> 12) << 8);
    work->field_31C = sound;
    SndEvt_EnqueueType6(sound, D_actor_105400_8013CE64[Game_Session->field_4].field_0,
                        D_actor_105400_8013CE64[Game_Session->field_4].field_2);
    arg1->field_24 = D_actor_105400_80133A00;
    arg1->state    = 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_105400/actor_105400", D_actor_105400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_105400/actor_105400", D_actor_105400_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_105400/actor_105400", D_actor_105400_80131E30);
