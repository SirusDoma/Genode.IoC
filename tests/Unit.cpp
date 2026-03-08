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
