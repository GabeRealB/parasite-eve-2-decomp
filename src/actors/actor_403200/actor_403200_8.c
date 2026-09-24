#include "common.h"

#include "actors/actor_403200.h"
#include "actors/actor_403200_view.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "gte.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern MATRIX* D_80073B8C;
/// Global freeze flag: 1 while the game is halted, which stops the per-frame
/// body below from walking its model out.
extern u8 D_80072729;
/// The script pair the per-frame body's two one-shot sound cues spawn.
extern s32 D_actor_403200_80141C5C;
extern s32 D_actor_403200_80141C64;
/// Non-zero while the overlay is shutting down, which is what makes the spawn
/// state below tear its enemy down instead of standing it up.
extern s16 D_actor_403200_80141C50;
/// LCG state the spawn state below rolls a random yaw out of.
extern u32 Gp_LcgState;

static __inline__ void Actor403200_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head       = (u8*)SCRATCH_SP;
    dir        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_SP = (u32)dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x19);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    SCRATCH_SP = (u32)((u8*)SCRATCH_SP + sizeof(SVECTOR));
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_8", func_actor_403200_80133DD8);
