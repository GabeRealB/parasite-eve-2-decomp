#ifndef ACTORS_SHARED_80166B20_H
#define ACTORS_SHARED_80166B20_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Counts `field_412` up against the `field_446` hold and enters state 2 once
/// it runs out; over its last 0x30 frames, eases the yaw `field_424` back to
/// zero. Before that, while `field_43A` (the distance to the nearer player
/// actor) is under 0xDAC and the heading `field_444` is outside 0x3C0..0xC40,
/// eases the yaw toward it and enters state 3 (arming `Gp_ArmStateF0`) after
/// 16 such frames; otherwise swings it toward +-0x380 on bit 6 of
/// `field_442`. Shared by `actor_341700` and `actor_342400`, whose work blocks
/// are the same `ActorsShared80168d3cWork`.
void ActorsShared80166b20(Task* arg0);

#endif
