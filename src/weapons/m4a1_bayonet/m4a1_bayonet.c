#include "weapons/m4a1_bayonet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "m4a1_bayonet_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// 0x2C-byte scratch `func_m4a1_bayonet_8011D69C` carves off `G_SCRATCH_HEAD`
/// for one ribbon segment: `v` is the quad's four corners, taken from the
/// translation of the two trail coordinates at each end of the segment, `flag`
/// the `gte_stflg` of the projection (negative rejects the quad) and `otz` its
/// `gte_stszotz`, which picks the OT bucket the `POLY_G4` is linked into.
typedef struct _M4a1BayonetBeamScratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ s32     unused;
} M4a1BayonetBeamScratch;
STATIC_ASSERT_SIZEOF(M4a1BayonetBeamScratch, 0x2C);

static void func_m4a1_bayonet_8011D69C(s16 slot, s16 flags);

/// The blade tip's translation inside the muzzle frame, `(0, 0x300, 0x40)`. The
/// hilt's translation follows it directly, and state 0 reaches that as element 1
/// of this array.
static SVECTOR D_m4a1_bayonet_8011DEC8[1] = { { 0, 0x0300, 0x0040, 0 } };

/// The hilt's translation inside the muzzle frame, `(0, 0x180, 0x40)`, directly
/// after the tip. State 0 reaches it as `D_m4a1_bayonet_8011DEC8[1]` and the
/// sweep state names it directly; the two compile to different address
/// arithmetic, so it is an object of its own rather than element 1.
static SVECTOR D_m4a1_bayonet_8011DED0 = { 0, 0x0180, 0x0040, 0 };

/// Per-frame task for the M4A1 bayonet's blade trail. Nothing runs once the
/// effect control is paused (`Gp_State1C->effectControl` non-zero); the task is then released
/// as soon as that phase reaches 4. State 0 places the tip frame at
/// `D_m4a1_bayonet_8011DEC8[0]` under the muzzle and the hilt frame at
/// `[1]` under it, then seeds all sixteen trail slots with that pose. State 1
/// re-poses both frames every frame, writes them into trail slot
/// `age & 7`, re-runs the whole ring so the older slots follow their
/// parents, and hands the ribbon to `func_m4a1_bayonet_8011D69C`. The task
/// lives 13 frames.
void func_m4a1_bayonet_8011D1E4(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    GfxCoord*  slot;
    GfxCoord   hilt;
    s32        phase;
    SVECTOR*   vec;
    s32        vx;
    s32        vy;
    s32        vz;
    s32        i;
    s32        alive;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    phase = Gp_State1C->effectControl;
    if (phase == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        switch (task->state) {
            case 0:
                coord->parent       = work->parent;
                coord->coord.t[0]   = D_m4a1_bayonet_8011DEC8[0].vx;
                coord->coord.t[1]   = D_m4a1_bayonet_8011DEC8[0].vy;
                coord->coord.t[2]   = D_m4a1_bayonet_8011DEC8[0].vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                task->state = 1;

                vx                = D_m4a1_bayonet_8011DEC8[1].vx;
                vec               = &D_m4a1_bayonet_8011DEC8[1];
                vy                = vec->vy;
                vz                = vec->vz;
                hilt.parent       = coord;
                hilt.composeStamp = GRAPHICS_COORD_DIRTY;
                hilt.coord.t[0]   = vx;
                hilt.coord.t[1]   = vy;
                hilt.coord.t[2]   = vz;
                Gp_UpdateCoord(&hilt);

                for (i = 0; i < 8; i++) {
                    slot         = &D_m4a1_bayonet_8012D398[i];
                    slot->parent = &gGfxViewCoord;
                    slot->workm  = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &slot->workm, &slot->coord);

                    slot         = &D_m4a1_bayonet_8012D618[i];
                    slot->parent = &gGfxViewCoord;
                    slot->workm  = hilt.workm;
                    gte_SetRotMatrix(&hilt.workm);
                    gte_SetTransMatrix(&hilt.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &slot->workm, &slot->coord);
                }
                break;
            case 1:
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);

                hilt.parent       = work->parent;
                hilt.composeStamp = GRAPHICS_COORD_DIRTY;
                hilt.coord.t[0]   = D_m4a1_bayonet_8011DED0.vx;
                hilt.coord.t[1]   = D_m4a1_bayonet_8011DED0.vy;
                hilt.coord.t[2]   = D_m4a1_bayonet_8011DED0.vz;
                Gp_UpdateCoord(&hilt);

                slot         = &D_m4a1_bayonet_8012D398[work->age & 7];
                slot->parent = &gGfxViewCoord;
                slot->workm  = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &slot->workm, &slot->coord);

                slot         = &D_m4a1_bayonet_8012D618[work->age & 7];
                slot->parent = &gGfxViewCoord;
                slot->workm  = hilt.workm;
                gte_SetRotMatrix(&hilt.workm);
                gte_SetTransMatrix(&hilt.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &slot->workm, &slot->coord);

                for (i = 0; i < 8; i++) {
                    slot               = &D_m4a1_bayonet_8012D398[i];
                    slot->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(slot);
                    slot               = &D_m4a1_bayonet_8012D618[i];
                    slot->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(slot);
                }
                func_m4a1_bayonet_8011D69C(work->age & 7, 0x112);
                break;
        }
        alive = work->age < 0xD;
    } else {
        alive = phase < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    }
    if (!alive) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws the blade trail as seven Gouraud quads, one per trail slot, walking
/// backwards from `slot`. Each quad spans the tip and hilt coordinates of two
/// adjacent slots and fades out along the ribbon: the leading edge is scaled
/// by `0x40 - 9 * i` and the trailing edge by nine less. `flags` is the trail
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade.
static void func_m4a1_bayonet_8011D69C(s16 slot, s16 flags)
{
    M4a1BayonetBeamScratch* blk;
    GfxCoord*               a;
    GfxCoord*               b;
    POLY_G4*                prim;
    s32                     i;
    s32                     j;
    s32                     i0;
    s32                     i1;
    s32                     hi;
    s32                     lo;
    s32                     fade;

    SCRATCH_PUSH_BYTES(sizeof(M4a1BayonetBeamScratch));
    blk = SCRATCH_HEAD(M4a1BayonetBeamScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_m4a1_bayonet_8012D398[i0];
        blk->v[0].vx = (u16)a->workm.t[0];
        blk->v[0].vy = (u16)a->workm.t[1];
        b            = &D_m4a1_bayonet_8012D618[i0];
        blk->v[0].vz = (u16)a->workm.t[2];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        a            = &D_m4a1_bayonet_8012D398[i1];
        blk->v[1].vz = (u16)b->workm.t[2];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        b            = &D_m4a1_bayonet_8012D618[i1];
        blk->v[2].vz = (u16)a->workm.t[2];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade = 0x40 - i * 9;
            hi   = fade & 0xFF;
            lo   = (fade - 9) & 0xFF;
            setRGB0(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB1(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB2(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            setRGB3(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    SCRATCH_POP_BYTES(sizeof(M4a1BayonetBeamScratch));
}
