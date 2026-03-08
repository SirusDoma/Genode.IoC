#include <Genode/Context.hpp>

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <functional>

struct InputSystem
{
    int Value = 42;
};

struct PhysicsSystem
{
    float Gravity = 9.81f;
};

struct MovementSystem
{
    MovementSystem(InputSystem& input, PhysicsSystem& physics)
        : m_input(&input), m_physics(&physics) {}

    InputSystem*  m_input;
    PhysicsSystem* m_physics;
};

// Interface / concrete pair
class IRenderer
{
public:
    virtual ~IRenderer() = default;
    virtual std::string Name() const = 0;

protected:
    IRenderer() = default;
};

class OpenGLRenderer : public IRenderer
{
public:
    std::string Name() const override { return "OpenGL"; }
};

// Type that requires an interface dependency
struct RenderPipeline
{
    RenderPipeline(IRenderer& renderer) : m_renderer(&renderer) {}
    IRenderer* m_renderer;
};

// Type for builder test
struct AudioSystem
{
    AudioSystem(int sampleRate, int channels)
        : SampleRate(sampleRate), Channels(channels) {}

    int SampleRate;
    int Channels;
};

// Complex dependency tree (5 levels, 2-5 deps per node)
namespace Complex
{
    struct Config    { int version = 1; };
    struct Logger    { int level = 0; };
    struct Timer     { float delta = 0.016f; };
    struct Allocator { int poolSize = 1024; };
    struct RNG       { int seed = 42; };

    struct Profiler  { int sampleRate = 60; };
    struct Debugger  { bool attached = false; };
    struct Analytics { std::string endpoint = "localhost"; };

    struct FileSystem
    {
        FileSystem(Config& c, Logger& l, Profiler* p)
            : config(&c), logger(&l), profiler(p) {}
        Config* config; Logger* logger; Profiler* profiler;
    };

    struct Network
    {
        Network(Config& c, Logger& l, Timer& t, Debugger* d)
            : config(&c), logger(&l), timer(&t), debugger(d) {}
        Config* config; Logger* logger; Timer* timer; Debugger* debugger;
    };

    struct AudioDriver
    {
        AudioDriver(Config& c, Allocator& a) : config(&c), allocator(&a) {}
        Config* config; Allocator* allocator;
    };

    struct GraphicsDriver
    {
        GraphicsDriver(Config& c, Logger& l, Timer& t, Allocator& a, RNG& r)
            : config(&c), logger(&l), timer(&t), allocator(&a), rng(&r) {}
        Config* config; Logger* logger; Timer* timer; Allocator* allocator; RNG* rng;
    };

    struct ResourceManager
    {
        ResourceManager(FileSystem& fs, Network& net, AudioDriver& ad)
            : fileSystem(&fs), network(&net), audioDriver(&ad) {}
        FileSystem* fileSystem; Network* network; AudioDriver* audioDriver;
    };

    struct SceneGraph
    {
        SceneGraph(GraphicsDriver& gd, AudioDriver& ad, FileSystem& fs, Analytics* a)
            : graphicsDriver(&gd), audioDriver(&ad), fileSystem(&fs), analytics(a) {}
        GraphicsDriver* graphicsDriver; AudioDriver* audioDriver; FileSystem* fileSystem;
        Analytics* analytics;
    };

    struct EventBus
    {
        EventBus(Network& net, FileSystem& fs)
            : network(&net), fileSystem(&fs) {}
        Network* network; FileSystem* fileSystem;
    };

    struct World
    {
        World(ResourceManager& rm, SceneGraph& sg, EventBus& eb)
            : resourceManager(&rm), sceneGraph(&sg), eventBus(&eb) {}
        ResourceManager* resourceManager; SceneGraph* sceneGraph; EventBus* eventBus;
    };

    struct Simulation
    {
        Simulation(SceneGraph& sg, EventBus& eb)
            : sceneGraph(&sg), eventBus(&eb) {}
        SceneGraph* sceneGraph; EventBus* eventBus;
    };

    struct Engine
    {
        Engine(World& w, Simulation& s, Debugger* d)
            : world(&w), simulation(&s), debugger(d) {}
        World* world; Simulation* simulation; Debugger* debugger;
    };
}

