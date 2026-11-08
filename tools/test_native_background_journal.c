#include <stdint.h>
#include <stdio.h>
static void testFail(unsigned line,uint32_t actual,uint32_t expected){fprintf(stderr,"Journal failure line%u actual%u expected%u\n",line,actual,expected);}
static void testTimingBegin(unsigned kind){(void)kind;__asm__ volatile("" ::: "memory");}
static void testTimingEnd(void){__asm__ volatile("" ::: "memory");}
#define WILLOW_JOURNAL_ROUTE_FIXTURE 1
#include "test_native_background_journal_cases.h"
int main(void){if(!testRun())return 1;printf("{\"cases\":%u,\"checks\":%u,\"digest\":%u}\n",cases,checks,digest);return 0;}
