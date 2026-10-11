#include "types.h"

enum { PACKAGE_GAMEPLAY_ID = 4 };

/// Gameplay package identity stored in the image's first read-only 32-bit word.
///
/// The format word is zero-extended from id 4. This private object places
/// bytes before gameplay's other read-only data; no C reference is required.
static const u32 _gPackageId = PACKAGE_GAMEPLAY_ID;
