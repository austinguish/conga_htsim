//
// Created by 蒋逸伟 on 24-10-30.
//

#include "leafswitch.h"
#include <map>
#include <vector>
#include "queue.h"
#include "eventlist.h"
#include "leafswitch.h"
#include <aprx-fairqueue.h>
#include <fairqueue.h>
#include <priorityqueue.h>
#include <stoc-fairqueue.h>
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "queue.h"
#include "pipe.h"
#include "leafswitch.h"
#include <vector>

LeafSwitch::LeafSwitch(uint32_t id, uint32_t numUplinks) : leafId(id) {
    uplinks.reserve(numUplinks);
    dre_alpha = 0.5;
    dre_Ct = 1;
    dre_T = 5;
    flowlet_Tfl = 1;
}

Queue *LeafSwitch::selectUplink(uint32_t dstLeafId) {
    Queue *bestUplink = nullptr;
    double minCongestion = std::numeric_limits<double>::max();

    cleanupStaleEntries(EventList::Get().now());

    for (auto &uplink: uplinks) {
        double pathCongestion = getPathCongestion(dstLeafId, uplink.queue);
        if (pathCongestion < minCongestion) {
            minCongestion = pathCongestion;
            bestUplink = uplink.queue;
        }
    }

    return bestUplink;
}

void LeafSwitch::updateCongestionFromLeaf(uint32_t leafId, double congestionMetric) {
    CongestionInfo info;
    info.leafId = leafId;
    info.congestionMetric = congestionMetric;
    info.timestamp = EventList::Get().now();

    congestionFromLeafTable[leafId] = info;
}

void LeafSwitch::updateCongestionToLeaf(uint32_t leafId, double congestionMetric) {
    CongestionInfo info;
    info.leafId = leafId;
    info.congestionMetric = congestionMetric;
    info.timestamp = EventList::Get().now();

    congestionToLeafTable[leafId] = info;
}

double LeafSwitch::calculateDRE(Queue *queue) {
    // Simplified DRE calculation based on queue occupancy
    return 1.0;
}

void LeafSwitch::addUplink(Queue *queue, uint32_t remoteLeafId) {
    UplinkInfo info;
    info.queue = queue;
    info.remoteLeafId = remoteLeafId;
    info.currentCongestion = 0.0;
    uplinks.push_back(info);
}

void LeafSwitch::cleanupStaleEntries(time_t currentTime) {
    const time_t ENTRY_TIMEOUT = timeFromMs(100); // 100ms timeout

    auto shouldRemove = [currentTime, ENTRY_TIMEOUT](const CongestionInfo &info) {
        return (currentTime - info.timestamp) > ENTRY_TIMEOUT;
    };

    // Clean up stale entries from both tables
    for (auto it = congestionToLeafTable.begin(); it != congestionToLeafTable.end();) {
        if (shouldRemove(it->second)) {
            it = congestionToLeafTable.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = congestionFromLeafTable.begin(); it != congestionFromLeafTable.end();) {
        if (shouldRemove(it->second)) {
            it = congestionFromLeafTable.erase(it);
        } else {
            ++it;
        }
    }
}

double LeafSwitch::getPathCongestion(uint32_t dstLeafId, Queue *uplink) {
    // Find local congestion
    double localCongestion = calculateDRE(uplink);

    // Find remote congestion from table
    auto it = congestionToLeafTable.find(dstLeafId);
    double remoteCongestion = (it != congestionToLeafTable.end()) ?
                              it->second.congestionMetric : 0.0;

    // Return maximum of local and remote congestion
    return std::max(localCongestion, remoteCongestion);
}

uint32_t getSpineIdFromQueue(Queue *queue) {
    return 0;
}


//void conga_simulation(const ArgList &args, Logfile &logfile) {
//    // Basic simulation parameters
//    uint32_t Duration = 10;
//    uint64_t LinkSpeed = 10000000000; // 10Gbps
//    uint64_t LinkDelay = 25;          // 25 microseconds
//    uint64_t LinkBuffer = 512000;     // Buffer size
//
//    // Parse command line arguments
//    parseInt(args, "duration", Duration);
//    parseLongInt(args, "linkspeed", LinkSpeed);
//    parseLongInt(args, "linkdelay", LinkDelay);
//    parseLongInt(args, "linkbuffer", LinkBuffer);
//
//    // Create loggers
//    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(10));
//    logfile.addLogger(*qs);
//
//    // Create leaf switches
//    std::vector<LeafSwitch *> leafSwitches;
//    const uint32_t NUM_LEAVES = 4;
//    const uint32_t UPLINKS_PER_LEAF = 2;
//
//    for (uint32_t i = 0; i < NUM_LEAVES; i++) {
//        LeafSwitch *leaf = new LeafSwitch(i, UPLINKS_PER_LEAF);
//        leafSwitches.push_back(leaf);
//    }
//
//    // Create and connect queues and pipes
//    for (uint32_t i = 0; i < NUM_LEAVES; i++) {
//        for (uint32_t j = 0; j < UPLINKS_PER_LEAF; j++) {
//            Queue *uplink = new Queue(LinkSpeed, LinkBuffer, qs);
//            uplink->setName("leaf" + std::to_string(i) + "_uplink" + std::to_string(j));
//            logfile.writeName(*uplink);
//
//            Pipe *pipe = new Pipe(timeFromUs(LinkDelay));
//            pipe->setName("leaf" + std::to_string(i) + "_pipe" + std::to_string(j));
//            logfile.writeName(*pipe);
//
//            // Connect leaf switch to uplink
//            leafSwitches[i]->addUplink(uplink, (i + j + 1) % NUM_LEAVES);
//        }
//    }
//
//    // Set simulation end time
//    EventList::Get().setEndtime(timeFromSec(Duration));
//}
//
// void createQueues(std::string &qType,
//                       Queue *&queue,
//                       uint64_t speed,
//                       uint64_t buffer,
//                       Logfile &logfile)
// {
// #if MING_PROF
//     QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(100));
//     //QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(10));
//     //QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(50));
// #else
//     QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromMs(10));
// #endif
//     logfile.addLogger(*qs);
//
//     if (qType == "fq") {
//         queue = new FairQueue(speed, buffer, qs);
//     } else if (qType == "afq") {
//         queue = new AprxFairQueue(speed, buffer, qs);
//     } else if (qType == "pq") {
//         queue = new PriorityQueue(speed, buffer, qs);
//     } else if (qType == "sfq") {
//         queue = new StocFairQueue(speed, buffer, qs);
//     } else {
//         queue = new Queue(speed, buffer, qs);
//     }
// }