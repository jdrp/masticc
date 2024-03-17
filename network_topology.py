# Client-server topology
#
#         C0
#          \ 192.168.0.0
#      .1.0 \
#  C1 ------- R1 ------------ R2 ------------ Server
#           /      10.0.0.0        10.0.2.0
#          / .2.0
#         C2



try:
    from ns import ns
except ModuleNotFoundError:
    raise SystemExit(
        "Error: ns3 Python module not found;"
        " Python bindings may not be enabled"
        " or your PYTHONPATH might not be properly configured"
    )
import sys
from ctypes import c_bool, c_int

# command line arguments
nClients = c_int(3)
accessRate = c_int(1024)  # Mbps
accessDelay = c_int(5)  # ms
bottleneckRate = c_int(5)  # Mbps
bottleneckDelay = c_int(10)  # ms
verbose = c_bool(True)
cmd = ns.CommandLine(__file__)
cmd.AddValue("nClients", "Number of clients connected to R1", nClients)
cmd.AddValue("accessRate", "Rate of access links (Mbps)", accessRate)
cmd.AddValue("accessDelay", "Delay of access links (ms)", accessDelay)
cmd.AddValue("bottleneckRate", "Rate of access links (Mbps)", bottleneckRate)
cmd.AddValue("bottleneckDelay", "Delay of access links (ms)", bottleneckDelay)
cmd.AddValue("verbose", "Tell echo applications to log if true", verbose)
cmd.Parse(sys.argv)

# logging
if verbose.value:
    ns.core.LogComponentEnable("UdpEchoClientApplication", ns.core.LOG_LEVEL_INFO)
    ns.core.LogComponentEnable("UdpEchoServerApplication", ns.core.LOG_LEVEL_INFO)

# helpers
stack = ns.internet.InternetStackHelper()
address = ns.internet.Ipv4AddressHelper()
pointToPoint = ns.point_to_point.PointToPointHelper()

# client star network
pointToPoint.SetDeviceAttribute("DataRate", ns.core.StringValue(f"{accessRate.value}Mbps"))
pointToPoint.SetChannelAttribute("Delay", ns.core.StringValue(f"{accessDelay.value}ms"))
star = ns.point_to_point.PointToPointStarHelper(nClients.value, pointToPoint)
star.InstallStack(stack)
address.SetBase(ns.network.Ipv4Address("192.168.0.0"), ns.network.Ipv4Mask("255.255.255.0"))
star.AssignIpv4Addresses(address)

# point-to-point between R2 and Server
serverNodes = ns.network.NodeContainer()
serverNodes.Create(2)
serverDevices = pointToPoint.Install(serverNodes)
stack.Install(serverNodes)
address.SetBase(ns.network.Ipv4Address("10.0.2.0"), ns.network.Ipv4Mask("255.255.255.0"))
serverInterfaces = address.Assign(serverDevices)

# point-to-point between R1 and R2
routerNodes = ns.network.NodeContainer()
routerNodes.Add(star.GetHub())
routerNodes.Add(serverNodes.Get(0))
pointToPoint.SetDeviceAttribute("DataRate", ns.core.StringValue(f"{bottleneckRate.value}Mbps"))
pointToPoint.SetChannelAttribute("Delay", ns.core.StringValue(f"{bottleneckDelay.value}ms"))
routerDevices = pointToPoint.Install(routerNodes)
address.SetBase(ns.network.Ipv4Address("10.0.0.0"), ns.network.Ipv4Mask("255.255.255.252"))
routerInterfaces = address.Assign(routerDevices)

# set up server
echoServer = ns.applications.UdpEchoServerHelper(9)
serverApps = echoServer.Install(serverNodes.Get(1))
serverApps.Start(ns.core.Seconds(1.0))
serverApps.Stop(ns.core.Seconds(10.0))

# set up client
echoClient = ns.applications.UdpEchoClientHelper(serverInterfaces.GetAddress(1).ConvertTo(), 9)
echoClient.SetAttribute("MaxPackets", ns.core.UintegerValue(1))
echoClient.SetAttribute("Interval", ns.core.TimeValue(ns.core.Seconds(1.)))
echoClient.SetAttribute("PacketSize", ns.core.UintegerValue(1024))
clientApps = echoClient.Install(star.GetSpokeNode(0))
clientApps.Start(ns.core.Seconds(2.0))
clientApps.Stop(ns.core.Seconds(10.0))

# routing
ns.internet.Ipv4GlobalRoutingHelper.PopulateRoutingTables()

# tracing
pointToPoint.EnablePcapAll("masticc/traces/network_topology")

# log all IP addresses
print()
for i in range(nClients.value):
    print(f"Client{i} {star.GetSpokeIpv4Address(i)} --- {star.GetHubIpv4Address(i)} Router1")
print()
print(f"Router1 {routerInterfaces.GetAddress(0)} --- {routerInterfaces.GetAddress(1)} Router2")
print()
print(f"Router2 {serverInterfaces.GetAddress(0)} --- {serverInterfaces.GetAddress(1)} Server")
print()

# simulation
ns.core.Simulator.Run()
ns.core.Simulator.Destroy()
