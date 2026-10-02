#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/gpu_image_upload.h"

extern GpuImageUpload* D_aya_10500_8011D168[];

extern GpuImageUpload* D_aya_10500_8011D170[];

extern GpuImageUpload* D_aya_10500_8011D180[];

extern GpuImageUpload* D_aya_10500_8011D190[];

extern GpuImageUpload* D_aya_10500_8011D1A8[];

extern GpuImageUpload* D_aya_10500_8011D1B0[];

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 *
 * The images are byte-identical to aya_10200's, and an extracted asset is
 * stored once whatever carries it, so they come from that package's assets.
 */

static u_long _gAya10500Aya10200Image06AC4[] = {
#include "assets/aya_10200_image_06AC4.inc"
};
static GpuImageUpload D_aya_10500_8011C628[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10500Aya10200Image06AC4 },
    { GP_IMG_REC_END },
};

static u_long _gAya10500Aya10200Image06E04[] = {
#include "assets/aya_10200_image_06E04.inc"
};
static GpuImageUpload D_aya_10500_8011C968[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10500Aya10200Image06E04 },
    { GP_IMG_REC_END },
};

static u_long _gAya10500Aya10200Image07144[] = {
#include "assets/aya_10200_image_07144.inc"
};
static GpuImageUpload D_aya_10500_8011CCA8[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 16 }, _gAya10500Aya10200Image07144 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10500_8011CCC8[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpuImageUpload D_aya_10500_8011CEF8[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, D_aya_10500_8011CCC8 },
    { GP_IMG_REC_END },
};

/* Byte-identical to the image before it, so both come from one asset. */
static u_long D_aya_10500_8011CF18[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpuImageUpload D_aya_10500_8011D148[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, D_aya_10500_8011CF18 },
    { GP_IMG_REC_END },
};

GpuImageUpload* D_aya_10500_8011D168[] = { D_aya_10500_8011C628, NULL };
GpuImageUpload* D_aya_10500_8011D170[] = { D_aya_10500_8011C628, D_aya_10500_8011C968, D_aya_10500_8011CCA8, NULL };
GpuImageUpload* D_aya_10500_8011D180[] = { D_aya_10500_8011CCA8, D_aya_10500_8011C968, D_aya_10500_8011C628, NULL };
GpuImageUpload* D_aya_10500_8011D190[] = {
    D_aya_10500_8011C628,
    D_aya_10500_8011C968,
    D_aya_10500_8011CCA8,
    D_aya_10500_8011C968,
    D_aya_10500_8011C628,
    NULL,
};
GpuImageUpload* D_aya_10500_8011D1A8[] = { D_aya_10500_8011CEF8, NULL };
GpuImageUpload* D_aya_10500_8011D1B0[] = { D_aya_10500_8011D148, NULL };
