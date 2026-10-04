#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/gpu_image_upload.h"
#include "main/tmd_types.h"

extern GpuImageUpload* D_aya_10300_8011CD1C[];

extern GpuImageUpload* D_aya_10300_8011CD24[];

extern GpuImageUpload* D_aya_10300_8011CD34[];

extern GpuImageUpload* D_aya_10300_8011CD44[];

extern GpuImageUpload* D_aya_10300_8011CD5C[];

extern GpuImageUpload* D_aya_10300_8011CD64[];

static TmdBone _gAya10300Model019A8Skeleton[19] = {
#include "assets/aya_10300_model_019A8_skeleton.inc"
};

static u32 _gAya10300Model019A8PartVerts[19] = {
#include "assets/aya_10300_model_019A8_partVerts.inc"
};

static SVECTOR _gAya10300Model019A8Verts[352] = {
#include "assets/aya_10300_model_019A8_verts.inc"
};

static SVECTOR _gAya10300Model019A8Normals[373] = {
#include "assets/aya_10300_model_019A8_normals.inc"
};

static u32 _gAya10300Model019A8Stream[3829] = {
#include "assets/aya_10300_model_019A8_stream.inc"
};

TmdSource D_aya_10300_8011ACE8 = {
    0,
    21504,
    5396,
    19,
    _gAya10300Model019A8PartVerts,
    _gAya10300Model019A8Verts,
    _gAya10300Model019A8Normals,
    _gAya10300Model019A8Skeleton,
    _gAya10300Model019A8Stream,
};

static TmdBone _gAya10300Model05734Skeleton[1] = {
#include "assets/aya_10300_model_05734_skeleton.inc"
};

static u32 _gAya10300Model05734PartVerts[1] = {
#include "assets/aya_10300_model_05734_partVerts.inc"
};

static SVECTOR _gAya10300Model05734Verts[23] = {
#include "assets/aya_10300_model_05734_verts.inc"
};

static SVECTOR _gAya10300Model05734Normals[23] = {
#include "assets/aya_10300_model_05734_normals.inc"
};

static u32 _gAya10300Model05734Stream[161] = {
#include "assets/aya_10300_model_05734_stream.inc"
};

TmdSource D_aya_10300_8011B128 = {
    0,
    1120,
    0,
    1,
    _gAya10300Model05734PartVerts,
    _gAya10300Model05734Verts,
    _gAya10300Model05734Normals,
    _gAya10300Model05734Skeleton,
    _gAya10300Model05734Stream,
};

static TmdBone _gAya10300Aya10200Model0019CSkeleton[1] = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

static u32 _gAya10300Aya10200Model0019CPartVerts[1] = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

static SVECTOR _gAya10300Aya10200Model0019CVerts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

static SVECTOR _gAya10300Aya10200Model0019CNormals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

static u32 _gAya10300Aya10200Model0019CStream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource D_aya_10300_8011B568 = {
    0,
    1120,
    0,
    1,
    _gAya10300Aya10200Model0019CPartVerts,
    _gAya10300Aya10200Model0019CVerts,
    _gAya10300Aya10200Model0019CNormals,
    _gAya10300Aya10200Model0019CSkeleton,
    _gAya10300Aya10200Model0019CStream,
};

static TmdBone _gAya10300Model05FF4Skeleton[1] = {
#include "assets/aya_10300_model_05FF4_skeleton.inc"
};

static u32 _gAya10300Model05FF4PartVerts[1] = {
#include "assets/aya_10300_model_05FF4_partVerts.inc"
};

static SVECTOR _gAya10300Model05FF4Verts[27] = {
#include "assets/aya_10300_model_05FF4_verts.inc"
};

static SVECTOR _gAya10300Model05FF4Normals[27] = {
#include "assets/aya_10300_model_05FF4_normals.inc"
};

static u32 _gAya10300Model05FF4Stream[189] = {
#include "assets/aya_10300_model_05FF4_stream.inc"
};

TmdSource D_aya_10300_8011BA58 = {
    0,
    1328,
    0,
    1,
    _gAya10300Model05FF4PartVerts,
    _gAya10300Model05FF4Verts,
    _gAya10300Model05FF4Normals,
    _gAya10300Model05FF4Skeleton,
    _gAya10300Model05FF4Stream,
};

static TmdBone _gAya10300Model064A4Skeleton[1] = {
#include "assets/aya_10300_model_064A4_skeleton.inc"
};

static u32 _gAya10300Model064A4PartVerts[1] = {
#include "assets/aya_10300_model_064A4_partVerts.inc"
};

static SVECTOR _gAya10300Model064A4Verts[23] = {
#include "assets/aya_10300_model_064A4_verts.inc"
};

static SVECTOR _gAya10300Model064A4Normals[23] = {
#include "assets/aya_10300_model_064A4_normals.inc"
};

static u32 _gAya10300Model064A4Stream[161] = {
#include "assets/aya_10300_model_064A4_stream.inc"
};

TmdSource D_aya_10300_8011BE98 = {
    0,
    1120,
    0,
    1,
    _gAya10300Model064A4PartVerts,
    _gAya10300Model064A4Verts,
    _gAya10300Model064A4Normals,
    _gAya10300Model064A4Skeleton,
    _gAya10300Model064A4Stream,
};

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long _gAya10300Image0674C[] = {
#include "assets/aya_10300_image_0674C.inc"
};
static GpuImageUpload D_aya_10300_8011C1DC[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10300Image0674C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image06A8C[] = {
#include "assets/aya_10300_image_06A8C.inc"
};
static GpuImageUpload D_aya_10300_8011C51C[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10300Image06A8C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image06DCC[] = {
#include "assets/aya_10300_image_06DCC.inc"
};
static GpuImageUpload D_aya_10300_8011C85C[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10300Image06DCC },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image0710C[] = {
#include "assets/aya_10300_image_0710C.inc"
};
static GpuImageUpload D_aya_10300_8011CAAC[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, _gAya10300Image0710C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image0735C[] = {
#include "assets/aya_10300_image_0735C.inc"
};
static GpuImageUpload D_aya_10300_8011CCFC[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, _gAya10300Image0735C },
    { GP_IMG_REC_END },
};

GpuImageUpload* D_aya_10300_8011CD1C[] = { D_aya_10300_8011C1DC, NULL };
GpuImageUpload* D_aya_10300_8011CD24[] = { D_aya_10300_8011C1DC, D_aya_10300_8011C51C, D_aya_10300_8011C85C, NULL };
GpuImageUpload* D_aya_10300_8011CD34[] = { D_aya_10300_8011C85C, D_aya_10300_8011C51C, D_aya_10300_8011C1DC, NULL };
GpuImageUpload* D_aya_10300_8011CD44[] = {
    D_aya_10300_8011C1DC,
    D_aya_10300_8011C51C,
    D_aya_10300_8011C85C,
    D_aya_10300_8011C51C,
    D_aya_10300_8011C1DC,
    NULL,
};
GpuImageUpload* D_aya_10300_8011CD5C[] = { D_aya_10300_8011CAAC, NULL };
GpuImageUpload* D_aya_10300_8011CD64[] = { D_aya_10300_8011CCFC, NULL };
