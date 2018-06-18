#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h" 
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
// #include "constant-velocity-loop-mobility-model.h"
#include "ns3/ns2-mobility-helper.h"
// #include "constant-velocity-loop-mobility-model.h-backup"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
/*
 *说明：创建30个节点，第一个和最后一个安装socket用于发送监听和接收监听，并没有路由拓扑过程，还需要修改为论文中的实际场景。 
 */
using namespace ns3;
using namespace std;
// #define TIME_STAMP Simulator::Now().GetSeconds()<<": "//当前时间

int packetsSent = 0;
int packetsReceived = 0;

//统计相关
static double simRunTime=0.0;
clock_t simStartRealTime;
clock_t simFinishRealTime;
// clock_t simStartRealTime;

//统计时延
static Time receivetime;
static Time sendtime;

NodeContainer nodes; //实体测试
NodeContainer mobileSinkNode;
// NodeContainer nodes1;//群体测试

NodeContainer SinkNode;//接收器

//Net device container
NetDeviceContainer senseDevices;
NetDeviceContainer mobileSinkDevice;

//Ipv4 related container
Ipv4InterfaceContainer senseIfs;
Ipv4InterfaceContainer mobileSinkIf;


//全局变量声明
uint32_t size=10;
double step=100;
double totalTime=100;

int packetSize = 1024;
int totalPackets = totalTime-1;
double interval = 1.0;
Time interPacketInterval = Seconds (interval);

TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
vector<Ptr<Socket>> recvSocket(size+1);

Ipv4Address GetNodeIpv4Address(Ptr<Node> n);

/*考虑用这个回调机制计算吞吐量，现在每隔1S，进入此函数计算一次,本例子中只有最后一个节点安装了此回调*/
void RecvPacketCallback (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  simRunTime = Simulator::Now().GetSeconds();
  while ((packet = socket->Recv ()))
    {
		Ptr<Node> thisNode = socket->GetNode();
        Ipv4Address address = GetNodeIpv4Address(thisNode);


		Ipv4Header h;
		packet->PeekHeader(h);
	  	Ipv4Address sourceAdr = h.GetSource();	//Packet的source节点地址
		packetsReceived++;
        std::cout<<address<<" Received packet - "<<packetsReceived<<" from:"<<sourceAdr<<" and Size is "<<packet->GetSize ()<<" Bytes."<<std::endl;
    }
    //吞吐量测试
    // std::cout<<"simRunTime now - "<<simRunTime<<std::endl;
    receivetime= Simulator::Now();
    std::cout<<"package relay - "<<(receivetime-sendtime)<<std::endl;
    // std::cout<<"Throughput now - "<<packetsReceived*1024*8/simRunTime<<"bps"<<std::endl;
}


//生成数据
static void GenerateTraffic (Ipv4Header ipv4Header,Ptr<Packet> pkt,Ptr<Socket> socket,
                             uint32_t pktCount, Time pktInterval )
{ 
    if (pktCount > 0){
      socket->Send (pkt);
      packetsSent++;
      std::cout<<"Packet sent - "<<"FROM: "<<ipv4Header.GetSource()<<"---num is :"<<packetsSent<<std::endl;
      //通过回调机制实现定时发送功能。
      Simulator::Schedule (pktInterval, &GenerateTraffic, 
                           ipv4Header,pkt,socket, pktCount-1, pktInterval);
      sendtime= Simulator::Now();//延时相关记录发送时间
    }else{
      socket->Close ();
    }
}

/*
 * 返回某个节点的Ipv4Address
 */
Ipv4Address GetNodeIpv4Address(Ptr<Node> n){
	return n->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();//1表示interface 的index，0表示address index
}
/*
 * 创建节点
 */
void createNode(){
	mobileSinkNode.Create(1);
	nodes.Create(size);
//	NS_LOG_DEBUG("Create nodes done!");
}

