/* SPDX-License-Identifier: BSD-3-Clause */
/* Keep the original smoke-build CLI; denial now returns to the Guest. */
#ifdef STATIC_PROBE_WRITE
#define PMP_PROBE_KIND 7
#elif defined(STATIC_PROBE_EXECUTE)
#define PMP_PROBE_KIND 1
#else
#define PMP_PROBE_KIND 5
#endif
#include "pmp-recover-guest.s"
