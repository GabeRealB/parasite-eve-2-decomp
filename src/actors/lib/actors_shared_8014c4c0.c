#include "common.h"
#include "actors/actor_201200.h"
#include "gameplay/3A34.h"
#include "main/mem.h"

extern u32 Gp_LcgState;

/// Picks a random offset and coordinate index for the hit effect from the hit
/// angle `arg1` (front, back, right or left), copies it into `work->effOfs` and
/// spawns the effect for hit id `arg2` against coordinate 1.
///
/// Shared: the three actor slots (`actor_101200` / `201200` / `301200`) carry
/// the same body, so one object serves every overlay that lists this unit in
/// `configs/USA/overlays.toml`.
void ActorsShared8014c4c0(Actor201200* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*         sc;
    Actor201200Work* work;
    s32              mag;
    GsCOORDINATE2*   coord;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    coord                = &arg0->field_2C->field_8[1];
    work->eff1A8.field_4 = 0x80;
    work->eff1A8.field_6 = 2;
    work->eff1A8.field_0 = coord;
    work->effOfs         = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->field_8[sc->pad], &work->effOfs, &work->eff1A8);
    *(u32*)G_SCRATCH_HEAD += sizeof(SVECTOR);
}
