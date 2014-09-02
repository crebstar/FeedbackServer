#ifndef includedUDPServer
#define includedUDPServer
#pragma once

#include <string>
#include <map>

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ConnectedUDPClient.hpp"

const char PLAYER_DATA_PACKET_ID = 2;
const char PLAYER_EXIT_DATA_PACKET_ID = 4;

struct PlayerDataPacket {
public:
	PlayerDataPacket() :
	  m_packetID( PLAYER_DATA_PACKET_ID ),
		  m_red( 250 ),
		  m_green( 250 ),
		  m_blue( 250 ),
		  m_playerID( -1 ),
		  m_xPos( 0.0f ),
		  m_yPos( 0.0f )
	  {}

	  unsigned char		m_packetID;
	  unsigned char		m_red;
	  unsigned char		m_green;
	  unsigned char		m_blue;
	  int				m_playerID;
	  float				m_xPos;
	  float				m_yPos;
};

const int	 NEW_PLAYER_ACK_ID = 3;
const double DURATION_THRESHOLD_FOR_DISCONECT = 5.0;
const double TIME_DIF_SECONDS_FOR_USER_DISPLAY = 5.5;
const double TIME_DIF_SECONDS_FOR_PACKET_UPDATE = 0.0045;

class UDPServer {
public:
	~UDPServer();
	explicit UDPServer( const std::string& ipAddress, const std::string& portNumber );

	void initializeAndRun();

	void initialize();
	void run();

protected:

	SOCKET												m_listenSocket;

	std::string											m_IPAddress;
	std::string											m_PortNumber;

	bool												m_serverShouldRun;

	std::map<std::string,ConnectedUDPClient*>			m_clients;  // IPandPort = Key | Last packet received is value
	double												m_durationSinceLastUserConnectedUpdate;
	double												m_durationSinceLastPacketUpdate;

private:

	void convertIPAndPortToSingleString( char* ipAddress, int portNumber, std::string& out_combinedIPAndPort );
	void updateOrCreateNewClient( const std::string& combinedIPAndPort, const sockaddr_in& clientAddress, const PlayerDataPacket& playerData );
	void checkForExpiredClients();
	void displayConnectedUsers();

	void sendPlayerDataToClients();
};


#endif