#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <tuple>
#include <utility>
#include <stdexcept>

namespace Gx
{
    namespace priv
    {
        // The number of parameters supported.
        // See Constructible for more information.
        constexpr unsigned int MaxParameterCount = 100;

        // tag<T, N> generates friend declarations and helps with overload resolution.
        // There are two types: one with the auto return type, which is the way we read types later.
        // The second one is used in the detection of instantiations without which we'd get multiple
        // definitions.
        template <typename T, int N>
        struct tag
        {
            friend auto& loophole(tag<T, N>);
            constexpr friend int cloophole(tag<T, N>);
        };

        // The definitions of friend functions.
        template <typename T, typename U, int N, bool B,
                  typename = typename std::enable_if_t<
                    !std::is_same_v<
                      std::remove_cv_t<std::remove_reference_t<T>>,
                      std::remove_cv_t<std::remove_reference_t<U>>>>>
        struct fn_def
        {
            // TODO: Find cleaner way for abstract and non-public constructor?
            // ReSharper disable once CppDFANullDereference
            friend auto& loophole(tag<T, N>) { return *static_cast<U*>(nullptr); }
            constexpr friend int cloophole(tag<T, N>) { return 0; }
        };

        // This specialization is to avoid multiple definition errors.
        template <typename T, typename U, int N> struct fn_def<T, U, N, true> {};

        // This has a templated conversion operator which in turn triggers instantiations.
        // Important point, using sizeof seems to be more reliable. Also default template
        // arguments are "cached" (I think). To fix that I provide a U template parameter to
        // the ins functions which do the detection using constexpr friend functions and SFINAE.
        template <typename T, int N>
        struct c_op
        {
            template <typename U, int M>
            static auto ins(...) -> int;
            template <typename U, int M, int = cloophole(tag<T, M>{})>
            static auto ins(int) -> char;

            template <typename U, int = sizeof(fn_def<T, U, N, sizeof(ins<U, N>(0)) == sizeof(char)>)>
            operator U*();

            template <typename U, int = sizeof(fn_def<T, U, N, sizeof(ins<U, N>(0)) == sizeof(char)>)>
            operator U&();
        };

        // This is a helper to turn a ctor into a tuple type.
        // Usage is: refl::as_tuple<data_t>
        template <typename T, typename U> struct loophole_tuple;

        // This is a helper to turn a ctor into a tuple type.
        template <typename T, int... Ns>
        struct loophole_tuple<T, std::integer_sequence<int, Ns...>>
        {
            using type = std::tuple<decltype(loophole(tag<T, Ns>{}))...>;
        };
    }

    // Here is a version of fields_number to handle user-provided ctor.
    // NOTE: It finds the first ctor having the shortest unambigious set
    //       of parameters.
    template <typename T, int... Ns>
    constexpr auto GetConstructorParameterCount(int) -> decltype(T(priv::c_op<T, Ns>{}...), 0)
    {
        return sizeof...(Ns);
    }

    template <typename T, int... Ns>
    constexpr int GetConstructorParameterCount(...)
    {
        if constexpr (sizeof...(Ns) < priv::MaxParameterCount)
            return GetConstructorParameterCount<T, Ns..., sizeof...(Ns)>(0);
        else
            return sizeof...(Ns);
    }

    template <typename T, int... Ns>
    constexpr int GetConstructorParameterCount(std::integral_constant<int, priv::MaxParameterCount>)
    {
        return sizeof...(Ns);
    }

    // Usage is: Gx::ConstructorDescriptor<MyClass>
    template <typename T>
    using ConstructorDescriptor = typename priv::loophole_tuple<T, std::make_integer_sequence<int, GetConstructorParameterCount<T>(0)>>::type;

