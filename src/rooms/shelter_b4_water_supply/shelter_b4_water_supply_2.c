#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc   D_shelter_b4_water_supply_80182620[];
extern GpMsgEntry D_shelter_b4_water_supply_801825F0[];
extern TaskDesc   D_shelter_b4_water_supply_8018263C[];
void              func_shelter_b4_water_supply_8017DB18(void);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017DDFC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017DE74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017E5D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017ED28);
