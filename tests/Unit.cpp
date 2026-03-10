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
    [[nodiscard]] virtual std::string Name() const = 0;

protected:
    IRenderer() = default;
};

class OpenGLRenderer : public IRenderer
{
public:
    [[nodiscard]] std::string Name() const override { return "OpenGL"; }
};

// Type that requires an interface dependency
struct RenderPipeline
{
    explicit RenderPipeline(IRenderer& renderer) : m_renderer(&renderer) {}
    IRenderer* m_renderer;
};

// Type for builder test
struct AudioSystem
{
    AudioSystem(const int sampleRate, const int channels)
        : SampleRate(sampleRate), Channels(channels) {}

    int SampleRate;
    int Channels;
};

// Destruction order tracking
namespace Lifecycle
{
    struct DestructionLog
    {
        std::vector<std::string> Entries;

        [[nodiscard]] int IndexOf(const std::string& name) const
        {
            for (int i = 0; i < static_cast<int>(Entries.size()); ++i)
            {
                if (Entries[i] == name)
                    return i;
            }

            throw std::runtime_error(name + " not found");
        }
    };

    // Simple chain (3 levels)
    struct SoundBuffer
    {
        explicit SoundBuffer(DestructionLog& log) : m_log(&log) {}
        ~SoundBuffer() { m_log->Entries.emplace_back("SoundBuffer"); }
        DestructionLog* m_log;
    };

    struct SoundSource
    {
        SoundSource(SoundBuffer& b, DestructionLog& log) : buffer(&b), m_log(&log) {}
        ~SoundSource() { m_log->Entries.emplace_back("SoundSource"); }
        SoundBuffer* buffer;
        DestructionLog* m_log;
    };

    struct SoundMixer
    {
        SoundMixer(SoundSource& s, DestructionLog& log) : source(&s), m_log(&log) {}
        ~SoundMixer() { m_log->Entries.emplace_back("SoundMixer"); }
        SoundSource* source;
        DestructionLog* m_log;
    };

    // Input chain (3 levels)
    struct InputDevice
    {
        explicit InputDevice(DestructionLog& log) : m_log(&log) {}
        ~InputDevice() { m_log->Entries.emplace_back("InputDevice"); }
        DestructionLog* m_log;
    };

    struct InputMapper
    {
        InputMapper(InputDevice& d, DestructionLog& log) : device(&d), m_log(&log) {}
        ~InputMapper() { m_log->Entries.emplace_back("InputMapper"); }
        InputDevice* device;
        DestructionLog* m_log;
    };

    struct InputProcessor
    {
        InputProcessor(InputMapper& m, DestructionLog& log) : mapper(&m), m_log(&log) {}
        ~InputProcessor() { m_log->Entries.emplace_back("InputProcessor"); }
        InputMapper* mapper;
        DestructionLog* m_log;
    };

    // Animation chain (3 levels)
    struct Skeleton
    {
        explicit Skeleton(DestructionLog& log) : m_log(&log) {}
        ~Skeleton() { m_log->Entries.emplace_back("Skeleton"); }
        DestructionLog* m_log;
    };

    struct AnimationClip
    {
        AnimationClip(Skeleton& s, DestructionLog& log) : skeleton(&s), m_log(&log) {}
        ~AnimationClip() { m_log->Entries.emplace_back("AnimationClip"); }
        Skeleton* skeleton;
        DestructionLog* m_log;
    };

    struct Animator
    {
        Animator(AnimationClip& c, DestructionLog& log) : clip(&c), m_log(&log) {}
        ~Animator() { m_log->Entries.emplace_back("Animator"); }
        AnimationClip* clip;
        DestructionLog* m_log;
    };

    // Physics chain (3 levels)
    struct Collider
    {
        explicit Collider(DestructionLog& log) : m_log(&log) {}
        ~Collider() { m_log->Entries.emplace_back("Collider"); }
        DestructionLog* m_log;
    };

