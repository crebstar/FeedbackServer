#include "GameLobby.hpp"


#include "../../CBEngine/EngineCode/TimeUtil.hpp"

#include "ConnectedUDPClient.hpp"
#include "GameRoom.hpp"


GameLobby::~GameLobby() {

}


GameLobby::GameLobby( UDPServer* server ) {

	setGameLobbyDefaults();
	m_server = server;

	createGameRooms();
}


void GameLobby::createGameRooms() {

	for ( size_t i = 1; i <= NUM_GAME_ROOMS; ++i ) {

		GameRoom* room = new GameRoom( i, nullptr, m_server, this );
		m_gameRooms.push_back( room );
	}
}


void GameLobby::updateLobby() {

	displayListOfLobbyUsers();

	sendListOfGamesToClients();

	updateGameRooms();
}


void GameLobby::updateGameRooms() {

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


		FinalPacket packetToSend;

		// Header
		packetToSend.type = TYPE_LobbyUpdate;
		packetToSend.number = 0; // Populated from server
		packetToSend.timestamp = cbutil::getCurrentTimeSeconds();
		

		if ( m_gameRooms.size() < NUM_GAME_ROOMS ) {
			printf( "Warning-> Number of game rooms does not match required the game room number" );

			for ( size_t i = 0; i < NUM_GAME_ROOMS; ++i ) {

				packetToSend.data.updatedLobby.playersInRoomNumber[i] = 0;
			}
		}


		// Data
		for ( size_t i = 0; i < m_gameRooms.size(); ++i ) {

			GameRoom* room = m_gameRooms[i];
			if ( room != nullptr ) {

				packetToSend.data.updatedLobby.playersInRoomNumber[i] = room->getNumPlayersInRoom();
			} 
		}

		bool bPacketGuarenteed = packetToSend.IsGuaranteed();
		ConnectedUDPClient* client = nullptr;
		std::set<ConnectedUDPClient*>::iterator itClient;

		for ( itClient = m_lobbyUsers.begin(); itClient != m_lobbyUsers.end(); ++itClient ) {

			client = *(itClient);
			if ( client != nullptr ) {

				packetToSend.clientID = client->m_clientID;
				m_server->sendPacket( packetToSend, client, bPacketGuarenteed );
			}
		}
	}
	

	lastTimeStampSeconds = cbutil::getCurrentTimeSeconds();
}


