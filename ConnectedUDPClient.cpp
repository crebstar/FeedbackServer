#include "ConnectedUDPClient.hpp"

int ConnectedUDPClient::s_numberOfClients = 0;

ConnectedUDPClient::~ConnectedUDPClient() {

	--s_numberOfClients;
}


ConnectedUDPClient::ConnectedUDPClient() {
	
	m_timeStampSecondsForLastPacketReceived = 0.0;
	++s_numberOfClients;
	m_playerID = s_numberOfClients;

	assignColorForPlayer();
}


// Temp hacky way to assign colors for players
void ConnectedUDPClient::assignColorForPlayer() {

	if ( s_numberOfClients == 1 ) {

		m_red = 250;
		m_green = 200;
		m_blue = 200;

	} else if ( s_numberOfClients == 2 ) {

		m_red = 220;
		m_green = 50;
		m_blue = 50;

	} else if ( s_numberOfClients == 3 ) {

		m_red = 50;
		m_green = 250;
		m_blue = 50;

	} else if ( s_numberOfClients == 4 ) {

		m_red = 50;
		m_green = 50;
		m_blue = 250;
	}
}