static int testsPassed = 0;
static int testsFailed = 0;

struct TestEntry { std::string name; std::function<void()> fn; };
static std::vector<TestEntry>& Tests()
{
    static std::vector<TestEntry> tests;
    return tests;
}

#define ASSERT(expr)                                                 \
    if (!(expr))                                                     \
        throw std::runtime_error(                                    \
            std::string(__FILE__) + ":" + std::to_string(__LINE__)   \
            + ": assertion failed: " #expr)

#define REGISTER_TEST(name) \
    static bool registered_##name = (Tests().push_back({#name, name}), true)

void TestDefaultConstruction()
{
    auto context = Gx::Context();
    auto& input = context.Require<InputSystem>();
    ASSERT(input.Value == 42);
}
REGISTER_TEST(TestDefaultConstruction);

void TestAutoWiring()
{
    auto context = Gx::Context();
    auto& movement = context.Require<MovementSystem>();
    ASSERT(movement.m_input  != nullptr);
    ASSERT(movement.m_physics != nullptr);
    ASSERT(movement.m_input->Value == 42);
}
REGISTER_TEST(TestAutoWiring);

void TestInterfaceBinding()
{
    auto context = Gx::Context();
    context.Provide<IRenderer, OpenGLRenderer>();
    auto& renderer = context.Require<IRenderer>();
    ASSERT(renderer.Name() == "OpenGL");
}
REGISTER_TEST(TestInterfaceBinding);

void TestInterfaceDependency()
{
    auto context = Gx::Context();
    context.Provide<IRenderer, OpenGLRenderer>();
    auto& pipeline = context.Require<RenderPipeline>();
    ASSERT(pipeline.m_renderer != nullptr);
    ASSERT(pipeline.m_renderer->Name() == "OpenGL");
}
REGISTER_TEST(TestInterfaceDependency);

void TestBuilderFactory()
{
    auto context = Gx::Context();
    context.Provide<AudioSystem>(
        std::function<std::unique_ptr<AudioSystem>(Gx::Context&)>(
            [](Gx::Context&) {
                return std::make_unique<AudioSystem>(44100, 2);
            }
        )
    );

    auto& audio = context.Require<AudioSystem>();
    ASSERT(audio.SampleRate == 44100);
    ASSERT(audio.Channels == 2);
}
REGISTER_TEST(TestBuilderFactory);

void TestSingletonScope()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Singleton);

    auto& a = context.Require<InputSystem>();
    auto& b = context.Require<InputSystem>();

    // Same instance within the same scope
    ASSERT(&a == &b);

    {
        // Same instance in a child scope (singleton)
        auto scope = context.CreateScope();
        auto& c = scope.Require<InputSystem>();
        auto& d = scope.Require<InputSystem>();

        ASSERT(&c == &d);   // Same within child scope
        ASSERT(&a == &c);   // Same across scopes
    }
}
REGISTER_TEST(TestSingletonScope);

void TestLocalScope()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Local);

    auto& a = context.Require<InputSystem>();
    auto& b = context.Require<InputSystem>();

    // Same instance within the same scope
    ASSERT(&a == &b);

    {
        // Different instance in a child scope
        auto scope = context.CreateScope();
        auto& c = scope.Require<InputSystem>();
        auto& d = scope.Require<InputSystem>();

        ASSERT(&c == &d);   // Same within child scope
        ASSERT(&a != &c);   // Different across scopes
    }
}
REGISTER_TEST(TestLocalScope);

void TestPointerReturnsNullptr()
{
    auto context = Gx::Context();
    auto instance = context.Require<AudioSystem*>();
    ASSERT(instance == nullptr);
}
REGISTER_TEST(TestPointerReturnsNullptr);

void TestOutOfOrderRegistration()
{
    auto context = Gx::Context();

    // Register dependent before its dependency
    context.Provide<MovementSystem>();
    context.Provide<InputSystem>();

    // Should still resolve correctly (lazy)
    auto& movement = context.Require<MovementSystem>();
    ASSERT(movement.m_input != nullptr);
    ASSERT(movement.m_physics != nullptr);
    ASSERT(movement.m_input->Value == 42);
}
REGISTER_TEST(TestOutOfOrderRegistration);

