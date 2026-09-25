#include "common.h"

#include "actors/actor_105400.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// The spawn's offset pair; `field_8` is the vector the enemy's local
/// position and the second list node are both seeded from.
extern Actor05400Pose D_actor_105400_80133A30;

/// The pair source `GpEnemy::param` points at; its `hpMax` is the HP the
/// context's `field_40` is seeded with.
extern GpPairSrcE D_actor_105400_8013CE30;

/// HP/pose words read straight out of the overlay data: `[0]` is the value
/// stored in `Actor05400Work::field_33C`, which the per-frame handler reads as
/// `Actor05400Work::field_338`.
extern u16 D_actor_105400_8013CE34[];

/// Sound-event base the spawn ORs `(enemy id >> 12) << 8` into.
extern s32 D_actor_105400_8013CE60;

/// One pan/volume row per `gGameSession::at4.loc.view`, played at spawn.
extern Actor05400SndRow D_actor_105400_8013CE64[];

/// Task descriptors the spawn hands `Gp_SpawnEnemyFromTable` (entry 1 is the
/// per-frame dispatcher `func_actor_105400_801337DC`).
extern TaskDesc D_actor_105400_8013CEA0[];

/// Animation bank `func_800B3F84` builds the work block's clip context from.
extern u8 D_actor_105400_8013CEB8[];

/// The message table the task is put on (`Task::msgTable`).
extern u8 D_actor_105400_80133A00[];

extern Actor05400SpawnPos D_actor_105400_80133A20[2];
extern GpPairSrcE         D_actor_105400_8013CE40;
extern s32                D_actor_105400_8013CE50[];
extern s32                D_actor_105400_8013CE54;
extern u8                 D_801153F4;

void func_8017E524(s32 arg0);
void func_8017FD88(s32 arg0);

