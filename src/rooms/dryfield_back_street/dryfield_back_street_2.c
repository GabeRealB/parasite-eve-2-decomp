#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_back_street_8017F964[];
extern TaskDesc   D_dryfield_back_street_8017F98C[];

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

void func_dryfield_back_street_8017D970(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x60296;
        D_8011572C  = 0x60297;
        D_80115750  = 0x60298;
        arg0->state = 1;
    }
    Gp_State1C->field_A = 2;
}
