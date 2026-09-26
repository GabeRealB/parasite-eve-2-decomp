#ifndef AYA_AYA_H
#define AYA_AYA_H

/* The aya family's interface to the resident code: the models main's task
 * descriptor tables name inside an aya package.
 */

#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Models named after the package that holds them.
extern TmdSource D_aya_10200_80115B90;
extern TmdSource D_aya_10200_80115FD0;
extern TmdSource D_aya_10200_801164C0;
extern TmdSource D_aya_10200_80116900;
extern TmdSource D_aya_10500_80115B90;
extern TmdSource D_aya_10500_80115FD0;
extern TmdSource D_aya_10500_801164C0;
extern TmdSource D_aya_10500_80116900;
extern TmdSource D_aya_10300_8011ACE8;
extern TmdSource D_aya_10400_8011B078;
extern TmdSource D_aya_10300_8011B128;
extern TmdSource D_aya_10400_8011B4CC;
extern TmdSource D_aya_10300_8011B568;
extern TmdSource D_aya_10400_8011B9BC;
extern TmdSource D_aya_10300_8011BA58;
extern TmdSource D_aya_10400_8011BE10;
extern TmdSource D_aya_10300_8011BE98;
extern TmdSource D_aya_10200_8011C210;
extern TmdSource D_aya_10500_8011C2E4;

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
extern struct _GpImgRec* D_aya_10300_8011CD1C[];
extern struct _GpImgRec* D_aya_10300_8011CD24[];
extern struct _GpImgRec* D_aya_10300_8011CD34[];
extern struct _GpImgRec* D_aya_10300_8011CD44[];
extern struct _GpImgRec* D_aya_10300_8011CD5C[];
extern struct _GpImgRec* D_aya_10300_8011CD64[];
extern struct _GpImgRec* D_aya_10400_8011CC94[];
extern struct _GpImgRec* D_aya_10400_8011CC9C[];
extern struct _GpImgRec* D_aya_10400_8011CCAC[];
extern struct _GpImgRec* D_aya_10400_8011CCBC[];
extern struct _GpImgRec* D_aya_10400_8011CCD4[];
extern struct _GpImgRec* D_aya_10400_8011CCDC[];
extern struct _GpImgRec* D_aya_10500_8011D168[];
extern struct _GpImgRec* D_aya_10500_8011D170[];
extern struct _GpImgRec* D_aya_10500_8011D180[];
extern struct _GpImgRec* D_aya_10500_8011D190[];
extern struct _GpImgRec* D_aya_10500_8011D1A8[];
extern struct _GpImgRec* D_aya_10500_8011D1B0[];

#endif /* AYA_AYA_H */
