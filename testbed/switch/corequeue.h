//
// Created by Yushan Qin on 11/5/24.
//

#ifndef COREQUEUE_H
#define COREQUEUE_H
#include <htsim.h>
#include <datapacket.h>

#include "../queue.h"
#include "../network.h"
#include "constants.h"
#include <map>
#include <vector>

#include <unordered_map>

namespace conga {
    class CoreQueue : public Queue {
    public:
        CoreQueue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger* logger)
            : Queue(bitrate, maxsize, logger), leaf_id(0) {}

        void updateCongestion(Packet& pkt);

        // override receivePacket
        void receivePacket(Packet& pkt) override;

        void setCoreId(uint32_t id) { core_id = id; }
        void setLeafId(uint32_t id) { leaf_id = id; }
        void setDstLeafId(uint32_t id) { dst_leaf_id = id; }

        uint32_t leaf_id;
        uint32_t core_id;
        uint32_t dst_leaf_id;

        // 拥塞信息结构
        struct CongestionInfo {
            double metric;
            uint32_t core_id;
            simtime_picosec timestamp;
        };

        std::unordered_map<uint32_t, double> dre_map;
    };
}

#endif //COREQUEUE_H
