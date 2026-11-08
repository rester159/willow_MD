#include <stddef.h>
#include "willow_native_background_writer.h"
#define ABI(n,x) typedef char abi_##n[(x)?1:-1]
ABI(size,sizeof(WillowNativeBackgroundWriter)==484);
ABI(rawEpoch,offsetof(WillowNativeBackgroundWriter,rawEpoch)==0);
ABI(mapGeneration,offsetof(WillowNativeBackgroundWriter,mapGeneration)==4);
ABI(rawBusy,offsetof(WillowNativeBackgroundWriter,rawBusy)==12);
ABI(touchedMask,offsetof(WillowNativeBackgroundWriter,touchedMask)==14);
ABI(active,offsetof(WillowNativeBackgroundWriter,active)==16);
ABI(fault,offsetof(WillowNativeBackgroundWriter,fault)==18);
ABI(journal,offsetof(WillowNativeBackgroundWriter,journal)==20);
ABI(journalSize,sizeof(WillowNativeBackgroundJournal)==398);
ABI(journalEntries,offsetof(WillowNativeBackgroundJournal,entries)==14);
ABI(index,offsetof(WillowNativeBackgroundWriter,index)==418);
ABI(indexedCount,offsetof(WillowNativeBackgroundWriter,indexedCount)==482);
