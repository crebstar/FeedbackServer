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
#include "../../CBEngine/EngineCode/Vector3D.hpp"
#include "../../CBEngine/EngineCode/EulerAngles.hpp"

#include "UDPServer.hpp"
#include "FinalPacket.hpp"

const unsigned int	NOT_IN_GAME_ID = 0;
const int STARTING_HEALTH = 1;

class ConnectedUDPClient {
public:
	static int											s_numberOfClients;

	~ConnectedUDPClient();
	ConnectedUDPClient();

	double												m_timeStampSecondsForLastPacketReceived;

	cbengine::Vector3<float>							m_position;
	cbengine::Vector3<float>							m_velocity;
	cbengine::Vector3<float>							m_acceleration;
	EulerAngles											m_orientationDegrees;

	unsigned char										m_red;
	unsigned char										m_green;
	unsigned char										m_blue;

	sockaddr_in											m_clientAddress;
	std::string											m_userID;
	int													m_playerID;
	bool												m_isInLobby;
	unsigned int										m_gameID;
	unsigned int										m_clientID;
	int													m_health;
	int													m_score;

	std::map<int,FinalPacket>							m_reliablePacketsSentButNotAcked;

protected:

	void assignColorForPlayer();

private:


};

#endif