#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_motel_balcony.h"

extern const TaskFuncTable3 RoomsShared8017d878Table;
void                        func_dryfield_motel_balcony_8017DBC8(void);
extern s32                  D_80115720;
extern s32                  D_80115728;
extern s32                  D_8011572C;
extern s32                  D_8011573C;
extern s32                  D_80115744;
extern s32                  D_80115750;
extern s32                  D_80115758;

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony_3", func_dryfield_motel_balcony_8017DCB8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony_3", func_dryfield_motel_balcony_8017DF84);
