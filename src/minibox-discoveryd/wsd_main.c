#define _POSIX_C_SOURCE 200809L
#include "wsd_runtime.h"
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t stop;
static void on_signal(int sig){(void)sig;stop=1;}

int main(void){
    int rc;
    signal(SIGINT,on_signal);signal(SIGTERM,on_signal);
    rc=mb_wsdd_run(&stop);
    if(rc)fprintf(stderr,"minibox-wsdd: stopped with error %d\n",rc);
    return rc?1:0;
}
