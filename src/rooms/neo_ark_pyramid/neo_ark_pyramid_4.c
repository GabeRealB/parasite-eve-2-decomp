#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

void func_neo_ark_pyramid_8017DBF0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758          = 0x601E2;
        D_8011572C          = 0x601FE;
        D_80115750          = 0x6021A;
        Gp_State1C->field_A = 2;
        arg0->state         = 1;
    }
}
