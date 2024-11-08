#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "aprx-fairqueue.h"
#include "fairqueue.h"
#include "priorityqueue.h"
#include "stoc-fairqueue.h"
#include "flow-generator.h"
#include "pipe.h"
#include "test.h"
#include "prof.h"
#include<string>
#include<functional>
#include"switch/ecmp_switch.h"
#include "switch/leafswitch.h"
#include "switch/constants.h"
#include "switch/corequeue.h"
#include <random>
#include <chrono>
#include"switch/statistics.h"


namespace conga {
    // Network components
    Pipe *pCoreLeaf[N_CORE][N_LEAF]; // Core to Leaf pipes
    CoreQueue *qCoreLeaf[N_CORE][N_LEAF]; // Core to Leaf queues

    Pipe *pLeafCore[N_CORE][N_LEAF]; // Leaf to Core pipes
    LeafSwitch *qLeafCore[N_CORE][N_LEAF]; // Leaf to Core queues

    Pipe *pLeafServer[N_LEAF][N_SERVER]; // Leaf to Server pipes
    LeafSwitch *qLeafServer[N_LEAF][N_SERVER]; // Leaf to Server queues

    Pipe *pServerLeaf[N_LEAF][N_SERVER]; // Server to Leaf pipes
    Queue *qServerLeaf[N_LEAF][N_SERVER]; // Server to Leaf queues

    // Helper functions
    ECMPSwitch ecmpSwitch;

    std::unordered_map<uint32_t, uint32_t> flowPathTable;

    uint32_t flowHash(const TCPFlow &flow) {
        std::hash<TCPFlow> hasher;
        return static_cast<uint32_t>(hasher(flow));
    }
    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    // Helper function to generate random numbers in a range
    inline uint32_t getRandomInRange(uint32_t min, uint32_t max) {
        std::uniform_int_distribution<uint32_t> dist(min, max);
        return dist(rng);
    }
    // Modified route generation function that uses ECMP switch
    void generateCongaRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst) {
        // measure the select route time
        auto now = EventList::Get().now();
        const int TOTAL_SERVERS = N_LEAF * N_SERVER;

        // Generate random source and destination if not specified
        if (src == 0) {
            src = getRandomInRange(0, TOTAL_SERVERS - 1);
        } else {
            src = src % TOTAL_SERVERS;
        }
        if (dst == 0) {
            dst = getRandomInRange(0, TOTAL_SERVERS - 2);
        } else {
            dst = dst % (TOTAL_SERVERS - 1);
        }
        if (dst >= src) dst++;

        // Calculate source and destination leaf switches
        uint32_t src_leaf = src / N_SERVER;
        uint32_t dst_leaf = dst / N_SERVER;
        uint32_t src_server = src % N_SERVER;
        uint32_t dst_server = dst % N_SERVER;

        TCPFlow flow;
        flow.src_ip = src;
        flow.dst_ip = dst;
        uint32_t hash = flowHash(flow);

        uint32_t core_switch;
        auto it = flowPathTable.find(hash);

        if (it != flowPathTable.end()) {
            // 如果流已存在，使用之前选择的路径
            core_switch = it->second;
            // std::cout << "[DEBUG-HASH] Using existing path for flow " << hash
            //         << " via core " << core_switch << std::endl;
        } else {
            // 新流，选择最佳路径
            LeafSwitch *srcLeafSwitch;
            double minCongestion = std::numeric_limits<double>::max();

            for (int i = 0; i < N_CORE; i++) {
                srcLeafSwitch = qLeafCore[i][src_leaf];
                double currentCongestion = srcLeafSwitch->measureLocalCongestion(i);
                if (currentCongestion < minCongestion) {
                    minCongestion = currentCongestion;
                    core_switch = i;
                }
            }

            // 存储新流的路径选择
            flowPathTable[hash] = core_switch;
            // std::cout << "[DEBUG-HASH] New flow " << hash
            //         << " selecting core " << core_switch
            //         << " with congestion " << minCongestion << std::endl;
        }

        //todo test
        std::cout << "conga chose core switch " << core_switch << std::endl;

        if (src_leaf != dst_leaf) {
            // 源叶子到核心
            qLeafCore[core_switch][src_leaf]->setDstLeafId(dst_leaf);
            qLeafCore[core_switch][src_leaf]->setCoreId(core_switch);

            // 目标叶子到服务器
            qLeafServer[dst_leaf][dst_server]->setDstLeafId(dst_leaf);
            qLeafServer[dst_leaf][dst_server]->setCoreId(core_switch);
        }

        // 反向路径
        if (src_leaf != dst_leaf) {
            // 目标叶子到核心
            qLeafCore[core_switch][dst_leaf]->setDstLeafId(src_leaf);
            qLeafCore[core_switch][dst_leaf]->setCoreId(core_switch);

            // 源叶子到服务器
            qLeafServer[src_leaf][src_server]->setDstLeafId(src_leaf);
            qLeafServer[src_leaf][src_server]->setCoreId(core_switch);
        }

        // std::cout << "[DEBUG] Selected core switch " << core_switch
        //         << " for route from leaf " << src_leaf
        //         << " to leaf " << dst_leaf << std::endl;

        // Create routes
        fwd = new route_t();
        rev = new route_t();

        // Forward path
        // Server to Leaf
        fwd->push_back(qServerLeaf[src_leaf][src_server]); // This is now a LeafSwitch
        fwd->push_back(pServerLeaf[src_leaf][src_server]);

        if (src_leaf != dst_leaf) {
            // Leaf to Core
            fwd->push_back(qLeafCore[core_switch][src_leaf]); // This is a LeafSwitch
            fwd->push_back(pLeafCore[core_switch][src_leaf]);

            // Core to Leaf
            fwd->push_back(qCoreLeaf[core_switch][dst_leaf]); // This remains a Queue
            fwd->push_back(pCoreLeaf[core_switch][dst_leaf]);
        }

        // Leaf to Server
        fwd->push_back(qLeafServer[dst_leaf][dst_server]); // This is a LeafSwitch
        fwd->push_back(pLeafServer[dst_leaf][dst_server]);

        // Reverse path (for ACKs)
        rev->push_back(qServerLeaf[dst_leaf][dst_server]); // LeafSwitch
        rev->push_back(pServerLeaf[dst_leaf][dst_server]);

        if (src_leaf != dst_leaf) {
            rev->push_back(qLeafCore[core_switch][dst_leaf]); // LeafSwitch
            rev->push_back(pLeafCore[core_switch][dst_leaf]);
            rev->push_back(qCoreLeaf[core_switch][src_leaf]); // Queue
            rev->push_back(pCoreLeaf[core_switch][src_leaf]);
        }

        rev->push_back(qLeafServer[src_leaf][src_server]); // LeafSwitch
        rev->push_back(pServerLeaf[src_leaf][src_server]);

        // measure the select route time
        auto end = EventList::Get().now();
        auto duration = end - now;
        // std::cout << "[DEBUG-ROUTE] Route selection took " << timeAsMs(duration) << " ms" << std::endl;

    }

    void generateECMPRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst) {
        auto now = EventList::Get().now();
        // Create a TCPFlow object with the source and destination IPs
        TCPFlow flow;
        flow.src_ip = src;
        flow.dst_ip = dst;
        // Use ECMP switch to generate routes
        ecmpSwitch.generateECMPRoute(fwd, rev, flow);
        auto end = EventList::Get().now();
        auto duration = end - now;
        std::cout << "[DEBUG-ROUTE] Route selection took " << timeAsMs(duration) << " ms" << std::endl;
    }

    void generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);

    void createQueue(std::string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &lf, std::string name);
}


