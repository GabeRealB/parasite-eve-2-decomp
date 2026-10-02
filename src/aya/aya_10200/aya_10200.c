#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/gpu_image_upload.h"

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
