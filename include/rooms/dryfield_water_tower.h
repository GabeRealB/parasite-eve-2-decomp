#ifndef ROOMS_DRYFIELD_WATER_TOWER_H
#define ROOMS_DRYFIELD_WATER_TOWER_H

#include "common.h"

#include "main/task.h"

/// The room's task table at 0x80182384: entry 0 is the cap script
/// `func_dryfield_water_tower_8017F128`, which the room's entry task spawns,
/// entry 1 the prop `func_dryfield_water_tower_8017E764` and entry 2 the prop
/// `func_dryfield_water_tower_8017E1DC`, the two the cap script spawns.
extern TaskDesc D_dryfield_water_tower_80182384[];

/// Shows or hides the view's sprites from nibble 0x55.
void func_dryfield_water_tower_8017DCB4(void);

/// Sets the current view's skip-OT-link byte: a zero low byte skips the view's
/// sprites, non-zero draws them.
void func_dryfield_water_tower_801802D8(u8 arg0);

#endif // ROOMS_DRYFIELD_WATER_TOWER_H
