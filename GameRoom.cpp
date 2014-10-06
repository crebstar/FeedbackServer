#include "GameRoom.hpp"

#include <vector>
#include <math.h>

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

		addPlayer( m_owner, true );
	}
}



void GameRoom::updateGameRoom() {

	sendUpdatePacketsToPlayers();

	checkForWinCondition();
}


void GameRoom::checkForWinCondition() {

	std::set<ConnectedUDPClient*>::iterator itPlayer;
	for ( itPlayer = m_players.begin(); itPlayer != m_players.end(); ++itPlayer ) {

		ConnectedUDPClient* client = *(itPlayer);
		
		if ( client->m_score >= WINNING_SCORE_NUM ) {

			printf( "User %s has achieved victory in GameRoom: %d ", client->m_userID.c_str(), m_gameRoomNum );

			endGameRoom();
		}
	}
}


void GameRoom::OnClientPacketReceived( ConnectedUDPClient* client, const FinalPacket& playerData ) {

	if ( client == nullptr || m_owner == nullptr ) {

		printf( "Warning-> GameRoom cannot receive packets when client or owner is non existent" );
		return;
	}

	//GAME LOOP
	//	Client->Server: Update, Hit, Fire
	//	Server->Client: Update, Respawn
	
	if ( playerData.type == TYPE_Ack ) {

		double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
		int ackCountID = playerData.data.acknowledged.number;

		client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;

		std::map<int,FinalPacket>::iterator itAck;
		itAck = client->m_reliablePacketsSentButNotAcked.find( ackCountID );
		if ( itAck != client->m_reliablePacketsSentButNotAcked.end() ) {

			client->m_reliablePacketsSentButNotAcked.erase( itAck );

		} else {

			printf( "WARNING-> Could not find packet with corresponding AckCountID" );
		}

	} else if ( playerData.type == TYPE_Fire ) {

		validateFireAndSendHitPackets( client, playerData );

	} else if ( playerData.type == TYPE_GameUpdate) {

		double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
		client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;
		client->m_position.x = playerData.data.updatedGame.xPosition;
		client->m_position.y = playerData.data.updatedGame.yPosition;
		client->m_velocity.x = playerData.data.updatedGame.xVelocity;
		client->m_velocity.y = playerData.data.updatedGame.yVelocity;
		client->m_acceleration.x = playerData.data.updatedGame.xAcceleration;
		client->m_acceleration.y = playerData.data.updatedGame.yAcceleration;
		client->m_orientationDegrees.yawDegreesAboutZ = playerData.data.updatedGame.orientationDegrees;
		
		// Note: Not taking score or health as these are decided by the server

	} else  {

		printf( "WARNING-> Received a packet of type %d that is not accepted by GameRoom. This is NOT allowed!\n", playerData.type );
		printf( "Packet received from client: %s\n", client->m_userID.c_str() );
	}
	
}


