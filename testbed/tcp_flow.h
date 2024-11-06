//
// Created by 蒋逸伟 on 24-10-30.
#include <cstdint>
#include <functional>
#ifndef HTSIM_TCP_FLOW_H
#define HTSIM_TCP_FLOW_H
struct TCPFlow{

    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    bool operator == (const TCPFlow &other) const {
        return src_ip == other.src_ip && dst_ip == other.dst_ip && src_port == other.src_port && dst_port == other.dst_port;
    }

    // 添加路由信息
    struct RouteInfo {
        uint32_t src_leaf_id;
        uint32_t core_id;
        uint32_t dst_leaf_id;
        double congestion_metric;  // congestion metric from scr leaf switch

        RouteInfo() : src_leaf_id(0), core_id(0),
                     dst_leaf_id(0), congestion_metric(0.0) {}
    } route_info;

    // Helper methods
    void setRouteInfo(uint32_t src, uint32_t core, uint32_t dst) {
        route_info.src_leaf_id = src;
        route_info.core_id = core;
        route_info.dst_leaf_id = dst;
    }

    void updateCongestionMetric(double metric) {
        route_info.congestion_metric = metric;
    }
};

namespace std {
    template<>
    struct hash<TCPFlow> {
        size_t operator()(const TCPFlow& flow) const {
            // 使用更好的混合函数
            size_t hash = 17;  // 使用质数作为初始值

            // 通过连续乘以另一个质数并加上新的hash值来创建更好的分布
            hash = hash * 31 + std::hash<uint32_t>{}(flow.src_ip);
            hash = hash * 31 + std::hash<uint32_t>{}(flow.dst_ip);
//            hash = hash * 31 + std::hash<uint16_t>{}(flow.src_port);
//            hash = hash * 31 + std::hash<uint16_t>{}(flow.dst_port);

            // 可选：添加一个最终的混淆步骤
            hash = hash ^ (hash >> 16);

            return hash;
        }
    };
}
#endif //HTSIM_TCP_FLOW_H
