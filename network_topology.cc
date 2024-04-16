// Client-server topology
//
//        C0
//          \ 192.168.0.0
//      .1.0 \
// C1 ------- R1 ------------ R2 ------------ Server
//           / 10.0.0.0 10.0.2.0
//          / .2.0
//        C2
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-star.h"
#include "ns3/random_noise_client-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("MasticcTopology");
int
main(int argc, char * argv[]) {
    bool verbose = true;
    uint32_t nClients = 2;
    uint32_t accessRate = 1024; // Mbps
    uint32_t accessDelay = 5; // ms
    uint32_t bottleneckRate = 1; // Mbps
    uint32_t bottleneckDelay = 10; // ms
    uint32_t meanNoiseInterval = 10; // ms
    uint32_t meanNoiseSize = 1000; // bytes
    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Number of clients connected to R1", nClients);
    cmd.AddValue("accessRate", "Rate of access links (Mbps)", accessRate);
    cmd.AddValue("accessDelay", "Delay of access links (ms)", accessDelay);
    cmd.AddValue("bottleneckRate", "Rate of access links (Mbps)", bottleneckRate);
    cmd.AddValue("bottleneckDelay", "Delay of access links (ms)", bottleneckDelay);
    cmd.AddValue("meanNoiseInterval", "Average delay between noise packets (ms)", meanNoiseInterval);
    cmd.AddValue("meanNoiseSize", "Average size of noise packets (bytes)", meanNoiseInterval);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.Parse(argc, argv);
    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }
    nClients = nClients < 3 ? 3 : nClients;
    InternetStackHelper stack;
    Ipv4AddressHelper address;

    // client star network
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(std::to_string(accessRate) + "Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue(std::to_string(accessDelay) + "ms"));
    pointToPoint.SetQueue("ns3::DropTailQueue",  "MaxSize", QueueSizeValue (QueueSize ("1p")));
    PointToPointStarHelper star(nClients, pointToPoint);
    star.InstallStack(stack);
    address.SetBase("192.168.0.0", "255.255.255.0");
    star.AssignIpv4Addresses(address);

    // p2p between R2 and Server
    NodeContainer serverNodes;
    serverNodes.Create(2);
    NetDeviceContainer serverDevices = pointToPoint.Install(serverNodes);
    stack.Install(serverNodes);
    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterfaces = address.Assign(serverDevices);

    // p2p between R1 and R2
    NodeContainer routerNodes;
    routerNodes.Add(star.GetHub());
    routerNodes.Add(serverNodes.Get(0));
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(std::to_string(bottleneckRate) + "Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue(std::to_string(bottleneckDelay) + "ms"));
    NetDeviceContainer routerDevices = pointToPoint.Install(routerNodes);
    address.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);
    //Ptr<DropTailQueue> queue = DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(routerDevices.Get(0))->GetQueue());
    //queue->SetAttribute ("MaxPackets", UintegerValue (3));

    // set up server
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(serverNodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(12.0));
    // set up noise client
    RandomNoiseClientHelper noiseClient(serverInterfaces.GetAddress(1), 9);
    noiseClient.SetAttribute("IntervalMean", DoubleValue(meanNoiseInterval / 1000.));
    noiseClient.SetAttribute("PacketSizeMean", DoubleValue(meanNoiseSize));
    float stdev = meanNoiseSize * .3;
    float variance = stdev * stdev;
    noiseClient.SetAttribute("PacketSizeVariance", DoubleValue(variance));
    ApplicationContainer noiseApps = noiseClient.Install(star.GetSpokeNode(0));
    noiseApps.Start(Seconds(2.0));
    noiseApps.Stop(Seconds(10.0));

    // set up main client
    UdpEchoClientHelper echoClient(serverInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(star.GetSpokeNode(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(12.0));

    /*Ptr < UniformRandomVariable > interval_randomizer = CreateObject < UniformRandomVariable > ();
    interval_randomizer -> SetAttribute("Min", DoubleValue(0.005));
    interval_randomizer -> SetAttribute("Max", DoubleValue(0.02));
    Ptr < UniformRandomVariable > start_randomizer = CreateObject < UniformRandomVariable > ();
    start_randomizer -> SetAttribute("Min", DoubleValue(1.0));
    start_randomizer -> SetAttribute("Max", DoubleValue(3.0));
    for (int i = 1; i < nClients; i++) {
        UdpEchoClientHelper echoClient(serverInterfaces.GetAddress(1), 9);
        //echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        if(i != 1)
            echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        if (i == 1) {
            echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        } else {
            double randomized_interval = interval_randomizer -> GetValue();
            echoClient.SetAttribute("Interval", TimeValue(Seconds(randomized_interval)));
            std::cout << "Randomized interval: " << randomized_interval << std::endl;
        }
        if (i == 1) {
            echoClient.SetAttribute("PacketSize", UintegerValue(1480));
        } else {
            echoClient.SetAttribute("PacketSize", UintegerValue(2960));
        }

        ApplicationContainer clientApps = echoClient.Install(star.GetSpokeNode(i));
        if (i == 1) {
            clientApps.Start(Seconds(1.0));
        } else {
            double randomized_start = start_randomizer -> GetValue();
            clientApps.Start(Seconds(randomized_start));
            std::cout << "Randomized start: " << randomized_start << std::endl;
        }
        clientApps.Stop(Seconds(10.0));
    }*/

    // Flow monitor
    //Ptr<FlowMonitor> flowMonitor;
    //FlowMonitorHelper flowHelper;
    //flowMonitor = flowHelper.InstallAll();
    //flowMonitor = flowHelper.Install(routerNodes);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    pointToPoint.EnablePcap("traces/client.pcap", star.GetSpokeNode(1) -> GetDevice(0), false, true);
    pointToPoint.EnablePcap("traces/router1.pcap", star.GetHub() -> GetDevice(1), false, true);
    pointToPoint.EnablePcap("traces/router2.pcap", serverDevices.Get(1), false, true);
    //pointToPoint.EnablePcapAll("traces/ppp");

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();
    //flowMonitor->GetAllProbes();

    // flowMonitor->SerializeToXmlFile("NameOfFile.xml", true, true);
    // AnimationInterface anim("NameOfFile.xml");
    //anim.ShowNode(serverNodes.Get(0));
    return 0;
}
