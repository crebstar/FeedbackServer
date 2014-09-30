#include "GameLobby.hpp"


#include "../../CBEngine/EngineCode/TimeUtil.hpp"

#include "ConnectedUDPClient.hpp"
#include "GameRoom.hpp"


GameLobby::~GameLobby() {

}


GameLobby::GameLobby( UDPServer* server ) {

	setGameLobbyDefaults();
	m_server = server;
}


void GameLobby::updateLobby() {

	displayListOfLobbyUsers();

	sendListOfGamesToClients();

	for ( size_t i = 0; i < m_gameRooms.size(); ++i ) {

		GameRoom* room = m_gameRooms[i];
		room->updateGameRoom();
	}
}


void GameLobby::sendListOfGamesToClients() {

	if ( m_server == nullptr ) {

		return;
	}

	static double lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();
	static double durationSinceLastPacketUpdate = 0.0;

	double currentTimeSeconds = cbutil::getCurrentTimeSeconds();
	double timeDifSeconds = currentTimeSeconds - lastTimeStampSeconds;

	durationSinceLastPacketUpdate += timeDifSeconds;

	if ( durationSinceLastPacketUpdate > TIME_DIF_SECONDS_FOR_PACKET_UPDATE ) {

		CS6Packet packetToSend;

		// Header
		packetToSend.packetType = TYPE_GameList;
		packetToSend.timestamp = cbutil::getCurrentTimeSeconds();
		packetToSend.packetNumber = 0; // This is populated from the server

		// Data
		packetToSend.data.gameList.totalNumGames = m_gameRooms.size();

		std::set<ConnectedUDPClient*>::iterator itClient;

		ConnectedUDPClient* client = nullptr;
		for ( itClient = m_lobbyUsers.begin(); itClient != m_lobbyUsers.end(); ++itClient ) {

			client = *(itClient);
			if ( client != nullptr ) {

				// Don't time out clients who are in the lobby
				client->m_timeStampSecondsForLastPacketReceived = cbutil::getCurrentTimeSeconds(); 

				packetToSend.playerColorAndID[0] = client->m_red;
				packetToSend.playerColorAndID[1] = client->m_green;
				packetToSend.playerColorAndID[2] = client->m_blue;

				m_server->sendPacket( packetToSend, client, false );
			}
		}
	}

	lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();
}


void GameLobby::OnClientPacketReceived( ConnectedUDPClient* client, const CS6Packet& clientPacket ) {

	ConnectedUDPClient* lobbyClient = nullptr;

	if ( client == nullptr )
	{
		return;
	}

	if ( !client->m_isInLobby && client->m_gameID > 0 ) {

		for ( size_t i = 0; i < m_gameRooms.size(); ++i ) {

			GameRoom* room = m_gameRooms[i];
			if ( room->m_gameRoomNum == client->m_gameID ) {

				room->OnClientPacketReceived( client, clientPacket );
				return;
			}
		}
	}

	/*
		PR:
		Only three valid packets to receive in the GameLobby are Create and Join and Ack
		All other packets are ignored 
	*/

	// First check if the client is actually in the lobby
	std::set<ConnectedUDPClient*>::iterator itLob;
	itLob = m_lobbyUsers.find( client );

	if ( itLob != m_lobbyUsers.end() ) {

		lobbyClient = *(itLob);

		if ( clientPacket.packetType == TYPE_JoinGame ) {

			printf( "JoinGame packet received from User %s \n", lobbyClient->m_userID.c_str() );
			if ( clientPacket.data.joinGame.gameNumToJoin > m_gameRooms.size() ) {

				printf( "Warning-> Client %s is attempting to join a game that does NOT exist\n", lobbyClient->m_userID.c_str() );
				return;
			}

			for ( size_t i = 0; i < m_gameRooms.size(); ++i ) {

				GameRoom* room = m_gameRooms[i];
				if ( room->m_gameRoomNum == clientPacket.data.joinGame.gameNumToJoin ) {

					room->addPlayer( client );
					
					m_lobbyUsers.erase( client );
				}
			}

		} else if ( clientPacket.packetType == TYPE_CreateGame ) {

			printf( "CreateGame packet received from User %s \n", lobbyClient->m_userID.c_str() );
			size_t currentNumGames = m_gameRooms.size();
			++currentNumGames;

			GameRoom* gameRoomToAdd = new GameRoom( currentNumGames, client, m_server, this );
			m_gameRooms.push_back( gameRoomToAdd );

			m_lobbyUsers.erase( client );

		} else if ( clientPacket.packetType == TYPE_Acknowledge ) {

			double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
			int ackCountID = clientPacket.data.acknowledged.packetNumber;

			client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;

			std::map<int,CS6Packet>::iterator itAck;
			itAck = client->m_reliablePacketsSentButNotAcked.find( ackCountID );
			if ( itAck != client->m_reliablePacketsSentButNotAcked.end() ) {

				client->m_reliablePacketsSentButNotAcked.erase( itAck );

			}

		} else {

			printf( "Warning-> Client %s is sending a packet type which is not supported for the GameLobby \n", lobbyClient->m_userID.c_str() );
		}

	} else {

		// TODO :: Handle this error in pipeline
		printf( "Warning-> Client %s is not in the Game Lobby and is having packets directed to the GameLobby\n", client->m_userID.c_str() );
	}
}


