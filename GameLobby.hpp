#ifndef included_GameLobby
#define included_GameLobby
#pragma once

#include <set>
#include <vector>

#include "../../CBEngine/EngineCode/EngineMacros.hpp"

#include "FinalPacket.hpp"

const double TIME_DIF_SECONDS_FOR_LOBBY_USER_UPDATE = 7.55;
const size_t NUM_GAME_ROOMS = 8;

class ConnectedUDPClient;
class UDPServer;
class GameRoom;

class GameLobby {
public:

	~GameLobby();
	explicit GameLobby( UDPServer* server );

	void updateLobby();
	void OnClientPacketReceived( ConnectedUDPClient* client, const FinalPacket& clientPacket );

	void addUserToLobby( ConnectedUDPClient* userToAdd );
	void removeClientDueToInactivity( ConnectedUDPClient* clientToRemove );

	void updateGameRooms();
	GameRoom* getRoomByNumber( int gameRoomNum );

	// Debug Related
	void displayListOfLobbyUsers();
	void printListOfLobbyUsers();

protected:

	bool sendUserToGame( unsigned int gameID, ConnectedUDPClient* userToSend );
	void sendListOfGamesToClients();

	void createGameRooms();
	void setGameLobbyDefaults();

private:
	PREVENT_COPY_AND_ASSIGN( GameLobby );

	std::set<ConnectedUDPClient*>					m_lobbyUsers;
	UDPServer*										m_server;

	std::vector<GameRoom*>							m_gameRooms;
};

#endif