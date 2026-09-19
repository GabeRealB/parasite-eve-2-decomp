#include "common.h"

#include "main/fs.h"
#include "main/mem.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_403900.h"

#include "psyq/inline_c.h"

/// `rtv0` / `rtps`: the `inline_c.h` macros of those names assemble to
/// different words.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// The spawn's tables: the task's next handler record, the `GpU16Pair`
/// `Gp_PackPair` packs into the third collision object, the `GpPairSrcE` whose
/// `hpMax` seeds the enemy's HP, the stage / room box-table index run, the
/// box tables it selects, the per-stage cue-bank arrays and the animation data.
extern s32                D_actor_403900_801383A0[];
extern GpU16Pair          D_actor_403900_80153BF0;
extern GpPairSrcE         D_actor_403900_80153C00;
extern Actor403900Spot    D_actor_403900_80153C7C[];
extern Actor403900Region* D_actor_403900_80153F04[];
extern s16*               D_actor_403900_8015409C[];
extern u8                 D_actor_403900_801540EC[];

INCLUDE_RODATA("actors/nonmatchings/actor_403900/actor_403900_5", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_5", func_actor_403900_80135A24);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_5", func_actor_403900_80135BE0);

void func_actor_403900_80136184(Actor403900* arg0);

/// Aims the actor: brings the root coordinate local to the fourth part to park
/// the aim point in the work block, then resolves the ground record under the
/// muzzle and projects both world points to screen for the draw step.
void func_actor_403900_80135D5C(Actor403900* arg0)
{
    u8*                    head;
    Actor403900AimScratch* sc;
    Actor403900Work*       work;
    Actor403900Coord*      coord;
    GsCOORDINATE2*         part;
    s32                    i;
    s16                    dist;

    coord                 = arg0->field_2C->field_8;
    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor403900AimScratch);
    sc                    = (Actor403900AimScratch*)(head - sizeof(Actor403900AimScratch));
    work                  = arg0->field_1C;
    part                  = &coord->field_F0 + 1;
    coord->field_0.flg    = 0;
    part->flg             = 0;
    Gp_UpdateCoord(part);
    if (work->field_6D6 > 0) {
        Gp_WorldToLocal(&coord->field_0.workm, &part->workm, &sc->m);
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&sc->m);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        work->field_634  = sc->m.t[0] + sc->out.vx;
        work->field_636  = sc->m.t[1] + sc->out.vy;
        work->field_638  = sc->m.t[2] + sc->out.vz;
        work->field_62A |= 0xC000;
    } else {
        work->field_62A &= 0x3FFF;
    }
    if (work->field_6D6 < 9) {
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&part->workm);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        sc->pts[1].vx = part->workm.t[0] + sc->out.vx;
        sc->pts[1].vy = part->workm.t[1] + sc->out.vy;
        sc->pts[1].vz = part->workm.t[2] + sc->out.vz;
        sc->pts[0].vx = 0;
        sc->pts[0].vy = -0x514;
        if (Gp_FindRec18(&work->field_644, 0) != 0) {
            sc->out.vx    = work->field_644.point.vx - sc->pts[1].vx;
            sc->out.vy    = work->field_644.point.vy - sc->pts[1].vy;
            sc->out.vz    = work->field_644.point.vz - sc->pts[1].vz;
            dist          = SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vy * sc->out.vy + sc->out.vz * sc->out.vz);
            sc->pts[0].vz = dist;
            if ((work->field_644.key & 0xFFFF0000) == 0x10000) {
                sc->pts[0].vz = dist + 0x12C;
            }
            Gp_ClearRec18Occupied(&work->field_644);
        } else {
            sc->pts[0].vz = 10000;
        }
        gte_SetRotMatrix(&coord->field_0.workm);
        gte_ldv0(&sc->pts[0]);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        sc->pts[0].vx = coord->field_0.workm.t[0] + sc->out.vx;
        sc->pts[0].vy = coord->field_0.workm.t[1] + sc->out.vy;
        sc->pts[0].vz = coord->field_0.workm.t[2] + sc->out.vz;
        for (i = 0; i < 2; i++) {
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_ldv0(&sc->pts[i]);
            gte_rtps_real();
            gte_stsxy(&sc->sxy);
            gte_stszotz(&sc->otz);
            work->field_6FC[i] = sc->sxy;
            work->field_700[i] = sc->sxy >> 16;
            work->field_704[i] = sc->otz;
        }
        func_actor_403900_80136184(arg0);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor403900AimScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_5", func_actor_403900_80136184);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_5", func_actor_403900_801368E0);
