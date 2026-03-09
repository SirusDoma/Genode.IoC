cmake_minimum_required(VERSION 3.14)
project(Genode.IoC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Genode.IoC INTERFACE)
target_include_directories(Genode.IoC INTERFACE include)

enable_testing()

add_executable(tests tests/Unit.cpp)
target_link_libraries(tests PRIVATE Genode.IoC)

set(TEST_NAMES
    TestDefaultConstruction
    TestAutoWiring
    TestInterfaceBinding
    TestInterfaceDependency
    TestBuilderFactory
    TestSingletonScope
    TestLocalScope
    TestPointerReturnsNullptr
    TestOutOfOrderRegistration
    TestCapture
    TestCaptureFromScope
    TestRequireAutoRegistersInCurrentContext
    TestInstantiate
    TestRequireThrowsOnUnresolvable
    TestInstantiateThrowsOnUnresolvable
    TestDestructionOrder
    TestDestructionOrderMultipleChains
    TestDestructionOrderComplex
    TestDestructionOrderSingleton
    TestComplexDependencyTree
)

foreach(name IN LISTS TEST_NAMES)
    add_test(NAME ${name} COMMAND tests ${name})
endforeach()

add_executable(benchmarks tests/Benchmark.cpp)
target_link_libraries(benchmarks PRIVATE Genode.IoC)
