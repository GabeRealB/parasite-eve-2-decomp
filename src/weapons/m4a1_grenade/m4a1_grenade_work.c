#include "m4a1_grenade_private.h"

#include "types.h"
#include "main/tmd_types.h"

/// Sits in the middle of this package's trailing data, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link between the two runs of split data around it.
u16 D_m4a1_grenade_8012E08C[4] = { 0x1F4, 0x4B0, 0x7D0, 0 };

static TmdBone _gM4a1GrenadeModel10F7CSkeleton[1] = {
#include "assets/m4a1_grenade_model_10F7C_skeleton.inc"
};

static u32 _gM4a1GrenadeModel10F7CPartVerts[1] = {
#include "assets/m4a1_grenade_model_10F7C_partVerts.inc"
};

static SVECTOR _gM4a1GrenadeModel10F7CVerts[8] = {
#include "assets/m4a1_grenade_model_10F7C_verts.inc"
};

static SVECTOR _gM4a1GrenadeModel10F7CNormals[8] = {
#include "assets/m4a1_grenade_model_10F7C_normals.inc"
};

static u32 _gM4a1GrenadeModel10F7CStream[48] = {
#include "assets/m4a1_grenade_model_10F7C_stream.inc"
};

TmdSource D_m4a1_grenade_8012E1FC = {
    0,
    312,
    0,
    1,
    _gM4a1GrenadeModel10F7CPartVerts,
    _gM4a1GrenadeModel10F7CVerts,
    _gM4a1GrenadeModel10F7CNormals,
    _gM4a1GrenadeModel10F7CSkeleton,
    _gM4a1GrenadeModel10F7CStream,
};
