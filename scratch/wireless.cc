#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#define TCP_SEGMENT_SIZE 1000
#define DURATION 60.0
#define DATA_RATE "100Mbps"

using namespace ns3;

Ptr<PacketSink> sink; 
uint64_t lastTotalRx = 0;

void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " <<newCwnd << std::endl;
}

void SsThreshChange(Ptr<OutputStreamWrapper> stream, uint32_t oldSsThresh, uint32_t newSsThresh) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " <<newSsThresh << std::endl;
}

static void findThroughput(Ptr<OutputStreamWrapper> stream) {
    // AsciiTraceHelper ascii;
    // Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("outputs/wireless.throughput");
    Time currentTime = Simulator::Now();
    double currentThroughput = (sink->GetTotalRx() - lastTotalRx) * 8.0 / 1e5;
    *stream->GetStream() << currentTime.GetSeconds() << " " << currentThroughput << std::endl;
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(MilliSeconds(100), &findThroughput, stream);
}

static void TraceCwnd() {
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("outputs/wireless.cwnd");
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback (&CwndChange, stream));
}

static void TraceSsThresh()
{
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("outputs/wireless.ssthresh");
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
                    MakeBoundCallback(&SsThreshChange, stream));
}


NS_LOG_COMPONENT_DEFINE("TCPCubicSimulation");

int main() {

    int tcpSegmentSize = TCP_SEGMENT_SIZE;
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcpSegmentSize));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpCubic"));

    std::string phyRate = "HtMcs3";

    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5e9));

    YansWifiPhyHelper wifiPhy;

    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode",
                                       StringValue(phyRate),
                                       "ControlMode",
                                       StringValue("HtMcs0"));


    NodeContainer nodes;
    nodes.Create(3);
    
    Ptr<Node> client = nodes.Get(0);
    Ptr<Node> router = nodes.Get(1);
    Ptr<Node> server = nodes.Get(2); 

    Ssid ssid = Ssid("network");

    NetDeviceContainer clientDevice, routerDevice, serverDevice;

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    routerDevice = wifiHelper.Install(wifiPhy, wifiMac, router);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    clientDevice = wifiHelper.Install(wifiPhy, wifiMac, client);
    serverDevice = wifiHelper.Install(wifiPhy, wifiMac, server);

    /* Mobility model */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(200.0, 0.0, 0.0));
    positionAlloc->Add(Vector(400.0, 0.0, 0.0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(client);
    mobility.Install(router);
    mobility.Install(server);

    InternetStackHelper stack;
    stack.Install(router);
    stack.Install(client);
    stack.Install(server);

    Ipv4AddressHelper ipv4Address;
    ipv4Address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterface = ipv4Address.Assign(routerDevice);
    Ipv4InterfaceContainer clientInterface = ipv4Address.Assign(clientDevice);
    Ipv4InterfaceContainer serverInterface = ipv4Address.Assign(serverDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t serverPort = 9;

    // set up the server to receive tcp segments
    Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(server);
    sinkApp.Start(Seconds(0.01));
    sinkApp.Stop(Seconds(DURATION)); 
    sink = DynamicCast<PacketSink> (sinkApp.Get(0));

    OnOffHelper sourceHelper ("ns3::TcpSocketFactory", (InetSocketAddress (serverInterface.GetAddress (0), serverPort)));
    sourceHelper.SetAttribute ("PacketSize", UintegerValue (tcpSegmentSize));
    sourceHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    sourceHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    sourceHelper.SetAttribute ("DataRate", DataRateValue (DataRate (DATA_RATE)));
    ApplicationContainer sourceApp = sourceHelper.Install (client);

    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(DURATION));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("outputs/wireless.throughput");


    Simulator::Schedule(Seconds(0.01), &TraceCwnd);
    Simulator::Schedule(Seconds(0.01), &TraceSsThresh);
    Simulator::Schedule(Seconds(0.01), &findThroughput, stream);

    // wifiPhy.EnablePcapAll("wireless");

    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    std::cout << "Total Bytes Received from Client: " << sink->GetTotalRx() << std::endl;    
    return 0;
}