    template <typename T>
    struct Constructible
    {
        // GetConstructorParameterCount will repeatedly recurse itself infinitely when a given type has no public constructor.
        // Use this as an advantage to determine whether the class can be publicly constructible by setting a depth limit to the recursive.
        static constexpr bool value = !std::is_abstract_v<T> && (std::is_default_constructible_v<T> || GetConstructorParameterCount<T>(0) < priv::MaxParameterCount);
    };
}

namespace Gx
{
    class Context final
    {
    public:
        template<typename T>
        using Builder = std::function<std::unique_ptr<T>(const Context&)>;

        enum class Scope
        {
            Local,
            Shared
        };

        Context() = default;
        Context(Context&& other) noexcept
        {
            m_factories = std::exchange(other.m_factories, {});
            m_instances = std::exchange(other.m_instances, {});
        }

        virtual ~Context() = default;

        Context& operator=(Context&& other) noexcept
        {
            m_factories = std::exchange(other.m_factories, {});
            m_instances = std::exchange(other.m_instances, {});

            return *this;
        }

        template<typename T>
        void Provide(Scope scope = Scope::Local);

        template<typename T, typename U>
        std::enable_if_t<std::is_base_of_v<T, U>>
        Provide(Scope scope = Scope::Local);

        template<typename T>
        void Provide(Builder<T> builder, Scope scope = Scope::Local);

        template<typename T>
        Builder<T> As() const;

        template<typename T>
        std::enable_if_t<!std::is_pointer_v<T>, T&>
        Require() const;

        template<typename T>
        std::enable_if_t<std::is_pointer_v<T>, T>
        Require() const;

        template<typename T>
        std::unique_ptr<T> Create() const;

        template<typename T, typename... Args>
        T Invoke(std::function<T(Args&&...)> func) const;

        Context Capture() const
        {
            return {*this};
        }

        bool Empty() const { return m_factories.empty() && m_instances.empty(); }

    private:
        Context(const Context& other)
        {
            for (auto& [type, factory] : other.m_factories)
            {
                if (factory->Scope == Scope::Shared)
                    m_factories[type] = std::shared_ptr(factory);
            }

            for (auto& [type, instance] : other.m_instances)
            {
                if (instance->Scope == Scope::Shared)
                    m_instances[type] = std::shared_ptr(instance);
            }
        }

        struct Scoppable
        {
            explicit Scoppable(const Scope scope) : Scoppable::Scope(scope) {};
            virtual ~Scoppable() = default;

            Context::Scope Scope;
        };

        using ScoppableMap = std::unordered_map<std::type_index, std::shared_ptr<Scoppable>>;

        template<typename T>
        struct Instance final : Scoppable
        {
            explicit Instance(std::unique_ptr<T> handle, const Context::Scope scope) :
                Scoppable(scope), Handle(std::move(handle)) {};

            std::unique_ptr<T> Handle;
        };

        template<typename T>
        struct Factory final : Scoppable
        {
            Factory(Builder<T> builder, Context::Scope scope) :
                Scoppable(std::move(scope)), Create(std::move(builder)) {};

            Context::Builder<T> Create;
        };

        template <typename T>
        decltype(auto) BuildParameter() const;

        template <typename Tuple, std::size_t... Is>
        auto BuildParameters(std::index_sequence<Is...>) const;

        template <typename Tuple>
        auto BuildParameters() const;

        mutable ScoppableMap m_factories{};
        mutable ScoppableMap m_instances{};
    };
}

namespace Gx
{
    template<typename T>
    void Context::Provide(const Scope scope)
    {
        static_assert(Constructible<T>::value, "Use Provide<T>(Builder<T>, Scope) instead for interface or complex constructible type");
        Provide<T>(As<T>(), scope);
    }

    template<typename T, typename U>
    std::enable_if_t<std::is_base_of_v<T, U>>
    Context::Provide(Scope scope)
    {
        Provide<T>(As<U>(), scope);
    }

    template<typename T>
    void Context::Provide(Builder<T> builder, const Scope scope)
    {
        const std::type_index type = typeid(T);
        auto factory = std::make_shared<Factory<T>>(builder, scope);

        m_instances[type] = std::make_shared<Instance<T>>(std::move(factory->Create(*this)), scope);
        m_factories[type] = std::move(factory);
    }

