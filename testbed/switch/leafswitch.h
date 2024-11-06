#ifndef HTSIM_LEAFSWITCH_H
#define HTSIM_LEAFSWITCH_H

#include <datapacket.h>

#include "../queue.h"
#include "../network.h"
#include "constants.h"
#include <map>
#include <vector>

namespace conga {


    class LeafSwitch : public Queue {
    public:
        LeafSwitch(linkspeed_bps bitrate, mem_b maxsize, QueueLogger* logger)
            : Queue(bitrate, maxsize, logger), leaf_id(0) {}

        void setLeafId(uint32_t id) { leaf_id = id; }
        uint32_t getLeafId() const { return leaf_id; }

        void setCoreId(uint32_t id) { core_id = id; }

        void setDstLeafId(uint32_t id) { dst_leaf_id = id; }

        double measureLocalCongestion(uint32_t core_id);

        // override receivePacket
        void receivePacket(Packet& pkt) override;

    private:
        uint32_t leaf_id;
        uint32_t core_id;
        uint32_t dst_leaf_id;

        // 拥塞信息结构
        struct CongestionInfo {
            double metric;
            uint32_t core_id;
            simtime_picosec timestamp;
        };

        struct QueueMetrics {
            simtime_picosec lastUpdateTime;
            double currentDRE;
            double ALPHA = 0.1;  // EWMA smoothing factor

            // Add constructor for proper initialization
            QueueMetrics(simtime_picosec time, double dre)
                : lastUpdateTime(time), currentDRE(dre) {}

            // Default constructor if needed
            QueueMetrics() : lastUpdateTime(0), currentDRE(0.0) {}
        } metrics_;
        static constexpr simtime_picosec UPDATE_INTERVAL = 50000000;

        double calculateDRE(uint32_t core_id);
        double getPathCongestion(uint32_t core_id) const;

        // core_id, metrics
        std::unordered_map<uint32_t, double> congestionToLeafTable;
        // 存储从其他叶子收到的拥塞信息
        std::unordered_map<uint32_t, std::vector<CongestionInfo>> congestionFromLeafTable;


        // 用于反馈选择的计数器
        std::unordered_map<uint32_t, uint32_t> feedbackCounter;

        // 处理不同类型的数据包
        void processDataPacket(Packet& pkt);
        void processAck(Packet& pkt);

        // 更新和选择拥塞信息
        void updateCongestionFromLeaf(uint32_t src_leaf, uint32_t core_id, double metric);
        void updateCongestionToLeaf(uint32_t core_id, double metric);
        CongestionInfo selectFeedbackMetric(uint32_t dst_leaf);

        // 清理过期条目
        void cleanupStaleEntries();
        static constexpr simtime_picosec ENTRY_TIMEOUT = 500000000; // 500us
    };
} // namespace conga

#endif //HTSIM_LEAFSWITCH_H