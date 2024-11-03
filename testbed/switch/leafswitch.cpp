//
// Created by 蒋逸伟 on 24-10-30.
//

#include "leafswitch.h"
#include <map>
#include <vector>
#include "queue.h"
#include "eventlist.h"
#include <priorityqueue.h>
#include "loggers.h"

using namespace conga;

LeafSwitch::LeafSwitch() {
    // do nothing;
}

void LeafSwitch::initializeQueues() const {
    for (int c = 0; c < N_CORE; c++) {
        qLeafCore[c][leaf_id]->setLeafInfo(leaf_id, c, true);
        qCoreLeaf[c][leaf_id]->setLeafInfo(leaf_id, c, false);
    }

    for (int s = 0; s < N_SERVER; s++) {
        qLeafServer[leaf_id][s]->setLeafInfo(leaf_id, 0, true);
        qServerLeaf[leaf_id][s]->setLeafInfo(leaf_id, 0, true);
    }
}


void LeafSwitch::generateCongaRoute(route_t *&fwd, route_t *&rev, TCPFlow &flow) {
    const int TOTAL_SERVERS = N_LEAF * N_SERVER;
    uint32_t src = flow.src_ip;
    uint32_t dst = flow.dst_ip;

    if (src == 0) src = rand() % TOTAL_SERVERS;
    if (dst == 0) dst = rand() % (TOTAL_SERVERS - 1);
    if (dst >= src) dst++;
    flow.src_ip = src;
    flow.dst_ip = dst;

    // Calculate source and destination leaf switches
    uint32_t src_leaf = src / N_SERVER;
    uint32_t dst_leaf = dst / N_SERVER;
    uint32_t src_server = src % N_SERVER;
    uint32_t dst_server = dst % N_SERVER;

    uint32_t core_switch = selectUplink(dst_leaf);
    printf("select core switch %d\n", core_switch);

    fwd = new route_t();
    rev = new route_t();

    fwd->push_back(qServerLeaf[src_leaf][src_server]);
    fwd->push_back(pServerLeaf[src_leaf][src_server]);

    if (src_leaf != dst_leaf) {
        fwd->push_back(qLeafCore[core_switch][src_leaf]);
        fwd->push_back(pLeafCore[core_switch][src_leaf]);
        fwd->push_back(qCoreLeaf[core_switch][dst_leaf]);
        fwd->push_back(pCoreLeaf[core_switch][dst_leaf]);
    }

    fwd->push_back(qLeafServer[dst_leaf][dst_server]);
    fwd->push_back(pLeafServer[dst_leaf][dst_server]);

    rev->push_back(qServerLeaf[dst_leaf][dst_server]);
    rev->push_back(pServerLeaf[dst_leaf][dst_server]);

    if (src_leaf != dst_leaf) {
        rev->push_back(qLeafCore[core_switch][dst_leaf]);
        rev->push_back(pLeafCore[core_switch][dst_leaf]);
        rev->push_back(qCoreLeaf[core_switch][src_leaf]);
        rev->push_back(pCoreLeaf[core_switch][src_leaf]);
    }

    rev->push_back(qLeafServer[src_leaf][src_server]);
    rev->push_back(pLeafServer[src_leaf][src_server]);
}


uint32_t LeafSwitch::selectUplink(uint32_t dstLeafId) {
    uint32_t bestCorePath = 0;
    double minCongestion = std::numeric_limits<double>::max();

    for (int core_id = 0; core_id < N_CORE; core_id++) {
        double localDRE = calculateDRE(core_id);
        double remoteCongestion = getPathCongestion(dstLeafId, core_id);
        double pathCongestion = std::max(localDRE, remoteCongestion);

        std::cout << "calculated DRE:" << localDRE << "read RemoteCongestion:" << remoteCongestion << '\n';

        if (pathCongestion < minCongestion) {
            minCongestion = pathCongestion;
            bestCorePath = core_id;
        }
    }

    return bestCorePath;
}

double LeafSwitch::calculateDRE(uint32_t core_id) {
    Queue* uplinkQueue = qLeafCore[core_id][this->leaf_id];
    auto now = EventList::Get().now();

    // Initialize metrics if not exists
    if (uplinkMetrics.find(core_id) == uplinkMetrics.end()) {
        uplinkMetrics.insert({core_id, QueueMetrics(now, 0.0)});
        return 0.0;
    }

    auto& metrics = uplinkMetrics[core_id];

    // Only update at intervals
    if (now - metrics.lastUpdateTime < UPDATE_INTERVAL) {
        return metrics.currentDRE;
    }

    // Calculate queue utilization using available queue info
    double queueUtilization = static_cast<double>(uplinkQueue->_queuesize) /
                            static_cast<double>(uplinkQueue->_maxsize);

    // Calculate new DRE using EWMA
    double newDRE = (metrics.ALPHA * queueUtilization) +
                   ((1 - metrics.ALPHA) * metrics.currentDRE);

    // Update metrics
    metrics.lastUpdateTime = now;
    metrics.currentDRE = newDRE;

    return newDRE;
}

void LeafSwitch::processCongestionFeedback(const DataAck& ack) {
    if (ack.hasCongaFeedback() && ack.congaFeedback.leafId != leaf_id) {
        auto now = EventList::Get().now();

        // Update congestion table
        congestionToLeafTable[ack.congaFeedback.leafId][ack.congaFeedback.coreId] = {
            ack.congaFeedback.congestionMetric,
            now
        };
    }
}

double LeafSwitch::getPathCongestion(uint32_t dstLeafId, uint32_t coreId) {
    auto now = EventList::Get().now();

    // Check for remote congestion feedback
    double remoteCongestion = 0.0;
    auto leafIt = congestionToLeafTable.find(dstLeafId);
    if (leafIt != congestionToLeafTable.end()) {
        auto coreIt = leafIt->second.find(coreId);
        if (coreIt != leafIt->second.end()) {
            // Check if feedback is still valid
            if (now - coreIt->second.timestamp < CONGESTION_TIMEOUT) {
                remoteCongestion = coreIt->second.congestionMetric;
            }
        }
    }

    // Return maximum of local and remote congestion
    return remoteCongestion;
}

void LeafSwitch::cleanupStaleEntries(simtime_picosec now) {
    for (auto leafIt = congestionToLeafTable.begin(); leafIt != congestionToLeafTable.end();) {
        for (auto coreIt = leafIt->second.begin(); coreIt != leafIt->second.end();) {
            if (now - coreIt->second.timestamp > CONGESTION_TIMEOUT) {
                coreIt = leafIt->second.erase(coreIt);
            } else {
                ++coreIt;
            }
        }

        if (leafIt->second.empty()) {
            leafIt = congestionToLeafTable.erase(leafIt);
        } else {
            ++leafIt;
        }
    }
}
