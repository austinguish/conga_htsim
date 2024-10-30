//
// Created by 蒋逸伟 on 24-10-30.
//

#ifndef HTSIM_LEAFSWITCH_H
#define HTSIM_LEAFSWITCH_H
#include <cstdint>
#include <logfile.h>
#include <map>
#include <vector>
#include "test.h"

// Forward declarations
class Queue;
class EventList;


class LeafSwitch {
public:
    struct CongestionInfo {
        uint32_t leafId;
        double congestionMetric;
        time_t timestamp;
    };

    struct UplinkInfo {
        Queue* queue;
        uint32_t remoteLeafId;
        double currentCongestion;
    };

    LeafSwitch(uint32_t id, uint32_t numUplinks);

    // Path selection
    Queue* selectUplink(uint32_t dstLeafId);

    // Congestion monitoring
    void updateCongestionFromLeaf(uint32_t leafId, double congestionMetric);
    void updateCongestionToLeaf(uint32_t leafId, double congestionMetric);

    // DRE (Direct Response to ECN) implementation
    double calculateDRE(Queue* queue);

    // Uplink management
    void addUplink(Queue* queue, uint32_t remoteLeafId);

    // Setters
    void setDREAlpha(float alpha) { dre_alpha = alpha; }
    void setDRET(float T) { dre_T = T; }
    void setDRECt(float Ct) { dre_Ct = Ct; }
    void setFlowletTfl(float Tfl) { flowlet_Tfl = Tfl; }

    // Initilizer
    void InitCongestion(uint32_t leafId, uint32_t port, uint32_t congestionMetric);

    uint32_t getSpineIdFromQueue(Queue * queue);

private:
    uint32_t leafId;
    std::vector<UplinkInfo> uplinks;
    std::map<uint32_t, CongestionInfo> congestionToLeafTable;
    std::map<uint32_t, CongestionInfo> congestionFromLeafTable;

    // dre config
    float dre_alpha;
    float dre_T; //decremented periodically by dre_alpha every dre_T seconds
    float dre_Ct; // the link speed

    // flowlet aging
    float flowlet_Tfl; // flowlet timer


    // Helper methods
    void cleanupStaleEntries(time_t currentTime);
    double getPathCongestion(uint32_t dstLeafId, Queue* uplink);
};

#endif //HTSIM_LEAFSWITCH_H
