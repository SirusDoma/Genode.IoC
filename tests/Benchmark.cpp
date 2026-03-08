#include <Genode/Context.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <iomanip>

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

    InputSystem*   m_input;
    PhysicsSystem* m_physics;
};

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

struct BenchmarkEntry
{
    std::string name;
    double ns_per_op;
};

static std::vector<BenchmarkEntry> results;

template <typename Fn>
void Bench(const std::string& name, int iterations, Fn fn)
{
    // Warmup
    for (int i = 0; i < iterations / 10 + 1; ++i)
        fn();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        fn();
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double avg = static_cast<double>(ns) / iterations;
    results.push_back({name, avg});
}

void RunBenchmarks()
{
    const int N = 100000;

    // Provide
    Bench("Provide", N, []
    {
        auto ctx = Gx::Context();
        ctx.Provide<InputSystem>();
    });

    // Require (first call, cold)
    Bench("Require (first)", N, []
    {
        auto ctx = Gx::Context();
        ctx.Provide<InputSystem>();
        ctx.Require<InputSystem>();
    });

    // Require (cached)
    {
        auto ctx = Gx::Context();
        ctx.Provide<InputSystem>();
        ctx.Require<InputSystem>(); // prime the cache
        Bench("Require (cached)", N, [&]
        {
            ctx.Require<InputSystem>();
        });
    }

    // Require (auto-wire with dependencies)
    Bench("Require (auto-wire)", N, []
    {
        auto ctx = Gx::Context();
        ctx.Require<MovementSystem>();
    });

    // Require (interface)
    Bench("Require (interface)", N, []
    {
        auto ctx = Gx::Context();
        ctx.Provide<IRenderer, OpenGLRenderer>();
        ctx.Require<IRenderer>();
    });

    // Require (pointer, miss)
    {
        auto ctx = Gx::Context();
        Bench("Require (pointer, miss)", N, [&]
        {
            ctx.Require<InputSystem*>();
        });
    }

    // Instantiate
    {
        auto ctx = Gx::Context();
        ctx.Provide<InputSystem>();
        Bench("Instantiate", N, [&]
        {
            auto ptr = ctx.Instantiate<InputSystem>();
        });
    }

    // CreateScope
    {
        auto ctx = Gx::Context();
        ctx.Provide<InputSystem>(Gx::Scope::Singleton);
        ctx.Provide<PhysicsSystem>(Gx::Scope::Local);
        ctx.Provide<IRenderer, OpenGLRenderer>();
        Bench("CreateScope", N, [&]
        {
            auto scope = ctx.CreateScope();
        });
    }

    // Capture
    {
        auto ctx = Gx::Context();
        ctx.Provide<InputSystem>(Gx::Scope::Singleton);
        ctx.Provide<PhysicsSystem>(Gx::Scope::Local);
        ctx.Provide<IRenderer, OpenGLRenderer>();
        ctx.Require<InputSystem>();
        Bench("Capture", N, [&]
        {
            auto captured = ctx.Capture();
        });
    }
}

void PrintPlain()
{
    std::cout << "Genode.IoC Benchmarks" << std::endl;
    std::cout << "=====================" << std::endl;

    for (auto& r : results)
    {
        std::cout << "  " << std::left << std::setw(30) << r.name
                  << std::right << std::setw(10) << std::fixed << std::setprecision(1)
                  << r.ns_per_op << " ns/op" << std::endl;
    }
}

void PrintMarkdown()
{
    std::cout << "| Benchmark | Result |" << std::endl;
    std::cout << "|-----------|-------:|" << std::endl;

    for (auto& r : results)
    {
        std::cout << "| " << r.name << " | "
                  << std::fixed << std::setprecision(1) << r.ns_per_op
                  << " ns/op |" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    RunBenchmarks();

    bool markdown = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--md")
            markdown = true;
    }

    if (markdown)
        PrintMarkdown();
    else
        PrintPlain();

    return 0;
}
