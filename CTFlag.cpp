#include "CTFlag.hpp"

#include "../../CBEngine/EngineCode/MathUtil.hpp"

CTFlag::~CTFlag() {

}


CTFlag::CTFlag() {

	m_xPos = 0.0f;
	m_yPos = 0.0f;
}


void CTFlag::resetPositionOfFlag( float arenaWidth, float arenaHeight ) {

	float randomNumberZeroToOneForX = cbengine::getRandomZeroToOne();
	float randomNumberZeroToOneForY = cbengine::getRandomZeroToOne();

	m_xPos = arenaWidth * randomNumberZeroToOneForX;
	m_yPos = arenaHeight * randomNumberZeroToOneForY;
}