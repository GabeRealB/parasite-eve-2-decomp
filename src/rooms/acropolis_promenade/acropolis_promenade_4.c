#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern SVECTOR D_acropolis_promenade_80181AFC[];
extern SVECTOR D_acropolis_promenade_80181B0C[];
extern SVECTOR D_acropolis_promenade_80181B14[];
extern u16     D_acropolis_promenade_80181B74;
extern u16     D_acropolis_promenade_80181B76;
extern u16     D_acropolis_promenade_80181B78[];

/// Per-frame effect spawner for the promenade. `D_acropolis_promenade_80181B74`
/// / `_80181B76` and the twelve-entry mask table `_80181B78` are per-view bit
/// masks: bit `view - 1` of an entry says whether that emitter is visible from
/// the camera `Gp_GetViewIndex` reports, and the parallel twelve-entry
/// `_80181B14` array holds each emitter's offset from the room's coordinate
/// frame. View 7 spawns nothing.
void func_acropolis_promenade_8017E03C(Task* task)
{
    GsCOORDINATE2* coord;
    RoomEffWork*   work;
    u8             view;
    s32            i;
    s32            mask;
    s16            prev;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    view  = Gp_GetViewIndex();
    if (Gp_State1C->field_4 >= 4) {
        return;
    }
    work->field_22++;
    if (view == 7) {
        return;
    }
    mask = 1 << (view - 1);
    if (D_acropolis_promenade_80181B74 & mask) {
        Gp_SpawnEff(0x8006004B, coord, (s16)work->field_22, &D_acropolis_promenade_80181AFC[0]);
        Gp_SpawnEff(0x8006004B, coord, (s16)work->field_22, &D_acropolis_promenade_80181AFC[1]);
        Gp_SpawnEff(0x60057, coord, (s16)work->field_22, &D_acropolis_promenade_80181B0C[0]);
        Room_Draw21(&D_acropolis_promenade_80181AFC[-1], 0x100, 0x5C40);
    }
    for (i = 0; i < 3; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 0, &D_acropolis_promenade_80181B14[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i]);
        }
        if (D_acropolis_promenade_80181B78[i + 2] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 2]);
        }
        if (D_acropolis_promenade_80181B78[i + 4] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i + 4]);
        }
        if (D_acropolis_promenade_80181B78[i + 6] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 6]);
        }
    }
    if (D_acropolis_promenade_80181B78[11] & mask) {
        Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[11]);
    }
    if (D_acropolis_promenade_80181B76 & mask) {
        prev = work->field_24;
        if (prev != view) {
            for (i = 0; i < 0x28; i++) {
                Gp_SpawnEff(0x60056, coord, view, NULL);
            }
        } else {
            Gp_SpawnEff(0x60056, coord, prev, NULL);
            Gp_SpawnEff(0x60056, coord, prev, NULL);
        }
    }
    work->field_24 = view;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_4", func_acropolis_promenade_8017E394);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_4", func_acropolis_promenade_8017E634);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_4", func_acropolis_promenade_8017ED44);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_4", func_acropolis_promenade_8017F0BC);
