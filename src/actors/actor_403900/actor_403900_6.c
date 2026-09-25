#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/mem.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_403900.h"

/// Per-animation-id value `func_actor_403900_80137EF0` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_403900_801383B0[];

extern u8 D_801153F4;

/// 1BC.h keeps this out of scope on purpose: callers hand it a sign-extended
/// animation id, which a `u16` prototype would zero-extend.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_403900_80134968(Actor403900* arg0);

/// Inlined copy of `func_actor_403900_80137EF0`: reseeds animation slots
/// 1..0x12 when the animation id changes, otherwise ticks them a frame.
static inline void Actor403900_ReseedAnim(Actor403900* arg0)
{
    Actor403900Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = D_actor_403900_801383B0[work->field_6C0];
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

/// Inlined copy of `func_actor_403900_80137FB4`: relights the actor from its root
/// coordinate and consumes a pending `field_6EA` tint request.
static inline void Actor403900_UpdateTint(Actor403900* arg0)
{
    Actor403900Work* work;
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

/// Inlined copy of `func_actor_403900_80138070`: draws the ground shadow quad.
static inline void Actor403900_DrawShadow(Actor403900* arg0)
{
    Actor403900Work* work;
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
void func_actor_403900_801368E0(GpEnemy* arg0, Actor403900* arg1)
{
    u8*              head;
    SVECTOR*         sc;
    Actor403900Work* work;
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
            Actor403900_UpdateTint(arg1);
            Actor403900_DrawShadow(arg1);
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
            func_actor_403900_8013820C(&arg1->field_2C->field_8->field_F0, 0xC);
            func_actor_403900_80134968(arg1);
            func_8009EA50(work->field_6D8);
            work->field_6CE = 3;
            break;
    }
    func_actor_403900_801380DC(arg1);
    Actor403900_ReseedAnim(arg1);
    coord->flg                            = 0;
    arg1->field_2C->field_8->field_F0.flg = 0;
    Gp_UpdateCoord(coord);
    Actor403900_UpdateTint(arg1);
    Actor403900_DrawShadow(arg1);
    *(u8**)G_SCRATCH_HEAD += sizeof(SVECTOR);
}
