#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/item_pickup.h"

struct _GpImgRec;

extern struct _GpImgRec* D_aya_10300_8011CD1C[];

extern struct _GpImgRec* D_aya_10300_8011CD24[];

extern struct _GpImgRec* D_aya_10300_8011CD34[];

extern struct _GpImgRec* D_aya_10300_8011CD44[];

extern struct _GpImgRec* D_aya_10300_8011CD5C[];

extern struct _GpImgRec* D_aya_10300_8011CD64[];

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long _gAya10300Image0674C[] = {
#include "assets/aya_10300_image_0674C.inc"
};
static GpImgRec D_aya_10300_8011C1DC[] = {
    { 0, 0, { 0, 0, 25, 16 }, _gAya10300Image0674C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image06A8C[] = {
#include "assets/aya_10300_image_06A8C.inc"
};
static GpImgRec D_aya_10300_8011C51C[] = {
    { 0, 0, { 0, 0, 25, 16 }, _gAya10300Image06A8C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image06DCC[] = {
#include "assets/aya_10300_image_06DCC.inc"
};
static GpImgRec D_aya_10300_8011C85C[] = {
    { 0, 0, { 0, 0, 25, 16 }, _gAya10300Image06DCC },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image0710C[] = {
#include "assets/aya_10300_image_0710C.inc"
};
static GpImgRec D_aya_10300_8011CAAC[] = {
    { 0, 0, { 0, 0, 14, 20 }, _gAya10300Image0710C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image0735C[] = {
#include "assets/aya_10300_image_0735C.inc"
};
static GpImgRec D_aya_10300_8011CCFC[] = {
    { 0, 0, { 0, 0, 14, 20 }, _gAya10300Image0735C },
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