void GameRoom::validateFireAndSendHitPackets( ConnectedUDPClient* firingClient, const FinalPacket& firePacket ) {

	const size_t numTimesToSample = 1000;
	const float incrementMagnitude = 0.70f;
	const float tankRadius = 10.0f;

	cbengine::Disk2D tankDisk;
	tankDisk.radius = tankRadius;
	cbengine::Disk2D lazerDisk;
	lazerDisk.radius = 2.0f;

	int firingClientID = firingClient->m_clientID;
	float firingAngle = firingClient->m_orientationDegrees.yawDegreesAboutZ;
	
	lazerDisk.origin.x = firingClient->m_position.x;
	lazerDisk.origin.y = firingClient->m_position.y;

	cbengine::Vector2 incrementVector;
	incrementVector.x = cos( cbengine::degreesToRadians( firingAngle ) );
	incrementVector.y = sin ( cbengine::degreesToRadians( firingAngle ) );
	
	bool wasHit = false;
	std::vector<int> hitIDs;

	for ( size_t i = 0; i < numTimesToSample; ++i ) {

		lazerDisk.origin += incrementVector;

		std::set<ConnectedUDPClient*>::iterator itPlayer;
		for ( itPlayer = m_players.begin(); itPlayer != m_players.end(); ++itPlayer ) {

			ConnectedUDPClient* player = *(itPlayer);
			if ( player->m_clientID == firingClientID ) {
				continue;
			}

			for ( size_t j = 0; j < hitIDs.size(); ++j ) {

				if ( player->m_clientID == hitIDs[j] ) {

					continue;
				}
			}

			tankDisk.origin.x = player->m_position.x;
			tankDisk.origin.y = player->m_position.y;
			
			wasHit = cbengine::doesDiskIntersectDiskOrTouch( lazerDisk, tankDisk );
			if ( wasHit ) {

				// PR: Assuming health is one for now
				firingClient->m_score += 1;
				hitIDs.push_back( player->m_clientID );

				resetPlayer( player );
			}
		}
	}

	for ( size_t i = 0; i < hitIDs.size(); ++i ) {

		FinalPacket hitPacket;

		// Header
		hitPacket.type = TYPE_Hit;
		hitPacket.timestamp = cbutil::getCurrentTimeSeconds();

		// Data
		hitPacket.data.hit.damageDealt = 1;
		hitPacket.data.hit.instigatorID = firingClient->m_clientID;
		hitPacket.data.hit.targetID = hitIDs[i];
		
		std::set<ConnectedUDPClient*>::iterator itPlayer;
		for ( itPlayer = m_players.begin(); itPlayer != m_players.end(); ++itPlayer ) {

			ConnectedUDPClient* player = *(itPlayer);
			hitPacket.clientID = player->m_clientID;

			m_server->sendPacket( hitPacket, player, hitPacket.IsGuaranteed() );
		}
	}
}



void GameRoom::resetPlayer( ConnectedUDPClient* playerToReset ) {

	playerToReset->m_health = STARTING_HEALTH;

	FinalPacket respawnPacket;
	
	// Header
	respawnPacket.type = TYPE_Respawn;
	respawnPacket.timestamp = cbutil::getCurrentTimeSeconds();
	respawnPacket.clientID = playerToReset->m_clientID;

	// Data
	respawnPacket.data.respawn.orientationDegrees = 0.0f;
	
	float randomNumZeroToOne = cbengine::getRandomZeroToOne();

	respawnPacket.data.respawn.xPosition = ARENA_WIDTH * randomNumZeroToOne;

	randomNumZeroToOne = cbengine::getRandomZeroToOne();

	respawnPacket.data.respawn.yPosition = ARENA_HEIGHT * randomNumZeroToOne;
	
	m_server->sendPacket( respawnPacket, playerToReset, respawnPacket.IsGuaranteed() );
}


void GameRoom::addPlayer( ConnectedUDPClient* playerToAdd, bool isOwner /* = false */ ) {

	if ( playerToAdd == nullptr || m_server == nullptr ) {

		return;
	}

	if ( isOwner ) {

		m_owner = playerToAdd;
	}

	
	std::set<ConnectedUDPClient*>::iterator itPlayer;
	itPlayer = m_players.find( playerToAdd );

	if ( itPlayer == m_players.end() ) {

		playerToAdd->m_isInLobby = false;
		playerToAdd->m_gameID = m_gameRoomNum;
		playerToAdd->m_score = 0;

		m_players.insert( playerToAdd );
		printf( "Added client: %s to GameRoom #%d", playerToAdd->m_userID.c_str(), m_gameRoomNum );

		FinalPacket resetPacket;

		// Header
		resetPacket.type = TYPE_GameReset;
		resetPacket.timestamp = cbutil::getCurrentTimeSeconds();
		resetPacket.clientID = playerToAdd->m_clientID;

		// Data
		resetPacket.data.reset.id = resetPacket.clientID;
		resetPacket.data.reset.orientationDegrees = 0.0f;

		float randomZeroToOne = cbengine::getRandomZeroToOne();

		resetPacket.data.reset.xPosition = ARENA_WIDTH * randomZeroToOne;

		randomZeroToOne = cbengine::getRandomZeroToOne();

		resetPacket.data.reset.yPosition = ARENA_HEIGHT * randomZeroToOne;

		playerToAdd->m_position.x = resetPacket.data.reset.xPosition;
		playerToAdd->m_position.y = resetPacket.data.reset.yPosition;

		m_server->sendPacket( resetPacket, playerToAdd, resetPacket.IsGuaranteed() );
	}
}


