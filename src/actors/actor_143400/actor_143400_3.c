#include "common.h"
#include "gameplay/D4.h"

extern GpAreaApplyRec D_8018A638;

/// Applies the 0xFF-terminated area record list at `D_8018A638` through
/// `Gp_ApplyAreaRecs`. It is reached only through the function pointers in
/// the actor's data.
void func_actor_143400_80131E6C(void)
{
    Gp_ApplyAreaRecs(&D_8018A638);
}
