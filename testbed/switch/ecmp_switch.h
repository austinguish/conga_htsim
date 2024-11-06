#ifndef CONGA_ECMP_SWITCH_H
#define CONGA_ECMP_SWITCH_H

#include <random>
#include "../eventlist.h"
#include "../logfile.h"
#include "../queue.h"
#include "../pipe.h"
#include "tcp_flow.h"
#include "constants.h"
#include "corequeue.h"
#include"statistics.h"
namespace conga {

// Forward declarations of external network configuration
    extern Pipe *pCoreLeaf[N_CORE][N_LEAF];
    extern CoreQueue *qCoreLeaf[N_CORE][N_LEAF];
    extern Pipe *pLeafCore[N_CORE][N_LEAF];
    extern LeafSwitch *qLeafCore[N_CORE][N_LEAF];
    extern Pipe *pLeafServer[N_LEAF][N_SERVER];
    extern LeafSwitch *qLeafServer[N_LEAF][N_SERVER];
    extern Pipe *pServerLeaf[N_LEAF][N_SERVER];
    extern Queue *qServerLeaf[N_LEAF][N_SERVER];


    class ECMPSwitch {
    private:
        std::random_device rd;
        std::mt19937 gen;

        uint32_t flowHash(const TCPFlow& flow) const {
            std::hash<TCPFlow> hasher;
            return static_cast<uint32_t>(hasher(flow));
        }

    public:
        ECMPSwitch() : gen(rd()) {}

        uint32_t selectCorePath(const TCPFlow& flow) {
            // uint32_t hash = flowHash(flow);
            return (flow.src_ip ^ flow.dst_ip) % N_CORE;
        }

        void generateECMPRoute(route_t*& fwd, route_t*& rev, TCPFlow& flow) {

            const int TOTAL_SERVERS = N_LEAF * N_SERVER;
            uint32_t src = flow.src_ip;
            uint32_t dst = flow.dst_ip;
            // Generate random source and destination if not specified
            // use the gen object to generate random numbers

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

            uint32_t core_switch = selectCorePath(flow);
            std::cout<<"ecmp chose core switch "<<core_switch<<std::endl;

            //printf("select core switch %d\n", core_switch);
            fwd = new route_t();
            rev = new route_t();
            // put to the packet from server to leaf
            fwd->push_back(qServerLeaf[src_leaf][src_server]);
            fwd->push_back(pServerLeaf[src_leaf][src_server]);
            // if the source leaf is not equal to the destination leaf
            // put to the packet from leaf to core
            if (src_leaf != dst_leaf) {
                fwd->push_back(qLeafCore[core_switch][src_leaf]);
                fwd->push_back(pLeafCore[core_switch][src_leaf]);
                fwd->push_back(qCoreLeaf[core_switch][dst_leaf]);
                fwd->push_back(pCoreLeaf[core_switch][dst_leaf]);
            }
            // put to the packet from leaf to server
            fwd->push_back(qLeafServer[dst_leaf][dst_server]);
            fwd->push_back(pLeafServer[dst_leaf][dst_server]);
            // build the ack path
            // put to the packet from server to leaf
            rev->push_back(qServerLeaf[dst_leaf][dst_server]);
            rev->push_back(pServerLeaf[dst_leaf][dst_server]);
            // if the source leaf is not equal to the destination leaf
            if (src_leaf != dst_leaf) {
                rev->push_back(qLeafCore[core_switch][dst_leaf]);
                rev->push_back(pLeafCore[core_switch][dst_leaf]);
                rev->push_back(qCoreLeaf[core_switch][src_leaf]);
                rev->push_back(pCoreLeaf[core_switch][src_leaf]);
            }
            // put to the packet from leaf to server
            rev->push_back(qLeafServer[src_leaf][src_server]);
            rev->push_back(pLeafServer[src_leaf][src_server]);
        }
    };

} // namespace conga

#endif // CONGA_ECMP_SWITCH_H