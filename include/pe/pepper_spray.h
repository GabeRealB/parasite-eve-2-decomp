#ifndef PE_PEPPER_SPRAY_H
#define PE_PEPPER_SPRAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>
#include "main/coord.h"
#include "gameplay/3FB8.h"
#include "overlay.h"

/// The six spray-cone yaws, refilled once per cast by
/// `func_pepper_spray_8012EF34` from `Gp_LcgState`: entry `i` is a 0x400-wide
/// draw offset into the quadrant `i & 3`, so the six quads fan around the
/// nozzle. `func_pepper_spray_8012F634` draws one quad per entry every frame.
extern s16 D_pepper_spray_8012FB9C[6];

#endif /* PE_PEPPER_SPRAY_H */
