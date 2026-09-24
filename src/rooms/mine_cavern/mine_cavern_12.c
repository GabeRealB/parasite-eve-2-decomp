#include "common.h"

/// One byte of gameplay state. Read back with `lb` elsewhere, so it is signed.
extern s8 D_8007272D;

/// Room script callback: stores its argument into `D_8007272D`.
void func_mine_cavern_8017E150(s8 arg0)
{
    D_8007272D = arg0;
}
