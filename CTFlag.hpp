#ifndef included_CTFlag
#define included_CTFlag
#pragma once

#include "../../CBEngine/EngineCode/EngineMacros.hpp"

class CTFlag {
public:

	~CTFlag();
	explicit CTFlag();

	float		m_xPos;
	float		m_yPos;

	void resetPositionOfFlag( float arenaWidth, float arenaHeight );

protected:

private:
	PREVENT_COPY_AND_ASSIGN( CTFlag );

};


#endif