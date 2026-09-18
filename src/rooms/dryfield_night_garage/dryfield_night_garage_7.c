#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "rooms/room_common.h"

/// The garage's two point-pair runs, 8-byte `SVECTOR`s laid back to back from
/// `801833A4`: four pairs the visit-3/15 case sweeps (`A4[0]`, `A4[2]`, `A4[4]`,
/// `A4[6]`), of which the last two are also what visit 11 sweeps from
/// `801833D4`. `801833D4` is named separately because visit 11 reaches it by
/// name where the visit-3/15 case reaches the same address as `A4[6]` -
/// indexing emits the base plus 0x30, naming it emits its own `lui`.
extern SVECTOR D_dryfield_night_garage_801833A4[];
extern SVECTOR D_dryfield_night_garage_801833D4;

/// Garage room draw: sweeps the glowing strip the current visit
/// (`gGameSession->field_4`) selects. Visits 3 and 15 sweep all four of the
/// room's run, 7 and 14 only its first pair, and 11 the last pair of the run
/// with its own blend (`arg2` 0x800 instead of 0). Each case names its own last
/// draw, which `jump.c` cross-jumps into one tail block after the last case.
void func_dryfield_night_garage_80181518(void)
{
    switch (gGameSession->field_4) {
        case 3:
        case 15: {
            SVECTOR* p = D_dryfield_night_garage_801833A4;
            Room_Draw34(&p[0], 0x200, 0);
            Room_Draw34(&p[2], 0x200, 0);
            Room_Draw34(&p[4], 0x200, 0);
            Room_Draw34(&p[6], 0x200, 0);
            break;
        }
        case 7:
        case 14:
            Room_Draw34(&D_dryfield_night_garage_801833A4[0], 0x200, 0);
            break;
        case 11: {
            SVECTOR* p = &D_dryfield_night_garage_801833D4;
            Room_Draw34(&p[0], 0x200, 0x800);
            Room_Draw34(&p[2], 0x200, 0x800);
            break;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage_7", RoomsShared8017eb5cIdList);
