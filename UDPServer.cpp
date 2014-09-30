#include "UDPServer.hpp"
#include <stdio.h>
#include <iostream>

#include <vector>
#include <random>
#include <time.h>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

#include "ConnectedUDPClient.hpp"
#include "GameLobby.hpp"
#include "GameRoom.hpp"

#include "../../CBEngine/EngineCode/TimeUtil.hpp"
#include "../../CBEngine/EngineCode/MathUtil.hpp"

UDPServer::~UDPServer() {

	if ( m_lobby ) {

		delete m_lobby;
	}
}


UDPServer::UDPServer( const std::string& ipAddress, const std::string& portNumber ) {

	m_thresholdForPacketLossSimulation = 1.00f;

	m_IPAddress = ipAddress;
	m_PortNumber = portNumber;

	m_durationSinceLastUserConnectedUpdate = 0.0;
	m_durationSinceLastPacketUpdate = 0.0;
	
	m_currentAckCount = 0;

	srand( time( nullptr ) );

	m_flag.resetPositionOfFlag( ARENA_WIDTH, ARENA_HEIGHT );

	m_lobby = new GameLobby( this );
}


void UDPServer::initialize() {

	printf( "\n\nAttempting to create UDP Server with IP: %s and Port: %s \n", m_IPAddress.c_str(), m_PortNumber.c_str() );

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

		CS6Packet clientPacketReceived;

		sockaddr_in clientSocketAddr;
		int sizeOfResultAddress = sizeof( clientSocketAddr );
		winSockResult = recvfrom( m_listenSocket, (char*) &clientPacketReceived, sizeof( CS6Packet ), 0, (sockaddr*) &clientSocketAddr, &sizeOfResultAddress );

		if ( winSockResult > 0 ) {

			char* connectedIP = inet_ntoa( clientSocketAddr.sin_addr );
			int portNumber = ntohs( clientSocketAddr.sin_port );
			
			std::string combinedIPAndPort;
			convertIPAndPortToSingleString( connectedIP, portNumber, combinedIPAndPort );
			updateOrCreateNewClient( combinedIPAndPort, clientSocketAddr, clientPacketReceived );
		} 

		if ( m_lobby ) {

			m_lobby->updateLobby();
		}

		checkForExpiredReliablePacketsWithNoAcks();
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


void UDPServer::updateOrCreateNewClient( const std::string& combinedIPAndPort, const sockaddr_in& clientAddress, const CS6Packet& playerData ) {

	std::map<std::string,ConnectedUDPClient*>::iterator itClient;

	itClient = m_clients.find( combinedIPAndPort );

	if ( itClient != m_clients.end() ) {

		// ******************** Client Exists ******************** //
		ConnectedUDPClient* client = itClient->second;
		
		if ( m_lobby != nullptr ) {

			m_lobby->OnClientPacketReceived( client, playerData );
		}
		
	} else {

		// ******************** New Client ******************** //

		if ( playerData.packetType == TYPE_Acknowledge ) {

			float randomNumberZeroToOneForX = cbengine::getRandomZeroToOne();
			float randomNumberZeroToOneForY = cbengine::getRandomZeroToOne();

			double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
			ConnectedUDPClient* client = new ConnectedUDPClient;
			client->m_clientAddress = clientAddress;
			client->m_userID = combinedIPAndPort;
			client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;
			client->m_position.x = randomNumberZeroToOneForX * ARENA_WIDTH; // For Now
			client->m_position.y = randomNumberZeroToOneForY * ARENA_HEIGHT;
			m_clients.insert( std::pair<std::string,ConnectedUDPClient*>( combinedIPAndPort, client ) );

			printf( "ACK packet received from a new client!" );
			printf( "A new client has been created: %s \n", combinedIPAndPort.c_str() );

			++m_currentAckCount;
			CS6Packet newPlayerLobbyPacket;
			// Header
			newPlayerLobbyPacket.packetType = TYPE_JoinLobby;
			newPlayerLobbyPacket.timestamp = cbutil::getCurrentTimeSeconds();
			newPlayerLobbyPacket.playerColorAndID[0] = client->m_red;
			newPlayerLobbyPacket.playerColorAndID[1] = client->m_green;
			newPlayerLobbyPacket.playerColorAndID[2] = client->m_blue;
			newPlayerLobbyPacket.packetNumber = m_currentAckCount;

			// Data
			newPlayerLobbyPacket.data.joinLobby.playerColorAndID[0] = client->m_red;
			newPlayerLobbyPacket.data.joinLobby.playerColorAndID[1] = client->m_green;
			newPlayerLobbyPacket.data.joinLobby.playerColorAndID[2] = client->m_red;

			client->m_reliablePacketsSentButNotAcked.insert( std::pair<int,CS6Packet>( m_currentAckCount, newPlayerLobbyPacket ) );
			
			if ( m_lobby != nullptr ) {

				m_lobby->addUserToLobby( client );
			}

			int winSockSendResult = 0;
			winSockSendResult = sendto( m_listenSocket, (char*) &newPlayerLobbyPacket, sizeof( newPlayerLobbyPacket ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );

			if ( winSockSendResult == SOCKET_ERROR ) {

				printf( "send join lobby packet call failed with error number: %d\n", WSAGetLastError() );
			}

		} else {

			printf( "WARNING-> Packet received from unrecognized client which is not an ACK for joining server" );
		}
	}
}




void UDPServer::sendPacket( CS6Packet& packetToSend, ConnectedUDPClient* client, bool bIsReliable ) {

	if ( client == nullptr ) {

		return;
	}

	++m_currentAckCount;
	packetToSend.packetNumber = m_currentAckCount;

	int winSockSendResult = 0;
	winSockSendResult = sendto( m_listenSocket, (char*) &packetToSend, sizeof( CS6Packet ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );

	if ( bIsReliable ) {

		if ( packetToSend.packetType == TYPE_JoinLobby ) 
		{

			int x = 3;
		}

		client->m_reliablePacketsSentButNotAcked.insert( std::pair<int,CS6Packet>( packetToSend.packetNumber, packetToSend ) );
	}
}


void UDPServer::checkForExpiredClients() {

	std::map<std::string,ConnectedUDPClient*>::iterator itClient;
	std::vector<std::string> clientsToRemove;

	for ( itClient = m_clients.begin(); itClient != m_clients.end(); ++itClient ) {

		ConnectedUDPClient* client = itClient->second;

		if ( client->m_isInLobby ) {
			continue;
		}

		double lastTimePacketReceived = client->m_timeStampSecondsForLastPacketReceived;
		double currentTimeSeconds = cbutil::getCurrentTimeSeconds();

		double secondsSinceLastPacketReceived = currentTimeSeconds - lastTimePacketReceived;

		if ( secondsSinceLastPacketReceived > DURATION_THRESHOLD_FOR_DISCONECT ) {

			clientsToRemove.push_back( itClient->first );

			if ( !client->m_isInLobby && m_lobby != nullptr ) {

				m_lobby->removeClientDueToInactivity( client );
			} 
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


void UDPServer::checkForExpiredReliablePacketsWithNoAcks() {

	double currentTimeSeconds = cbutil::getCurrentTimeSeconds();

	std::map<std::string,ConnectedUDPClient*>::iterator itClient;
	for ( itClient = m_clients.begin(); itClient != m_clients.end(); ++itClient ) {

		ConnectedUDPClient* client = itClient->second;

		std::map<int,CS6Packet>::iterator itRel;
		for ( itRel = client->m_reliablePacketsSentButNotAcked.begin(); itRel != client->m_reliablePacketsSentButNotAcked.end(); ++itRel ) {

			int ackCountIDForPacket = itRel->first;
			CS6Packet& packet = itRel->second;
			double timeStampForSendPacket = packet.timestamp;

			double timeDifSeconds = currentTimeSeconds - timeStampForSendPacket;

			if ( timeDifSeconds > TIME_THRESHOLD_TO_RESEND_RELIABLE_PACKETS ) {

				packet.timestamp = currentTimeSeconds;
				int winSockSendResult = 0;

				if ( packet.packetType == TYPE_JoinLobby ) 
				{

					int x = 3;
				}

				winSockSendResult = sendto( m_listenSocket, (char*) &packet, sizeof( packet ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );
			}
		}
	}
}