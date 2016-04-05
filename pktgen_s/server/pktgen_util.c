#include "pktgen_util.h"

u8 ranval( ranctx *x ) {
    u8 e = x->a - rot2(x->b, 7);
    x->a = x->b ^ rot2(x->c, 13);
    x->b = x->c + rot2(x->d, 37);
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void raninit( ranctx *x, u8 seed ) {
    u8 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i=0; i<20; ++i) {
        (void)ranval(x);
    }
}

/* Misc. */
void sig_handler(int sig UNUSED) {
    printf("\n");
    write_history(HISTORY_FILE);
    exit(0);
}

int double_compare(const void *a, const void *b) {
    if (*(const double*)a > *(const double*)b) {
        return 1;
    }
    if (*(const double*)a < *(const double*)b) {
        return -1;
    }
    return 0;
}

double get_time_sec(void) {
    return rte_get_tsc_cycles() / (double) rte_get_tsc_hz();
}