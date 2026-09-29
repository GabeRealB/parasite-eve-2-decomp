#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/item_pickup.h"

struct _GpImgRec;

/// Texture animation frame lists, six per costume package. Each is a
/// NULL-terminated run of `GpImgRec` upload lists, one per frame; the player's
/// task uploads the frames in turn over a fixed rectangle of the costume
/// model's texture page. Gameplay's two frame tables hold their addresses:
/// the first four lists of a package belong to the first table, the last two
/// to the second.
extern struct _GpImgRec* D_aya_10200_8011D094[];

extern struct _GpImgRec* D_aya_10200_8011D09C[];

extern struct _GpImgRec* D_aya_10200_8011D0AC[];

extern struct _GpImgRec* D_aya_10200_8011D0BC[];

extern struct _GpImgRec* D_aya_10200_8011D0D4[];

extern struct _GpImgRec* D_aya_10200_8011D0DC[];

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long D_aya_10200_8011C234[] = {
#include "assets/aya_10200_image_06AC4.inc"
};
static GpImgRec D_aya_10200_8011C554[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10200_8011C234 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10200_8011C574[] = {
#include "assets/aya_10200_image_06E04.inc"
};
static GpImgRec D_aya_10200_8011C894[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10200_8011C574 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10200_8011C8B4[] = {
#include "assets/aya_10200_image_07144.inc"
};
static GpImgRec D_aya_10200_8011CBD4[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10200_8011C8B4 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10200_8011CBF4[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpImgRec D_aya_10200_8011CE24[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10200_8011CBF4 },
    { GP_IMG_REC_END },
};

/* Byte-identical to the image before it, so both come from one asset. */
static u_long D_aya_10200_8011CE44[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpImgRec D_aya_10200_8011D074[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10200_8011CE44 },
    { GP_IMG_REC_END },
};

GpImgRec* D_aya_10200_8011D094[] = { D_aya_10200_8011C554, NULL };
GpImgRec* D_aya_10200_8011D09C[] = { D_aya_10200_8011C554, D_aya_10200_8011C894, D_aya_10200_8011CBD4, NULL };
GpImgRec* D_aya_10200_8011D0AC[] = { D_aya_10200_8011CBD4, D_aya_10200_8011C894, D_aya_10200_8011C554, NULL };
GpImgRec* D_aya_10200_8011D0BC[] = {
    D_aya_10200_8011C554,
    D_aya_10200_8011C894,
    D_aya_10200_8011CBD4,
    D_aya_10200_8011C894,
    D_aya_10200_8011C554,
    NULL,
};
GpImgRec* D_aya_10200_8011D0D4[] = { D_aya_10200_8011CE24, NULL };
GpImgRec* D_aya_10200_8011D0DC[] = { D_aya_10200_8011D074, NULL };
