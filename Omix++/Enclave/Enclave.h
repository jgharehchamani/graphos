#ifndef _ENCLAVE_H_
#define _ENCLAVE_H_

#include <stdlib.h>
#include <assert.h>
#include "Enclave_t.h"  /* print_string */
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <vector>
#include <array>
#include "OMAP/Types.hpp"
using namespace std;

using block = std::vector<uint8_t>;

#define PLAINTEXT_LENGTH sizeof(GraphNode)
#define ASC 0
#define DSC 1

extern "C" {
    void printf(const char *fmt, ...);
}

#endif /* !_ENCLAVE_H_ */
