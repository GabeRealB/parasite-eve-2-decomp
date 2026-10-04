#ifndef MAIN_PRIVATE_WIPSYS_H
#define MAIN_PRIVATE_WIPSYS_H

/// Seeds the live player record for the opening shooting-gallery scene.
///
/// After the caller clears the live save image, sets current and maximum HP
/// and MP to 100 points, selects the M93R and resource variant 4, and resets
/// unspent experience and the byte whose role remains unproven (`field_20`).
/// All other fields and the serialized backup retain their existing values;
/// gameplay applies mode-specific stats and inventory separately.
void playerSeedNewGameStatus(void);

#endif // MAIN_PRIVATE_WIPSYS_H
