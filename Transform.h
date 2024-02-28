#pragma once
#include "CygnusMath.h"

namespace Cygnus {
	class Transform
	{
	public:
		Cygnus::Float3 scale;
		Cygnus::Float3 rotate;
		Cygnus::Float3 translate;

		Cygnus::Matrix MakeAffineMatrix();
	};
}

