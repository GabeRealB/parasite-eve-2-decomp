#include "types.h"

/* Every package opens with its own id, a u16 in a u32 slot; the gameplay
 * package's is 4. Nothing refers to the word: the unit exists to place the
 * bytes ahead of the rest of the package's read-only data. It is `const`
 * because the id belongs to the image's read-only region.
 */

static const s32 packageId = 4;
