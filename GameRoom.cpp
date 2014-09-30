#include "GameRoom.hpp"

#include <vector>

#include "../../CBEngine/EngineCode/TimeUtil.hpp"
#include "../../CBEngine/EngineCode/MathUtil.hpp"

#include "ConnectedUDPClient.hpp"
#include "UDPServer.hpp"
#include "GameLobby.hpp"



GameRoom::~GameRoom() {

}


GameRoom::GameRoom( unsigned int gameRoomNum, ConnectedUDPClient* owner, UDPServer* server, GameLobby* lobby ) {

	setGameRoomDefaults();

	durationSinceLastPacketUpdate = 0.0;
	lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();
	m_gameRoomNum = gameRoomNum;
	m_owner = owner;
	m_server = server;
	m_lobby = lobby;

	if ( m_owner != nullptr ) {

		addPlayer( m_owner );
	}
}



void GameRoom::updateGameRoom() {

	sendUpdatePacketsToPlayers();
}


void GameRoom::OnClientPacketReceived( ConnectedUDPClient* client, const CS6Packet& playerData ) {

	if ( client == nullptr ) {

		return;
	}

	if ( playerData.packetType == TYPE_Acknowledge ) {

		double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
		int ackCountID = playerData.data.acknowledged.packetNumber;

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

			std::map<int,CS6Packet>::iterator itAck;
			itAck = client->m_reliablePacketsSentButNotAcked.find( ackCountID );
			if ( itAck != client->m_reliablePacketsSentButNotAcked.end() ) {

				client->m_reliablePacketsSentButNotAcked.erase( itAck );

			}

		} else if ( playerData.data.acknowledged.packetType == TYPE_JoinLobby ) {

			std::map<int,CS6Packet>::iterator itAck;
			itAck = client->m_reliablePacketsSentButNotAcked.find( ackCountID );
			if ( itAck != client->m_reliablePacketsSentButNotAcked.end() ) {

				client->m_reliablePacketsSentButNotAcked.erase( itAck );

			}

		}

	} else if ( playerData.packetType == TYPE_Victory ) {

		m_server->m_flag.resetPositionOfFlag( ARENA_WIDTH, ARENA_HEIGHT );
		sendVictoryAndResetPacketToAllClients( playerData );

	} else if ( playerData.packetType == TYPE_Update ) {

		double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
		client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;
		client->m_position.x = playerData.data.updated.xPosition;
		client->m_position.y = playerData.data.updated.yPosition;
		client->m_velocity.x = playerData.data.updated.xVelocity;
		client->m_velocity.y = playerData.data.updated.yVelocity;
		client->m_orientationDegrees = playerData.data.updated.yawDegrees;

	} else if ( playerData.packetType == TYPE_Reset ) {

		printf( "WARNING-> Received a packet of type Reset from the client. This is NOT allowed!\n" );
		printf( "Packet received from client: %s\n", client->m_userID.c_str() );
	}
}


void GameRoom::addPlayer( ConnectedUDPClient* playerToAdd ) {

	if ( playerToAdd == nullptr || m_server == nullptr ) {

		return;
	}

	std::set<ConnectedUDPClient*>::iterator itPlayer;
	itPlayer = m_players.find( playerToAdd );

	if ( itPlayer == m_players.end() ) {

		playerToAdd->m_isInLobby = false;
		playerToAdd->m_gameID = m_gameRoomNum;

		m_players.insert( playerToAdd );
		printf( "Added client: %s to GameRoom #%d", playerToAdd->m_userID.c_str(), m_gameRoomNum );

		CS6Packet resetPacket;

		resetPacket.packetType = TYPE_Reset;
		resetPacket.timestamp = cbutil::getCurrentTimeSeconds();
		resetPacket.playerColorAndID[0] = playerToAdd->m_red;
		resetPacket.playerColorAndID[1] = playerToAdd->m_green;
		resetPacket.playerColorAndID[2] = playerToAdd->m_blue;

		float randomZeroToOne = cbengine::getRandomZeroToOne();
		
		resetPacket.data.reset.flagXPosition = m_server->m_flag.m_xPos;
		resetPacket.data.reset.flagYPosition = m_server->m_flag.m_yPos;
		resetPacket.data.reset.playerColorAndID[0] = resetPacket.playerColorAndID[0];
		resetPacket.data.reset.playerColorAndID[1] = resetPacket.playerColorAndID[1];
		resetPacket.data.reset.playerColorAndID[2] = resetPacket.playerColorAndID[2];
		resetPacket.data.reset.playerXPosition = ARENA_WIDTH * randomZeroToOne;
		randomZeroToOne = cbengine::getRandomZeroToOne();
		resetPacket.data.reset.playerYPosition = ARENA_HEIGHT * randomZeroToOne;

		playerToAdd->m_position.x = resetPacket.data.reset.playerXPosition;
		playerToAdd->m_position.y = resetPacket.data.reset.playerYPosition;

		m_server->sendPacket( resetPacket, playerToAdd, true );
	}
}


