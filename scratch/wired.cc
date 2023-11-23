#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#define TCP_SEGMENT_SIZE 1000
#define DATA_RATE1 "10Mbps"
#define DATA_RATE2 "5Mbps"
#define DURATION 60.0

using namespace ns3;

Ptr<PacketSink> sink;
uint64_t lastTotalRx = 0;

void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

void SsThreshChange(Ptr<OutputStreamWrapper> stream, uint32_t oldSsThresh, uint32_t newSsThresh) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newSsThresh << std::endl;
}

static void findThroughput(Ptr<OutputStreamWrapper> stream) {
    Time currentTime = Simulator::Now();
    double currentThroughput = (sink->GetTotalRx() - lastTotalRx) * 8.0 / 1e5;
    *stream->GetStream() << currentTime.GetSeconds() << " " << currentThroughput << std::endl;
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(MilliSeconds(100), &findThroughput, stream);
}

static void TraceCwnd() {
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("outputs/wired.cwnd");
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndChange, stream));
}

static void TraceSsThresh() {
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("outputs/wired.ssthresh");
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
                                   MakeBoundCallback(&SsThreshChange, stream));
}

NS_LOG_COMPONENT_DEFINE("TCPCubicWiredSimulation");

int main() {
    int tcpSegmentSize = TCP_SEGMENT_SIZE; // Set your desired segment size
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(tcpSegmentSize));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> client = nodes.Get(0);
    Ptr<Node> router = nodes.Get(1);
    Ptr<Node> server = nodes.Get(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(DATA_RATE2));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientRouterDevices = pointToPoint.Install(client, router);
    NetDeviceContainer routerServerDevices = pointToPoint.Install(router, server);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t serverPort = 9;

    Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(server);
    sinkApp.Start(Seconds(0.01));
    sinkApp.Stop(Seconds(DURATION));
    sink = DynamicCast<PacketSink>(sinkApp.Get(0));

    OnOffHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(routerServerInterfaces.GetAddress(1), serverPort));
    sourceHelper.SetAttribute("PacketSize", UintegerValue(tcpSegmentSize));
    sourceHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    sourceHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    sourceHelper.SetAttribute("DataRate", DataRateValue(DataRate(DATA_RATE1)));
    ApplicationContainer sourceApp = sourceHelper.Install(client);
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(DURATION));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> throughputStream = ascii.CreateFileStream("outputs/wired.throughput");

    Simulator::Schedule(Seconds(0.01), &TraceCwnd);
    Simulator::Schedule(Seconds(0.01), &TraceSsThresh);
    Simulator::Schedule(Seconds(0.01), &findThroughput, throughputStream);

    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    std::cout << "Total Bytes Received from Client: " << sink->GetTotalRx() << std::endl;

    return 0;
}
