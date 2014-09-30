#ifndef included_GameLobby
#define included_GameLobby
#pragma once

#include <set>
#include <vector>

#include "../../CBEngine/EngineCode/EngineMacros.hpp"

#include "CS6Packet.hpp"

const double TIME_DIF_SECONDS_FOR_LOBBY_USER_UPDATE = 7.55;

class ConnectedUDPClient;
class UDPServer;
class GameRoom;

class GameLobby {
public:

	~GameLobby();
	explicit GameLobby( UDPServer* server );

	void updateLobby();
	void OnClientPacketReceived( ConnectedUDPClient* client, const CS6Packet& clientPacket );

	void addUserToLobby( ConnectedUDPClient* userToAdd );
	void removeClientDueToInactivity( ConnectedUDPClient* clientToRemove );


	// Debug Related
	void displayListOfLobbyUsers();
	void printListOfLobbyUsers();

protected:

	bool sendUserToGame( unsigned int gameID, ConnectedUDPClient* userToSend );
	void sendListOfGamesToClients();

	void setGameLobbyDefaults();

private:
	PREVENT_COPY_AND_ASSIGN( GameLobby );

	std::set<ConnectedUDPClient*>					m_lobbyUsers;
	UDPServer*										m_server;

	std::vector<GameRoom*>								m_gameRooms;
};

#endif