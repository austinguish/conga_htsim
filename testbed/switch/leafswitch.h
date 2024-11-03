//
// Created by 蒋逸伟 on 24-10-30.
//
#ifndef HTSIM_LEAFSWITCH_H
#define HTSIM_LEAFSWITCH_H
#include <random>
#include "../eventlist.h"
#include "../queue.h"
#include "../pipe.h"
#include "../datapacket.h"
#include "tcp_flow.h"
#include "constants.h"

// Forward declarations
class Queue;
class EventList;

// struct CongestionInfo {
//     uint32_t leafId;
//     double congestionMetric;
//     time_t timestamp;
// };

struct UplinkInfo {
    Queue* queue;
    uint32_t coreId;
    double currentCongestion;
};

namespace conga {

    extern Pipe *pCoreLeaf[N_CORE][N_LEAF];
    extern Queue *qCoreLeaf[N_CORE][N_LEAF];
    extern Pipe *pLeafCore[N_CORE][N_LEAF];
    extern Queue *qLeafCore[N_CORE][N_LEAF];
    extern Pipe *pLeafServer[N_LEAF][N_SERVER];
    extern Queue *qLeafServer[N_LEAF][N_SERVER];
    extern Pipe *pServerLeaf[N_LEAF][N_SERVER];
    extern Queue *qServerLeaf[N_LEAF][N_SERVER];

    class LeafSwitch {
    public:
        uint32_t leaf_id;

        LeafSwitch();

        void initializeQueues() const;

        // Path selection
        uint32_t selectUplink(uint32_t dstLeafId);

        // Congestion monitoring
        void updateCongestionFromLeaf(uint32_t leafId, double congestionMetric);
        void updateCongestionToLeaf(uint32_t leafId, double congestionMetric);

        // DRE (Direct Response to ECN) implementation
        double calculateDRE(uint32_t core_id);

        // Uplink management
        // void addUplink(Queue* queue, uint32_t remoteLeafId);

        void generateCongaRoute(route_t*& fwd, route_t*& rev, TCPFlow& flow);

        void processCongestionFeedback(const DataAck& ack);

    private:
        struct CongestionEntry {
            double congestionMetric;
            simtime_picosec timestamp;
        };

        struct QueueMetrics {
            simtime_picosec lastUpdateTime;
            double currentDRE;
            static constexpr double ALPHA = 0.1;  // EWMA smoothing factor

            // Add constructor for proper initialization
            QueueMetrics(simtime_picosec time, double dre)
                : lastUpdateTime(time), currentDRE(dre) {}

            // Default constructor if needed
            QueueMetrics() : lastUpdateTime(0), currentDRE(0.0) {}
        };



        std::vector<UplinkInfo> uplinks;
        std::map<uint32_t, std::map<uint32_t, CongestionEntry>> congestionToLeafTable; // <dst leaf_id, <core_id, CongestionEntry>>
        static constexpr simtime_picosec CONGESTION_TIMEOUT = 500000000;

        // flowlet aging
        float flowlet_Tfl; // flowlet timer


        // Helper methodd
        double getPathCongestion(uint32_t dstLeafId, uint32_t coreId);
        void cleanupStaleEntries(simtime_picosec now);



        // Store metrics for each uplink queue
        std::map<uint32_t, QueueMetrics> uplinkMetrics;
        static constexpr double ALPHA = 0.1;  // EWMA smoothing factor
        static constexpr simtime_picosec UPDATE_INTERVAL = 50000000;
    };
}


#endif //HTSIM_LEAFSWITCH_H