    struct RigidBody
    {
        RigidBody(Collider& c, DestructionLog& log) : collider(&c), m_log(&log) {}
        ~RigidBody() { m_log->Entries.emplace_back("RigidBody"); }
        Collider* collider;
        DestructionLog* m_log;
    };

    struct PhysicsSolver
    {
        PhysicsSolver(RigidBody& r, DestructionLog& log) : rigidBody(&r), m_log(&log) {}
        ~PhysicsSolver() { m_log->Entries.emplace_back("PhysicsSolver"); }
        RigidBody* rigidBody;
        DestructionLog* m_log;
    };

    // Network chain (3 levels)
    struct Socket
    {
        explicit Socket(DestructionLog& log) : m_log(&log) {}
        ~Socket() { m_log->Entries.emplace_back("Socket"); }
        DestructionLog* m_log;
    };

    struct PacketQueue
    {
        PacketQueue(Socket& s, DestructionLog& log) : socket(&s), m_log(&log) {}
        ~PacketQueue() { m_log->Entries.emplace_back("PacketQueue"); }
        Socket* socket;
        DestructionLog* m_log;
    };

    struct NetworkPeer
    {
        NetworkPeer(PacketQueue& q, DestructionLog& log) : queue(&q), m_log(&log) {}
        ~NetworkPeer() { m_log->Entries.emplace_back("NetworkPeer"); }
        PacketQueue* queue;
        DestructionLog* m_log;
    };

    // Diamond + shared deps (6 types)
    struct ShaderCompiler
    {
        explicit ShaderCompiler(DestructionLog& log) : m_log(&log) {}
        ~ShaderCompiler() { m_log->Entries.emplace_back("ShaderCompiler"); }
        DestructionLog* m_log;
    };

    struct TextureAtlas
    {
        explicit TextureAtlas(DestructionLog& log) : m_log(&log) {}
        ~TextureAtlas() { m_log->Entries.emplace_back("TextureAtlas"); }
        DestructionLog* m_log;
    };

    struct MeshRenderer
    {
        MeshRenderer(ShaderCompiler& s, DestructionLog& log) : shader(&s), m_log(&log) {}
        ~MeshRenderer() { m_log->Entries.emplace_back("MeshRenderer"); }
        ShaderCompiler* shader;
        DestructionLog* m_log;
    };

    struct ParticleEmitter
    {
        ParticleEmitter(ShaderCompiler& s, DestructionLog& log) : shader(&s), m_log(&log) {}
        ~ParticleEmitter() { m_log->Entries.emplace_back("ParticleEmitter"); }
        ShaderCompiler* shader;
        DestructionLog* m_log;
    };

    struct SceneRenderer
    {
        SceneRenderer(MeshRenderer& m, ParticleEmitter& p, DestructionLog& log)
            : mesh(&m), particles(&p), m_log(&log) {}
        ~SceneRenderer() { m_log->Entries.emplace_back("SceneRenderer"); }
        MeshRenderer* mesh; ParticleEmitter* particles;
        DestructionLog* m_log;
    };

    struct DebugRenderer
    {
        DebugRenderer(MeshRenderer& m, TextureAtlas& t, DestructionLog& log)
            : mesh(&m), textures(&t), m_log(&log) {}
        ~DebugRenderer() { m_log->Entries.emplace_back("DebugRenderer"); }
        MeshRenderer* mesh; TextureAtlas* textures;
        DestructionLog* m_log;
    };
}

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
    const auto& input = context.Require<InputSystem>();
    ASSERT(input.Value == 42);
}
REGISTER_TEST(TestDefaultConstruction);

void TestAutoWiring()
{
    auto context = Gx::Context();
    const auto& movement = context.Require<MovementSystem>();
    ASSERT(movement.m_input  != nullptr);
    ASSERT(movement.m_physics != nullptr);
    ASSERT(movement.m_input->Value == 42);
}
REGISTER_TEST(TestAutoWiring);

