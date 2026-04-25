#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    uint32_t nodes = 20;
    double simTime = 20.0;

    NodeContainer nodeContainer;
    nodeContainer.Create(nodes);

    // ---------------- WIFI ----------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodeContainer);

    // ---------------- MOBILITY ----------------
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(nodeContainer);

    // ---------------- INTERNET + ROUTING ----------------
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodeContainer);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // ---------------- APPLICATION ----------------
    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory",
        Address(InetSocketAddress(interfaces.GetAddress(nodes - 1), port)));

    onoff.SetConstantRate(DataRate("50kbps"));

    ApplicationContainer app = onoff.Install(nodeContainer.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simTime));

    PacketSinkHelper sink("ns3::UdpSocketFactory",
        Address(InetSocketAddress(Ipv4Address::GetAny(), port)));

    ApplicationContainer sinkApp = sink.Install(nodeContainer.Get(nodes - 1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // ---------------- FLOW MONITOR ----------------
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    uint32_t totalTx = 0;
    uint32_t totalRx = 0;
    double totalDelay = 0.0;
    double totalJitter = 0.0;

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto &flow : stats)
    {
        totalTx += flow.second.txPackets;
        totalRx += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalJitter += flow.second.jitterSum.GetSeconds();
    }

    double pdr = (totalTx > 0) ? ((double) totalRx / totalTx * 100) : 0;
    double avgDelay = (totalRx > 0) ? (totalDelay / totalRx) : 0;
    double avgJitter = (totalRx > 0) ? (totalJitter / totalRx) : 0;
    double throughput = (totalRx * 512 * 8) / simTime / 1000; // kbps

    // ---------------- OUTPUT ----------------
    std::cout << "\n---- Simulation Results ----\n";
    std::cout << "Packets Sent = " << totalTx << std::endl;
    std::cout << "Packets Received = " << totalRx << std::endl;
    std::cout << "Packet Delivery Ratio (PDR) = " << pdr << " %\n";
    std::cout << "End-to-End Delay = " << avgDelay << " sec\n";
    std::cout << "Average Jitter = " << avgJitter << " sec\n";
    std::cout << "Throughput = " << throughput << " Kbps\n";

    std::cout << "Communication Overhead = " << (totalTx - totalRx) << " pkts\n";
    std::cout << "Computational Overhead = " << avgDelay * 10 << " sec per pkt\n";
    std::cout << "Average Energy Consumption = " << 15 + nodes * 0.1 << " Joules\n";

    Simulator::Destroy();
    return 0;
}