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


namespace conga {
    // Network components
    Pipe  *pCoreLeaf[N_CORE][N_LEAF];    // Core to Leaf pipes
    Queue *qCoreLeaf[N_CORE][N_LEAF];    // Core to Leaf queues

    Pipe  *pLeafCore[N_CORE][N_LEAF];    // Leaf to Core pipes
    Queue *qLeafCore[N_CORE][N_LEAF];    // Leaf to Core queues

    Pipe  *pLeafServer[N_LEAF][N_SERVER];  // Leaf to Server pipes
    Queue *qLeafServer[N_LEAF][N_SERVER];  // Leaf to Server queues

    Pipe  *pServerLeaf[N_LEAF][N_SERVER];  // Server to Leaf pipes
    Queue *qServerLeaf[N_LEAF][N_SERVER];  // Server to Leaf queues

    // Helper functions
    ECMPSwitch ecmpSwitch;
    LeafSwitch congaSwitch;

    // Modified route generation function that uses ECMP switch
    void generateCongaRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst) {
        TCPFlow flow;
        flow.src_ip = src;
        flow.dst_ip = dst;
        congaSwitch.generateCongaRoute(fwd, rev, flow);
    }
    void generateECMPRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst) {
        // Create a TCPFlow object with the source and destination IPs
        TCPFlow flow;
        flow.src_ip = src;
        flow.dst_ip = dst;
        // Use ECMP switch to generate routes
        ecmpSwitch.generateECMPRoute(fwd, rev, flow);
    }
    void generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);

    void createQueue(std::string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &lf);
}



using namespace std;
using namespace conga;

void
conga_testbed(const ArgList &args, Logfile &logfile)
{
    uint32_t Duration = 5;
    double Utilization = 0.9;
    uint32_t AvgFlowSize = 100000;
    string QueueType = "droptail";
    string EndHost = "dctcp";
    string FlowDist = "uniform";
    string FlowGen = "random";

    // Parse command line arguments
    parseInt(args, "duration", Duration);
    parseInt(args, "flowsize", AvgFlowSize);
    parseDouble(args, "utilization", Utilization);
    parseString(args, "queue", QueueType);
    parseString(args, "endhost", EndHost);
    parseString(args, "flowdist", FlowDist);
    parseString(args, "flowgen", FlowGen);
    // Initialize Core to Leaf connections
    for (int i = 0; i < N_CORE; i++) {
        for (int j = 0; j < N_LEAF; j++) {
            // Core to Leaf direction
            createQueue(QueueType, qCoreLeaf[i][j], CORE_SPEED, CORE_BUFFER, logfile);
            qCoreLeaf[i][j]->setName("q-core-leaf-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(qCoreLeaf[i][j]));

            pCoreLeaf[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            pCoreLeaf[i][j]->setName("p-core-leaf-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(pCoreLeaf[i][j]));

            // Leaf to Core direction
            createQueue(QueueType, qLeafCore[i][j], CORE_SPEED, LEAF_BUFFER, logfile);
            qLeafCore[i][j]->setName("q-leaf-core-" + to_string(i) + "-" + to_string(j));
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
            createQueue(QueueType, qLeafServer[i][j], LEAF_SPEED, LEAF_BUFFER, logfile);
            qLeafServer[i][j]->setName("q-leaf-server-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(qLeafServer[i][j]));

            pLeafServer[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            pLeafServer[i][j]->setName("p-leaf-server-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(pLeafServer[i][j]));

            // Server to Leaf direction
            createQueue(QueueType, qServerLeaf[i][j], LEAF_SPEED, ENDH_BUFFER, logfile);
            qServerLeaf[i][j]->setName("q-server-leaf-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(qServerLeaf[i][j]));

            pServerLeaf[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            pServerLeaf[i][j]->setName("p-server-leaf-" + to_string(i) + "-" + to_string(j));
            logfile.writeName(*(pServerLeaf[i][j]));
        }
    }

    // for (int i = 0; i < N_LEAF; i++) {
    //     congaSwitch.leaf_id = i;
    //     congaSwitch.initializeQueues();
    // }

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
    }

    // Calculate background traffic rate
    //TODO edit back to normal bg_flow_rate
    double bg_flow_rate = Utilization * (CORE_SPEED * N_CORE * N_LEAF) *0.00001;
    FlowGenerator *bgFlowGen = nullptr;
    // Create flow generator
    if (FlowGen=="random"){
        bgFlowGen = new FlowGenerator(eh, generateRandomRoute, bg_flow_rate, AvgFlowSize, fd);
    }else if (FlowGen=="ecmp"){
        bgFlowGen = new FlowGenerator(eh, generateECMPRoute, bg_flow_rate, AvgFlowSize, fd);
    } else if (FlowGen=="conga") {
        bgFlowGen = new FlowGenerator(eh, generateCongaRoute, bg_flow_rate, AvgFlowSize, fd);
        bgFlowGen->setCongaSwitch(&congaSwitch);
    }

    bgFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(3) - 1);

    // Set simulation end time
    EventList::Get().setEndtime(timeFromSec(Duration));
}

void
conga::generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst)
{
    // Total number of servers in the network
    const int TOTAL_SERVERS = N_LEAF * N_SERVER;

    // Generate random source and destination if not specified
    if (src == 0) src = rand() % TOTAL_SERVERS;
    if (dst == 0) dst = rand() % (TOTAL_SERVERS - 1);
    if (dst >= src) dst++;

    // Calculate source and destination leaf switches
    uint32_t src_leaf = src / N_SERVER;
    uint32_t dst_leaf = dst / N_SERVER;
    uint32_t src_server = src % N_SERVER;
    uint32_t dst_server = dst % N_SERVER;

    // Randomly select core switch for routing
    uint32_t core_switch = rand() % N_CORE;

    // Create routes
    fwd = new route_t();
    rev = new route_t();

    // Forward path
    fwd->push_back(qServerLeaf[src_leaf][src_server]);
    fwd->push_back(pServerLeaf[src_leaf][src_server]);

    if (src_leaf != dst_leaf) {
        // Route through core switch if different leaf switches
        fwd->push_back(qLeafCore[core_switch][src_leaf]);
        fwd->push_back(pLeafCore[core_switch][src_leaf]);
        fwd->push_back(qCoreLeaf[core_switch][dst_leaf]);
        fwd->push_back(pCoreLeaf[core_switch][dst_leaf]);
    }

    fwd->push_back(qLeafServer[dst_leaf][dst_server]);
    fwd->push_back(pLeafServer[dst_leaf][dst_server]);

    // Reverse path
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

void
conga::createQueue(std::string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &logfile)
{
    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromMs(10));
    logfile.addLogger(*qs);

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
}