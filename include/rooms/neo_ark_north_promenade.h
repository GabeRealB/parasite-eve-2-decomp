#ifndef ROOMS_NEO_ARK_NORTH_PROMENADE_H
#define ROOMS_NEO_ARK_NORTH_PROMENADE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Draw helpers the room's glow effects call each frame on the effect's
/// coordinate frame, with a frame counter and, where present, an RGB tint.
void func_neo_ark_north_promenade_8017E164(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_neo_ark_north_promenade_8017EA3C(GsCOORDINATE2* coord, s16 frame);
void func_neo_ark_north_promenade_8017F2E0(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_neo_ark_north_promenade_801804A4(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_neo_ark_north_promenade_801813A8(GsCOORDINATE2* coord, s16 frame, u8* rgb);

#endif // ROOMS_NEO_ARK_NORTH_PROMENADE_H