void GameLobby::addUserToLobby( ConnectedUDPClient* userToAdd ) {

	if ( userToAdd == nullptr ) {

		printf( "Warning-> Cannot add a client to the lobby that has the value of nullptr\n" );
		return;
	}

	userToAdd->m_isInLobby = true;

	std::set<ConnectedUDPClient*>::iterator itLob;
	itLob = m_lobbyUsers.find( userToAdd );
	if ( itLob != m_lobbyUsers.end() ) {

		printf( "Warning-> User already exists in the lobby! Cannot add to lobby twice\n" );
		userToAdd->m_isInLobby = true;

	} else {

		printf( "User: %s has been successfully added to the Game Lobby\n", userToAdd->m_userID.c_str() );
		m_lobbyUsers.insert( userToAdd );
	}
}


void GameLobby::removeClientDueToInactivity( ConnectedUDPClient* clientToRemove ) {

	if ( clientToRemove == nullptr ) {

		printf( "Warning-> Cannot remove a client with the value of nullptr from the lobby\n" );
		return;
	}

	bool wasOwner = false;

	if ( !clientToRemove->m_isInLobby ) {

		size_t roomToRemove = 0;

		for ( size_t i = 0; i < m_gameRooms.size(); ++i ) {

			GameRoom* room = m_gameRooms[i];
			if ( room->m_gameRoomNum == clientToRemove->m_gameID ) {

				if ( clientToRemove == room->m_owner ) {

					wasOwner = true;
					room->endGameRoom();
					roomToRemove = i;

					delete room;
					room = nullptr;
					break;
				}
			}
		}

		if ( wasOwner ) {

			m_gameRooms.erase( m_gameRooms.begin() + roomToRemove );
		}
	}

	std::set<ConnectedUDPClient*>::iterator itLob;
	itLob = m_lobbyUsers.find( clientToRemove );

	if ( itLob != m_lobbyUsers.end() ) {

		printf( "User %s has been removed from the Game Lobby due to inactivity\n", clientToRemove->m_userID.c_str() );
		clientToRemove->m_isInLobby = false;
		m_lobbyUsers.erase( itLob );

	} else {

		printf( "Warning-> User did not exist in the lobby and cannot be removed due to inactivity\n" );
	}
}


bool GameLobby::sendUserToGame( unsigned int gameID, ConnectedUDPClient* userToSend ) {

	bool bUserCouldJoinGame = false;
	ConnectedUDPClient* lobbyUser = nullptr;

	std::set<ConnectedUDPClient*>::iterator itLob;
	itLob = m_lobbyUsers.find( userToSend );

	if ( itLob != m_lobbyUsers.end() ) {

		lobbyUser = *(itLob);
		lobbyUser->m_isInLobby = false; // Change this to occur after transfer is successful

		// TODO ::
		// Send user to the game then erase

		m_lobbyUsers.erase( itLob );

	} else {

		printf( "Warning: Cannot send a user to game when the user is not in the lobby!\n" );
	}


	return bUserCouldJoinGame;
}


void GameLobby::displayListOfLobbyUsers() {

	static double lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();
	static double durationSinceLastDisplay = 0.0;

	double currentTimeSeconds = cbutil::getCurrentTimeSeconds();
	double timeDifSeconds = currentTimeSeconds - lastTimeStampSeconds;

	durationSinceLastDisplay += timeDifSeconds;

	if ( durationSinceLastDisplay > TIME_DIF_SECONDS_FOR_LOBBY_USER_UPDATE ) {

		printListOfLobbyUsers();

		durationSinceLastDisplay = 0.0;
	}

	lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();
}


void GameLobby::printListOfLobbyUsers() {

	ConnectedUDPClient* lobbyUser = nullptr;

	std::set<ConnectedUDPClient*>::iterator itLob;

	printf( "\n-----------------------------------------------------------------------\n" );

	if ( m_lobbyUsers.empty() ) {

		printf( "\nThere are currently no users in the Game Lobby\n" );
	}

	for ( itLob = m_lobbyUsers.begin(); itLob != m_lobbyUsers.end(); ++itLob ) {

		lobbyUser = *(itLob);
		if ( lobbyUser != nullptr ) {

			printf( "User: %s is currently connected to the lobby\n", lobbyUser->m_userID.c_str() );
		}
	}

	printf( "\n-----------------------------------------------------------------------\n" );
}


void GameLobby::setGameLobbyDefaults() {

	m_server = nullptr;
}