using namespace std;
using namespace conga;

void
conga_testbed(const ArgList &args, Logfile &logfile) {
    uint32_t Duration = 5;
    double Utilization = 0.9;
    uint32_t AvgFlowSize = 100000;
    string QueueType = "droptail";
    string EndHost = "dctcp";
    string FlowDist = "uniform";
    string FlowGen = "random";
    uint32_t Load = 50;

    // Parse command line arguments
    parseInt(args, "duration", Duration);
    parseInt(args, "flowsize", AvgFlowSize);
    parseDouble(args, "utilization", Utilization);
    parseString(args, "queue", QueueType);
    parseString(args, "endhost", EndHost);
    parseString(args, "flowdist", FlowDist);
    parseString(args, "flowgen", FlowGen);
    parseInt(args, "load", Load);

    Utilization = Load / 100.0;

    // Initialize Core to Leaf connections
    for (int i = 0; i < N_CORE; i++) {
        for (int j = 0; j < N_LEAF; j++) {
            // Core to Leaf direction
            Queue *coreLeafQueue;
            string name = "q-core-leaf-" + to_string(i) + "-" + to_string(j);
            createQueue(QueueType, coreLeafQueue, CORE_SPEED, CORE_BUFFER, logfile, name);
            coreLeafQueue->setName(name);
            qCoreLeaf[i][j] = dynamic_cast<CoreQueue *>(coreLeafQueue);
            logfile.writeName(*(qCoreLeaf[i][j]));

            pCoreLeaf[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            pCoreLeaf[i][j]->setName("p-core-leaf-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(pCoreLeaf[i][j]));

            // Leaf to Core direction - 使用LeafSwitch
            Queue *leafCoreQueue;
            name = "q-leaf-core-" + to_string(i) + "-" + to_string(j);
            createQueue(QueueType, leafCoreQueue, CORE_SPEED, LEAF_BUFFER, logfile, name);
            leafCoreQueue->setName(name);
            qLeafCore[i][j] = dynamic_cast<LeafSwitch *>(leafCoreQueue); // 转换为LeafSwitch
            // if (qLeafCore[i][j]) {
            //     cout << "[DEBUG] Successfully created LeafSwitch for leaf-core "
            //             << i << "-" << j << endl;
            // }
            logfile.writeName(*(qLeafCore[i][j]));

            pLeafCore[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            pLeafCore[i][j]->setName("p-leaf-core-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(pLeafCore[i][j]));
        }
    }

    // Initialize Leaf to Server connections
    for (int i = 0; i < N_LEAF; i++) {
        for (int j = 0; j < N_SERVER; j++) {
            // Leaf to Server direction
            Queue *leafServerQueue;
            string name = "q-leaf-server-" + to_string(i) + "-" + to_string(j);
            createQueue(QueueType, leafServerQueue, LEAF_SPEED, LEAF_BUFFER, logfile, name);
            leafServerQueue->setName(name);
            qLeafServer[i][j] = dynamic_cast<LeafSwitch *>(leafServerQueue);
            // if (qLeafServer[i][j]) {
            //     cout << "[DEBUG] Successfully created LeafSwitch for leaf-server "
            //             << i << "-" << j << endl;
            // }
            logfile.writeName(*(qLeafServer[i][j]));

            pLeafServer[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            pLeafServer[i][j]->setName("p-leaf-server-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(pLeafServer[i][j]));

            // Server to Leaf direction
            Queue *serverLeafQueue;
            name = "q-server-leaf-" + to_string(i) + "-" + to_string(j);
            createQueue(QueueType, serverLeafQueue, LEAF_SPEED, ENDH_BUFFER, logfile, name);
            serverLeafQueue->setName(name);
            qServerLeaf[i][j] = serverLeafQueue;
            logfile.writeName(*(qServerLeaf[i][j]));

            pServerLeaf[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            pServerLeaf[i][j]->setName("p-server-leaf-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(pServerLeaf[i][j]));
        }
    }

    // Setup flow generation
    // todo change the data Source and the workloads
    DataSource::EndHost eh = DataSource::TCP;
    Workloads::FlowDist fd = Workloads::UNIFORM;

    // Configure end host type
    if (EndHost == "dctcp") {
        eh = DataSource::DCTCP;
    } else if (EndHost == "tcp") {
        eh = DataSource::TCP;
    }

    // Configure flow distribution
    if (FlowDist == "pareto") {
        fd = Workloads::PARETO;
    } else if (FlowDist == "enterprise") {
        fd = Workloads::ENTERPRISE;
    } else if (FlowDist == "datamining") {
        fd = Workloads::DATAMINING;
    }

    // Calculate background traffic rate
    double bg_flow_rate = Utilization * (CORE_SPEED * N_CORE * N_LEAF);
    FlowGenerator *bgFlowGen = nullptr;
    // Create flow generator
    if (FlowGen == "random") {
        // bgFlowGen = new FlowGenerator(eh, generateRandomRoute, bg_flow_rate, AvgFlowSize, fd);
    } else if (FlowGen == "ecmp") {
        bgFlowGen = new FlowGenerator(eh, generateECMPRoute, bg_flow_rate, AvgFlowSize, fd);
    } else if (FlowGen == "conga") {
        bgFlowGen = new FlowGenerator(eh, generateCongaRoute, bg_flow_rate, AvgFlowSize, fd);
    }

    bgFlowGen->setTimeLimits(timeFromUs(1), timeFromMs(Duration) - 1);

    // Set simulation end time
    EventList::Get().setEndtime(timeFromMs(Duration));

    cout << "Starting simulation with:\n"
         << "Algorithm: " << FlowGen << "\n"
         << "Workload: " << FlowDist << "\n"
         << "Load: " << Load << "%\n"
         << "Duration: " << Duration << "s\n";
}

// void
// conga::generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst) {
//     // Total number of servers in the network
//     const int TOTAL_SERVERS = N_LEAF * N_SERVER;
//
//     // Generate random source and destination if not specified
//     if (src == 0) src = rand() % TOTAL_SERVERS;
//     if (dst == 0) dst = rand() % (TOTAL_SERVERS - 1);
//     if (dst >= src) dst++;
//
//     // Calculate source and destination leaf switches
//     uint32_t src_leaf = src / N_SERVER;
//     uint32_t dst_leaf = dst / N_SERVER;
//     uint32_t src_server = src % N_SERVER;
//     uint32_t dst_server = dst % N_SERVER;
//
//     // Randomly select core switch for routing
//     uint32_t core_switch = rand() % N_CORE;
//
//     // Create routes
//     fwd = new route_t();
//     rev = new route_t();
//
//     // Forward path
//     fwd->push_back(qServerLeaf[src_leaf][src_server]);
//     fwd->push_back(pServerLeaf[src_leaf][src_server]);
//
//     if (src_leaf != dst_leaf) {
//         // Route through core switch if different leaf switches
//         fwd->push_back(qLeafCore[core_switch][src_leaf]);
//         fwd->push_back(pLeafCore[core_switch][src_leaf]);
//         fwd->push_back(qCoreLeaf[core_switch][dst_leaf]);
//         fwd->push_back(pCoreLeaf[core_switch][dst_leaf]);
//     }
//
//     fwd->push_back(qLeafServer[dst_leaf][dst_server]);
//     fwd->push_back(pLeafServer[dst_leaf][dst_server]);
//
//     // Reverse path
//     rev->push_back(qServerLeaf[dst_leaf][dst_server]);
//     rev->push_back(pServerLeaf[dst_leaf][dst_server]);
//
//     if (src_leaf != dst_leaf) {
//         rev->push_back(qLeafCore[core_switch][dst_leaf]);
//         rev->push_back(pLeafCore[core_switch][dst_leaf]);
//         rev->push_back(qCoreLeaf[core_switch][src_leaf]);
//         rev->push_back(pCoreLeaf[core_switch][src_leaf]);
//     }
//
//     rev->push_back(qLeafServer[src_leaf][src_server]);
//     rev->push_back(pLeafServer[src_leaf][src_server]);
// }

void conga::createQueue(std::string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &logfile,
                        std::string name) {
    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromMs(10));
    logfile.addLogger(*qs);

    // 解析队列名称
    bool isLeafQueue = (name.find("leaf-server") != string::npos ||
                        name.find("leaf-core") != string::npos);
    bool isCoreQueue = (name.find("core-leaf") != string::npos);

    if (isCoreQueue) {
        size_t lastDash = name.find_last_of('-');
        size_t secondLastDash = name.find_last_of('-', lastDash - 1);
        size_t firstDash = name.find_first_of('-');
        uint32_t core_id = 0;
        core_id = stoi(name.substr(secondLastDash + 1, lastDash - secondLastDash - 1));
        CoreQueue *corequeue = new CoreQueue(speed, buffer, qs);
        corequeue->core_id = core_id;
        queue = corequeue;
        return;
    }

    if (!isLeafQueue) {
        // 核心交换机队列使用原有逻辑
        if (qType == "fq") {
            queue = new FairQueue(speed, buffer, qs);
        } else if (qType == "afq") {
            queue = new AprxFairQueue(speed, buffer, qs);
        } else if (qType == "pq") {
            queue = new PriorityQueue(speed, buffer, qs);
        } else if (qType == "sfq") {
            queue = new StocFairQueue(speed, buffer, qs);
        } else {
            queue = new Queue(speed, buffer, qs);
        }
        return;
    }

    // 解析ID
    size_t lastDash = name.find_last_of('-');
    size_t secondLastDash = name.find_last_of('-', lastDash - 1);
    size_t firstDash = name.find_first_of('-');
    uint32_t leaf_id = 0;
    uint32_t core_id = 0;

    LeafSwitch *leafSwitch = new LeafSwitch(speed, buffer, qs);

    // 根据不同类型的队列设置ID
    if (name.find("leaf-core") != string::npos) {
        // q-leaf-core-[core_id]-[leaf_id]
        core_id = stoi(name.substr(secondLastDash + 1, lastDash - secondLastDash - 1));
        leaf_id = stoi(name.substr(lastDash + 1));
        leafSwitch->setLeafId(leaf_id);
        leafSwitch->setCoreId(core_id);
    } else if (name.find("leaf-server") != string::npos) {
        // q-leaf-server-[leaf_id]-[server_id]
        leaf_id = stoi(name.substr(secondLastDash + 1, lastDash - secondLastDash - 1));
        leafSwitch->setLeafId(leaf_id);
    }

    queue = leafSwitch;

    // cout << "[DEBUG] Created LeafSwitch for " << name
    //         << "\n  Type: " << (name.find("leaf-core") != string::npos
    //                                 ? "leaf-core"
    //                                 : name.find("leaf-server") != string::npos
    //                                       ? "leaf-server"
    //                                       : "server-leaf")
    //         << "\n  Leaf ID: " << leaf_id
    //         << "\n  Core ID: " << core_id
    //         << endl;
}
