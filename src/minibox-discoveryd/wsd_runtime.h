#ifndef MINIBOX_WSD_RUNTIME_H
#define MINIBOX_WSD_RUNTIME_H
#include <signal.h>
int mb_wsdd_run(volatile sig_atomic_t *stop);
#endif
