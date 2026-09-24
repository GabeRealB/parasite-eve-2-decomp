#include "common.h"

extern s8 D_8011540A;

/// Script callback: stores its argument in `D_8011540A`.
void func_mine_gorge_8017D8C8(s32 arg0)
{
    D_8011540A = arg0;
}
