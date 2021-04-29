#pragma once

#include "libavstream/common_maths.h"

namespace scr
{
	//Static class for testing logic; all tests should pass or there is a logic error that needs fixing.
	class Tests
	{
	public:
		//There should be no constructor for a static class.
		Tests() = delete;

		static void RunAllTests();

		static void RunConversionEquivalenceTests();
		static void RunConversionEquivalenceTest(avs::AxesStandard fromStandard, avs::AxesStandard toStandard);
	};
}