    template<typename T>
    Context::Builder<T> Context::As() const
    {
        return Context::Builder<T>([this] (const Context&) -> std::unique_ptr<T>
        {
            if constexpr (std::is_default_constructible_v<T>)
                return std::make_unique<T>();
            else if constexpr (Constructible<T>::value)
                return std::apply([](auto&&... args){ return std::make_unique<T>(args...); }, BuildParameters<ConstructorDescriptor<T>>());
            else
                throw std::runtime_error(std::string(typeid(T).name()) + " is not constructible");
        });
    }

    template<typename T>
    std::enable_if_t<!std::is_pointer_v<T>, T&>
    Context::Require() const
    {
        using U = std::decay_t<T>;

        if (auto instance = Require<U*>(); instance)
            return *instance;

        if constexpr (Constructible<U>::value)
        {
            const std::type_index type = typeid(T);
            auto factory = std::make_shared<Factory<U>>(As<U>(), Scope::Local);

            m_instances[type] = std::make_shared<Instance<U>>(std::move(factory->Create(*this)), Scope::Local);
            m_factories[type] = std::move(factory);

            return static_cast<T&>(*(static_cast<Instance<U>*>(m_instances[type].get()))->Handle.get());
        }
        else
            throw std::runtime_error(std::string(typeid(T).name()) + " is not constructible and not provided within the current context");
    }

    template<typename T>
    std::enable_if_t<std::is_pointer_v<T>, T>
    Context::Require() const
    {
        using R = std::remove_pointer_t<T>;
        const std::type_index type = typeid(R);

        if (const auto it = m_instances.find(type); it != m_instances.end())
            return static_cast<T>((static_cast<Instance<R>*>(it->second.get()))->Handle.get());

        if (const auto it = m_factories.find(type); it != m_factories.end())
        {
            auto factory      = static_cast<Factory<R>*>(it->second.get());
            m_instances[type] = std::make_shared<Instance<R>>(std::move(factory->Create(*this)), Scope::Local);

            return static_cast<T>((static_cast<Instance<R>*>(m_instances[type].get()))->Handle.get());
        }

        return nullptr;
    }

    template<typename T>
    std::unique_ptr<T> Context::Create() const
    {
        if constexpr (!std::is_default_constructible_v<T>)
        {
            const std::type_index type = typeid(T);
            if (const auto it = m_factories.find(type); it != m_factories.end())
                return static_cast<Factory<T>*>(it->second.get())->Create(*this);

            auto factory = std::make_unique<Factory<T>>(As<T>(), Scope::Local);
            return factory->Create(*this);
        }
        else
            return std::make_unique<T>();
    }

    template<typename T, typename... Args>
    T Context::Invoke(std::function<T(Args&&...)> func) const
    {
        static_assert((Constructible<std::decay_t<Args>>::value && ...), "Function parameters cannot be constructed");

        return std::apply([func](auto&&... args)
        {
            return func(std::forward<Args>(args)...);
        }, BuildParameters<std::tuple<std::decay_t<Args>...>>());
    }

    template <typename T>
    decltype(auto) Context::BuildParameter() const
    {
        if constexpr (std::is_pointer_v<T>)
            return std::tuple { Require<T>() };
        else
            return std::tuple { std::tie(Require<T>()) };
    }

    template <typename Tuple, std::size_t... Is>
    auto Context::BuildParameters(std::index_sequence<Is...>) const
    {
        return std::tuple_cat(BuildParameter<std::tuple_element_t<Is, Tuple>>()...);
    }

    template <typename Tuple>
    auto Context::BuildParameters() const
    {
        constexpr std::size_t N = std::tuple_size_v<Tuple>;
        return BuildParameters<Tuple>(std::make_index_sequence<N>{});
    }
}
