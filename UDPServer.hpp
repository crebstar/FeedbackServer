#ifndef includedUDPServer
#define includedUDPServer
#pragma once

#include <string>
#include <map>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "CS6Packet.hpp"
#include "CTFlag.hpp"


const float ARENA_WIDTH = 500.0f;
const float ARENA_HEIGHT = 500.0f;

const char PLAYER_DATA_PACKET_ID = 2;
const char PLAYER_EXIT_DATA_PACKET_ID = 4;
const char RELIABLE_ACK_ID	= 30;
const int  PACKET_ACK_ID_NON_RELIABLE = -1;

struct PlayerDataPacket {
public:
	PlayerDataPacket() :
	  m_packetID( PLAYER_DATA_PACKET_ID ),
		  m_red( 250 ),
		  m_green( 250 ),
		  m_blue( 250 ),
		  m_playerID( -1 ),
		  m_xPos( 0.0f ),
		  m_yPos( 0.0f ),
		  m_packetAckID( PACKET_ACK_ID_NON_RELIABLE ),
		  m_packetTimeStamp( 0.0 )
	  {}

	  unsigned char		m_packetID;
	  unsigned char		m_red;
	  unsigned char		m_green;
	  unsigned char		m_blue;
	  float				m_xPos;
	  float				m_yPos;
	  int				m_packetAckID;
	  int				m_playerID;
	  double			m_packetTimeStamp;
};


const int	 NEW_PLAYER_ACK_ID = 3;
const double DURATION_THRESHOLD_FOR_DISCONECT = 10.0; // FOR TESTING
const double TIME_DIF_SECONDS_FOR_USER_DISPLAY = 5.5;
const double TIME_DIF_SECONDS_FOR_PACKET_UPDATE = 0.0045;
const double TIME_THRESHOLD_TO_RESEND_RELIABLE_PACKETS = 0.350;

class ConnectedUDPClient;
class GameLobby;
class GameRoom;

class UDPServer {
public:
	friend class GameLobby;
	friend class GameRoom;

	~UDPServer();
	explicit UDPServer( const std::string& ipAddress, const std::string& portNumber );

	void initializeAndRun();

	void initialize();
	void run();

	void sendPacket( CS6Packet& packetToSend, ConnectedUDPClient* client, bool bIsReliable );



protected:

	SOCKET												m_listenSocket;

	std::string											m_IPAddress;
	std::string											m_PortNumber;

	bool												m_serverShouldRun;

	std::map<std::string,ConnectedUDPClient*>			m_clients;  // IPandPort = Key | Last packet received is value
	double												m_durationSinceLastUserConnectedUpdate;
	double												m_durationSinceLastPacketUpdate;

	// Reliable Stuff
	
	// Guarentee Delivery
	float												m_thresholdForPacketLossSimulation;
	int													m_currentAckCount;

	GameLobby*											m_lobby;
	CTFlag												m_flag;

private:

	void convertIPAndPortToSingleString( char* ipAddress, int portNumber, std::string& out_combinedIPAndPort );
	void updateOrCreateNewClient( const std::string& combinedIPAndPort, const sockaddr_in& clientAddress, const CS6Packet& playerData );
	void checkForExpiredClients();
	void displayConnectedUsers();
	
	// Guarenteed Delivery
	void checkForExpiredReliablePacketsWithNoAcks();
};


#endif