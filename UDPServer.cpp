#include "UDPServer.hpp"


#include <stdio.h>
#include <iostream>

#include <vector>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

#include "../../CBEngine/EngineCode/TimeUtil.hpp"

UDPServer::~UDPServer() {

}


UDPServer::UDPServer( const std::string& ipAddress, const std::string& portNumber ) {

	m_IPAddress = ipAddress;
	m_PortNumber = portNumber;
}


void UDPServer::initialize() {

	printf( "\n\nAttempting to create UDP Server with IP: %s and Port: %s \n", m_IPAddress.c_str(), m_PortNumber.c_str() );

	m_durationSinceLastUserConnectedUpdate = 0.0;
	m_durationSinceLastPacketUpdate = 0.0;

	WSAData wsaData;
	int winSockResult = 0;

	m_listenSocket = INVALID_SOCKET;

	struct addrinfo* result = nullptr;
	struct addrinfo hints;

	winSockResult = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	if ( winSockResult != 0 ) {

		printf( "WSAStartup failed with error number: %d\n", winSockResult );
		return;
	}

	ZeroMemory( &hints, sizeof( hints ) );
	hints.ai_family		= AF_INET;
	hints.ai_socktype	= SOCK_DGRAM;
	hints.ai_protocol	= IPPROTO_UDP;
	hints.ai_flags		= AI_PASSIVE;


	winSockResult = getaddrinfo( m_IPAddress.c_str(), m_PortNumber.c_str(), &hints, &result );
	if ( winSockResult != 0 ) {

		printf( "getaddrinfo function call failed with error number: %d\n", winSockResult );
		WSACleanup();

		return;
	}

	// Initialize the ListenSocket ( Connect, Bind, then listen )
	m_listenSocket = socket( result->ai_family, result->ai_socktype, result->ai_protocol );
	if ( m_listenSocket == INVALID_SOCKET ) {

		printf( "socket function call failed with error number: %ld\n", WSAGetLastError() );

		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	winSockResult = bind( m_listenSocket, result->ai_addr, static_cast<int>( result->ai_addrlen ) );

	if ( winSockResult == SOCKET_ERROR ) {

		printf( "Bind to listenSocket failed with error number: %d\n", WSAGetLastError() );

		freeaddrinfo( result );
		closesocket( m_listenSocket );
		WSACleanup();

		return;
	}

	u_long iMode = 1; // 0 = blocking ... != 0 is non blocking
	winSockResult = ioctlsocket( m_listenSocket, FIONBIO, &iMode );
	if ( winSockResult != NO_ERROR ) {

		printf( "ioctlsocket failed with error: %ld\n", winSockResult );
	}

	freeaddrinfo(result);
}


void UDPServer::run() {

	int winSockResult = 0;
	int winSockSendResult = 0;
	m_serverShouldRun = true;

	while ( m_serverShouldRun ) {

		checkForExpiredClients();
		displayConnectedUsers();

		PlayerDataPacket packetReceived;

		sockaddr_in clientSocketAddr;
		int sizeOfResultAddress = sizeof( clientSocketAddr );
		winSockResult = recvfrom( m_listenSocket, (char*) &packetReceived, sizeof( PlayerDataPacket ), 0, (sockaddr*) &clientSocketAddr, &sizeOfResultAddress );

		if ( winSockResult > 0 ) {

			char* connectedIP = inet_ntoa( clientSocketAddr.sin_addr );
			int portNumber = ntohs( clientSocketAddr.sin_port );
			
			std::string combinedIPAndPort;
			convertIPAndPortToSingleString( connectedIP, portNumber, combinedIPAndPort );
			updateOrCreateNewClient( combinedIPAndPort, clientSocketAddr, packetReceived );

			// Debug Stuff
			//printf( "Num Bytes received: %d\n", winSockResult );
			//printf( "Client IP of packet sent: %s  Port: %d \n", connectedIP, portNumber );
			//printf( "Position X: %f", packetReceived.m_xPos );
			//printf( "Position Y: %f", packetReceived.m_yPos );
			
		} 

		sendPlayerDataToClients();
	} 

	WSACleanup();

	printf( "UDP Server has finished executing\n\n" );
}


void UDPServer::convertIPAndPortToSingleString( char* ipAddress, int portNumber, std::string& out_combinedIPAndPort ) {

	char* portNumAsCString = new char[32];
	itoa( portNumber, portNumAsCString, 10 );

	out_combinedIPAndPort += ipAddress;
	out_combinedIPAndPort += portNumAsCString;

	delete[] portNumAsCString;
}


void UDPServer::updateOrCreateNewClient( const std::string& combinedIPAndPort, const sockaddr_in& clientAddress, const PlayerDataPacket& playerData ) {

	std::map<std::string,ConnectedUDPClient*>::iterator itClient;

	itClient = m_clients.find( combinedIPAndPort );

	if ( itClient != m_clients.end() ) {

		// Update existing client
		ConnectedUDPClient* client = itClient->second;
		double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
		client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;
		client->m_position.x = playerData.m_xPos;
		client->m_position.y = playerData.m_yPos;

	} else {

		double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
		ConnectedUDPClient* client = new ConnectedUDPClient;
		client->m_clientAddress = clientAddress;
		client->m_userID = combinedIPAndPort;
		client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;
		client->m_position.x = playerData.m_xPos;
		client->m_position.y = playerData.m_yPos;
		m_clients.insert( std::pair<std::string,ConnectedUDPClient*>( combinedIPAndPort, client ) );

		printf( "A new client has been created: %s \n", combinedIPAndPort.c_str() );

		
		// Send an ack
		PlayerDataPacket playerData;
		playerData.m_packetID = NEW_PLAYER_ACK_ID;
		playerData.m_playerID = client->m_playerID;
		playerData.m_xPos = client->m_position.x;
		playerData.m_yPos = client->m_position.y;
		playerData.m_red = client->m_red;
		playerData.m_green = client->m_green;
		playerData.m_blue = client->m_blue;

		int winSockSendResult = 0;
		winSockSendResult = sendto( m_listenSocket, (char*) &playerData, sizeof( playerData ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );

		if ( winSockSendResult == SOCKET_ERROR ) {

			printf( "send function call failed with error number: %d\n", WSAGetLastError() );
		}
	}
}


void UDPServer::checkForExpiredClients() {

	std::map<std::string,ConnectedUDPClient*>::iterator itClient;
	std::vector<std::string> clientsToRemove;

	for ( itClient = m_clients.begin(); itClient != m_clients.end(); ++itClient ) {

		ConnectedUDPClient* client = itClient->second;
		double lastTimePacketReceived = client->m_timeStampSecondsForLastPacketReceived;
		double currentTimeSeconds = cbutil::getCurrentTimeSeconds();

		double secondsSinceLastPacketReceived = currentTimeSeconds - lastTimePacketReceived;

		if ( secondsSinceLastPacketReceived > DURATION_THRESHOLD_FOR_DISCONECT ) {

			clientsToRemove.push_back( itClient->first );
		}
	}

	for ( int i = 0; i < static_cast<int>( clientsToRemove.size() ); ++i ) {

		std::map<std::string,ConnectedUDPClient*>::iterator itClientRem;
		itClientRem = m_clients.find( clientsToRemove[i] );

		if ( itClientRem != m_clients.end() ) {

			ConnectedUDPClient* client = itClientRem->second;
			delete client;

			m_clients.erase( itClientRem );

			printf( "\nRemoving client due to inactivity. Client IP and Port: %s \n", clientsToRemove[i].c_str() );
		}
	}
}


void UDPServer::sendPlayerDataToClients() {

	static double lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();

	double currentTimeSeconds = cbutil::getCurrentTimeSeconds();
	double timeDifSeconds = currentTimeSeconds - lastTimeStampSeconds;

	m_durationSinceLastPacketUpdate += timeDifSeconds;

	if ( m_durationSinceLastPacketUpdate > TIME_DIF_SECONDS_FOR_PACKET_UPDATE ) {

		m_durationSinceLastPacketUpdate = 0.0;

		std::vector<PlayerDataPacket> playerPackets;
		int winSockSendResult = 0;

		std::map<std::string,ConnectedUDPClient*>::iterator itClient;
		for ( itClient = m_clients.begin(); itClient != m_clients.end(); ++itClient ) {

			ConnectedUDPClient* client = itClient->second;

			PlayerDataPacket playerData;
			playerData.m_playerID = client->m_playerID;
			playerData.m_xPos = client->m_position.x;
			playerData.m_yPos = client->m_position.y;
			playerData.m_red = client->m_red;
			playerData.m_green = client->m_green;
			playerData.m_blue = client->m_blue;

			playerPackets.push_back( playerData );
		}

		std::map<std::string,ConnectedUDPClient*>::iterator itClientPacket;
		for ( itClientPacket = m_clients.begin(); itClientPacket != m_clients.end(); ++itClientPacket ) {

			ConnectedUDPClient* client = itClientPacket->second;

			for ( int i = 0; i < static_cast<int>( playerPackets.size() ); ++i ) {

				PlayerDataPacket& packetToSend = playerPackets[i];

				winSockSendResult = sendto( m_listenSocket, (char*) &packetToSend, sizeof( packetToSend ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );

				if ( winSockSendResult == SOCKET_ERROR ) {

					printf( "send function call failed with error number: %d\n", WSAGetLastError() );
				}
			}
		}
	}

	lastTimeStampSeconds = currentTimeSeconds;
}


void UDPServer::displayConnectedUsers() {

	static double lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();


	double currentTimeSeconds = cbutil::getCurrentTimeSeconds();
	double timeDifSeconds = currentTimeSeconds - lastTimeStampSeconds;

	m_durationSinceLastUserConnectedUpdate += timeDifSeconds;

	if ( m_durationSinceLastUserConnectedUpdate > TIME_DIF_SECONDS_FOR_USER_DISPLAY ) {

		m_durationSinceLastUserConnectedUpdate = 0.0;

		if ( m_clients.empty() ) {

			printf( "---- There are currently no clients connected ----\n\n" );

		} else {

			printf( "---- Displaying List Of Connected Clients ----\n\n");

			std::map<std::string,ConnectedUDPClient*>::iterator itClient;
			for ( itClient = m_clients.begin(); itClient != m_clients.end(); ++itClient ) {

				ConnectedUDPClient* client = itClient->second;
				printf( "Client with user ID: %s is connected to the server.\n", client->m_userID.c_str() );
			}

			printf( "---- End List Of Connected Clients ----\n\n");
		}
	}

	lastTimeStampSeconds = currentTimeSeconds;
}