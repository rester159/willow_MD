#include <stdint.h>
#include <stddef.h>
#include "willow_native_background_journal.h"
void*memcpy(void*dst,const void*src,unsigned long n){uint8_t*d=dst;const uint8_t*s=src;while(n--)*d++=*s++;return dst;}
#define ABI(name,value) typedef char abi_##name[(value)?1:-1]
ABI(entry_size,sizeof(WillowNativeBackgroundJournalEntry)==6);
ABI(key,offsetof(WillowNativeBackgroundJournalEntry,key)==0);
ABI(code,offsetof(WillowNativeBackgroundJournalEntry,code)==2);
ABI(attribute,offsetof(WillowNativeBackgroundJournalEntry,attribute)==4);
ABI(journal_size,sizeof(WillowNativeBackgroundJournal)==398);
ABI(epoch,offsetof(WillowNativeBackgroundJournal,epoch)==0);
ABI(open_epoch,offsetof(WillowNativeBackgroundJournal,openEpoch)==4);
ABI(count,offsetof(WillowNativeBackgroundJournal,count)==8);
ABI(state,offsetof(WillowNativeBackgroundJournal,state)==10);
ABI(fault,offsetof(WillowNativeBackgroundJournal,fault)==12);
ABI(entries,offsetof(WillowNativeBackgroundJournal,entries)==14);
ABI(view_size,sizeof(WillowNativeBackgroundJournalView)==14);
ABI(view_epoch,offsetof(WillowNativeBackgroundJournalView,epoch)==8);
static void testFail(unsigned line,uint32_t actual,uint32_t expected){*(volatile uint16_t*)0xff0010=line;*(volatile uint32_t*)0xff0012=actual;*(volatile uint32_t*)0xff0016=expected;}
static void testTimingBegin(unsigned kind){*(volatile uint16_t*)0xff0020=kind;__asm__ volatile("" ::: "memory");*(volatile uint16_t*)0xff0004=1;__asm__ volatile("" ::: "memory");}
static void testTimingEnd(void){__asm__ volatile("" ::: "memory");*(volatile uint16_t*)0xff0004=2;__asm__ volatile("" ::: "memory");}
#define WILLOW_JOURNAL_ROUTE_FIXTURE 1
#include "test_native_background_journal_cases.h"
uint16_t nativeObjectPresentation(void){if(!testRun())return 0;*(volatile uint32_t*)0xff0008=cases;*(volatile uint32_t*)0xff000c=checks;*(volatile uint32_t*)0xff0024=digest;return 1;}
