#include "common.h"

/// One byte of gameplay state. Read back with `lb` elsewhere, so it is signed.
extern s8 D_8007272D;

/// Room script callback, named by two of the room's script records (command
/// 0xD, argument 5): stores its argument into `D_8007272D`.
void func_dryfield_junk_yard_8017DC54(s8 arg0)
{
    D_8007272D = arg0;
}
