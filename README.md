# Genode.IoC #
Genode.IoC is a non-intrusive single file header IoC Container for C++ 17.  
It is one of my in-house game engine's subset modules, **Genode** (**G**ame **E**ngi**N**e **O**n **DE**mand).  

While several great IoC solutions are available that are much more powerful and flexible, Genode.IoC aims to enable lightweight/small-footprint projects with a simple implementation.

## Features ##
- Small, simple, and fast IoC Container.
- Single header file.
- Non-intrusive: No interface or contracts neeeded.
- Minimum configuration.
- Autowire class dependencies with the constructor.
- Simple and easy lifetime management.

## Integration ##

Genode.IoC is distributed as a single header file, which can be included and compiled in other projects.  
Add [`Context.hpp`](./include/Genode/Context.hpp) into your project files and include the header in the source files you wish to interact with IoC Container:

```cpp
#include <Genode/Context.hpp>
```

## Usage ##

## Registration ##

The container does not require registration or configuration for concrete classes by default. 
It will automatically create the object, including resolving the dependencies for you.  

However, the type registration can be called explicitly by calling `Provide` method.  

Consider the following structures:
```c++
struct InputSystem {};
struct MovementSystem {
    MovementSystem(InputSystem& input) : m_input(&input) {}
    InputSystem* m_input;
};
```

The following code demonstrates how to register types of the above structures:
```c++
auto context = Gx::Context();
context.Provide<MovementSystem>();
context.Provide<InputSystem>();
```

> [!Tip]
> You can register your type out-of-order.  
> 
> The object will be lazily created when you call `Require<T>()`, and the container will resolve its dependency auto-magically.

> [!Important]
> The registrant type needs at least one public constructor.
> The container will use the constructor with the least number of parameters.
> 
> If the public constructor with the least number of parameters has overload, the container will fail to resolve and a runtime error will be thrown when retrieving the object by reference. 
> You must bind the type with [builder](#binding-with-builder) to register the type before retrieving such type.
> 
> Additionally, the type must never accept smart pointers in the constructor as the container handles the lifetime of dependencies.
> 
> For `private`/`protected` constructors, see below.

## Binding interface ##

If your object constructor require an interface type, you must bind the interface before retrieving the type from the container.  
Use `As<T>()` to bind your interface with concrete type when calling `Provide<T>()`.  

Consider the following structures:
```c++
class IInputSystem
{
protected:
    IInputSystem();
};

struct InputSystem : IInputSystem {};
struct MovementSystem {
    MovementSystem(InputSystem& input) : m_input(&input) {}
    InputSystem* m_input;
};
```
The following code demonstrate how to register interface type above:
```c++
auto context = Gx::Context();
context.Provide<IInputSystem>(context.As<InputSystem>());
context.Provide<MovementSystem>(); // Optional
```
> [!Important]
> A runtime error will be thrown if you try to resolve a type that depends on an unregistered interface type.  
> You also can't bind interface type with default `Provide<T>`; otherwise, it will be a compile-time error.

## Binding with builder ##

You can specify how the container should create the object using the builder overload.

```c++
auto context = Gx::Context();
context.Provide<IInputSystem>([] (auto& ctx)
{
    return std::make_unique<InputSystem>( // return as a std::unique_ptr
        ctx.Require<KeyboardSystem>(), 
        ctx.Require<MouseSystem>()
    );
});
```

## Retrieving object ##

Use `Require<T>` to resolve the object. The method will try to call the `Provide<T>` method with the type that is not registered within the container.

```c++
auto context = Gx::Context();

// Create or retrieve MovementSystem from the container
// If the type is not registered, the container will automatically register the type for you.
auto& movementSystem = context.Require<MovementSystem>();

// Use the pointer overload if you don't want the container to register the type automatically.
// If the type is not registered, it will return `nullptr` instead.
auto lifeSystem = context.Require<LifeSystem*>();
assert(lifeSystem == nullptr, "Life System is not registered within the context!");
```

## Lifetime ##

Each `Provide<T>` method overload has an optional `Scope` parameter which allows you to choose between `Scope::Local` and `Scope::Singleton` to control the lifetime of the object.
By default, `Provide<T>` use `Scope::Local` which makes each call to `Require<T>` creates a new instance.

A singleton is created by specifying `Scope::Singleton` during registration:

```c++
auto context = Gx::Context();
context.Provide<SharedService>(Scope::Singleton);

auto& instance1 = context.Require<SharedService>();
auto& instance2 = context.Require<SharedService>();
assert(&instance1 == &instance2);

```

Scopes allow finer-grained lifetime control, where all types registered as local context are unique within a given scope. 
This allows singleton-like behavior within a scope but multiple object instances can be created across scopes.  

Scopes are created by calling `CreateScope()` on a context instance:

```c++
auto context = Gx::Context();
context.Provide<FooBar>(Scope::Local); // Specifying `Scope::Local` is optional

auto& instance1 = context.Require<FooBar>();
auto& instance2 = context.Require<FooBar>();

// Container is itself a scope
assert(&instance1 == &instance2);

{
    // Create a new scope
    auto scope = context.CreateScope();
    auto& instance3 = scope.Require<FooBar>();
    auto& instance4 = scope.Require<FooBar>();
    
    // Instances should be equal inside a scope
    assert(&instance3 == &instance4);
    
    // Instances should not be equal across scopes
    assert(&instance1 != &instance3);
}
```

## License ##
This is an open-sourced library licensed under the [MIT license](LICENSE).
