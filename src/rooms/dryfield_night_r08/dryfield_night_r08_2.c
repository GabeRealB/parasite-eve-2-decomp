#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_dryfield_night_r08_8018056C[];
extern SVECTOR D_dryfield_night_r08_801805AC[];
extern SVECTOR D_dryfield_night_r08_801805BC[];
extern SVECTOR D_dryfield_night_r08_801805CC[];
extern SVECTOR D_dryfield_night_r08_801805DC[];
extern SVECTOR D_dryfield_night_r08_80180664[];

/// On the task's first tick, stores three fixed ids into `D_80115758`,
/// `D_8011572C` and `D_80115750`, then draws the placements the current camera
/// view shows with `Room_Draw12` and `Room_Draw20`. The placement names are
/// windows onto one run of 8-byte `SVECTOR`s, so `80180664` is `805BC[21]`,
/// `805AC[23]` and `805CC[19]` as well; views 3 and 9 name it directly and the
/// compiler merges their last two calls into one tail.
void func_dryfield_night_r08_8017D718(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601C7;
        D_8011572C  = 0x601E3;
        D_80115750  = 0x601FF;
        arg0->state = 1;
    }

    switch (gGameSession->at4.loc.view) {
        case 3:
            Room_Draw12(&D_dryfield_night_r08_801805BC[0], 0x200, 0x800, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805BC[2], 0x200, 0, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805BC[4], 0x200, 0, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805BC[6], 0x200, 0x800, 0x100);
            Room_Draw20(&D_dryfield_night_r08_801805BC[11], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[12], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[13], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[14], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[18], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[19], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[20], 1, 0x300);
            Room_Draw20(D_dryfield_night_r08_80180664, 1, 0x300);
            break;
        case 2:
        case 5:
            Room_Draw12(&D_dryfield_night_r08_8018056C[0], 0x200, 0x800, 0x111);
            Room_Draw12(&D_dryfield_night_r08_8018056C[2], 0x200, 0x800, 0x111);
            Room_Draw12(&D_dryfield_night_r08_8018056C[4], 0x200, 0, 0x111);
            Room_Draw12(&D_dryfield_night_r08_8018056C[6], 0x200, 0, 0x111);
            break;
        case 4:
            Room_Draw12(&D_dryfield_night_r08_801805BC[0], 0x200, 0x800, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805BC[4], 0x200, 0, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805BC[6], 0x200, 0x800, 0x100);
            Room_Draw20(&D_dryfield_night_r08_801805BC[14], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[20], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805BC[21], 1, 0x300);
            break;
        case 6:
            Room_Draw12(&D_dryfield_night_r08_801805AC[0], 0x200, 0x800, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805AC[2], 0x200, 0x800, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805AC[6], 0x200, 0, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805AC[8], 0x200, 0x800, 0x100);
            Room_Draw20(&D_dryfield_night_r08_801805AC[13], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805AC[14], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805AC[15], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805AC[16], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805AC[20], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805AC[21], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805AC[22], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805AC[23], 1, 0x300);
            break;
        case 7:
            Room_Draw12(D_dryfield_night_r08_801805AC, 0x200, 0x800, 0x10);
            break;
        case 8:
            Room_Draw12(&D_dryfield_night_r08_801805CC[0], 0x200, 0, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805CC[2], 0x200, 0, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805CC[4], 0x200, 0x800, 0x100);
            Room_Draw20(&D_dryfield_night_r08_801805CC[9], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805CC[10], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805CC[11], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805CC[12], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805CC[16], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805CC[17], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805CC[18], 1, 0x300);
            Room_Draw20(&D_dryfield_night_r08_801805CC[19], 1, 0x300);
            break;
        case 9:
            Room_Draw12(&D_dryfield_night_r08_801805DC[0], 0x200, 0, 0x10);
            Room_Draw12(&D_dryfield_night_r08_801805DC[2], 0x200, 0x800, 0x100);
            Room_Draw20(&D_dryfield_night_r08_801805DC[10], 1, 0x300);
            Room_Draw20(D_dryfield_night_r08_80180664, 1, 0x300);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_r08/dryfield_night_r08_2", D_dryfield_night_r08_8017D5F0);
