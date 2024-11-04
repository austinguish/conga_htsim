/*
 * FIFO queue
 */
#include "queue.h"
#include "prof.h"

using namespace std;

Queue::Queue(linkspeed_bps bitrate,
             mem_b maxsize,
             QueueLogger *logger)
    : EventSource("queue"),
      _maxsize(maxsize),
      _queuesize(0),
      _bitrate(bitrate),
      _logger(logger) {
    _ps_per_byte = (simtime_picosec) (8 * 1000000000000UL / _bitrate);
}

void
Queue::beginService() {
    assert(!_enqueued.empty());
    EventList::Get().sourceIsPendingRel(*this, drainTime(_enqueued.back()));
}

void
Queue::completeService() {
    assert(!_enqueued.empty());

    Packet *pkt = _enqueued.back();
    _enqueued.pop_back();
    _queuesize -= pkt->size();

    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);

    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
    }

    applyEcnMark(*pkt);
    pkt->sendOn();

    if (!_enqueued.empty()) {
        beginService();
    }
}

void
Queue::doNextEvent() {
    completeService();
}

void
Queue::receivePacket(Packet &pkt) {
    // First handle the packet queuing
    if (_queuesize + pkt.size() > _maxsize) {
        cout << "[DEBUG-QUEUE] Queue " << str()
             << " dropped packet due to overflow"
             << " current size: " << _queuesize
             << " packet size: " << pkt.size()
             << " max size: " << _maxsize
             << endl;

        if (_logger) {
            _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        }
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
        pkt.free();
        return;
    }

    // Add packet to queue
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = _enqueued.empty();
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();

    // Now check for ACK and add congestion feedback after queue size is updated
    if (isDstLeafQueue) {
        DataAck* ack = dynamic_cast<DataAck*>(&pkt);
        if (ack != nullptr) {
            double queueUtilization = static_cast<double>(_queuesize) /
                                    static_cast<double>(_maxsize);

            cout << "[DEBUG-QUEUE] Queue " << str()
                 << " current size: " << _queuesize
                 << " max size: " << _maxsize
                 << " utilization: " << queueUtilization
                 << endl;

            ack->setCongaFeedback(leaf_id, core_id, queueUtilization);

            cout << "[DEBUG-QUEUE] Set feedback - "
                 << "leaf_id: " << leaf_id
                 << " core_id: " << core_id
                 << " util: " << queueUtilization
                 << endl;
        }
    }

    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    }

    if (queueWasEmpty) {
        assert(_enqueued.size() == 1);
        beginService();
    }
}

void
Queue::applyEcnMark(Packet &pkt) {
    if (ENABLE_ECN && _queuesize > dctcpThreshold()) {
        pkt.setFlag(Packet::ECN_FWD);
    }
}

void
Queue::printStats() {
    unordered_map<uint32_t, uint32_t> counts;

    for (auto const &i: _enqueued) {
        uint32_t fid = i->flow().id;
        if (counts.find(fid) == counts.end()) {
            counts[fid] = 0;
        }
        counts[fid] = counts[fid] + 1;
    }

#if MING_PROF
    cout << str() << " " << timeAsUs(EventList::Get().now()) << " stats";
#else
    cout << str() << " " << timeAsMs(EventList::Get().now()) << " stats";
#endif

    for (auto it = counts.begin(); it != counts.end(); it++) {
        cout << " " << it->first << "->" << it->second;
    }
    cout << endl;
}
