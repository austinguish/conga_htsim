#include "corequeue.h"


using namespace conga;

void CoreQueue::receivePacket(Packet &pkt) {
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

    // Edit packet
    if (pkt.getFlags() < 16) {
        updateCongestion(pkt);
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

void CoreQueue::updateCongestion(Packet& pkt) {
    auto congaInfo = pkt.getCongaInfo();

    // 检查是否是源叶子交换机
    if (!congaInfo.has_congestion_info) {
        // 如果没有拥塞信息，直接返回
        return;
    }
    auto now = EventList::Get().now();
    //
    // // Only update at intervals
    // if (now - metrics_.lastUpdateTime < UPDATE_INTERVAL) {
    //     return metrics_.currentDRE;
    // }

    // Calculate queue utilization using available queue info
    double queueUtilization = (this->_queuesize * 1.0) / this->_maxsize;
    // std::cout << "core" << core_id << " queuesize" << this->_queuesize << std::endl;

    dre_map[congaInfo.src_leaf_id] = queueUtilization;

    // Update metrics
    auto local_cmp = std::max(queueUtilization, congaInfo.congestion_metric);
    // std::cout << "core " << core_id << " downlink congestion: " << queueUtilization << std::endl;
    pkt.setCongaInfo(congaInfo.src_leaf_id, congaInfo.core_id, congaInfo.dst_leaf_id, local_cmp);

    return;
}

