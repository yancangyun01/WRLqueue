#ifndef Wr2Rdup_H
#define Wr2Rdup_H

#include <string.h>
#include "primitives.h"
#include "align.h"

typedef struct TBuffInfo {
    uint8_t          *buff CACHE_ALIGNED;
    volatile uint32_t writePos DOUBLE_CACHE_ALIGNED;
	volatile uint32_t nWriter DOUBLE_CACHE_ALIGNED;
} TBuffInfo DOUBLE_CACHE_ALIGNED;

typedef struct {
    volatile uint32_t CitWriter CACHE_ALIGNED; //The index for writer. The index for reader is dataInx^1
    volatile uint32_t lostCnt CACHE_ALIGNED; //The count for failed writer allocation request.
} record_t DOUBLE_CACHE_ALIGNED;

#endif
