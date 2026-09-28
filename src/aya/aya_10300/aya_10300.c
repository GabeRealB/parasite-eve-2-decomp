#include "common.h"

#include "aya/aya.h"

#include "gameplay/collision.h"
#include "gameplay/item_pickup.h"

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long D_aya_10300_8011BEBC[] = {
#include "assets/aya_10300_image_0674C.inc"
};
static GpImgRec D_aya_10300_8011C1DC[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10300_8011BEBC },
    { GP_IMG_REC_END },
};

static u_long D_aya_10300_8011C1FC[] = {
#include "assets/aya_10300_image_06A8C.inc"
};
static GpImgRec D_aya_10300_8011C51C[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10300_8011C1FC },
    { GP_IMG_REC_END },
};

static u_long D_aya_10300_8011C53C[] = {
#include "assets/aya_10300_image_06DCC.inc"
};
static GpImgRec D_aya_10300_8011C85C[] = {
    { 0, 0, { 0, 0, 25, 16 }, D_aya_10300_8011C53C },
    { GP_IMG_REC_END },
};

static u_long D_aya_10300_8011C87C[] = {
#include "assets/aya_10300_image_0710C.inc"
};
static GpImgRec D_aya_10300_8011CAAC[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10300_8011C87C },
    { GP_IMG_REC_END },
};

static u_long D_aya_10300_8011CACC[] = {
#include "assets/aya_10300_image_0735C.inc"
};
static GpImgRec D_aya_10300_8011CCFC[] = {
    { 0, 0, { 0, 0, 14, 20 }, D_aya_10300_8011CACC },
    { GP_IMG_REC_END },
};

GpImgRec* D_aya_10300_8011CD1C[] = { D_aya_10300_8011C1DC, NULL };
GpImgRec* D_aya_10300_8011CD24[] = { D_aya_10300_8011C1DC, D_aya_10300_8011C51C, D_aya_10300_8011C85C, NULL };
GpImgRec* D_aya_10300_8011CD34[] = { D_aya_10300_8011C85C, D_aya_10300_8011C51C, D_aya_10300_8011C1DC, NULL };
GpImgRec* D_aya_10300_8011CD44[] = {
    D_aya_10300_8011C1DC,
    D_aya_10300_8011C51C,
    D_aya_10300_8011C85C,
    D_aya_10300_8011C51C,
    D_aya_10300_8011C1DC,
    NULL,
};
GpImgRec* D_aya_10300_8011CD5C[] = { D_aya_10300_8011CAAC, NULL };
GpImgRec* D_aya_10300_8011CD64[] = { D_aya_10300_8011CCFC, NULL };