/// Spawn state of the part task: allocates the 0x48-byte part object, seeds
/// its coordinate's translation from the parent work block's sub-state entry
/// in `D_actor_105400_80133A20`, links it into `Gp_ObjLists[2]` with the main
/// body's collision key, and for that sub-state calls `func_8017FD88` or
/// `func_8017E524` with 1 and clears game flag 0x147 or 0x148 - the two the
/// part teardown undoes. A failed allocation tears the enemy down instead and
/// leaves the task on this handler; otherwise the task moves to the tick
/// handler (`state` 1).
void func_actor_105400_80132BAC(GpEnemy* arg0, Task* arg1)
{
    Actor05400Obj2C* obj;
    Actor05400Work*  work;
    Actor05400Part*  part;
    GsCOORDINATE2*   coord;
    GpRec18*         rec18;
    s32              flag;
    u16              type;

    obj   = arg1->extra;
    coord = obj->field_8;
    work  = (Actor05400Work*)arg1->parent->work;
    part  = memCalloc(0x48, 0);
    if (part == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work        = (TaskIdMap*)part;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = D_actor_105400_80133A20[work->field_334].x;
    coord->coord.t[1] = D_actor_105400_80133A20[work->field_334].y;
    coord->coord.t[2] = D_actor_105400_80133A20[work->field_334].z;
    coord->flg        = 0;
    arg0->field_4     = &coord->coord;
    arg0->field_48    = 0;
    Gp_LinkNode(&arg0->node);
    rec18                     = part->rec18;
    arg0->coord               = coord;
    arg0->bodyPos.vx          = 0;
    arg0->bodyPos.vy          = 0;
    arg0->bodyPos.vz          = 0;
    arg0->param               = &D_actor_105400_8013CE40;
    arg0->recs                = rec18;
    arg0->hp                  = D_actor_105400_8013CE40.hpMax;
    part->field_38.spawnArgLo = 0x500;
    part->field_38.coord      = coord;
    part->field_38.spawnArgHi = 2;
    part->obj.coord           = coord;
    part->obj.ctx.recs        = rec18;
    part->obj.pos.vx          = 0;
    part->obj.pos.vy          = 0;
    part->obj.pos.vz          = 0;
    part->obj.key             = ((Actor05400Work*)arg1->parent->work)->node0.key;
    part->obj.radius          = 0xC8;
    part->obj.flags           = 1;
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

/// Tick handler of the part task (its state 1). Gameplay modes
/// (`D_801153F4`) 1 and 2 skip it, mode 2 setting the lock-on node's flags to
/// 1; mode 0 sets them to 8 first. After the cooldown `field_40` has run out,
/// a contact the player's attack claimed on the part's record deals damage by
/// distance, quadrupled on a critical roll (a record flagged 0x8000 deals
/// none). A killed part moves the task to its teardown, marks the parent work
/// block's `field_336` and plays the death effects and sound; a surviving one
/// spawns its hit effect at most every ten frames, restarts the cooldown from
/// the attack id's parameter 2 and plays the hit sound.
void func_actor_105400_80132DAC(GpEnemy* arg0, Task* arg1)
{
    VECTOR*         vec;
    Actor05400Part* part;
    GsCOORDINATE2*  coord;
    s32             damage;
    s32             snd;
    s32             hitTime;

    coord = ((Actor05400Obj2C*)arg1->extra)->field_8;
    part  = (Actor05400Part*)arg1->work;
    switch (D_801153F4) {
        case 1:
            return;
        case 0:
            arg0->node.flags = 8;
            break;
        case 2:
            arg0->node.flags = 1;
            return;
    }
    vec = --*(VECTOR**)0x1F8003FC;
    if (part->field_40 != 0) {
        part->field_40--;
        if (part->field_40 <= 0) {
            part->field_40 = 0;
        }
    }
    if (part->field_44 != 0) {
        part->field_44--;
    }
    if (part->field_40 == 0 && (part->rec18[0].key & 0xFFFF0000) == 0x20000) {
        if (part->rec18[0].key & 0x8000) {
            func_800DA6E8(&arg0->node, 0, 0);
        } else {
            vec->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage  = Gp_ComputeDamage(part->rec18[0].key, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (Gp_RollEnemyChance(arg0, part->rec18[0].key, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
            func_800DA6E8(&arg0->node, damage, 0);
            arg0->hp -= damage;
            if (arg0->hp <= 0) {
                arg1->state                                      = 2;
                part->field_42                                   = 0;
                ((Actor05400Work*)arg1->parent->work)->field_336 = 1;
                Gp_SpawnEff(0x6005C, coord, 0x10002400, NULL);
                Gp_SpawnEff(0x60070, coord, 0x32FF1400, NULL);
                snd  = D_actor_105400_8013CE54;
                snd |= (arg0->placeKey >> 12) << 8;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else if (damage > 0) {
                if (part->field_44 == 0) {
                    if ((Gp_GetIdParam0(part->rec18[0].key) & 0xFFFF) == 7) {
                        func_800FDB18(3, coord, NULL, &part->field_38);
                    }
                    func_800FDB18(7, coord, NULL, &part->field_38);
                    part->field_44 = 10;
                }
                hitTime = Gp_GetIdParam2(part->rec18[0].key);
                if (hitTime > 0) {
                    part->field_40 = hitTime;
                }
                snd  = D_actor_105400_8013CE50[0];
                snd |= (arg0->placeKey >> 12) << 8;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
    }
    Gp_ClearRec18Occupied(part->rec18);
    *(VECTOR**)0x1F8003FC += 1;
}

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
    coord = obj->coords;
    work  = memCalloc(0x340, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_264;
    obj->colorMtx  = &work->field_244;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = coord;
    pose                       = &D_actor_105400_80133A30;
    arg0->bodyPos.vx           = pose->field_8.vx;
    arg0->bodyPos.vy           = pose->field_8.vy;
    arg0->bodyPos.vz           = pose->field_8.vz;
    arg0->param                = &D_actor_105400_8013CE30;
    arg0->recs                 = work->rec18;
    arg0->hp                   = D_actor_105400_8013CE30.hpMax;
    work->field_2F4.coord      = coord;
    work->field_2F4.spawnArgLo = 0x500;
    work->field_2F4.spawnArgHi = 3;
    func_800B3F84(&work->anim, D_actor_105400_8013CEB8, obj, work->poses,
                  work->slots);
    for (i = 1; i < 0xA; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_334      = 1;
    work->field_326      = 0x1000;
    work->field_2FC      = coord->coord;
    work->field_338      = 1;
    work->field_33C      = D_actor_105400_8013CE34[0];
    work->node0.coord    = coord;
    work->node0.ctx.recs = work->rec18;
    work->node0.pos.vx   = 0;
    work->node0.pos.vy   = 0;
    work->node0.pos.vz   = 0;
    work->node0.key      = 0x30036;
    work->node0.radius   = 0x5DC;
    work->node0.flags    = 1;
    Gp_LinkObj(2, &work->node0);
    Gp_InitRec18Table(work->rec18, 2, 0);
    work->node1.coord    = coord;
    work->node1.ctx.recs = work->rec18;
    work->node0.flags    = (u16)(work->node0.flags | 0x8000);
    pose2                = &D_actor_105400_80133A30;
    work->node1.pos.vx   = pose2->field_8.vx;
    work->node1.pos.vy   = pose2->field_8.vy;
    work->node1.pos.vz   = pose2->field_8.vz;
    work->node1.key      = 0x30036;
    work->node1.radius   = 0x12C;
    work->node1.flags    = 1;
    Gp_LinkObj(2, &work->node1);
    work->node1.flags = (u16)(work->node1.flags | 0x8000);
    model             = Gp_SpawnEnemyFromTable(&D_actor_105400_8013CEA0, 1, 0, arg0)->task->extra;
    idx               = arg0->placeKey >> 12;
    sessionKey        = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage         = sessionKey->stage;
    key.area          = sessionKey->area;
    key.room          = sessionKey->room;
    key.view          = sessionKey->view;
    Gp_SyncAreaKeyIndex(&key);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    sound           = D_actor_105400_8013CE60 | ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8);
    work->field_31C = sound;
    SndEvt_EnqueueType6(sound, D_actor_105400_8013CE64[gGameSession->at4.loc.view].field_0,
                        D_actor_105400_8013CE64[gGameSession->at4.loc.view].field_2);
    arg1->msgTable = D_actor_105400_80133A00;
    arg1->state    = 1;
}
