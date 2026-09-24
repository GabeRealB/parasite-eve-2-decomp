#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_motel_balcony.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

/// On its first tick, stores the room's seven effect ids into the gameplay
/// slots `D_80115720`..`D_80115758`, then idles.
void func_dryfield_motel_balcony_8017DC28(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60282;
        D_80115744  = 0x60283;
        D_8011573C  = 0x60284;
        D_80115720  = 0x60285;
        D_80115758  = 0x60005;
        D_8011572C  = 0x60073;
        D_80115750  = 0x60074;
        arg0->state = 1;
    }
}
