#ifndef PKTGEN_UTIL_H
#define PKTGEN_UTIL_H 1

#include <stdio.h>
#include <rte_cycles.h>
#include <readline/history.h>

#define UNUSED __attribute__((__unused__))
#define HISTORY_FILE "./.pktgen_history"

#define rot2(x,k) (((x)<<(k))|((x)>>(64-(k))))

/* smallprng
 * source: http://burtleburtle.net/bob/rand/smallprng.html
 */
typedef unsigned long long u8;
typedef struct ranctx { u8 a; u8 b; u8 c; u8 d; } ranctx;

u8 ranval(ranctx *x);
void raninit(ranctx *x, u8 seed);
void sig_handler(int sig UNUSED);
int double_compare(const void *a, const void *b);
double get_time_sec(void);

#endif
