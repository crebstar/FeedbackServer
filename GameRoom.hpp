#ifndef included_GameRoom
#define included_GameRoom
#pragma once

#include <set>

#include "../../CBEngine/EngineCode/EngineMacros.hpp"

#include "FinalPacket.hpp"

const int WINNING_SCORE_NUM = 10;

class ConnectedUDPClient;
class UDPServer;
class GameLobby;

class GameRoom {
public:

	~GameRoom();
	explicit GameRoom( unsigned int gameRoomNum, ConnectedUDPClient* owner, UDPServer* server, GameLobby* lobby );

	void updateGameRoom();
	void checkForWinCondition();

	void addPlayer( ConnectedUDPClient* playerToAdd, bool isOwner = false );

	void OnClientPacketReceived( ConnectedUDPClient* client, const FinalPacket& playerData );

	void endGameRoom();

	int getNumPlayersInRoom() const;
	bool isRoomEmpty() const;

	unsigned int 						m_gameRoomNum;
	ConnectedUDPClient*					m_owner;

protected:

	void sendUpdatePacketsToPlayers();
	void sendVictoryAndResetPacketToAllClients( const FinalPacket& victoryPacketFromWinner );

	void validateFireAndSendHitPackets( ConnectedUDPClient* firingClient, const FinalPacket& firePacket );
	void resetPlayer( ConnectedUDPClient* playerToReset );

	void setGameRoomDefaults();

private:

	PREVENT_COPY_AND_ASSIGN( GameRoom );

	std::set<ConnectedUDPClient*>		m_players;
	UDPServer*							m_server;
	GameLobby*							m_lobby;

	double								durationSinceLastPacketUpdate;
	double								lastTimeStampSeconds;
};


inline int GameRoom::getNumPlayersInRoom() const {

	return m_players.size();
}


inline bool GameRoom::isRoomEmpty() const {

	return m_players.empty();
}

/*
// FROM UPDATE IN UDPSERVER
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

m_flag.resetPositionOfFlag( ARENA_WIDTH, ARENA_HEIGHT );
sendVictoryAndResetPacketToAllClients( playerData );

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


*/


#endif