#ifndef __YSP_SAMPLE_H__
#define __YSP_SAMPLE_H__

#include "xorshift.h"

void *sample_nc(struct xs_state_s *, void *, uint32_t);
void *sample_r(struct xs_state_s *, void *);

#endif