void TestCapture()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Singleton);

    auto& original = context.Require<InputSystem>();

    // Capture creates a standalone snapshot
    auto captured = context.Capture();

    // Singleton instance is shared (same pointer)
    auto& fromCaptured = captured.Require<InputSystem>();
    ASSERT(&original == &fromCaptured);

    // Register a new type on the source AFTER capture
    context.Provide<PhysicsSystem>();
    context.Require<PhysicsSystem>();

    // Captured context does NOT see the new registration
    auto instance = captured.Require<PhysicsSystem*>();
    ASSERT(instance == nullptr);
}
REGISTER_TEST(TestCapture);

void TestCaptureFromScope()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Singleton);
    context.Provide<PhysicsSystem>(Gx::Scope::Local);

    auto& singletonInst = context.Require<InputSystem>();

    // Create a child scope, resolve local type in it
    auto scope = context.CreateScope();
    auto& localInScope = scope.Require<PhysicsSystem>();

    // Capture the child scope
    auto snapshot = scope.Capture();

    // Singleton from root is pulled into captured context
    auto& singletonFromCapture = snapshot.Require<InputSystem>();
    ASSERT(&singletonInst == &singletonFromCapture);

    // Local instance from the scope is also copied
    auto& localFromCapture = snapshot.Require<PhysicsSystem>();
    ASSERT(&localInScope == &localFromCapture);

    // Modifying parent after capture has no effect
    context.Provide<MovementSystem>();
    context.Require<MovementSystem>();

    auto instance = snapshot.Require<MovementSystem*>();
    ASSERT(instance == nullptr);
}
REGISTER_TEST(TestCaptureFromScope);

void TestRequireAutoRegistersInCurrentContext()
{
    auto parent = Gx::Context();
    auto child  = parent.CreateScope();

    // Require on child: PhysicsSystem not registered anywhere
    auto& physics = child.Require<PhysicsSystem>();
    ASSERT(physics.Gravity == 9.81f);

    // Parent must NOT have it
    auto* fromParent = parent.Require<PhysicsSystem*>();
    ASSERT(fromParent == nullptr);

    // Child must have it
    auto* fromChild = child.Require<PhysicsSystem*>();
    ASSERT(fromChild != nullptr);
    ASSERT(fromChild == &physics);
}
REGISTER_TEST(TestRequireAutoRegistersInCurrentContext);

void TestInstantiate()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>();

    // Instantiate returns a fresh instance each time
    auto a = context.Instantiate<InputSystem>();
    auto b = context.Instantiate<InputSystem>();
    ASSERT(a != nullptr);
    ASSERT(b != nullptr);
    ASSERT(a.get() != b.get());

    // Instantiate on unregistered type creates on the fly without providing
    auto c = context.Instantiate<PhysicsSystem>();
    ASSERT(c != nullptr);
    ASSERT(c->Gravity == 9.81f);

    // The type must NOT be registered in the context
    auto* ptr = context.Require<PhysicsSystem*>();
    ASSERT(ptr == nullptr);
}
REGISTER_TEST(TestInstantiate);

void TestRequireThrowsOnUnresolvable()
{
    auto context = Gx::Context();

    bool thrown = false;
    try
    {
        context.Require<IRenderer>();
    }
    catch (const std::runtime_error&)
    {
        thrown = true;
    }

    ASSERT(thrown);
}
REGISTER_TEST(TestRequireThrowsOnUnresolvable);

void TestInstantiateThrowsOnUnresolvable()
{
    auto context = Gx::Context();

    bool thrown = false;
    try
    {
        context.Instantiate<IRenderer>();
    }
    catch (const std::runtime_error&)
    {
        thrown = true;
    }

    ASSERT(thrown);
}
REGISTER_TEST(TestInstantiateThrowsOnUnresolvable);

