#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    NodeContainer nodes;
    nodes.Create(6);

    // CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices = csma.Install(nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 🔥 HYBRID ROUTING (OLSR + AODV)
    AodvHelper aodv;
    OlsrHelper olsr;

    Ipv4ListRoutingHelper list;
    list.Add(olsr, 10);   // proactive first
    list.Add(aodv, 20);   // reactive backup

    InternetStackHelper stack;
    stack.SetRoutingHelper(list);
    stack.Install(nodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Server
    UdpEchoServerHelper server(9);
    ApplicationContainer apps = server.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Client
    UdpEchoClientHelper client(interfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(60));
    client.SetAttribute("Interval", TimeValue(Seconds(0.15)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    apps = client.Install(nodes.Get(5));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    // 🎬 NetAnim output
    AnimationInterface anim("hybrid.xml");

    // Set node positions (for clean visualization)
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), 20, 10);
    anim.SetConstantPosition(nodes.Get(2), 40, 0);
    anim.SetConstantPosition(nodes.Get(3), 60, 10);
    anim.SetConstantPosition(nodes.Get(4), 80, 0);
    anim.SetConstantPosition(nodes.Get(5), 100, 10);

    // 📊 FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto &flow : stats) {
        std::cout << "Flow ID: " << flow.first << std::endl;
        std::cout << "Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "Lost Packets: " << flow.second.lostPackets << std::endl;

        double pdr = (double)flow.second.rxPackets / flow.second.txPackets;
        std::cout << "PDR: " << pdr << std::endl;

        double delay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
        std::cout << "Avg Delay: " << delay << std::endl;

        double throughput = flow.second.rxBytes * 8.0 / (10.0 * 1024 * 1024);
        std::cout << "Throughput (Mbps): " << throughput << std::endl;

        std::cout << "-------------------------" << std::endl;
    }

    monitor->SerializeToXmlFile("hybrid-stats.xml", true, true);

    Simulator::Destroy();
}