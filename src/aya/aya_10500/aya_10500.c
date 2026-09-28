#include "common.h"

#include "aya/aya.h"

#include "gameplay/collision.h"
#include "gameplay/item_pickup.h"

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 *
 * The images are byte-identical to aya_10200's, and an extracted asset is
 * stored once whatever carries it, so they come from that package's assets.
 */

static u_long D_aya_10500_8011C308[] = {
#include "assets/aya_10200_image_06AC4.inc"
};
static GpImgRec D_aya_10500_8011C628[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10500_8011C308 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10500_8011C648[] = {
#include "assets/aya_10200_image_06E04.inc"
};
static GpImgRec D_aya_10500_8011C968[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10500_8011C648 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10500_8011C988[] = {
#include "assets/aya_10200_image_07144.inc"
};
static GpImgRec D_aya_10500_8011CCA8[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10500_8011C988 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10500_8011CCC8[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpImgRec D_aya_10500_8011CEF8[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10500_8011CCC8 },
    { GP_IMG_REC_END },
};

/* Byte-identical to the image before it, so both come from one asset. */
static u_long D_aya_10500_8011CF18[] = {
#include "assets/aya_10200_image_07484.inc"
};
static GpImgRec D_aya_10500_8011D148[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10500_8011CF18 },
    { GP_IMG_REC_END },
};

GpImgRec* D_aya_10500_8011D168[] = { D_aya_10500_8011C628, NULL };
GpImgRec* D_aya_10500_8011D170[] = { D_aya_10500_8011C628, D_aya_10500_8011C968, D_aya_10500_8011CCA8, NULL };
GpImgRec* D_aya_10500_8011D180[] = { D_aya_10500_8011CCA8, D_aya_10500_8011C968, D_aya_10500_8011C628, NULL };
GpImgRec* D_aya_10500_8011D190[] = {
    D_aya_10500_8011C628,
    D_aya_10500_8011C968,
    D_aya_10500_8011CCA8,
    D_aya_10500_8011C968,
    D_aya_10500_8011C628,
    NULL,
};
GpImgRec* D_aya_10500_8011D1A8[] = { D_aya_10500_8011CEF8, NULL };
GpImgRec* D_aya_10500_8011D1B0[] = { D_aya_10500_8011D148, NULL };
