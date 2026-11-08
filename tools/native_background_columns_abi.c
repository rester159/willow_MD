#include <stddef.h>
#include "willow_native_background_columns.h"
#define ABI(n,x) typedef char abi_##n[(x)?1:-1]
ABI(size,sizeof(WillowNativeBackgroundColumns)==48);
ABI(rawEpoch,offsetof(WillowNativeBackgroundColumns,rawEpoch)==0);
ABI(mapGeneration,offsetof(WillowNativeBackgroundColumns,mapGeneration)==4);
ABI(rawBusy,offsetof(WillowNativeBackgroundColumns,rawBusy)==12);
ABI(touchedMask,offsetof(WillowNativeBackgroundColumns,touchedMask)==14);
ABI(active,offsetof(WillowNativeBackgroundColumns,active)==16);
ABI(fault,offsetof(WillowNativeBackgroundColumns,fault)==18);
ABI(completedEpoch,offsetof(WillowNativeBackgroundColumns,completedEpoch)==20);
ABI(openEpoch,offsetof(WillowNativeBackgroundColumns,openEpoch)==24);
ABI(state,offsetof(WillowNativeBackgroundColumns,state)==28);
ABI(reserved,offsetof(WillowNativeBackgroundColumns,reserved)==30);
ABI(pending,offsetof(WillowNativeBackgroundColumns,pending)==32);
