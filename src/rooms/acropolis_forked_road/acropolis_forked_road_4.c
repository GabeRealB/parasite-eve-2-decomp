#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

/// The fourteen spawn offsets of the forked road's ambient effects, indexed
/// 0..13 by the first-frame burst below.
extern SVECTOR D_acropolis_forked_road_80182178[14];

/// Forked-road ambient effect task. On its first frame it fires one effect per
/// entry of `D_acropolis_forked_road_80182178`, in four runs that differ only
/// in the flavour bits added to the entry's index - two 0x02000000, two
/// 0x03000000, eight 0x02000100 and two 0x00000200 - and then publishes the
/// room's three ambient sound events before marking itself done.
void func_acropolis_forked_road_8017E298(Task* task)
{
    GsCOORDINATE2* coord;
    s32            i;

    coord = ((TmdObject*)task->extra)->field_8;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 2; i < 4; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x3000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 4; i < 0xC; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000100, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 0xC; i < 0xE; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x200, &D_acropolis_forked_road_80182178[i]);
        }
        D_80115758  = 0x60290;
        D_8011572C  = 0x60291;
        D_80115750  = 0x60292;
        task->state = task->state + 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_4", func_acropolis_forked_road_8017E410);
