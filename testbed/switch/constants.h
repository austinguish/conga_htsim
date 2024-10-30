//
// Created by 蒋逸伟 on 24-10-30.
//
#include <cstdint>
#ifndef HTSIM_CONSTANTS_H
#define HTSIM_CONSTANTS_H
const int N_CORE = 12;      // Core(Spine) switches
const int N_LEAF = 24;      // Leaf switches
const int N_SERVER = 32;
// implement a hash function for TCPFlow

const uint64_t LEAF_BUFFER = 512000;    // Leaf switch buffer
const uint64_t CORE_BUFFER = 1024000;   // Core switch buffer
const uint64_t ENDH_BUFFER = 8192000;   // End host buffer

const uint64_t LEAF_SPEED = 10000000000ULL;  // 10Gbps for leaf-server
const uint64_t CORE_SPEED = 40000000000ULL;  // 40Gbps for leaf-core

const double LINK_DELAY = 0.1;  // Link delay in microseconds

#endif //HTSIM_CONSTANTS_H