void GameRoom::sendUpdatePacketsToPlayers() {

	if ( m_server == nullptr ) {

		return;
	}


	double currentTimeSeconds = cbutil::getCurrentTimeSeconds();
	double timeDifSeconds = currentTimeSeconds - lastTimeStampSeconds;

	durationSinceLastPacketUpdate += timeDifSeconds;

	if ( durationSinceLastPacketUpdate > TIME_DIF_SECONDS_FOR_PACKET_UPDATE ) {

		durationSinceLastPacketUpdate = 0.0;

		std::vector<CS6Packet> playerPackets;	
		int winSockSendResult = 0;

		std::set<ConnectedUDPClient*>::iterator itClient;
		for ( itClient = m_players.begin(); itClient != m_players.end(); ++itClient ) {

			ConnectedUDPClient* client = *(itClient);

			CS6Packet playerData;
			playerData.packetType = TYPE_Update;
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

		std::set<ConnectedUDPClient*>::iterator itClientPacket;
		for ( itClientPacket = m_players.begin(); itClientPacket != m_players.end(); ++itClientPacket ) {

			ConnectedUDPClient* client = *(itClientPacket);

			for ( int i = 0; i < static_cast<int>( playerPackets.size() ); ++i ) {

				CS6Packet& packetToSend = playerPackets[i];
				m_server->sendPacket( packetToSend, client, false );
			
				if ( winSockSendResult == SOCKET_ERROR ) {

					printf( "send function for update type packets call failed with error number: %d\n", WSAGetLastError() );
				}
			}
		}
	}

	lastTimeStampSeconds = currentTimeSeconds;
}


void GameRoom::endGameRoom() {

	if ( m_lobby == nullptr || m_server == nullptr ) {

		return;
	}

	std::set<ConnectedUDPClient*>::iterator itPlayer;
	for ( itPlayer = m_players.begin(); itPlayer != m_players.end(); ++itPlayer ) {

		ConnectedUDPClient* client = *(itPlayer);
		m_lobby->addUserToLobby( client );
		client->m_isInLobby = true;
		client->m_gameID = 0;

		CS6Packet lobbyPacket;
		lobbyPacket.packetType = TYPE_JoinLobby;
		lobbyPacket.timestamp = cbutil::getCurrentTimeSeconds();

		lobbyPacket.data.joinLobby.playerColorAndID[0] = client->m_red;
		lobbyPacket.data.joinLobby.playerColorAndID[1] = client->m_green;
		lobbyPacket.data.joinLobby.playerColorAndID[2] = client->m_blue;

		m_server->sendPacket( lobbyPacket, client, false );
	}
}


void GameRoom::sendVictoryAndResetPacketToAllClients( const CS6Packet& victoryPacketFromWinner ) {

	if ( m_server == nullptr ) {

		return;
	}

	CS6Packet victoryPacketToSend;
	victoryPacketToSend.packetType = TYPE_Victory;

	victoryPacketToSend.timestamp = cbutil::getCurrentTimeSeconds();
	victoryPacketToSend.data.victorious.playerColorAndID[0] = victoryPacketFromWinner.data.victorious.playerColorAndID[0];
	victoryPacketToSend.data.victorious.playerColorAndID[1] = victoryPacketFromWinner.data.victorious.playerColorAndID[1];
	victoryPacketToSend.data.victorious.playerColorAndID[2] = victoryPacketFromWinner.data.victorious.playerColorAndID[2];


	std::set<ConnectedUDPClient*>::iterator itClient;
	for ( itClient = m_players.begin(); itClient != m_players.end(); ++itClient ) {

		ConnectedUDPClient* client = *(itClient);
		victoryPacketToSend.playerColorAndID[0] = client->m_red;
		victoryPacketToSend.playerColorAndID[1] = client->m_green;
		victoryPacketToSend.playerColorAndID[2] = client->m_blue;
		victoryPacketToSend.packetNumber = m_server->m_currentAckCount;

		m_server->sendPacket( victoryPacketToSend, client, true );
		
		CS6Packet resetPacketToSend;
		resetPacketToSend.packetType = TYPE_Reset;
		resetPacketToSend.playerColorAndID[0] = client->m_red;
		resetPacketToSend.playerColorAndID[1] = client->m_green;
		resetPacketToSend.playerColorAndID[2] = client->m_blue;
		resetPacketToSend.timestamp = cbutil::getCurrentTimeSeconds();
		resetPacketToSend.packetNumber = m_server->m_currentAckCount;

		resetPacketToSend.data.reset.flagXPosition = m_server->m_flag.m_xPos;
		resetPacketToSend.data.reset.flagYPosition = m_server->m_flag.m_yPos;
		resetPacketToSend.data.reset.playerXPosition = client->m_position.x;
		resetPacketToSend.data.reset.playerYPosition = client->m_position.y;
		resetPacketToSend.data.reset.playerColorAndID[0] = client->m_red;
		resetPacketToSend.data.reset.playerColorAndID[1] = client->m_green;
		resetPacketToSend.data.reset.playerColorAndID[2] = client->m_blue;

	
		m_server->sendPacket( resetPacketToSend,client, true );
	}
}


void GameRoom::setGameRoomDefaults() {

	m_server = nullptr;
	m_owner = nullptr;
	m_lobby = nullptr;
	m_gameRoomNum = 0;
}