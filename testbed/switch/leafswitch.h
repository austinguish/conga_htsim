//
// Created by 蒋逸伟 on 24-10-30.
//
#ifndef HTSIM_LEAFSWITCH_H
#define HTSIM_LEAFSWITCH_H
#include <random>
#include "../eventlist.h"
#include "../queue.h"
#include "../pipe.h"
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
        LeafSwitch();

        // LeafSwitch(uint32_t id, uint32_t numUplinks);

        // Path selection
        void addUplink(Queue *queue, uint32_t coreId);
        uint32_t selectUplink(uint32_t dstLeafId);

        // Congestion monitoring
        void updateCongestionFromLeaf(uint32_t leafId, double congestionMetric);
        void updateCongestionToLeaf(uint32_t leafId, double congestionMetric);

        // DRE (Direct Response to ECN) implementation
        double calculateDRE(uint32_t core_id);

        // Uplink management
        // void addUplink(Queue* queue, uint32_t remoteLeafId);

        void generateCongaRoute(route_t*& fwd, route_t*& rev, TCPFlow& flow);

        // Initilizer
        void InitCongestion(uint32_t leafId, uint32_t port, uint32_t congestionMetric);

        uint32_t getSpineIdFromQueue(Queue * queue);

    private:
        uint32_t leaf_id;
        std::vector<UplinkInfo> uplinks;
        // <dst leaf_id, <core_id, congestionMetric>>
        std::map<uint32_t, std::map<uint32_t, double>> congestionToLeafTable;
        std::map<uint32_t, std::map<uint32_t, double>> congestionFromLeafTable;

        float dre_Ct; // the link speed

        // flowlet aging
        float flowlet_Tfl; // flowlet timer


        // Helper methodd
        double getPathCongestion(uint32_t dstLeafId, uint32_t coreId);

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

        // Store metrics for each uplink queue
        std::map<uint32_t, QueueMetrics> uplinkMetrics;
        static constexpr double ALPHA = 0.1;  // EWMA smoothing factor
        static constexpr simtime_picosec UPDATE_INTERVAL = 50000000;
    };
}


#endif //HTSIM_LEAFSWITCH_H
