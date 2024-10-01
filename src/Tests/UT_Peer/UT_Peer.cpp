//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include "Utilities/Assertions.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------

GTEST_API_ std::int32_t main(std::int32_t argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    Logger::Initialize(spdlog::level::off, true);
    assert(Assertions::Threading::RegisterCoreThread());
    return RUN_ALL_TESTS();
}

//----------------------------------------------------------------------------------------------------------------------
