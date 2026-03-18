#include <boost/ut.hpp>

using namespace boost::ut;

suite performance_tests = [] {
    skip / "Benchmark placeholder"_test = [] {
        // Тесты производительности обычно помечаются тегами или пропускаются в обычных unit-прогонах.
        // Здесь можно использовать std::chrono для замеров.
        expect(true);
    };
};