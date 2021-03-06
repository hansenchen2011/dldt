/*******************************************************************************
* Copyright 2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <string.h>
#define XBYAK64
#define XBYAK_NO_OP_NAMES

#include <cpu/xbyak/xbyak_util.h>
#if defined(_OPENMP)
#include <omp.h>
#else // defined(_OPENMP)
inline int omp_get_max_threads() { return 1; }
inline int omp_get_num_threads() { return 1; }
inline int omp_get_thread_num() { return 0; }
inline int omp_in_parallel() { return 0; }
#endif

#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#endif

#include "utils.hpp"
#include "mkldnn.h"

namespace mkldnn {
namespace impl {

int mkldnn_getenv(char *value, const char *name, int length) {
    int result = 0;
    int last_idx = 0;
    if (length > 1) {
        int value_length = 0;
#ifdef _WIN32
        value_length = GetEnvironmentVariable(name, value, length);
        if (value_length >= length) {
            result = -value_length;
        } else {
            last_idx = value_length;
            result = value_length;
        }
#else
        char *buffer = getenv(name);
        if (buffer != NULL) {
            value_length = strlen(buffer);
            if (value_length >= length) {
                result = -value_length;
            } else {
                strncpy(value, buffer, value_length);
                last_idx = value_length;
                result = value_length;
            }
        }
#endif
    }
    value[last_idx] = '\0';
    return result;
}

static bool dump_jit_code;

bool mkldnn_jit_dump() {
    static bool initialized = false;
    if (!initialized) {
        const int len = 2;
        char env_dump[len] = {0};
        dump_jit_code =
            mkldnn_getenv(env_dump, "MKLDNN_JIT_DUMP", len) == 1
            && atoi(env_dump) == 1;
        initialized = true;
    }
    return dump_jit_code;
}

FILE *mkldnn_fopen(const char *filename, const char *mode) {
#ifdef _WIN32
    FILE *fp = NULL;
    return fopen_s(&fp, filename, mode) ? NULL : fp;
#else
    return fopen(filename, mode);
#endif
}

void *malloc(size_t size, int alignment) {
    void *ptr;

#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
    int rc = ptr ? 0 : -1;
#else
    int rc = ::posix_memalign(&ptr, alignment, size);
#endif

    return (rc == 0) ? ptr : 0;
}

void free(void *p) {
#ifdef _WIN32
    _aligned_free(p);
#else
    ::free(p);
#endif
}

static Xbyak::util::Cpu cpu;

unsigned int get_cache_size(int level, bool per_core) {
    unsigned int l = level - 1;
    // Currently, if XByak is not able to fetch the cache topology
    // we default to 32KB of L1, 512KB of L2 and 1MB of L3 per core.
    if (cpu.data_cache_levels == 0){
        const int L1_cache_per_core = 32000;
        const int L2_cache_per_core = 512000;
        const int L3_cache_per_core = 1024000;
        int num_cores = per_core ? 1 : omp_get_max_threads();
        switch(l){
            case(0): return L1_cache_per_core * num_cores;
            case(1): return L2_cache_per_core * num_cores;
            case(2): return L3_cache_per_core * num_cores;
            default: return 0;
        }
    }
    if (l < cpu.data_cache_levels) {
        return cpu.data_cache_size[l]
               / (per_core ? cpu.cores_sharing_data_cache[l] : 1);
    } else
        return 0;
}

}
}

unsigned int mkldnn_get_cache_size(int level, int per_core) {
    return mkldnn::impl::get_cache_size(level, per_core != 0);
}