void GameRoom::sendUpdatePacketsToPlayers() {

	if ( m_server == nullptr || m_owner == nullptr ) {
		return;
	}

	double currentTimeSeconds = cbutil::getCurrentTimeSeconds();
	double timeDifSeconds = currentTimeSeconds - lastTimeStampSeconds;

	durationSinceLastPacketUpdate += timeDifSeconds;

	if ( durationSinceLastPacketUpdate > TIME_DIF_SECONDS_FOR_PACKET_UPDATE ) {

		durationSinceLastPacketUpdate = 0.0;

		std::vector<FinalPacket> playerPackets;	
		int winSockSendResult = 0;

		std::set<ConnectedUDPClient*>::iterator itClient;
		for ( itClient = m_players.begin(); itClient != m_players.end(); ++itClient ) {

			ConnectedUDPClient* client = *(itClient);

			FinalPacket playerData;

			// Header
			playerData.type											= TYPE_GameUpdate;
			playerData.timestamp									= cbutil::getCurrentTimeSeconds();
			playerData.clientID										= client->m_clientID;

			// Data
			playerData.data.updatedGame.health						= client->m_health;
			playerData.data.updatedGame.orientationDegrees			= client->m_orientationDegrees.yawDegreesAboutZ;
			playerData.data.updatedGame.xPosition					= client->m_position.x;
			playerData.data.updatedGame.yPosition					= client->m_position.y;
			playerData.data.updatedGame.xVelocity					= client->m_velocity.x;
			playerData.data.updatedGame.yVelocity					= client->m_velocity.y;
			playerData.data.updatedGame.xAcceleration				= client->m_acceleration.x;
			playerData.data.updatedGame.yAcceleration				= client->m_acceleration.y;
			playerData.data.updatedGame.score						= client->m_score;

			playerPackets.push_back( playerData );
		}

		std::set<ConnectedUDPClient*>::iterator itClientPacket;
		for ( itClientPacket = m_players.begin(); itClientPacket != m_players.end(); ++itClientPacket ) {

			ConnectedUDPClient* client = *(itClientPacket);

			for ( int i = 0; i < static_cast<int>( playerPackets.size() ); ++i ) {

				FinalPacket& packetToSend = playerPackets[i];
				m_server->sendPacket( packetToSend, client, false );	
			}
		}
	}

	lastTimeStampSeconds = currentTimeSeconds;
}


void GameRoom::endGameRoom() {

	if ( m_lobby == nullptr || m_server == nullptr ) {

		return;
	}

	//	When end score is reached OR host exits the game:
	//		Server->ALL Clients: ReturnToLobby
	//		Client->Server: Ack
	//		GOTO LOBBY LOOP

	std::set<ConnectedUDPClient*>::iterator itPlayer;
	for ( itPlayer = m_players.begin(); itPlayer != m_players.end(); ++itPlayer ) {

		ConnectedUDPClient* client = *(itPlayer);
		m_lobby->addUserToLobby( client );
		client->m_isInLobby = true;
		client->m_gameID = 0;
		client->m_score = 0;

		FinalPacket lobbyPacket;

		// Header
		lobbyPacket.type = TYPE_ReturnToLobby;
		lobbyPacket.timestamp = cbutil::getCurrentTimeSeconds();
		lobbyPacket.clientID = client->m_clientID;
		
		// Data ( Empty for ReturnLobby )
		
		m_server->sendPacket( lobbyPacket, client, lobbyPacket.IsGuaranteed() );
	}

	// Remove all players
	m_players.clear();
}


void GameRoom::sendVictoryAndResetPacketToAllClients( const FinalPacket& victoryPacketFromWinner ) {

	if ( m_server == nullptr ) {

		return;
	}

	/*
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
	*/
}


void GameRoom::setGameRoomDefaults() {

	m_server = nullptr;
	m_owner = nullptr;
	m_lobby = nullptr;
	m_gameRoomNum = 0;
}