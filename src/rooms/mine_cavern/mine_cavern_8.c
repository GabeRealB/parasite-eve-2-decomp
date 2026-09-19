#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The cavern's own ring pass, drawn every frame the effect is up: the same
/// sixteen-wedge ring as `Room_Draw09` but with the cavern's own scratch block
/// and texture. Declared here rather than in `room_common.h` because only this
/// overlay calls it.
extern void func_mine_cavern_801804CC(GsCOORDINATE2* arg0, s16 arg1);

/// Sixteen-wedge gouraud ring, declared locally the way
/// `dryfield_motel_balcony_5.c` does.
extern void Room_Draw09(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);

/// Frame callback for one of the cavern's expanding-ring effects. `Gp_State1C`'s
/// `field_4` gates the whole room-effect family: 1-3 park the effect for the
/// frame and 4 or more tear its work block down, so a task that sees them either
/// returns or releases. Otherwise the effect ticks its lifetime counter, stages
/// `field_24` into an RGB triple, advances the coordinate, and draws the
/// eight-wedge `Room_Draw04` ring at twice `field_26` plus the cavern's own
/// sixteen-wedge one at half-extent `field_26`. Once `field_28` reaches 0x19 the
/// two ramps swap roles - a `Room_Draw09` ring is drawn at `field_2A * 3 / 2` and
/// then `field_28` shrinks by 0x18 and `field_2A` grows by 0x30 - and the effect
/// otherwise fades `field_24` by 0x18 a frame until it drops under 0x18 and the
/// work block is handed back with `Gp_ReleaseState1CMem`.
void func_mine_cavern_80180320(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    u8             sp10[3];
    u16            temp;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        if (task->state == 0) {
            work->field_22 = 1;
            work->field_24 = 0xE0;
            work->field_26 = 0x80;
            work->field_28 = 0xE0;
            work->field_2A = 0x80;
            task->state    = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]        = (u8)work->field_24;
        sp10[1]        = (u8)(work->field_24 >> 1);
        sp10[2]        = (u8)(work->field_24 >> 2);
        temp           = work->field_26 + 0x10;
        work->field_26 = temp;
        Room_Draw04(coord, (s16)(temp * 2), sp10);
        func_mine_cavern_801804CC(coord, (s16)work->field_26);
        if ((s16)work->field_28 >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->field_28;
            sp10[1] = (u8)(work->field_28 >> 1);
            sp10[2] = (u8)(work->field_28 >> 2);
            temp_a1 = (s16)work->field_2A * 3;
            Room_Draw09(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->field_28 -= 0x18;
            work->field_2A += 0x30;
            return;
        }
        temp           = work->field_24 - 0x18;
        work->field_24 = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_8", func_mine_cavern_801804CC);
