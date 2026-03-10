# Genode.IoC #
Genode.IoC is a non-intrusive, single-header [IoC container](https://en.wikipedia.org/wiki/Inversion_of_control) for C++17.  
It is a subset module of **Genode** (**G**ame **E**ngi**N**e **O**n **DE**mand), my in-house game engine.

While more powerful and flexible IoC solutions exist, Genode.IoC is designed for lightweight projects that need a simple, small-footprint, yet still powerful dependency injection container.

## Features ##
- Small, simple, and fast IoC Container.
- Single header file.
- Non-intrusive: no base classes, no interfaces or contracts required.
- Minimum configuration.
- Autowire class dependencies with the constructor.
- Simple and easy lifetime management.

## Integration ##

Copy [`Context.hpp`](./include/Genode/Context.hpp) into your project and include it:

```cpp
#include <Genode/Context.hpp>
```

## Usage ##

### Registration ###

The container does not require explicit registration for resolviong reference of concrete classes with constructible dependencies (i.e, not an interface or abstract classes).
It will automatically create the object and resolve its dependencies when first requested.

However, you may have to register types explicitly using the `Provide` method when resolving as pointer type:

```cpp
struct InputSystem {};
struct MovementSystem {
    MovementSystem(InputSystem& input) : m_input(&input) {}
    InputSystem* m_input;
};
```

```cpp
auto context = Gx::Context();
context.Provide<MovementSystem>();
context.Provide<InputSystem>();
```

> [!Tip]
> Types can be registered in any order.
>
> Objects are lazily created on the first call to `Require<T>()`, and the container resolves dependencies automatically.

> [!Important]
> The type must have at least one public constructor.
> The container selects the constructor with the fewest parameters.
>
> If the shortest constructor has an ambiguous overload, the container will fail to resolve it and throw a runtime error.
> Use a [builder](#binding-with-builder) to register such types.
>
> Constructor parameters must not be not smart pointers, as the container manages dependency lifetimes internally.
>
> For types with only `private`/`protected` constructors, use a [binding](#binding-interface).

### Binding interface ###

If a type depends on an interface, you must bind the interface to a concrete type before resolving it.
Use `Provide<TInterface, TConcrete>()` to register the binding:

```cpp
class IInputSystem
{
public:
    virtual ~IInputSystem() = default;

protected:
    IInputSystem() = default;
};

struct InputSystem : IInputSystem {};
struct MovementSystem {
    MovementSystem(IInputSystem& input) : m_input(&input) {}
    IInputSystem* m_input;
};
```

```cpp
auto context = Gx::Context();
context.Provide<IInputSystem, InputSystem>();
context.Provide<MovementSystem>(); // Optional: auto-wired on Require
```

> [!Important]
> A runtime error is thrown if you resolve a type that depends on an unregistered interface.
> Abstract or interface types cannot be registered with the plain `Provide<T>()` overload; this will produce a compile-time error.

### Binding with builder ###

You can provide a custom factory to control how an object is created.
The builder receives the container as a parameter and must return a `std::unique_ptr<T>`:

```cpp
auto context = Gx::Context();
context.Provide<IInputSystem>([] (auto& ctx)
{
    return std::make_unique<InputSystem>(
        ctx.Require<KeyboardSystem>(),
        ctx.Require<MouseSystem>()
    );
});
```

### Retrieving objects ###

Use `Require<T>()` to resolve an object.
If the type is not registered, the container will automatically register and create it:

```cpp
auto context = Gx::Context();

// Retrieve (or auto-create) a MovementSystem
auto& movementSystem = context.Require<MovementSystem>();

// Use the pointer overload to query without auto-registration.
// Returns nullptr if the type is not registered.
auto* lifeSystem = context.Require<LifeSystem*>(); // nullptr if not registered
```

### Creating new instances ###

Use `Instantiate<T>()` to always get a fresh instance as a `std::unique_ptr<T>`, regardless of whether the type is registered as a singleton or local:

```cpp
auto context = Gx::Context();
context.Provide<InputSystem>(Gx::Scope::Singleton);

auto instance = context.Instantiate<InputSystem>(); // Always a new instance
```

### Lifetime ###

Each `Provide` overload accepts an optional `Gx::Scope` parameter to control object lifetime:

- **`Scope::Local`** (default) — each scope gets its own instance.
- **`Scope::Singleton`** — a single shared instance across all scopes.

A singleton is created by specifying `Gx::Scope::Singleton` during registration:

```cpp
auto context = Gx::Context();
context.Provide<SharedService>(Gx::Scope::Singleton);

auto& a = context.Require<SharedService>();
auto& b = context.Require<SharedService>();
assert(&a == &b); // Same instance
```

Scopes provide finer-grained lifetime control.
Local types get a unique instance per scope, while singletons are shared across all scopes:

```cpp
auto context = Gx::Context();
context.Provide<FooBar>(); // Scope::Local by default

auto& a = context.Require<FooBar>();
auto& b = context.Require<FooBar>();
assert(&a == &b); // Same instance within the same scope

{
    auto scope = context.CreateScope();
    auto& c = scope.Require<FooBar>();
    auto& d = scope.Require<FooBar>();

    assert(&c == &d);  // Same within child scope
    assert(&a != &c);  // Different across scopes
}
```

### Capture ###

Use `Capture()` to create a standalone snapshot of a context.
The captured context is independent, and changes to the original context after capture have no effect:

```cpp
auto context = Gx::Context();
context.Provide<InputSystem>(Gx::Scope::Singleton);

auto& original = context.Require<InputSystem>();
auto captured = context.Capture();

auto& fromCaptured = captured.Require<InputSystem>();
assert(&original == &fromCaptured); // Singleton instance is shared

// New registrations on the original are not visible in the captured context
context.Provide<PhysicsSystem>();
auto* ptr = captured.Require<PhysicsSystem*>();
assert(ptr == nullptr);
```

## Building and testing ##

Genode.IoC uses CMake to build and run the test suite:

```sh
cmake -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

> [!Tip]
> On single-config generators (GCC, Clang), the `-C Release` flag can be omitted.

To run the benchmarks:

```sh
cmake -B build
cmake --build build --config Release --target benchmarks
./build/Release/benchmarks        # Plain output
./build/Release/benchmarks --md   # Markdown table
```

## Benchmarks ##

> [!Note]
> These benchmarks use a simple `std::chrono`-based harness without optimizer fences or statistical analysis.
> Results may vary due to CPU throttling, OS scheduling, and compiler optimizations.
>
> Take them with grain of salt. It is to be treated as rough ballpark figures for relative comparison, not precise measurements.

The following benchmarks run using Github Action runners.

<!-- BENCHMARK_START -->
**MSVC** (`windows-latest`)

> CPU: AMD EPYC 7763 64-Core Processor                 (2 cores, 4 threads) | Memory: 15 GB

| Benchmark | Result |
|-----------|-------:|
| Provide | 193.5 ns/op |
| Require (first) | 301.8 ns/op |
| Require (cached) | 26.6 ns/op |
| Require (auto-wire) | 1142.6 ns/op |
| Require (interface) | 480.2 ns/op |
| Require (pointer, miss) | 15.2 ns/op |
| Instantiate | 57.8 ns/op |
| CreateScope | 392.6 ns/op |
| Capture | 448.4 ns/op |

**GCC** (`ubuntu-latest`)

> CPU: AMD EPYC 7763 64-Core Processor (4 cores, 4 threads) | Memory: 15 GB

| Benchmark | Result |
|-----------|-------:|
| Provide | 787.9 ns/op |
| Require (first) | 1387.3 ns/op |
| Require (cached) | 123.2 ns/op |
| Require (auto-wire) | 5075.8 ns/op |
| Require (interface) | 1523.7 ns/op |
| Require (pointer, miss) | 41.2 ns/op |
| Instantiate | 218.7 ns/op |
| CreateScope | 2086.3 ns/op |
| Capture | 2581.3 ns/op |

**Clang** (`macos-latest`)

> CPU: Apple M1 (Virtual) (3 cores, 3 threads) | Memory: 7 GB

| Benchmark | Result |
|-----------|-------:|
| Provide | 872.6 ns/op |
| Require (first) | 1268.5 ns/op |
| Require (cached) | 105.2 ns/op |
| Require (auto-wire) | 4644.7 ns/op |
| Require (interface) | 1460.2 ns/op |
| Require (pointer, miss) | 54.6 ns/op |
| Instantiate | 161.7 ns/op |
| CreateScope | 2178.1 ns/op |
| Capture | 2671.1 ns/op |

*Commit: [`238f2c9`](https://github.com/SirusDoma/Genode.IoC/commit/238f2c99bea87fbecd6fa7aabc1778f4b2d40793)*

<!-- BENCHMARK_END -->

## License ##
This is an open-sourced library licensed under the [MIT license](LICENSE).
