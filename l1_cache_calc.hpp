#pragma once

#include <cstddef>
#include <stdint.h>
#include <ostream>

namespace itmo_vm_hw1 {

struct L1CacheInfo {
  size_t size = 0;
  size_t line_size = 0;
  size_t associativity = 0;
};

L1CacheInfo calculate_l1_cache();

} // namespace itmo_vm_hw1

std::ostream& operator<<(std::ostream& os, const itmo_vm_hw1::L1CacheInfo& cache_info);