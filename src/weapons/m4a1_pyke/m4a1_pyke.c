#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/m4a1_pyke.h"

extern s32 Gp_LcgState;

/// Per-frame beam task for the M4A1 Pyke. Nothing runs while the player model
/// is hidden (`field_C & 0x80`) or the room is fading out
/// (`Gp_State1C->field_4 >= 2`). State 0 hangs the task's own coordinate off
/// `field_8` at the fixed muzzle offset with an identity rotation; state 1 then
/// dispatches on `spawnArg1`:
///
/// - 1 draws the beam head at `workm.t` every frame and claims room-light slot
///   1 as a narrow (`0x80` / `0x400`) light aimed at a random angle in
///   `0x400..0xB00`, arming the flare width in `field_24`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x6017F`
///   as a child of this task, and re-claims the light with a much wider
///   (`0x400` / `0x4000`) falloff and a `0x800..0xF00` angle.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `Gp_State1C->field_4` is non-zero the two drawing sub-states wind
/// `field_22` back down instead of advancing.
void func_m4a1_pyke_8011D1F8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GsCOORDINATE2* light;
    GpMtxWords*    rot;
    GpEffWork*     eff;
    u32            ang;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    base  = &Gp_RoomCoords[1];
    light = &base->coord;
    slot  = (GpCoordTail*)light;
    if ((((GpActorWork*)Game_GetPtrSlot(3))->extra->field_C & 0x80) != 0) {
        return;
    }
    if (Gp_State1C->field_4 >= 2) {
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = work->field_8;
            rot->w0           = 0x1000;
            rot->w1           = 0;
            rot->w2           = 0x1000;
            rot->w3           = 0;
            rot->h4           = 0x1000;
            coord->coord.t[0] = D_m4a1_pyke_8011E90C.vx;
            coord->coord.t[1] = D_m4a1_pyke_8011E90C.vy;
            coord->coord.t[2] = D_m4a1_pyke_8011E90C.vz;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            task->state = 1;
            break;
        case 1:
            switch (task->spawnArg1) {
                case 0:
                    break;
                case 1:
                    if (Gp_State1C->field_4 != 0) {
                        work->field_22--;
                        func_m4a1_pyke_8011D548(
                            (VECTOR3*)&coord->workm.t, work->field_22, 0x80);
                        break;
                    }
                    Gp_UpdateCoord(coord);
                    func_m4a1_pyke_8011D548((VECTOR3*)&coord->workm.t, work->field_22, 0x80);
                    base->field_0  = 4;
                    slot->field_58 = 0x80;
                    slot->field_5C = 0x400;
                    ang            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState    = ang;
                    /* `field_52` reads the halfword back as unsigned and
                       `field_54` as signed, so the two shifts come off the same
                       register: a plain `ang >> 1` / `ang >> 2` pair would drop
                       the sign-extension the ROM keeps for `field_54`. */
                    slot->field_50 = ((ang >> 16) & 0x700) + 0x400;
                    slot->field_52 = (u16)slot->field_50 >> 1;
                    slot->field_54 = slot->field_50 >> 2;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                    light->flg     = 0;
                    work->field_24 = 0x40;
                    break;
                case 2:
                    if (Gp_State1C->field_4 != 0) {
                        work->field_22--;
                        break;
                    }
                    Gp_UpdateCoord(coord);
                    if (work->field_24 < 0x180) {
                        work->field_24 = (u16)work->field_24 + 0x40;
                    }
                    eff = Gp_SpawnEff(0x6017F, coord, work->field_24, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->field_0);
                    }
                    base->field_0  = 4;
                    slot->field_58 = 0x400;
                    slot->field_5C = 0x4000;
                    ang            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState    = ang;
                    slot->field_50 = ((ang >> 16) & 0x700) + 0x800;
                    slot->field_52 = (u16)slot->field_50 >> 1;
                    slot->field_54 = slot->field_50 >> 2;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                    light->flg = 0;
                    break;
                case 3:
                    task->spawnArg1 = 1;
                    break;
                case 4:
                    task->spawnArg1 = 0;
                    break;
                case 5:
                    Gp_ReleaseState1CMem(work, task);
                    break;
            }
            break;
    }
}

INCLUDE_ASM("weapons/nonmatchings/m4a1_pyke/m4a1_pyke", func_m4a1_pyke_8011D548);

INCLUDE_ASM("weapons/nonmatchings/m4a1_pyke/m4a1_pyke", func_m4a1_pyke_8011D7D4);

INCLUDE_ASM("weapons/nonmatchings/m4a1_pyke/m4a1_pyke", func_m4a1_pyke_8011DCEC);

INCLUDE_ASM("weapons/nonmatchings/m4a1_pyke/m4a1_pyke", func_m4a1_pyke_8011E168);