void createMobilityModel(){
	  MobilityHelper mobility;
	  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
	                             "Bounds", RectangleValue (Rectangle (-50, 50, -25, 50)));

	  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
			  "GridWidth",UintegerValue(5),
			  "MinX", DoubleValue(0.0),
			  "MinY",DoubleValue(0.0),
			  "DeltaX", DoubleValue(5.0),
			  "DeltaY",DoubleValue(5.0));
	  mobility.Install(nodes);


	  //接收器安装移动模型-->固定位置
		Ptr<ListPositionAllocator> lpa = CreateObject<ListPositionAllocator>();
		lpa->Add(Vector(15, 8, 0));
		mobility.SetPositionAllocator(lpa);
		mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
		mobility.Install(mobileSinkNode);

}
//创建网络设备
void createWifiDevice(){
	  //协议栈部分
	  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
	  wifiMac.SetType ("ns3::AdhocWifiMac");
	  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	  wifiPhy.SetChannel (wifiChannel.Create ());
	  WifiHelper wifi = WifiHelper::Default ();
	  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "RtsCtsThreshold", UintegerValue (0));
	  senseDevices = wifi.Install (wifiPhy, wifiMac, nodes);
	  mobileSinkDevice = wifi.Install(wifiPhy, wifiMac, mobileSinkNode);
}
//安装路由协议
void installInternetStack(){
	  //使用AODV路由协议
	  AodvHelper aodv;
	  InternetStackHelper stack1;
	  stack1.SetRoutingHelper (aodv);
	  stack1.Install (nodes);//注意，AODV协议安装到nodes节点上

	  InternetStackHelper stack2;
	  stack2.Install (mobileSinkNode);

	  Ipv4AddressHelper address;
	  address.SetBase ("10.0.0.0", "255.0.0.0");
	  senseIfs = address.Assign (senseDevices);
	  mobileSinkIf = address.Assign(mobileSinkDevice);

	  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

/*
 *设置socket广播回调
 */
void createSocketCallBack(){
	  //nodes中的最后一个节点安装socket

	  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (size-1), tid);
	  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 8080);
	  recvSink->Bind (local);
	  recvSink->SetRecvCallback (MakeCallback (&RecvPacketCallback));

	  //nodes中的第一个节点安装socket，发送到远端最后一个节点。
	//   Ptr<Socket> source = Socket::CreateSocket (nodes.Get (0), tid);
	  InetSocketAddress remote = InetSocketAddress (senseIfs.GetAddress (size-1,0), 8080);
	  

	//改成把IP地址也发送出去
	uint32_t t = 0;
	for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End();
		i++) {
		if(t<nodes.GetN()-1){
			// cout<<t<<endl;
			Ptr<Node> thisNode = *i;
			// Ipv4Address sourceAdr = GetNodeIpv4Address(thisNode);
			Ipv4Address source =GetNodeIpv4Address(thisNode);
			stringstream ss;
			ss<<source;//将源地址打包发送出去
			stringstream pktContents(ss.str().c_str());//路由广播从SinkNode发出来的时候，数据里面的跳数是0，/后面是sinkSource的Ipv4Address
			Ptr<Packet> pkt = Create<Packet>((uint8_t *) pktContents.str().c_str(),
					pktContents.str().length());
			Ipv4Header ipv4Header;
			ipv4Header.SetSource(source);
			pkt->AddHeader(ipv4Header);
			//所有的源节点都将数据发送到接收器
			Ptr<Socket> socket = Socket::CreateSocket (nodes.Get (t++), tid);
			socket->Connect (remote);
			Simulator::Schedule (Seconds (0.5), &GenerateTraffic, ipv4Header,pkt,socket, totalPackets, interPacketInterval);
		}
		
	}
}


//仿真结束以后的统计
void finalRecord(){
	//仿真结束以后，会调用如上方法，所以，如下会最后输出。
	  std::cout<<"\n\n***** OUTPUT *****\n\n";
	  std::cout<<"Total Packets sent = "<<packetsSent<<std::endl;
	  std::cout<<"Total Packets received = "<<packetsReceived<<std::endl;
	  std::cout<<"Packet delivery ratio = "<<(float)(packetsReceived/packetsSent)*100<<" %"<<std::endl;
	  std::cout<<"startTime = "<<simStartRealTime<<std::endl;
	  std::cout<<"finishTime = "<<simFinishRealTime<<std::endl;
	  std::cout<<"Final throughPut = "<<(float)(packetsReceived*1024*8)/((simFinishRealTime-simStartRealTime)/1000000)<<std::endl;
}
//主函数入口
int main(int argc, char **argv)
{

//  LogComponentEnable("mobility", LOG_LEVEL_DEBUG);
  simStartRealTime = clock();
//  std::cout << "Creating " << (unsigned)size << " nodes " <<step << " m apart.\n";

  //此处用于测试群体移动模型   关键代码。
  // nodes1.Create(30);
  // std::string traceFile = "scratch/rpgmfile.ns_params";
  // Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);
  // ns2.Install ();
  //测试实体移动模型
//  nodes.Create(size);
//  mobileSinkNode.Create(1);
  createNode();
  createMobilityModel();
  createWifiDevice();
  installInternetStack();
  createSocketCallBack();

//  startSimulation();


  //生成XML文件
  std::cout << "Starting simulation for " << totalTime << " s ...\n";

  AnimationInterface *Panim = new AnimationInterface("/home/zlb/sim_temp/aodv/output.xml");
//   AnimationInterface anim ("/home/zlb/sim_temp/aodv/output.xml");
  Panim->UpdateNodeColor(mobileSinkNode.Get(0), 0, 255, 0);//接收器颜色设置成绿色。


	      
  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  Simulator::Destroy ();

  simFinishRealTime = clock();
  finalRecord();

}
