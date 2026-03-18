#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

// Forward declarations for test suites
boost::ut::suite LockFreeQueueSuite();
boost::ut::suite MessageParserSuite();
boost::ut::suite MatchingEngineSuite();
boost::ut::suite PerformanceSuite();

int main() {
    using namespace boost::ut;
    
    // Run all test suites
    LockFreeQueueSuite();
    MessageParserSuite();
    MatchingEngineSuite();
    PerformanceSuite();
    
    return cfg<>.run();
}
