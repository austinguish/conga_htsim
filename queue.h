/*
 * FIFO queue heder
 */
#ifndef QUEUE_H
#define QUEUE_H

#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"
#include "datapacket.h"

#include <list>

class Queue : public EventSource, public PacketSink {
public:
    Queue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger *logger);

    void doNextEvent();

    // virtual void receivePacket(Packet &pkt);

    virtual void printStats();

    inline simtime_picosec drainTime(Packet *pkt) {
        return (simtime_picosec) (pkt->size()) * _ps_per_byte;
    }

    inline mem_b serviceCapacity(simtime_picosec t) {
        return (mem_b) (timeAsSec(t) * (double) _bitrate);
    }

    inline mem_b dctcpThreshold() {
        if (_bitrate <= 1000000000) {
            // 1gbps
            return 10 * MSS_BYTES;
        } else if (_bitrate <= 10000000000) {
            // 10gbps
            return 30 * MSS_BYTES;
        } else {
            return 90 * MSS_BYTES;
        }
    }

    mem_b _maxsize; // Maximum queue size.
    mem_b _queuesize; // Current queue size.

    // add switch information
    uint32_t leaf_id;
    uint32_t core_id;
    bool isLeafQueue; // true if this queue is in a leaf switch

    void setLeafInfo(uint32_t leafId, uint32_t coreId, bool isLeaf) {
        leaf_id = leafId;
        core_id = coreId;
        isLeafQueue = isLeaf;
    }

    // Override receivePacket to handle congestion feedback
    virtual void receivePacket(Packet& pkt);

protected:
    // Start serving the item at the head of the queue.
    virtual void beginService();

    // Wrap up serving the item at the head of the queue.
    virtual void completeService();

    // Apply ECN marking.
    void applyEcnMark(Packet &pkt);

    std::list<Packet *> _enqueued; // List of packet enqueued.
    linkspeed_bps _bitrate; // Speed at which queue drains.
    simtime_picosec _ps_per_byte; // Service time, in picosec per byte.

    // Housekeeping
    QueueLogger *_logger;
};

#endif /* QUEUE_H */
