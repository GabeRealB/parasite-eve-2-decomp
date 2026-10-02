#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/gpu_image_upload.h"

extern GpuImageUpload* D_aya_10300_8011CD1C[];

extern GpuImageUpload* D_aya_10300_8011CD24[];

extern GpuImageUpload* D_aya_10300_8011CD34[];

extern GpuImageUpload* D_aya_10300_8011CD44[];

extern GpuImageUpload* D_aya_10300_8011CD5C[];

extern GpuImageUpload* D_aya_10300_8011CD64[];

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
