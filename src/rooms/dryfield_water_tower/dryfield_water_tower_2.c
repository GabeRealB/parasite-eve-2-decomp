#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern u8 D_8007216C;
extern u8 D_801153F4;

extern u32   D_dryfield_water_tower_8018768C;
extern Task* D_dryfield_water_tower_801876A0;

void func_dryfield_water_tower_8017DCB4(void);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017DE30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017DFAC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017E1DC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017E428);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017E5B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017E764);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017E93C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017EB7C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017F128);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_2", func_dryfield_water_tower_8017F700);
