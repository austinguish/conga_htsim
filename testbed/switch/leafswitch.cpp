#include "leafswitch.h"
#include <map>
#include <vector>
#include "queue.h"
#include "eventlist.h"
#include <priorityqueue.h>
#include "loggers.h"

using namespace conga;

// Override receivePacket as per the header file
void LeafSwitch::receivePacket(Packet& pkt) {
    // First handle the packet queuing
    if (_queuesize + pkt.size() > _maxsize) {
        std::cout << "[DEBUG-QUEUE] Queue " << str()
             << " dropped packet due to overflow"
             << " current size: " << _queuesize
             << " packet size: " << pkt.size()
             << " max size: " << _maxsize
             << std::endl;

        if (_logger) {
            _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        }
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
        pkt.free();
        return;
    }

    // Edit packet
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = _enqueued.empty();
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();

    auto* ack = dynamic_cast<DataAck*>(&pkt);

    if (ack == nullptr) {
        // for send packet, update packet info
        processDataPacket(pkt);
    } else {
        // for ack packet, add remote congestion
        processAck(pkt);
    }

    // continue normal operation
    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    }

    if (queueWasEmpty) {
        assert(_enqueued.size() == 1);
        beginService();
    }
}

// Process Data Packet
void LeafSwitch::processDataPacket(Packet& pkt) {
    // set pkt congainfo at src leaf switch
    auto congaInfo = pkt.getCongaInfo();
    if (congaInfo.has_congestion_info == false) {
        pkt.setCongaInfo(
            this->leaf_id,
            this->core_id,
            this->dst_leaf_id,
            this->measureLocalCongestion(core_id));
    }

    // update congestionfromleaf at dst leaf switch
    if (congaInfo.has_congestion_info == true
        && congaInfo.dst_leaf_id == this->leaf_id) {
        this->updateCongestionFromLeaf(
            congaInfo.src_leaf_id, congaInfo.core_id, congaInfo.congestion_metric);
    }
}

// Process ACK Packet
void LeafSwitch::processAck(Packet& pkt) {
    auto congaInfo = pkt.getCongaInfo();
    //add congestioninfo to pkt at dst leaf switch
    CongestionInfo feedback = this->selectFeedbackMetric(congaInfo.src_leaf_id);
    pkt.setFeedbackCore(feedback.core_id);
    pkt.setFeedbackCongestion(feedback.metric);

    //add congestioninfo to congestion-to-leaf at src leaf switch
    if (this->leaf_id == congaInfo.src_leaf_id) {
        this->updateCongestionToLeaf(congaInfo.core_id, congaInfo.congestion_metric);
    }
}

// Update congestion from other leaf switches
void LeafSwitch::updateCongestionFromLeaf(uint32_t src_leaf, uint32_t core_id, double metric) {
    auto now = EventList::Get().now();
    congestionFromLeafTable[src_leaf].push_back({metric, core_id, now});
}

void LeafSwitch::updateCongestionToLeaf(uint32_t core_id, double metric) {
    congestionToLeafTable[core_id] = metric;
}

// Select feedback metric based on congestion information
LeafSwitch::CongestionInfo LeafSwitch::selectFeedbackMetric(uint32_t src_leaf) {
    if (congestionFromLeafTable.find(src_leaf) != congestionFromLeafTable.end()) {
        const auto& congestionList = congestionFromLeafTable[src_leaf];
        // Select the most recent entry or implement a custom selection logic
        // todo change to round robin
        return congestionList.back();
    }
    return {0.0, 0, 0};  // Default feedback if no data available
}

// Measure local congestion based on core switch ID
double LeafSwitch::measureLocalCongestion(uint32_t core_id) {
    double localDRE = calculateDRE(core_id);
    double remoteCongestion = getPathCongestion(core_id);
    double pathCongestion = std::max(localDRE, remoteCongestion);

    return pathCongestion;
}

double LeafSwitch::calculateDRE(uint32_t core_id) {

    Queue* uplinkQueue = this;
    auto now = EventList::Get().now();

    // Only update at intervals
    if (now - metrics_.lastUpdateTime < UPDATE_INTERVAL) {
        return metrics_.currentDRE;
    }

    // Calculate queue utilization using available queue info
    double queueUtilization = static_cast<double>(this->_queuesize) /
                            static_cast<double>(this->_maxsize);

    // Calculate new DRE using EWMA
    double newDRE = (QueueMetrics::ALPHA * queueUtilization) +
                   ((1 - QueueMetrics::ALPHA) * metrics_.currentDRE);

    // Update metrics
    metrics_.lastUpdateTime = now;
    metrics_.currentDRE = newDRE;

    return newDRE;
}

double LeafSwitch::getPathCongestion(uint32_t core_id) const {
    return congestionToLeafTable.find(core_id)->second;
}

// Clean up stale entries from congestion information table
void LeafSwitch::cleanupStaleEntries() {
    auto now = EventList::Get().now();
    for (auto leafIt = congestionFromLeafTable.begin(); leafIt != congestionFromLeafTable.end();) {
        for (auto it = leafIt->second.begin(); it != leafIt->second.end();) {
            if (now - it->timestamp > ENTRY_TIMEOUT) {
                it = leafIt->second.erase(it);
            } else {
                ++it;
            }
        }
        if (leafIt->second.empty()) {
            leafIt = congestionFromLeafTable.erase(leafIt);
        } else {
            ++leafIt;
        }
    }
}