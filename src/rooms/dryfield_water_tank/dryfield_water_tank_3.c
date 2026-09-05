#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc       D_dryfield_water_tank_8017F34C;
extern s32            D_dryfield_water_tank_8017F114;
extern s32            D_dryfield_water_tank_8017F21C;
extern s32            D_dryfield_water_tank_80184E0C;
extern s32            D_dryfield_water_tank_801859DC;
extern GpAreaApplyRec D_dryfield_water_tank_80188D1C[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017DB48);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017DB98);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017DD20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017DEA4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E0B4);
