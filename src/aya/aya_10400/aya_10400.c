#include "common.h"

#include "aya/aya.h"

#include "gameplay/collision.h"
#include "gameplay/item_pickup.h"

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long D_aya_10400_8011BE34[] = {
#include "assets/aya_10400_image_066C4.inc"
};
static GpImgRec D_aya_10400_8011C154[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10400_8011BE34 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10400_8011C174[] = {
#include "assets/aya_10400_image_06A04.inc"
};
static GpImgRec D_aya_10400_8011C494[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10400_8011C174 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10400_8011C4B4[] = {
#include "assets/aya_10400_image_06D44.inc"
};
static GpImgRec D_aya_10400_8011C7D4[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10400_8011C4B4 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10400_8011C7F4[] = {
#include "assets/aya_10400_image_07084.inc"
};
static GpImgRec D_aya_10400_8011CA24[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10400_8011C7F4 },
    { GP_IMG_REC_END },
};

static u_long D_aya_10400_8011CA44[] = {
#include "assets/aya_10400_image_072D4.inc"
};
static GpImgRec D_aya_10400_8011CC74[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10400_8011CA44 },
    { GP_IMG_REC_END },
};

GpImgRec* D_aya_10400_8011CC94[] = { D_aya_10400_8011C154, NULL };
GpImgRec* D_aya_10400_8011CC9C[] = { D_aya_10400_8011C154, D_aya_10400_8011C494, D_aya_10400_8011C7D4, NULL };
GpImgRec* D_aya_10400_8011CCAC[] = { D_aya_10400_8011C7D4, D_aya_10400_8011C494, D_aya_10400_8011C154, NULL };
GpImgRec* D_aya_10400_8011CCBC[] = {
    D_aya_10400_8011C154,
    D_aya_10400_8011C494,
    D_aya_10400_8011C7D4,
    D_aya_10400_8011C494,
    D_aya_10400_8011C154,
    NULL,
};
GpImgRec* D_aya_10400_8011CCD4[] = { D_aya_10400_8011CA24, NULL };
GpImgRec* D_aya_10400_8011CCDC[] = { D_aya_10400_8011CC74, NULL };