void GameLobby::OnClientPacketReceived( ConnectedUDPClient* client, const FinalPacket& clientPacket ) {

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

		if ( clientPacket.type == TYPE_JoinRoom ) {

			//	Client->Server: JoinRoom( # )
			//		If room # is empty:
			//			Server->Client: Nack( ERROR_RoomEmpty )
			//			GOTO LOBBY LOOP
			//		If room # is full:
			//			Server->Client: Ack
			//			Server->Client: GameReset (implicit respawn)
			//			GOTO GAME LOOP

			int roomNumToJoin = clientPacket.data.joining.room;
			GameRoom* roomToJoin = getRoomByNumber( roomNumToJoin );

			if ( roomToJoin != nullptr ) {

				bool roomIsEmpty = roomToJoin->isRoomEmpty();
				if ( !roomIsEmpty ) {
					// Room can be joined and has an owner

					FinalPacket joinAckPacket;

					// Header
					joinAckPacket.type = TYPE_Ack;
					joinAckPacket.timestamp = cbutil::getCurrentTimeSeconds();
					joinAckPacket.clientID = client->m_clientID;

					// Data
					joinAckPacket.data.acknowledged.number = clientPacket.number;
					joinAckPacket.data.acknowledged.type = TYPE_JoinRoom;

					m_server->sendPacket( joinAckPacket, client, false ); 

					printf( "JoinGame packet received from User %s \n", lobbyClient->m_userID.c_str() );

					roomToJoin->addPlayer( client, false );
					m_lobbyUsers.erase( client );

				} else {
					// Room is not created :: Send Nack
					FinalPacket joinNackPacket;

					// Header
					joinNackPacket.type = TYPE_Nack;
					joinNackPacket.timestamp = cbutil::getCurrentTimeSeconds();
					joinNackPacket.clientID = client->m_clientID;

					// Data
					joinNackPacket.data.refused.errorCode = ERROR_RoomEmpty;
					joinNackPacket.data.refused.type = TYPE_JoinRoom;
					joinNackPacket.data.refused.number = clientPacket.number;

					m_server->sendPacket( joinNackPacket, client, false );

					printf( "Warning-> Client %s is attempting to join a game that has not been created\n", lobbyClient->m_userID.c_str() );

				}

			} else {
				// Bad room ID
				FinalPacket joinNackPacket;

				// Header
				joinNackPacket.type = TYPE_Nack;
				joinNackPacket.timestamp = cbutil::getCurrentTimeSeconds();
				joinNackPacket.clientID = client->m_clientID;

				// Data
				joinNackPacket.data.refused.errorCode = ERROR_RoomEmpty;
				joinNackPacket.data.refused.type = TYPE_JoinRoom;
				joinNackPacket.data.refused.number = clientPacket.number;

				m_server->sendPacket( joinNackPacket, client, false );

				printf( "Warning-> Client %s is attempting to join a game that does NOT exist\n", lobbyClient->m_userID.c_str() );
			}
		
		} else if ( clientPacket.type == TYPE_CreateRoom ) {

			//	Client->Server: CreateRoom( # )
			//		If room # is empty:
			//			Server->Client: Ack
			//			Server->Client: GameReset (implicit respawn)
			//			GOTO GAME LOOP
			//		If room # is full:
			//			Server->Client: Nack( ERROR_RoomFull )
			//			GOTO LOBBY LOOP

			int roomNumToCreate = clientPacket.data.creating.room;
			GameRoom* roomToCreate = getRoomByNumber( roomNumToCreate );
			bool roomCanBeCreated = false;

			if ( roomToCreate != nullptr ) {

				roomCanBeCreated = roomToCreate->isRoomEmpty();
				if ( roomCanBeCreated ) {
					// Room is empty and can be claimed/created

					roomToCreate->addPlayer( client, true );
					m_lobbyUsers.erase( client );

					FinalPacket createAckPacket;
					createAckPacket.type = TYPE_Ack;
					createAckPacket.timestamp = cbutil::getCurrentTimeSeconds();
					createAckPacket.clientID = client->m_clientID;
					createAckPacket.data.acknowledged.type = TYPE_CreateRoom;
					createAckPacket.data.acknowledged.number = clientPacket.number;

					m_server->sendPacket( createAckPacket, client, false );

				} else {
					// Room is not available for creation 

					FinalPacket createNackPacket;
					createNackPacket.type = TYPE_Nack;
					createNackPacket.timestamp = cbutil::getCurrentTimeSeconds();
					createNackPacket.clientID = client->m_clientID;
					createNackPacket.data.refused.errorCode = ERROR_RoomFull;
					createNackPacket.data.refused.number = clientPacket.number;
					createNackPacket.data.refused.type = TYPE_CreateRoom;

					m_server->sendPacket( createNackPacket, client, false );
				}

			} else {
				// Room not found

				FinalPacket createNackPacket;
				createNackPacket.type = TYPE_Nack;
				createNackPacket.timestamp = cbutil::getCurrentTimeSeconds();
				createNackPacket.clientID = client->m_clientID;
				createNackPacket.data.refused.errorCode = ERROR_BadRoomID;
				createNackPacket.data.refused.number = clientPacket.number;
				createNackPacket.data.refused.type = TYPE_CreateRoom;

				m_server->sendPacket( createNackPacket, client, false );
			}

		} else if ( clientPacket.type == TYPE_Ack ) {

			double currentTimeInSeconds = cbutil::getCurrentTimeSeconds();
			int ackCountID = clientPacket.data.acknowledged.number;

			client->m_timeStampSecondsForLastPacketReceived = currentTimeInSeconds;

			std::map<int,FinalPacket>::iterator itAck;
			itAck = client->m_reliablePacketsSentButNotAcked.find( ackCountID );
			if ( itAck != client->m_reliablePacketsSentButNotAcked.end() ) {

				client->m_reliablePacketsSentButNotAcked.erase( itAck );

			}

		} else if ( clientPacket.type == TYPE_KeepAlive ) {

			client->m_timeStampSecondsForLastPacketReceived = cbutil::getCurrentTimeSeconds();

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

					printf( "User %s being removed was the game room owner of room %d.", clientToRemove->m_userID.c_str(), clientToRemove->m_gameID );
				}
			}
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


GameRoom* GameLobby::getRoomByNumber( int gameRoomNum ) {

	GameRoom* roomToReturn = nullptr;

	for ( size_t i = 0; i < m_gameRooms.size(); ++i ) {

		GameRoom* room = m_gameRooms[i];
		if ( room != nullptr ) {
			if ( room->m_gameRoomNum == gameRoomNum ) {

				roomToReturn = room;
			}
		}
	}

	return roomToReturn;
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