void TestInterfaceBinding()
{
    auto context = Gx::Context();
    context.Provide<IRenderer, OpenGLRenderer>();
    const auto& renderer = context.Require<IRenderer>();
    ASSERT(renderer.Name() == "OpenGL");
}
REGISTER_TEST(TestInterfaceBinding);

void TestInterfaceDependency()
{
    auto context = Gx::Context();
    context.Provide<IRenderer, OpenGLRenderer>();
    const auto& pipeline = context.Require<RenderPipeline>();
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

    const auto& audio = context.Require<AudioSystem>();
    ASSERT(audio.SampleRate == 44100);
    ASSERT(audio.Channels == 2);
}
REGISTER_TEST(TestBuilderFactory);

void TestSingletonScope()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Singleton);

    const auto& a = context.Require<InputSystem>();
    const auto& b = context.Require<InputSystem>();

    // Same instance within the same scope
    ASSERT(&a == &b);

    {
        // Same instance in a child scope (singleton)
        auto scope = context.CreateScope();
        const auto& c = scope.Require<InputSystem>();
        const auto& d = scope.Require<InputSystem>();

        ASSERT(&c == &d);   // Same within child scope
        ASSERT(&a == &c);   // Same across scopes
    }
}
REGISTER_TEST(TestSingletonScope);

void TestLocalScope()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Local);

    const auto& a = context.Require<InputSystem>();
    const auto& b = context.Require<InputSystem>();

    // Same instance within the same scope
    ASSERT(&a == &b);

    {
        // Different instance in a child scope
        auto scope = context.CreateScope();
        const auto& c = scope.Require<InputSystem>();
        const auto& d = scope.Require<InputSystem>();

        ASSERT(&c == &d);   // Same within child scope
        ASSERT(&a != &c);   // Different across scopes
    }
}
REGISTER_TEST(TestLocalScope);

void TestPointerReturnsNullptr()
{
    auto context = Gx::Context();
    const auto instance = context.Require<AudioSystem*>();
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
    const auto& movement = context.Require<MovementSystem>();
    ASSERT(movement.m_input != nullptr);
    ASSERT(movement.m_physics != nullptr);
    ASSERT(movement.m_input->Value == 42);
}
REGISTER_TEST(TestOutOfOrderRegistration);

void TestCapture()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Singleton);

    const auto& original = context.Require<InputSystem>();

    // Capture creates a standalone snapshot
    auto captured = context.Capture();

    // Singleton instance is shared (same pointer)
    const auto& fromCaptured = captured.Require<InputSystem>();
    ASSERT(&original == &fromCaptured);

    // Register a new type on the source AFTER capture
    context.Provide<PhysicsSystem>();
    context.Require<PhysicsSystem>();

    // Captured context does NOT see the new registration
    const auto instance = captured.Require<PhysicsSystem*>();
    ASSERT(instance == nullptr);
}
REGISTER_TEST(TestCapture);

void TestCaptureFromScope()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>(Gx::Scope::Singleton);
    context.Provide<PhysicsSystem>(Gx::Scope::Local);

    const auto& singletonInst = context.Require<InputSystem>();

    // Create a child scope, resolve local type in it
    auto scope = context.CreateScope();
    const auto& localInScope = scope.Require<PhysicsSystem>();

    // Capture the child scope
    auto snapshot = scope.Capture();

    // Singleton from root is pulled into captured context
    const auto& singletonFromCapture = snapshot.Require<InputSystem>();
    ASSERT(&singletonInst == &singletonFromCapture);

    // Local instance from the scope is also copied
    const auto& localFromCapture = snapshot.Require<PhysicsSystem>();
    ASSERT(&localInScope == &localFromCapture);

    // Modifying parent after capture has no effect
    context.Provide<MovementSystem>();
    context.Require<MovementSystem>();

    const auto instance = snapshot.Require<MovementSystem*>();
    ASSERT(instance == nullptr);
}
REGISTER_TEST(TestCaptureFromScope);

