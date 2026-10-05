#include "l1_cache_calc.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <ostream>
#include <stdint.h>
#include <vector>

namespace itmo_vm_hw1 {
struct JumpInfo {
  size_t stride;
  size_t jump;
};

struct CacheDetection {
  size_t associativity = 0;
  size_t entity_stride = 0;
  size_t cache_size = 0;
};

struct Sample {
  size_t spots;
  double time;
};
} // namespace itmo_vm_hw1

std::ostream &operator<<(std::ostream &os,
                         const itmo_vm_hw1::CacheDetection &cache);

namespace itmo_vm_hw1 {

using clock_type = std::chrono::steady_clock;

volatile size_t sink = 0;

// one cycle of time measuring
double time_once(std::vector<size_t> &data, size_t stride, size_t spots,
                 size_t iterations) {

  const size_t stride_step = stride / sizeof(size_t);

  // initialization of chain
  for (size_t i = 0; i + 1 < spots; ++i) {
    data[i * stride_step] = (i + 1) * stride_step;
  }

  data[(spots - 1) * stride_step] = 0;

  size_t index = 0;

  // warm up
  for (size_t i = 0; i < spots * 10000; ++i) {
    index = data[index];
  }

  auto start = clock_type::now();

  for (size_t i = 0; i < iterations; ++i) {
    index = data[index];
  }

  auto end = clock_type::now();

  sink = index;

  double total_ns =
      std::chrono::duration<double, std::nano>(end - start).count();

  return total_ns / iterations;
}

// returning mediand value for several time_once run to get more truthful time
// measurment
double time(std::vector<size_t> &data, size_t stride, size_t spots,
            size_t iterations) {

  constexpr size_t REPEATS = 5;

  std::vector<double> results;

  for (size_t i = 0; i < REPEATS; ++i) {
    results.push_back(time_once(data, stride, spots, iterations));
  }

  std::sort(results.begin(), results.end());

  return results[REPEATS / 2];
}

// finding jump between time measurements
size_t find_jump(const std::vector<double> &times) {

  for (size_t i = 2; i + 2 < times.size(); ++i) {

    double before = (times[i - 2] + times[i - 1]) / 2.0;

    double after = (times[i + 1] + times[i + 2]) / 2.0;

    // jump is a increase for 25% in value
    if (after > before * 1.25) {
      return i + 2;
    }
  }

  // jump was no found
  return 0;
}

CacheDetection find_cache_detection(const std::vector<JumpInfo> &jumps) {
  for (size_t i = 0; i + 2 < jumps.size(); ++i) {
    const size_t first = jumps[i].jump;
    const size_t second = jumps[i + 1].jump;
    const size_t third = jumps[i + 2].jump;

    if (first == 0 || second == 0 || third == 0) {
      continue;
    }

    const bool plateau = std::abs(static_cast<long long>(first) -
                                  static_cast<long long>(second)) <= 1 &&
                         std::abs(static_cast<long long>(second) -
                                  static_cast<long long>(third)) <= 1;

    if (!plateau) {
      continue;
    }

    const size_t associativity = second;

    const size_t entity_stride = jumps[i].stride;

    const size_t cache_size = associativity * entity_stride;

    return {associativity, entity_stride, cache_size};
  }

  return {};
}

double time_line_candidate(std::vector<size_t> &data, size_t entity_stride,
                           size_t associativity, size_t offset,
                           size_t iterations) {
  std::vector<size_t> positions;

  for (size_t i = 0; i < associativity; ++i) {
    positions.push_back(i * entity_stride / sizeof(size_t));
  }

  positions.push_back((associativity * entity_stride + offset) /
                      sizeof(size_t));

  for (size_t i = 0; i + 1 < positions.size(); ++i) {
    data[positions[i]] = positions[i + 1];
  }

  data[positions.back()] = positions.front();

  size_t index = positions.front();

  // warm up
  for (size_t i = 0; i < positions.size() * 100; ++i) {
    index = data[index];
  }

  const auto start = clock_type::now();

  for (size_t i = 0; i < iterations; ++i) {
    index = data[index];
  }

  const auto end = clock_type::now();

  sink = index;

  const double total_ns =
      std::chrono::duration<double, std::nano>(end - start).count();

  return total_ns / iterations;
}

size_t detect_line_size(std::vector<size_t> &data,
                        const CacheDetection &cache) {
  constexpr size_t ITERATIONS = 200'000;

  std::vector<std::pair<size_t, double>> measurements;

  for (size_t offset = sizeof(size_t); offset <= 256; offset *= 2) {

    const double time_ns = time_line_candidate(
        data, cache.entity_stride, cache.associativity, offset, ITERATIONS);

    measurements.push_back({offset, time_ns});

    std::cout << "offset = " << offset << " B"
              << "\ttime = " << time_ns << " ns\n";
  }

  size_t line_size = 0;
  double best_ratio = 1.0;

  for (size_t i = 1; i < measurements.size(); ++i) {
    const double previous = measurements[i - 1].second;

    const double current = measurements[i].second;

    const double ratio = previous / current;

    if (ratio > best_ratio) {
      best_ratio = ratio;
      line_size = measurements[i].first;
    }
  }

  if (best_ratio < 1.20) {
    return 0;
  }

  return line_size;
}

L1CacheInfo calculate_l1_cache() {

  constexpr size_t MIN_STRIDE = 256;
  constexpr size_t MAX_STRIDE = 32 * 1024;

  constexpr size_t MIN_SPOTS = 1;
  constexpr size_t MAX_SPOTS = 128;

  constexpr size_t DATA_SIZE = MAX_STRIDE * MAX_SPOTS + 4096;

  constexpr size_t element_count = DATA_SIZE / sizeof(size_t);

  constexpr size_t ITERATIONS = 1'000'000;

  std::vector<size_t> data(element_count);

  // storing jump info for each stride run
  std::vector<JumpInfo> jumps;

  for (size_t stride = MIN_STRIDE; stride <= MAX_STRIDE; stride *= 2) {

    std::cout << "\n=== stride = " << stride << " bytes ===\n";

    // cache for times for current stide but for different spots
    std::vector<double> times;

    for (size_t spots = MIN_SPOTS; spots <= MAX_SPOTS; ++spots) {

      double time_ns = time(data, stride, spots, ITERATIONS);

      times.push_back(time_ns);

      //   std::cout << "spots = " << spots << "\t time = " << time_ns <<
      //   "\n";
    }

    // find jump index (same as sports value) in spots time vector
    size_t jump = find_jump(times);

    // log
    if (jump != 0) {

      std::cout << ">>> jump ~= " << jump << " spots\n";

    } else {

      std::cout << ">>> jump not found\n";
    }

    // saving jump info
    jumps.push_back({stride, jump});
  }

  std::cout << "\n\n========================\n"
            << "JUMPS SUMMARY\n"
            << "========================\n";

  for (const JumpInfo &info : jumps) {

    std::cout << "stride = " << info.stride << " B"
              << "\t jump = " << info.jump << "\n";
  }

  CacheDetection detection = find_cache_detection(jumps);

  if (detection.cache_size == 0) {
    std::cout << "Failed to detect cache size\n";
    return {};
  }

  const size_t cache_line_size = detect_line_size(data, detection);

  return {detection.cache_size, cache_line_size, detection.associativity};
}

} // namespace itmo_vm_hw1

std::ostream &operator<<(std::ostream &os,
                         const itmo_vm_hw1::CacheDetection &cache) {
  os << "\n=== CACHE DETECTION ===\n"
     << "Associativity: " << cache.associativity << "\n"
     << "Entity stride: " << cache.entity_stride << " bytes\n"
     << "Cache size: " << cache.cache_size
     << " bytes = " << cache.cache_size / 1024.0 << " KiB\n";

  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const itmo_vm_hw1::L1CacheInfo &cache) {
  os << "\n=== L1 CACHE INFO ===\n"
     << "Associativity: " << cache.associativity << "\n"
     << "Line size: " << cache.line_size << " bytes\n"
     << "Cache size: " << cache.size << " bytes = " << cache.size / 1024.0
     << " KiB\n";

  return os;
}