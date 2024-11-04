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
    auto congaInfo = pkt.getCongaInfo();

    std::cout << "[DEBUG-PROCESSDATA] Processing Data at leaf " << leaf_id
              << "\n  Src leaf: " << congaInfo.src_leaf_id
              << "\n  Core: " << congaInfo.core_id
              << "\n  Dst leaf: " << congaInfo.dst_leaf_id
              << "\n  Congestion: " << congaInfo.congestion_metric
              << std::endl;

    // 检查是否是源叶子交换机
    if (!congaInfo.has_congestion_info && this->leaf_id == congaInfo.src_leaf_id) {
        // 只有源叶子交换机设置初始拥塞信息
        std::cout << "[DEBUG-SRC] Source leaf switch setting initial congestion info" << std::endl;
        pkt.setCongaInfo(
            this->leaf_id,
            this->core_id,
            this->dst_leaf_id,
            this->measureLocalCongestion(core_id));
    }
    // 检查是否是目的叶子交换机
    else if (congaInfo.has_congestion_info && this->leaf_id == congaInfo.dst_leaf_id) {
        // 只有目的叶子交换机更新拥塞表
        std::cout << "[DEBUG-DST] Destination leaf switch updating congestion table" << std::endl;
        this->updateCongestionFromLeaf(
            congaInfo.src_leaf_id,
            congaInfo.core_id,
            congaInfo.congestion_metric);
    }
    // 其他叶子交换机不处理
    else {
        std::cout << "[DEBUG-SKIP] Leaf " << leaf_id << " skipping packet processing" << std::endl;
    }
}

// Process ACK Packet
void LeafSwitch::processAck(Packet& pkt) {
    auto congaInfo = pkt.getCongaInfo();
    if (!congaInfo.has_congestion_info) {
        return;
    }

    std::cout << "[DEBUG-ACK] Processing ACK at leaf " << leaf_id
              << "\n  Src leaf: " << congaInfo.src_leaf_id
              << "\n  Dst leaf: " << congaInfo.dst_leaf_id
              << std::endl;

    // 检查是否是目的叶子交换机（对于 ACK 是原始数据包的源叶子交换机）
    if (this->leaf_id == congaInfo.src_leaf_id) {
        std::cout << "[DEBUG-ACK-SRC] Updating congestion table at source" << std::endl;
        this->updateCongestionToLeaf(congaInfo.core_id, congaInfo.congestion_metric);
    }
    // 检查是否是源叶子交换机（对于 ACK 是原始数据包的目的叶子交换机）
    else if (this->leaf_id == congaInfo.dst_leaf_id) {
        std::cout << "[DEBUG-ACK-DST] Adding feedback at destination" << std::endl;
        CongestionInfo feedback = this->selectFeedbackMetric(congaInfo.src_leaf_id);
        pkt.setFeedbackCore(feedback.core_id);
        pkt.setFeedbackCongestion(feedback.metric);
    }
    // 其他叶子交换机不处理
    else {
        std::cout << "[DEBUG-ACK-SKIP] Leaf " << leaf_id << " skipping ACK processing" << std::endl;
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
    auto it = congestionFromLeafTable.find(src_leaf);
    if (it != congestionFromLeafTable.end() && !it->second.empty()) {
        const auto& congestionList = it->second;

        // Get or create counter
        auto& counter = feedbackCounter[src_leaf];

        // Select current entry
        CongestionInfo selected = congestionList[counter];

        // Update counter
        counter = (counter + 1) % congestionList.size();

        return selected;
    }
    return {0.0, 0, 0};
}

// Measure local congestion based on core switch ID
double LeafSwitch::measureLocalCongestion(uint32_t core_id) {
    double localDRE = calculateDRE(core_id);
    double remoteCongestion = getPathCongestion(core_id);

    std::cout<< "core_id" << core_id <<" localDRE: " << localDRE << " remoteDRE: " << remoteCongestion << std::endl;
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
    auto it = congestionToLeafTable.find(core_id);

    std::cout << "[DEBUG-CONGESTION] Leaf " << leaf_id
                 << " getting path congestion for core " << core_id
                 << ": " << it->second << std::endl;

    return (it != congestionToLeafTable.end()) ? it->second : 0.0;
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