void TestRequireAutoRegistersInCurrentContext()
{
    auto parent = Gx::Context();
    auto child  = parent.CreateScope();

    // Require on child: PhysicsSystem not registered anywhere
    const auto& physics = child.Require<PhysicsSystem>();
    ASSERT(physics.Gravity == 9.81f);

    // Parent must NOT have it
    const auto* fromParent = parent.Require<PhysicsSystem*>();
    ASSERT(fromParent == nullptr);

    // Child must have it
    const auto* fromChild = child.Require<PhysicsSystem*>();
    ASSERT(fromChild != nullptr);
    ASSERT(fromChild == &physics);
}
REGISTER_TEST(TestRequireAutoRegistersInCurrentContext);

void TestInstantiate()
{
    auto context = Gx::Context();
    context.Provide<InputSystem>();

    // Instantiate returns a fresh instance each time
    const auto a = context.Instantiate<InputSystem>();
    const auto b = context.Instantiate<InputSystem>();
    ASSERT(a != nullptr);
    ASSERT(b != nullptr);
    ASSERT(a.get() != b.get());

    // Instantiate on unregistered type creates on the fly without providing
    const auto c = context.Instantiate<PhysicsSystem>();
    ASSERT(c != nullptr);
    ASSERT(c->Gravity == 9.81f);

    // The type must NOT be registered in the context
    const auto* ptr = context.Require<PhysicsSystem*>();
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

void TestDestructionOrder()
{
    auto root = Gx::Context();
    root.Provide<Lifecycle::DestructionLog>(Gx::Scope::Singleton);

    {
        auto scope = root.CreateScope();
        scope.Require<Lifecycle::SoundMixer>();
    }

    const auto& log = root.Require<Lifecycle::DestructionLog>();
    ASSERT(log.Entries.size() == 3);
    ASSERT(log.Entries[0] == "SoundMixer");
    ASSERT(log.Entries[1] == "SoundSource");
    ASSERT(log.Entries[2] == "SoundBuffer");
}
REGISTER_TEST(TestDestructionOrder);

void TestDestructionOrderMultipleChains()
{
    auto root = Gx::Context();
    root.Provide<Lifecycle::DestructionLog>(Gx::Scope::Singleton);

    {
        auto scope = root.CreateScope();
        scope.Require<Lifecycle::SoundMixer>();
        scope.Require<Lifecycle::InputProcessor>();
        scope.Require<Lifecycle::Animator>();
        scope.Require<Lifecycle::PhysicsSolver>();
        scope.Require<Lifecycle::NetworkPeer>();
    }

    const auto& log = root.Require<Lifecycle::DestructionLog>();

    // All 15 types destroyed (5 chains x 3 levels)
    ASSERT(log.Entries.size() == 15);

    // Audio chain: SoundMixer -> SoundSource -> SoundBuffer
    ASSERT(log.IndexOf("SoundMixer")  < log.IndexOf("SoundSource"));
    ASSERT(log.IndexOf("SoundSource") < log.IndexOf("SoundBuffer"));

    // Input chain: InputProcessor -> InputMapper -> InputDevice
    ASSERT(log.IndexOf("InputProcessor") < log.IndexOf("InputMapper"));
    ASSERT(log.IndexOf("InputMapper")    < log.IndexOf("InputDevice"));

    // Animation chain: Animator -> AnimationClip -> Skeleton
    ASSERT(log.IndexOf("Animator")      < log.IndexOf("AnimationClip"));
    ASSERT(log.IndexOf("AnimationClip") < log.IndexOf("Skeleton"));

    // Physics chain: PhysicsSolver -> RigidBody -> Collider
    ASSERT(log.IndexOf("PhysicsSolver") < log.IndexOf("RigidBody"));
    ASSERT(log.IndexOf("RigidBody")     < log.IndexOf("Collider"));

    // Network chain: NetworkPeer -> PacketQueue -> Socket
    ASSERT(log.IndexOf("NetworkPeer") < log.IndexOf("PacketQueue"));
    ASSERT(log.IndexOf("PacketQueue") < log.IndexOf("Socket"));
}
REGISTER_TEST(TestDestructionOrderMultipleChains);

void TestDestructionOrderComplex()
{
    auto root = Gx::Context();
    root.Provide<Lifecycle::DestructionLog>(Gx::Scope::Singleton);

    {
        auto scope = root.CreateScope();
        scope.Require<Lifecycle::SceneRenderer>();
        scope.Require<Lifecycle::DebugRenderer>();
    }

    const auto& log = root.Require<Lifecycle::DestructionLog>();

    // All 6 types must be destroyed
    ASSERT(log.Entries.size() == 6);

    // SceneRenderer must be destroyed before its deps
    ASSERT(log.IndexOf("SceneRenderer") < log.IndexOf("MeshRenderer"));
    ASSERT(log.IndexOf("SceneRenderer") < log.IndexOf("ParticleEmitter"));

    // DebugRenderer must be destroyed before its deps
    ASSERT(log.IndexOf("DebugRenderer") < log.IndexOf("MeshRenderer"));
    ASSERT(log.IndexOf("DebugRenderer") < log.IndexOf("TextureAtlas"));

    // MeshRenderer and ParticleEmitter must be destroyed before ShaderCompiler
    ASSERT(log.IndexOf("MeshRenderer") < log.IndexOf("ShaderCompiler"));
    ASSERT(log.IndexOf("ParticleEmitter") < log.IndexOf("ShaderCompiler"));
}
REGISTER_TEST(TestDestructionOrderComplex);

void TestDestructionOrderSingleton()
{
    auto root = Gx::Context();
    root.Provide<Lifecycle::DestructionLog>(Gx::Scope::Singleton);
    root.Provide<Lifecycle::SoundBuffer>(Gx::Scope::Singleton);
    root.Provide<Lifecycle::SoundSource>(Gx::Scope::Singleton);
    root.Provide<Lifecycle::SoundMixer>(Gx::Scope::Local);

    {
        auto scope = root.CreateScope();
        scope.Require<Lifecycle::SoundMixer>();
    }

    const auto& log = root.Require<Lifecycle::DestructionLog>();

    // Only the Local type (SoundMixer) is destroyed
    ASSERT(log.Entries.size() == 1);
    ASSERT(log.Entries[0] == "SoundMixer");

    // Singletons survive child scope destruction
    const auto* source = root.Require<Lifecycle::SoundSource*>();
    const auto* buffer = root.Require<Lifecycle::SoundBuffer*>();
    ASSERT(source != nullptr);
    ASSERT(buffer != nullptr);
}
REGISTER_TEST(TestDestructionOrderSingleton);

void TestComplexDependencyTree()
{
    auto context = Gx::Context();

    // Resolve the root — auto-wires the entire 5-level tree
    const auto& engine = context.Require<Complex::Engine>();

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
    const auto* configFromFS = engine.world->resourceManager->fileSystem->config;
    const auto* configFromGD = engine.world->sceneGraph->graphicsDriver->config;
    const auto* configFromNet = engine.world->resourceManager->network->config;
    const auto* configFromAD = engine.world->resourceManager->audioDriver->config;
    ASSERT(configFromFS == configFromGD);
    ASSERT(configFromFS == configFromNet);
    ASSERT(configFromFS == configFromAD);

    // World and Simulation share the same SceneGraph and EventBus
    ASSERT(engine.world->sceneGraph == engine.simulation->sceneGraph);
    ASSERT(engine.world->eventBus == engine.simulation->eventBus);
}
REGISTER_TEST(TestComplexDependencyTree);

int main(const int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "--list")
    {
        for (auto& t : Tests())
            std::cout << t.name << std::endl;
        return 0;
    }

    if (argc == 2)
    {
        const std::string target = argv[1];
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

    for (auto& [name, fn] : Tests())
    {
        std::cout << "  " << name << "... ";
        try {
            fn();
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
