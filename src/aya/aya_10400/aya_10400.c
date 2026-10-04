#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/gpu_image_upload.h"
#include "main/tmd_types.h"

extern GpuImageUpload* D_aya_10400_8011CC94[];

extern GpuImageUpload* D_aya_10400_8011CC9C[];

extern GpuImageUpload* D_aya_10400_8011CCAC[];

extern GpuImageUpload* D_aya_10400_8011CCBC[];

extern GpuImageUpload* D_aya_10400_8011CCD4[];

extern GpuImageUpload* D_aya_10400_8011CCDC[];

static TmdBone _gAya10400Model01A48Skeleton[19] = {
#include "assets/aya_10400_model_01A48_skeleton.inc"
};

static u32 _gAya10400Model01A48PartVerts[19] = {
#include "assets/aya_10400_model_01A48_partVerts.inc"
};

static SVECTOR _gAya10400Model01A48Verts[362] = {
#include "assets/aya_10400_model_01A48_verts.inc"
};

static SVECTOR _gAya10400Model01A48Normals[383] = {
#include "assets/aya_10400_model_01A48_normals.inc"
};

static u32 _gAya10400Model01A48Stream[4017] = {
#include "assets/aya_10400_model_01A48_stream.inc"
};

TmdSource D_aya_10400_8011B078 = {
    0,
    22360,
    5800,
    19,
    _gAya10400Model01A48PartVerts,
    _gAya10400Model01A48Verts,
    _gAya10400Model01A48Normals,
    _gAya10400Model01A48Skeleton,
    _gAya10400Model01A48Stream,
};

static TmdBone _gAya10400Model05AC4Skeleton[1] = {
#include "assets/aya_10400_model_05AC4_skeleton.inc"
};

static u32 _gAya10400Model05AC4PartVerts[1] = {
#include "assets/aya_10400_model_05AC4_partVerts.inc"
};

static SVECTOR _gAya10400Model05AC4Verts[23] = {
#include "assets/aya_10400_model_05AC4_verts.inc"
};

static SVECTOR _gAya10400Model05AC4Normals[23] = {
#include "assets/aya_10400_model_05AC4_normals.inc"
};

static u32 _gAya10400Model05AC4Stream[166] = {
#include "assets/aya_10400_model_05AC4_stream.inc"
};

TmdSource D_aya_10400_8011B4CC = {
    0,
    1148,
    0,
    1,
    _gAya10400Model05AC4PartVerts,
    _gAya10400Model05AC4Verts,
    _gAya10400Model05AC4Normals,
    _gAya10400Model05AC4Skeleton,
    _gAya10400Model05AC4Stream,
};

static TmdBone _gAya10400Model05F58Skeleton[1] = {
#include "assets/aya_10400_model_05F58_skeleton.inc"
};

static u32 _gAya10400Model05F58PartVerts[1] = {
#include "assets/aya_10400_model_05F58_partVerts.inc"
};

static SVECTOR _gAya10400Model05F58Verts[27] = {
#include "assets/aya_10400_model_05F58_verts.inc"
};

static SVECTOR _gAya10400Model05F58Normals[27] = {
#include "assets/aya_10400_model_05F58_normals.inc"
};

static u32 _gAya10400Model05F58Stream[189] = {
#include "assets/aya_10400_model_05F58_stream.inc"
};

TmdSource D_aya_10400_8011B9BC = {
    0,
    1328,
    0,
    1,
    _gAya10400Model05F58PartVerts,
    _gAya10400Model05F58Verts,
    _gAya10400Model05F58Normals,
    _gAya10400Model05F58Skeleton,
    _gAya10400Model05F58Stream,
};

static TmdBone _gAya10400Model06408Skeleton[1] = {
#include "assets/aya_10400_model_06408_skeleton.inc"
};

static u32 _gAya10400Model06408PartVerts[1] = {
#include "assets/aya_10400_model_06408_partVerts.inc"
};

static SVECTOR _gAya10400Model06408Verts[23] = {
#include "assets/aya_10400_model_06408_verts.inc"
};

static SVECTOR _gAya10400Model06408Normals[23] = {
#include "assets/aya_10400_model_06408_normals.inc"
};

static u32 _gAya10400Model06408Stream[166] = {
#include "assets/aya_10400_model_06408_stream.inc"
};

TmdSource D_aya_10400_8011BE10 = {
    0,
    1148,
    0,
    1,
    _gAya10400Model06408PartVerts,
    _gAya10400Model06408Verts,
    _gAya10400Model06408Normals,
    _gAya10400Model06408Skeleton,
    _gAya10400Model06408Stream,
};

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long _gAya10400Image066C4[] = {
#include "assets/aya_10400_image_066C4.inc"
};
static GpuImageUpload D_aya_10400_8011C154[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10400Image066C4 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image06A04[] = {
#include "assets/aya_10400_image_06A04.inc"
};
static GpuImageUpload D_aya_10400_8011C494[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10400Image06A04 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image06D44[] = {
#include "assets/aya_10400_image_06D44.inc"
};
static GpuImageUpload D_aya_10400_8011C7D4[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10400Image06D44 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image07084[] = {
#include "assets/aya_10400_image_07084.inc"
};
static GpuImageUpload D_aya_10400_8011CA24[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, _gAya10400Image07084 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image072D4[] = {
#include "assets/aya_10400_image_072D4.inc"
};
static GpuImageUpload D_aya_10400_8011CC74[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, _gAya10400Image072D4 },
    { GP_IMG_REC_END },
};

GpuImageUpload* D_aya_10400_8011CC94[] = { D_aya_10400_8011C154, NULL };
GpuImageUpload* D_aya_10400_8011CC9C[] = { D_aya_10400_8011C154, D_aya_10400_8011C494, D_aya_10400_8011C7D4, NULL };
GpuImageUpload* D_aya_10400_8011CCAC[] = { D_aya_10400_8011C7D4, D_aya_10400_8011C494, D_aya_10400_8011C154, NULL };
GpuImageUpload* D_aya_10400_8011CCBC[] = {
    D_aya_10400_8011C154,
    D_aya_10400_8011C494,
    D_aya_10400_8011C7D4,
    D_aya_10400_8011C494,
    D_aya_10400_8011C154,
    NULL,
};
GpuImageUpload* D_aya_10400_8011CCD4[] = { D_aya_10400_8011CA24, NULL };
GpuImageUpload* D_aya_10400_8011CCDC[] = { D_aya_10400_8011CC74, NULL };
