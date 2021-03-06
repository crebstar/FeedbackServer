#ifndef included_ConnectedUDPClient
#define included_ConnectedUDPClient
#pragma once

#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")


#define WIN32_LEAN_AND_MEAN
#include <windows.h>


#include "../../CBEngine/EngineCode/Vector2.hpp"

#include "UDPServer.hpp"

class ConnectedUDPClient {
public:
	static int											s_numberOfClients;

	~ConnectedUDPClient();
	ConnectedUDPClient();

	double												m_timeStampSecondsForLastPacketReceived;

	cbengine::Vector2									m_position;
	char												m_red;
	char												m_green;
	char												m_blue;

	sockaddr_in											m_clientAddress;
	std::string											m_userID;
	int													m_playerID;

	std::map<int,PlayerDataPacket>						m_reliablePacketsSentButNotAcked;

protected:

	void assignColorForPlayer();

private:


};

#endif