// Client-server topology
//
//         C0
//          \ 192.168.0.0
//      .1.0 \
//  C1 ------- R1 ------------ R2 ------------ Server
//           /      10.0.0.0        10.0.2.0
//          / .2.0
//         C2

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-star.h"
#include "ns3/random_noise_client-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MasticcTopology");

int
main(int argc, char* argv[])
{
    bool verbose = true;
    uint32_t nClients = 3;
    uint32_t accessRate = 1024; // Mbps
    uint32_t accessDelay = 5; // ms
    uint32_t bottleneckRate = 5; // Mbps
    uint32_t bottleneckDelay = 10; // ms

    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Number of clients connected to R1", nClients);
    cmd.AddValue("accessRate", "Rate of access links (Mbps)", accessRate);
    cmd.AddValue("accessDelay", "Delay of access links (ms)", accessDelay);
    cmd.AddValue("bottleneckRate", "Rate of access links (Mbps)", bottleneckRate);
    cmd.AddValue("bottleneckDelay", "Delay of access links (ms)", bottleneckDelay);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    nClients = nClients < 2 ? 2 : nClients;

    InternetStackHelper stack;
    Ipv4AddressHelper address;

    // client star network
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(std::to_string(accessRate) + "Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue(std::to_string(accessDelay) + "ms"));
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

    // set up server
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(serverNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // set up noise client
    RandomNoiseClientHelper noiseClient(serverInterfaces.GetAddress(1), 9);
    noiseClient.SetAttribute("IntervalMean", DoubleValue(0.1));
    noiseClient.SetAttribute("PacketSizeMean", DoubleValue(1024.0));
    noiseClient.SetAttribute("PacketSizeVariance", DoubleValue(1024.0));
    ApplicationContainer noiseApps = noiseClient.Install(star.GetSpokeNode(1));
    noiseApps.Start(Seconds(2.0));
    noiseApps.Stop(Seconds(10.0));

    // set up main client
    UdpEchoClientHelper echoClient(serverInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(star.GetSpokeNode(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    pointToPoint.EnablePcapAll("traces/p2p");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
