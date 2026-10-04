#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/gpu_image_upload.h"
#include "main/tmd_types.h"

/// Texture animation frame lists, six per costume package. Each is a
/// NULL-terminated run of `GpuImageUpload` upload lists, one per frame; the player's
/// task uploads the frames in turn over a fixed rectangle of the costume
/// model's texture page. Gameplay's two frame tables hold their addresses:
/// the first four lists of a package belong to the first table, the last two
/// to the second.
extern GpuImageUpload* D_aya_10200_8011D094[];

extern GpuImageUpload* D_aya_10200_8011D09C[];

extern GpuImageUpload* D_aya_10200_8011D0AC[];

extern GpuImageUpload* D_aya_10200_8011D0BC[];

extern GpuImageUpload* D_aya_10200_8011D0D4[];

extern GpuImageUpload* D_aya_10200_8011D0DC[];

static TmdBone _gAya10200Model0019CSkeleton[1] = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

static u32 _gAya10200Model0019CPartVerts[1] = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

static SVECTOR _gAya10200Model0019CVerts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

static SVECTOR _gAya10200Model0019CNormals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

static u32 _gAya10200Model0019CStream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource D_aya_10200_80115B90 = {
    0,
    1120,
    0,
    1,
    _gAya10200Model0019CPartVerts,
    _gAya10200Model0019CVerts,
    _gAya10200Model0019CNormals,
    _gAya10200Model0019CSkeleton,
    _gAya10200Model0019CStream,
};

static TmdBone _gAya10200Model0019CAt00860Skeleton[1] = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

static u32 _gAya10200Model0019CAt00860PartVerts[1] = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

static SVECTOR _gAya10200Model0019CAt00860Verts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

static SVECTOR _gAya10200Model0019CAt00860Normals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

static u32 _gAya10200Model0019CAt00860Stream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource D_aya_10200_80115FD0 = {
    0,
    1120,
    0,
    1,
    _gAya10200Model0019CAt00860PartVerts,
    _gAya10200Model0019CAt00860Verts,
    _gAya10200Model0019CAt00860Normals,
    _gAya10200Model0019CAt00860Skeleton,
    _gAya10200Model0019CAt00860Stream,
};

static TmdBone _gAya10200Model00A5CSkeleton[1] = {
#include "assets/aya_10200_model_00A5C_skeleton.inc"
};

static u32 _gAya10200Model00A5CPartVerts[1] = {
#include "assets/aya_10200_model_00A5C_partVerts.inc"
};

static SVECTOR _gAya10200Model00A5CVerts[27] = {
#include "assets/aya_10200_model_00A5C_verts.inc"
};

static SVECTOR _gAya10200Model00A5CNormals[27] = {
#include "assets/aya_10200_model_00A5C_normals.inc"
};

static u32 _gAya10200Model00A5CStream[189] = {
#include "assets/aya_10200_model_00A5C_stream.inc"
};

TmdSource D_aya_10200_801164C0 = {
    0,
    1328,
    0,
    1,
    _gAya10200Model00A5CPartVerts,
    _gAya10200Model00A5CVerts,
    _gAya10200Model00A5CNormals,
    _gAya10200Model00A5CSkeleton,
    _gAya10200Model00A5CStream,
};

static TmdBone _gAya10200Model00F0CSkeleton[1] = {
#include "assets/aya_10200_model_00F0C_skeleton.inc"
};

static u32 _gAya10200Model00F0CPartVerts[1] = {
#include "assets/aya_10200_model_00F0C_partVerts.inc"
};

static SVECTOR _gAya10200Model00F0CVerts[23] = {
#include "assets/aya_10200_model_00F0C_verts.inc"
};

static SVECTOR _gAya10200Model00F0CNormals[23] = {
#include "assets/aya_10200_model_00F0C_normals.inc"
};

static u32 _gAya10200Model00F0CStream[161] = {
#include "assets/aya_10200_model_00F0C_stream.inc"
};

TmdSource D_aya_10200_80116900 = {
    0,
    1120,
    0,
    1,
    _gAya10200Model00F0CPartVerts,
    _gAya10200Model00F0CVerts,
    _gAya10200Model00F0CNormals,
    _gAya10200Model00F0CSkeleton,
    _gAya10200Model00F0CStream,
};

static TmdBone _gAya10200Model02B58Skeleton[19] = {
#include "assets/aya_10200_model_02B58_skeleton.inc"
};

static u32 _gAya10200Model02B58PartVerts[19] = {
#include "assets/aya_10200_model_02B58_partVerts.inc"
};

static SVECTOR _gAya10200Model02B58Verts[352] = {
#include "assets/aya_10200_model_02B58_verts.inc"
};

static SVECTOR _gAya10200Model02B58Normals[373] = {
#include "assets/aya_10200_model_02B58_normals.inc"
};

static u32 _gAya10200Model02B58Stream[4051] = {
#include "assets/aya_10200_model_02B58_stream.inc"
};

TmdSource D_aya_10200_8011C210 = {
    0,
    22344,
    5824,
    19,
    _gAya10200Model02B58PartVerts,
    _gAya10200Model02B58Verts,
    _gAya10200Model02B58Normals,
    _gAya10200Model02B58Skeleton,
    _gAya10200Model02B58Stream,
};

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long _gAya10200Image06AC4[] = {
#include "assets/aya_10200_image_06AC4.inc"
};
static GpuImageUpload D_aya_10200_8011C554[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10200Image06AC4 },
    { GP_IMG_REC_END },
};

static u_long _gAya10200Image06E04[] = {
#include "assets/aya_10200_image_06E04.inc"
};
static GpuImageUpload D_aya_10200_8011C894[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10200Image06E04 },
    { GP_IMG_REC_END },
};

static u_long _gAya10200Image07144[] = {
#include "assets/aya_10200_image_07144.inc"
};
static GpuImageUpload D_aya_10200_8011CBD4[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10200Image07144 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10200_8011CBF4[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpuImageUpload D_aya_10200_8011CE24[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, D_aya_10200_8011CBF4 },
    { GP_IMG_REC_END },
};

/* Byte-identical to the image before it, so both come from one asset. */
static u_long D_aya_10200_8011CE44[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpuImageUpload D_aya_10200_8011D074[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, D_aya_10200_8011CE44 },
    { GP_IMG_REC_END },
};

GpuImageUpload* D_aya_10200_8011D094[] = { D_aya_10200_8011C554, NULL };
GpuImageUpload* D_aya_10200_8011D09C[] = { D_aya_10200_8011C554, D_aya_10200_8011C894, D_aya_10200_8011CBD4, NULL };
GpuImageUpload* D_aya_10200_8011D0AC[] = { D_aya_10200_8011CBD4, D_aya_10200_8011C894, D_aya_10200_8011C554, NULL };
GpuImageUpload* D_aya_10200_8011D0BC[] = {
    D_aya_10200_8011C554,
    D_aya_10200_8011C894,
    D_aya_10200_8011CBD4,
    D_aya_10200_8011C894,
    D_aya_10200_8011C554,
    NULL,
};
GpuImageUpload* D_aya_10200_8011D0D4[] = { D_aya_10200_8011CE24, NULL };
GpuImageUpload* D_aya_10200_8011D0DC[] = { D_aya_10200_8011D074, NULL };
