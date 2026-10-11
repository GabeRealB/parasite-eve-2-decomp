#include "types.h"

enum { PACKAGE_TITLE_ID = 5 };

/// Title package identity stored in the image's first read-only 32-bit word.
///
/// The format word is zero-extended from id 5. This private object places
/// bytes before the title's other read-only data; no C reference is required.
static const u32 _gPackageId = PACKAGE_TITLE_ID;
