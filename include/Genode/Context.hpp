#ifndef GENODE_SYSTEM_CONTEXT_HPP
#define GENODE_SYSTEM_CONTEXT_HPP

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <tuple>
#include <utility>
#include <string>
#include <stdexcept>

namespace Gx
{
    namespace priv
    {
        // Based on
        // * https://stackoverflow.com/a/54493136
        //   https://godbolt.org/z/FxPDgU
        // * http://alexpolt.github.io/type-loophole.html
        //   https://github.com/alexpolt/luple/blob/master/type-loophole.h
        //   by Alexandr Poltavsky, http://alexpolt.github.io
        // * https://www.youtube.com/watch?v=UlNUNxLtBI0
        //   Better C++14 reflections - Antony Polukhin - Meeting C++ 2018
        // * Also special thanks to lapinozz

        // The number of parameters supported.
        // See Constructible for more information.
        constexpr unsigned int MaxParameterCount = 100;

        // tag<T, N> generates friend declarations and helps with overload resolution.
        // There are two types: one with the auto return type, which is the way we read types later.
        // The second one is used in the detection of instantiations without which we'd get multiple
        // definitions.
        template <typename T, int N>
        struct tag {
            friend auto loophole(tag<T, N>);
            constexpr friend int cloophole(tag<T, N>);
        };

        // The definitions of friend functions.
        template <typename T, typename U, int N, bool B,
                  typename = typename std::enable_if_t<
                    !std::is_same_v<
                      std::remove_cv_t<std::remove_reference_t<T>>,
                      std::remove_cv_t<std::remove_reference_t<U>>>>>
        struct fn_def {
            friend auto loophole(tag<T, N>) { return *static_cast<U*>(nullptr); }
            constexpr friend int cloophole(tag<T, N>) { return 0; }
        };

        // This specialization is to avoid multiple definition errors.
        template <typename T, typename U, int N> struct fn_def<T, U, N, true> {};

        // This has a templated conversion operator which in turn triggers instantiations.
        // Important point, using sizeof seems to be more reliable. Also default template
        // arguments are "cached" (I think). To fix that I provide a U template parameter to
        // the ins functions which do the detection using constexpr friend functions and SFINAE.
        template <typename T, int N>
        struct c_op {
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
        struct loophole_tuple<T, std::integer_sequence<int, Ns...>> {
            using type = std::tuple<decltype(loophole(tag<T, Ns>{}))...>;
        };

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

    class Context
    {
    public:
        template<typename T>
        using Builder = std::function<std::unique_ptr<T>(Context&)>;

        enum class Scope
        {
            Local,
            Singleton
        };

        Context() = default;
        virtual ~Context() = default;

        template<typename T>
        std::enable_if_t<priv::Constructible<T>::value, void> Provide(Scope scope = Scope::Local)
        {
            Provide<T>(As<T>(), scope);
        }

        template<typename T>
        std::enable_if_t<!priv::Constructible<T>::value, void> Provide(Scope scope = Scope::Local)
        {
            static_assert(priv::Constructible<T>::value, "Use Provide<T>(Builder<T>, Scope) instead for interface or complex constructible type.");
        }

        template<typename T>
        void Provide(Builder<T> builder, const Scope scope)
        {
            const std::type_index type = typeid(T);
            std::unique_ptr<Factory<T>> factory = std::make_unique<Factory<T>>(builder, scope);

            m_instances[type] = std::make_unique<Instance<T>>(std::move(factory->Builder(*this)), scope);
            m_factories[type] = std::move(factory);
        }

        template<typename T>
        Builder<T> As()
        {
            return Builder<T>([this] (Context& ctx) -> std::unique_ptr<T>
            {
                return std::make_unique<T>(std::make_from_tuple<T>(ctx.BuildParameters<priv::ConstructorDescriptor<T>>()));
            });
        }

        template<typename T>
        std::enable_if_t<std::is_pointer_v<T>, T> Require()
        {
            using R = std::remove_pointer_t<T>;
            const std::type_index type = typeid(R);

            if (const auto it = m_instances.find(type); it != m_instances.end())
                return static_cast<T>((static_cast<Instance<R>*>(it->second.get()))->Handle.get());

            // TODO: Should we share the singleton with parent?
            if (const auto it = m_factories.find(type); it != m_factories.end())
            {
                auto factory      = static_cast<Factory<R>*>(it->second.get());
                auto instance     = std::make_unique<Instance<R>>(std::move(factory->Builder(*this)), Scope::Local);
                m_instances[type] = std::move(instance);

                return static_cast<T>((static_cast<Instance<R>*>(m_instances[type].get()))->Handle.get());
            }

            if constexpr (priv::Constructible<R>::value)
            {
                Provide<R>();
                return Require<T>();
            }
            else
                return nullptr;
        }

        template<typename T>
        std::enable_if_t<!std::is_pointer_v<T>, T&>
        Require()
        {
            if (auto instance = Require<T*>(); instance)
                return *instance;

            throw std::runtime_error(std::string(typeid(T).name()) + " is not constructible and not provided within the current context.");
        }

        Context CreateScope()
        {
            return Context(*this);
        }

    private:
        Context(Context& other) :
            m_parent(&other),
            m_instances(),
            m_factories()
        {
            for (auto& [type, factory] : other.m_factories)
            {
                if (auto clone = factory->Clone(); clone)
                    m_factories[type] = std::move(clone);
            }

            for (auto& [type, instance] : other.m_instances)
            {
                if (auto clone = instance->Clone(); clone)
                    m_instances[type] = std::move(clone);
            }
        }

        // HACK: Ugly abstraction, but it works lol
        struct Base
        {
            explicit Base(const Scope scope) : Scope(scope) {};
            virtual ~Base() = default;
            virtual std::unique_ptr<Base> Clone() = 0;

            Context::Scope Scope;
        };
        using StorageMap = std::unordered_map<std::type_index, std::unique_ptr<Base>>;

        template<typename T>
        struct Instance final : Base
        {
            explicit Instance(std::unique_ptr<T> handle, const Context::Scope scope) : Base(scope), Handle(std::move(handle)) {};
            std::unique_ptr<Base> Clone() override
            {
                if (Base::Scope == Scope::Local)
                    return nullptr;

                return std::make_unique<Instance>(std::unique_ptr<T>(Handle.get()), Scope);
            }

            std::unique_ptr<T> Handle;
        };

        template<typename T>
        struct Factory final : Base
        {
            Factory(Context::Builder<T> builder, Context::Scope scope) : Base(std::move(scope)), Builder(std::move(builder)) {};
            std::unique_ptr<Base> Clone() override
            {
                if (Base::Scope == Scope::Singleton)
                    return nullptr;

                return std::make_unique<Factory>(Builder, Scope);
            }

            Context::Builder<T> Builder;
        };

        template <typename T>
        decltype(auto) BuildParameter()
        {
            if constexpr (std::is_pointer_v<T>)
                return std::tuple { Require<T>() };
            else
                return std::tuple { std::tie(Require<T>()) };
        }

        template <typename Tuple, std::size_t... Is>
        auto BuildParameters(std::index_sequence<Is...>)
        {
            return std::tuple_cat(BuildParameter<std::tuple_element_t<Is, Tuple>>()...);
        }

        template <typename Tuple>
        auto BuildParameters()
        {
            constexpr std::size_t N = std::tuple_size_v<Tuple>;
            return BuildParameters<Tuple>(std::make_index_sequence<N>{});
        }

        Context*   m_parent;
        StorageMap m_instances;
        StorageMap m_factories;
    };
}

#endif