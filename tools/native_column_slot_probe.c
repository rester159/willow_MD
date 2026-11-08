#include "../src/willow_native_column_window.h"
uint16_t slotFold(int16_t column){return willowColumnSlot(column);}
uint16_t slotDivide(int16_t column){int16_t slot=column%43;return slot<0?slot+43:slot;}
