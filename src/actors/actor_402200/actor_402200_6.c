#include "common.h"

#include "main/fs.h"
#include "main/sound.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_402200.h"
#include "actors/actors_shared_80136184.h"

#include "psyq/inline_c.h"

/// `rtv0` / `rtps`: the `inline_c.h` macros of those names assemble to
/// different words.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Cue-id table: `Actor402200Work::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `flags` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one.
extern s32 D_actor_402200_80138420[];

/// The spawn's tables: the task's next handler record, the `GpU16Pair`
/// `Gp_PackPair` packs into the third collision object, the `GpPairSrcE` whose
/// `hpMax` seeds the enemy's HP, the stage / room box-table index run, the
/// box tables it selects, the per-stage cue-bank arrays and the animation data.
extern s32                D_actor_402200_8013839C[];
extern GpU16Pair          D_actor_402200_80153BEC;
extern GpPairSrcE         D_actor_402200_80153BFC;
extern Actor402200Spot    D_actor_402200_80153C78[];
extern Actor402200Region* D_actor_402200_80153FA8[];
extern s16*               D_actor_402200_80154144[];
extern u8                 D_actor_402200_80154194[];

void      func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void      Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void      func_actor_402200_80138208(GsCOORDINATE2* arg0, s32 arg1);
void      func_actor_402200_80134968(Actor402200* arg0);
extern u8 D_801153F4;

/// Inlined copy of `func_actor_402200_80137EEC`: reseeds animation slots
/// 1..0x12 when the animation id changes, otherwise ticks them a frame.
static inline void Actor402200_ReseedAnim(Actor402200* arg0)
{
    Actor402200Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = D_actor_402200_801383AC[work->field_6C0];
        for (; i < 0x13; i++) {
            func_800B4114(work, i, work->field_6C0, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_6C4 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}

/// Inlined copy of `ActorsShared80137fb0`: relights the actor from its root
/// coordinate and consumes a pending `field_6EA` tint request.
static inline void Actor402200_UpdateTint(Actor402200* arg0)
{
    Actor402200Work* work;
    GsCOORDINATE2*   obj;
    VECTOR           vec;
    s16              r;
    s16              g;
    s16              b;

    obj    = (GsCOORDINATE2*)arg0->field_2C->field_8;
    work   = arg0->field_1C;
    vec.vx = obj->workm.t[0];
    vec.vy = obj->workm.t[1];
    vec.vz = obj->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
    switch (work->field_6EA) {
        case 1:
            r = 0;
            g = 0;
            b = 0x400;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 2:
            r = 0xFFF;
            g = 0xFFF;
            b = 0xFFF;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 0:
        default:
            return;
    }
}

/// Inlined copy of `ActorsShared8013806c`: draws the ground shadow quad.
static inline void Actor402200_DrawShadow(Actor402200* arg0)
{
    Actor402200Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   sub;
    VECTOR3          vec;

    work  = arg0->field_1C;
    coord = &arg0->field_2C->field_8->field_0;
    sub   = &arg0->field_2C->field_8->field_F0;
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}

/// Frame handler for the scene's `D_801153F4` mode. Mode 1 only refreshes the
/// coordinates, tint and shadow and mode 2 hides the model, both returning
/// without giving back the 8-byte `G_SCRATCH_HEAD` block. Otherwise the
/// `field_6CE` sequence runs: state 0 unlinks the actor and saves its pose,
/// state 1 sprays a randomly angled effect every fourth frame, and state 2
/// projects the actor before moving on to 3.
void func_actor_402200_801368E0(GpEnemy* arg0, Actor402200* arg1)
{
    u8*              head;
    SVECTOR*         sc;
    Actor402200Work* work;
    GsCOORDINATE2*   coord;
    s32              mode;
    u32              random;
    s16              anim;

    work                  = arg1->field_1C;
    coord                 = &arg1->field_2C->field_8->field_0;
    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(SVECTOR);
    sc                    = (SVECTOR*)(head - sizeof(SVECTOR));
    mode                  = D_801153F4;
    switch (mode) {
        case 0:
            arg1->field_2C->field_C = 0;
            break;
        case 1:
            coord->flg                            = 0;
            arg1->field_2C->field_8->field_F0.flg = 0;
            Gp_UpdateCoord(coord);
            Actor402200_UpdateTint(arg1);
            Actor402200_DrawShadow(arg1);
            return;
        case 2:
            arg1->field_2C->field_C = 0x80;
            return;
    }
    switch (work->field_6CE) {
        case 0:
            arg0->recs = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj((GpObj*)work->field_4E4);
            Gp_UnlinkObj((GpObj*)work->field_47C);
            Gp_UnlinkObj((GpObj*)work->field_564);
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, work->field_716);
            anim = 0x14;
            if (work->field_6F0 == 1) {
                anim = 0x10;
            }
            work->field_6C0  = anim;
            work->field_6CE  = 1;
            arg0->spawnState = work->field_6F0;
            Gp_SaveEnemyPose(arg0);
            break;
        case 1:
            if (!(work->field_6C4 & 3)) {
                sc->vx      = 0;
                sc->vz      = 0;
                random      = Gp_LcgState * 5 + 0x71357911;
                sc->vy      = -((random >> 16) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &arg1->field_2C->field_8->field_F0, 0x400, sc);
            }
            break;
        case 2:
            func_actor_402200_80138208(&arg1->field_2C->field_8->field_F0, 0xC);
            func_actor_402200_80134968(arg1);
            func_8009EA50(work->field_6D8);
            work->field_6CE = 3;
            break;
    }
    func_actor_402200_801380D8(arg1);
    Actor402200_ReseedAnim(arg1);
    coord->flg                            = 0;
    arg1->field_2C->field_8->field_F0.flg = 0;
    Gp_UpdateCoord(coord);
    Actor402200_UpdateTint(arg1);
    Actor402200_DrawShadow(arg1);
    *(u8**)G_SCRATCH_HEAD += sizeof(SVECTOR);
}
