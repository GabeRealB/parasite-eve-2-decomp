#include "common.h"

#include "gameplay/3CD8.h"

/// Native call in the power-on event script: halts the pad scripts.
void func_neo_ark_power_plant_1_8017D908(void)
{
    Gp_HaltPadScripts();
}
