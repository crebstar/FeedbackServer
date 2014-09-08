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

#include "../../CBEngine/EngineCode/TimeUtil.hpp"
#include "../../CBEngine/EngineCode/MathUtil.hpp"

UDPServer::~UDPServer() {

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

		sendPlayerDataToClients();
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

		// Update existing client
		if ( playerData.packetType == TYPE_Acknowledge ) {

	
			double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
			int ackCountID = playerData.data.acknowledged.packetNumber;

			ConnectedUDPClient* client = itClient->second;
			client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;

			if ( playerData.data.acknowledged.packetType == TYPE_Reset ) {

				std::map<int,CS6Packet>::iterator itAck;
				itAck = client->m_reliablePacketsSentButNotAcked.find( ackCountID );
				if ( itAck != client->m_reliablePacketsSentButNotAcked.end() ) {

					client->m_reliablePacketsSentButNotAcked.erase( itAck );

				} else {

					printf( "WARNING-> Could not find packet with corresponding AckCountID" );
				}

			} else if ( playerData.data.acknowledged.packetType == TYPE_Victory ) {
				// TODO::

			}

		} else if ( playerData.packetType == TYPE_Victory ) {
			// TODO::
			

		} else if ( playerData.packetType == TYPE_Update ) {

			ConnectedUDPClient* client = itClient->second;
			double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
			client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;
			client->m_position.x = playerData.data.updated.xPosition;
			client->m_position.y = playerData.data.updated.yPosition;
			client->m_velocity.x = playerData.data.updated.xVelocity;
			client->m_velocity.y = playerData.data.updated.yVelocity;
			client->m_orientationDegrees = playerData.data.updated.yawDegrees;

		} else if ( playerData.packetType == TYPE_Reset ) {

			ConnectedUDPClient* client = itClient->second;
			printf( "WARNING-> Received a packet of type Reset from the client. This is NOT allowed!\n" );
			printf( "Packet received from client: %s\n", client->m_userID.c_str() );
		}
	
	} else {

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

			CS6Packet newPlayerResetPacket;
			// Header
			newPlayerResetPacket.packetType = TYPE_Reset;
			newPlayerResetPacket.timestamp = cbutil::getCurrentTimeSeconds();
			newPlayerResetPacket.playerColorAndID[0] = client->m_red;
			newPlayerResetPacket.playerColorAndID[1] = client->m_green;
			newPlayerResetPacket.playerColorAndID[2] = client->m_blue;

			// Data
			newPlayerResetPacket.data.reset.flagXPosition = m_flag.m_xPos;
			newPlayerResetPacket.data.reset.flagYPosition = m_flag.m_yPos;
			newPlayerResetPacket.data.reset.playerXPosition = client->m_position.x;
			newPlayerResetPacket.data.reset.playerYPosition = client->m_position.y;
			newPlayerResetPacket.data.reset.playerColorAndID[0] = client->m_red;
			newPlayerResetPacket.data.reset.playerColorAndID[1] = client->m_green;
			newPlayerResetPacket.data.reset.playerColorAndID[2] = client->m_blue;

			++m_currentAckCount;

			client->m_reliablePacketsSentButNotAcked.insert( std::pair<int,CS6Packet>( m_currentAckCount, newPlayerResetPacket ) );

			int winSockSendResult = 0;
			winSockSendResult = sendto( m_listenSocket, (char*) &newPlayerResetPacket, sizeof( newPlayerResetPacket ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );

			if ( winSockSendResult == SOCKET_ERROR ) {

				printf( "send function call failed with error number: %d\n", WSAGetLastError() );
			}

		} else {

			printf( "WARNING-> Packet received from unrecognized client which is not an ACK for joining server" );
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

		std::vector<CS6Packet> playerPackets;
		int winSockSendResult = 0;

		std::map<std::string,ConnectedUDPClient*>::iterator itClient;
		for ( itClient = m_clients.begin(); itClient != m_clients.end(); ++itClient ) {

			ConnectedUDPClient* client = itClient->second;

			++m_currentAckCount;

			CS6Packet playerData;
			playerData.packetType = TYPE_Update;
			playerData.packetNumber = m_currentAckCount;
			playerData.timestamp = cbutil::getCurrentTimeSeconds();
			playerData.playerColorAndID[0] = client->m_red;
			playerData.playerColorAndID[1] = client->m_green;
			playerData.playerColorAndID[2] = client->m_blue;

			playerData.data.updated.xPosition = client->m_position.x;
			playerData.data.updated.yPosition = client->m_position.y;
			playerData.data.updated.xVelocity = client->m_velocity.x;
			playerData.data.updated.yVelocity = client->m_velocity.y;
			playerData.data.updated.yawDegrees = client->m_orientationDegrees;

			playerPackets.push_back( playerData );
		}

		std::map<std::string,ConnectedUDPClient*>::iterator itClientPacket;
		for ( itClientPacket = m_clients.begin(); itClientPacket != m_clients.end(); ++itClientPacket ) {

			ConnectedUDPClient* client = itClientPacket->second;

			for ( int i = 0; i < static_cast<int>( playerPackets.size() ); ++i ) {

				CS6Packet& packetToSend = playerPackets[i];

				// For this assignment we are not doung guarenteed delivery for updates
				//client->m_reliablePacketsSentButNotAcked.insert( std::pair<int,PlayerDataPacket>( packetToSend.m_packetAckID, packetToSend ) );

				//float randomNumberZeroToOne = cbengine::getRandomZeroToOne();
				//if ( randomNumberZeroToOne < m_thresholdForPacketLossSimulation ) {
					// Send 
					winSockSendResult = sendto( m_listenSocket, (char*) &packetToSend, sizeof( packetToSend ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );

				//} else {
					// Don't send but act like we did
					// Leaving this block for testing purposes
				//}

				if ( winSockSendResult == SOCKET_ERROR ) {

					printf( "send function for update type packets call failed with error number: %d\n", WSAGetLastError() );
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
				winSockSendResult = sendto( m_listenSocket, (char*) &packet, sizeof( packet ), 0, (sockaddr*) &client->m_clientAddress, sizeof( client->m_clientAddress ) );
			}
		}
	}
}