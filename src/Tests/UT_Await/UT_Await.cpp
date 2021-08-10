//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include "Utilities/Assertions.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------

GTEST_API_ std::int32_t main(std::int32_t argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    LogUtils::InitializeLoggers();
    spdlog::set_level(spdlog::level::off);
    assert(Assertions::Threading::SetCoreThread());
    return RUN_ALL_TESTS();
}

//----------------------------------------------------------------------------------------------------------------------