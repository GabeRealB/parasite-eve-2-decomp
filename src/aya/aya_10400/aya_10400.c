#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/item_pickup.h"

struct _GpImgRec;

extern struct _GpImgRec* D_aya_10400_8011CC94[];

extern struct _GpImgRec* D_aya_10400_8011CC9C[];

extern struct _GpImgRec* D_aya_10400_8011CCAC[];

extern struct _GpImgRec* D_aya_10400_8011CCBC[];

extern struct _GpImgRec* D_aya_10400_8011CCD4[];

extern struct _GpImgRec* D_aya_10400_8011CCDC[];

/* The costume's texture animation: five images, each with the one-image upload
 * list that places it, and the frame lists that sequence those uploads. The
 * image bytes are assets, generated from the extracted package.
 */

static u_long _gAya10400Image066C4[] = {
#include "assets/aya_10400_image_066C4.inc"
};
static GpImgRec D_aya_10400_8011C154[] = {
    { 0, 0, { 0, 0, 25, 16 }, _gAya10400Image066C4 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image06A04[] = {
#include "assets/aya_10400_image_06A04.inc"
};
static GpImgRec D_aya_10400_8011C494[] = {
    { 0, 0, { 0, 0, 25, 16 }, _gAya10400Image06A04 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image06D44[] = {
#include "assets/aya_10400_image_06D44.inc"
};
static GpImgRec D_aya_10400_8011C7D4[] = {
    { 0, 0, { 0, 0, 25, 16 }, _gAya10400Image06D44 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image07084[] = {
#include "assets/aya_10400_image_07084.inc"
};
static GpImgRec D_aya_10400_8011CA24[] = {
    { 0, 0, { 0, 0, 14, 20 }, _gAya10400Image07084 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image072D4[] = {
#include "assets/aya_10400_image_072D4.inc"
};
static GpImgRec D_aya_10400_8011CC74[] = {
    { 0, 0, { 0, 0, 14, 20 }, _gAya10400Image072D4 },
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
