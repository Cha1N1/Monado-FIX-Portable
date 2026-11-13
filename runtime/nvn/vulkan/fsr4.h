// [XCDE] T-FSR4-LITE: optional native FSR 4.1.2 Lite INT8 integration.
#pragma once
#include "fsr4_api.h"
#include <stdbool.h>
#ifdef __linux__  // [XCDE] native Linux component (scripts/build_fsr4_linux.sh)
extern bool fsr4_device_ok;
bool fsr4_available(void);
bool fsr4_record(const MonadoFsr4Frame *frame);
#else
static inline bool fsr4_available(void) { return false; }
static inline bool fsr4_record(const MonadoFsr4Frame *frame) { (void)frame;return false; }
#endif
