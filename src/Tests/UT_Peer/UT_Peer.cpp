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
    Logger::Initialize();
    spdlog::set_level(spdlog::level::off);
    assert(Assertions::Threading::SetCoreThread());
    return RUN_ALL_TESTS();
}

//----------------------------------------------------------------------------------------------------------------------