void TestComplexDependencyTree()
{
    auto context = Gx::Context();

    // Resolve the root — auto-wires the entire 5-level tree
    auto& engine = context.Require<Complex::Engine>();

    // Level 1: Engine
    ASSERT(engine.world != nullptr);
    ASSERT(engine.simulation != nullptr);

    // Level 2: World, Simulation
    ASSERT(engine.world->resourceManager != nullptr);
    ASSERT(engine.world->sceneGraph != nullptr);
    ASSERT(engine.world->eventBus != nullptr);
    ASSERT(engine.simulation->sceneGraph != nullptr);
    ASSERT(engine.simulation->eventBus != nullptr);

    // Level 3: ResourceManager, SceneGraph, EventBus
    ASSERT(engine.world->resourceManager->fileSystem != nullptr);
    ASSERT(engine.world->resourceManager->network != nullptr);
    ASSERT(engine.world->resourceManager->audioDriver != nullptr);
    ASSERT(engine.world->sceneGraph->graphicsDriver != nullptr);
    ASSERT(engine.world->sceneGraph->audioDriver != nullptr);
    ASSERT(engine.world->sceneGraph->fileSystem != nullptr);
    ASSERT(engine.world->eventBus->network != nullptr);
    ASSERT(engine.world->eventBus->fileSystem != nullptr);

    // Level 4: FileSystem, Network, AudioDriver, GraphicsDriver
    ASSERT(engine.world->resourceManager->fileSystem->config != nullptr);
    ASSERT(engine.world->resourceManager->fileSystem->logger != nullptr);
    ASSERT(engine.world->sceneGraph->graphicsDriver->config != nullptr);
    ASSERT(engine.world->sceneGraph->graphicsDriver->rng != nullptr);

    // Level 5: Leaf values
    ASSERT(engine.world->sceneGraph->graphicsDriver->config->version == 1);
    ASSERT(engine.world->sceneGraph->graphicsDriver->rng->seed == 42);
    ASSERT(engine.world->resourceManager->network->timer->delta == 0.016f);
    ASSERT(engine.world->resourceManager->audioDriver->allocator->poolSize == 1024);

    // Unregistered pointer params must be nullptr
    ASSERT(engine.debugger == nullptr);
    ASSERT(engine.world->resourceManager->fileSystem->profiler == nullptr);
    ASSERT(engine.world->resourceManager->network->debugger == nullptr);
    ASSERT(engine.world->sceneGraph->analytics == nullptr);

    // Shared dependencies resolve to the same instance within the context
    auto* configFromFS = engine.world->resourceManager->fileSystem->config;
    auto* configFromGD = engine.world->sceneGraph->graphicsDriver->config;
    auto* configFromNet = engine.world->resourceManager->network->config;
    auto* configFromAD = engine.world->resourceManager->audioDriver->config;
    ASSERT(configFromFS == configFromGD);
    ASSERT(configFromFS == configFromNet);
    ASSERT(configFromFS == configFromAD);

    // World and Simulation share the same SceneGraph and EventBus
    ASSERT(engine.world->sceneGraph == engine.simulation->sceneGraph);
    ASSERT(engine.world->eventBus == engine.simulation->eventBus);
}
REGISTER_TEST(TestComplexDependencyTree);

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "--list")
    {
        for (auto& t : Tests())
            std::cout << t.name << std::endl;
        return 0;
    }

    if (argc == 2)
    {
        std::string target = argv[1];
        for (auto& t : Tests())
        {
            if (t.name == target)
            {
                t.fn();
                return 0;
            }
        }

        std::cerr << "Unknown test: " << target << std::endl;
        return 1;
    }

    std::cout << "Genode.IoC Tests" << std::endl;
    std::cout << "================" << std::endl;

    for (auto& t : Tests())
    {
        std::cout << "  " << t.name << "... ";
        try {
            t.fn();
            std::cout << "PASSED" << std::endl;
            ++testsPassed;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << std::endl;
            ++testsFailed;
        } catch (...) {
            std::cout << "FAILED: unknown error" << std::endl;
            ++testsFailed;
        }
    }

    std::cout << "================" << std::endl;
    std::cout << "Passed: " << testsPassed << " / " << (testsPassed + testsFailed) << std::endl;

    if (testsFailed > 0)
    {
        std::cout << "SOME TESTS FAILED!" << std::endl;
        return 1;
    }

    std::cout << "ALL TESTS PASSED!" << std::endl;
    return 0;
}
