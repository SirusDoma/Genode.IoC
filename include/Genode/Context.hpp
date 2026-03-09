#pragma once

#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <type_traits>
#include <stdexcept>
#include <string>

namespace Gx
{
    class Context;

    enum class Scope
    {
        Local,
        Singleton
    };

    namespace Detail
    {
        template <typename Exclude>
        struct AnyType
        {
            template <typename T, typename = std::enable_if_t<
                !std::is_pointer_v<T> &&
                !std::is_same_v<std::decay_t<T>, std::decay_t<Exclude>> &&
                !std::is_same_v<std::decay_t<T>, Context>>>
            // ReSharper disable once CppFunctionIsNotImplemented CppNonExplicitConversionOperator
            operator T& () const noexcept;

            template <typename T, typename = std::enable_if_t<
                !std::is_pointer_v<T> &&
                !std::is_same_v<std::decay_t<T>, std::decay_t<Exclude>> &&
                !std::is_same_v<std::decay_t<T>, Context>>>
            // ReSharper disable once CppFunctionIsNotImplemented CppNonExplicitConversionOperator
            operator T&& () const noexcept;

            template <typename T, typename = std::enable_if_t<
                std::is_pointer_v<T> &&
                !std::is_same_v<std::decay_t<std::remove_pointer_t<T>>, std::decay_t<Exclude>> &&
                !std::is_same_v<std::decay_t<std::remove_pointer_t<T>>, Context>>, typename = void>
            // ReSharper disable once CppFunctionIsNotImplemented CppNonExplicitConversionOperator
            operator T () const noexcept;
        };

        template <typename T>
        using Probe = AnyType<T>;

        inline constexpr std::size_t MaxConstructorArity = 100;

        template <typename T, std::size_t>
        using ProbeFor = Probe<T>;

        template <typename T, typename Seq>
        struct IsConstructibleFromSeq;

        template <typename T, std::size_t... Is>
        struct IsConstructibleFromSeq<T, std::index_sequence<Is...>>
            : std::is_constructible<T, ProbeFor<T, Is>...> {};

        template <typename T, std::size_t N = 0>
        struct MinArity
            : std::conditional_t<
                  IsConstructibleFromSeq<T, std::make_index_sequence<N>>::value,
                  std::integral_constant<std::size_t, N>,
                  MinArity<T, N + 1>
              > {};

        template <typename T>
        struct MinArity<T, MaxConstructorArity + 1>
            : std::integral_constant<std::size_t, MaxConstructorArity + 1> {};

        template <typename T>
        inline constexpr std::size_t MinArityV = MinArity<T>::value;

        template <typename Owner>
        struct Resolver
        {
            Context& Ctx;

            template <typename T, typename = std::enable_if_t<
                !std::is_pointer_v<T> &&
                !std::is_same_v<std::decay_t<T>, std::decay_t<Owner>> &&
                !std::is_same_v<std::decay_t<T>, Context>>>
            // ReSharper disable once CppNonExplicitConversionOperator
            operator T& () const;

            template <typename T, typename = std::enable_if_t<
                !std::is_pointer_v<T> &&
                !std::is_same_v<std::decay_t<T>, std::decay_t<Owner>> &&
                !std::is_same_v<std::decay_t<T>, Context>>>
            // ReSharper disable once CppNonExplicitConversionOperator
            operator T&& () const;

            template <typename T, typename = std::enable_if_t<
                std::is_pointer_v<T> &&
                !std::is_same_v<std::decay_t<std::remove_pointer_t<T>>, std::decay_t<Owner>> &&
                !std::is_same_v<std::decay_t<std::remove_pointer_t<T>>, Context>>, typename = void>
            // ReSharper disable once CppNonExplicitConversionOperator
            operator T () const;
        };

    }

    template <typename T>
    inline constexpr bool IsConstructible =
        !std::is_abstract_v<T> &&
        (Detail::MinArityV<T> <= Detail::MaxConstructorArity);

    class Context
    {
    public:
        Context() : m_parent(nullptr) {}

        Context(const Context&)            = delete;
        Context& operator=(const Context&) = delete;

        Context(Context&& other) noexcept
            : m_parent(other.m_parent),
              m_entries(std::move(other.m_entries))
        {}

        Context& operator=(Context&& other) noexcept
        {
            if (this != &other)
            {
                m_parent  = other.m_parent;
                m_entries = std::move(other.m_entries);
                m_current = nullptr;
            }
            return *this;
        }

        template <typename T>
        void Provide(Scope scope = Scope::Local)
        {
            static_assert(IsConstructible<T>,
                "Cannot resolve type. Either it is abstract or no public "
                "constructor was found within the arity limit. "
                "Use Provide<Interface, Concrete>() or Provide<T>(builder) instead.");

            const auto key = std::type_index(typeid(T));
            auto svc = std::make_shared<Service<T>>();
            svc->Lifetime  = scope;
            svc->Builder   = CreateBuilder<T>();
            m_entries[key] = std::move(svc);
        }

        template <typename TInterface, typename TConcrete>
        void Provide(Scope scope = Scope::Local)
        {
            static_assert(std::is_base_of_v<TInterface, TConcrete>,
                "Concrete type must derive from the interface type.");
            static_assert(IsConstructible<TConcrete>,
                "Concrete type must be constructible (not abstract, valid constructor).");

            const auto key = std::type_index(typeid(TInterface));
            auto svc = std::make_shared<Service<TInterface>>();
            svc->Lifetime = scope;
            svc->Builder  = [](Context& c) -> std::unique_ptr<TInterface>
            {
                return CreateBuilder<TConcrete>()(c);
            };
            m_entries[key] = std::move(svc);
        }

        template <typename T>
        void Provide(std::function<std::unique_ptr<T>(Context&)> builder,
                     Scope scope = Scope::Local)
        {
            const auto key = std::type_index(typeid(T));
            auto svc = std::make_shared<Service<T>>();
            svc->Lifetime  = scope;
            svc->Builder   = std::move(builder);
            m_entries[key] = std::move(svc);
        }

        template <typename T>
        std::enable_if_t<!std::is_pointer_v<T>, T&>
        Require()
        {
            using Type = std::remove_cv_t<std::remove_reference_t<T>>;
            const auto key = std::type_index(typeid(Type));

            if (auto* svc = GetService<Type>(key))
            {
                if (svc->Instance)
                {
                    if (m_current)
                        m_current->Dependencies.push_back(m_entries[key]);

                    return *svc->Instance;
                }

                auto* prev = m_current;
                m_current = svc;
                svc->Instance = svc->Builder(*this);
                m_current = prev;

                if (m_current)
                    m_current->Dependencies.push_back(m_entries[key]);

                return *svc->Instance;
            }

            if (m_parent)
            {
                if (auto* ptr = m_parent->Require<Type*>())
                    return *ptr;
            }

            if constexpr (IsConstructible<Type>)
            {
                Provide<Type>();
                return Require<T>();
            }
            else
            {
                throw std::runtime_error(
                    std::string("Gx::Context::Require — type not registered and cannot be resolved: ") +
                    typeid(Type).name());
            }
        }

        template <typename T>
        std::enable_if_t<std::is_pointer_v<T>, T>
        Require()
        {
            using Type = std::remove_cv_t<std::remove_pointer_t<T>>;
            const auto key = std::type_index(typeid(Type));

            if (auto* svc = GetService<Type>(key))
            {
                if (svc->Instance)
                {
                    if (m_current)
                        m_current->Dependencies.push_back(m_entries[key]);
                    return svc->Instance.get();
                }

                auto* prev = m_current;
                m_current = svc;
                svc->Instance = svc->Builder(*this);
                m_current = prev;

                if (m_current)
                    m_current->Dependencies.push_back(m_entries[key]);

                return svc->Instance.get();
            }

            if (m_parent)
                return m_parent->Require<T>();

            return nullptr;
        }

        template <typename T>
        std::unique_ptr<T> Instantiate()
        {
            using Type = std::remove_cv_t<std::remove_reference_t<T>>;
            const auto key = std::type_index(typeid(Type));

            if (auto* svc = GetService<Type>(key))
                return svc->Builder(*this);

            if (m_parent)
            {
                if (auto* ptr = m_parent->Require<Type*>())
                    return m_parent->Instantiate<T>();
            }

            if constexpr (IsConstructible<Type>)
            {
                return CreateBuilder<Type>()(*this);
            }
            else
            {
                throw std::runtime_error(
                    std::string("Gx::Context::Instantiate — type not registered and cannot be resolved: ") +
                    typeid(Type).name());
            }
        }

        Context CreateScope()
        {
            Context scope;
            scope.m_parent = this;
            for (auto& [key, entry] : m_entries)
            {
                if (entry->Lifetime == Scope::Singleton)
                    scope.m_entries[key] = entry;
                else
                    scope.m_entries[key] = entry->Clone(true);
            }
            return scope;
        }

        Context Capture()
        {
            Context captured;
            captured.m_parent = nullptr;

            for (auto& [key, entry] : m_entries)
                captured.m_entries[key] = entry->Clone(false);

            return captured;
        }

    private:
        struct Scopable
        {
            Scope Lifetime = Scope::Local;
            std::vector<std::shared_ptr<Scopable>> Dependencies;
            virtual ~Scopable() = default;
            [[nodiscard]] virtual std::shared_ptr<Scopable> Clone(bool reset) const = 0;
        };

        template <typename T>
        struct Service final : Scopable
        {
            std::function<std::unique_ptr<T>(Context&)> Builder;
            std::shared_ptr<T> Instance;

            [[nodiscard]]
            std::shared_ptr<Scopable> Clone(bool reset) const override
            {
                auto clone = std::make_shared<Service<T>>();
                clone->Lifetime = Lifetime;
                clone->Builder  = Builder;
                clone->Instance = reset ? nullptr : Instance;
                return clone;
            }
        };

        using ScopableMap = std::unordered_map<std::type_index, std::shared_ptr<Scopable>>;

        template <typename T, std::size_t... Is>
        static std::unique_ptr<T> Construct(Context& c, std::index_sequence<Is...>)
        {
            return std::make_unique<T>(((void)Is, Detail::Resolver<T>{c})...);
        }

        template <typename T>
        static std::function<std::unique_ptr<T>(Context&)> CreateBuilder()
        {
            static_assert(IsConstructible<T>,
                "Type is not constructible: no public constructor found within "
                "the arity limit, or the type is abstract. "
                "Use Provide<T>(builder) to register a factory manually.");

            return [](Context& c) -> std::unique_ptr<T>
            {
                return Construct<T>(c, std::make_index_sequence<Detail::MinArityV<T>>{});
            };
        }

        template <typename T>
        Service<T>* GetService(std::type_index key)
        {
            if (const auto it = m_entries.find(key); it != m_entries.end())
                return static_cast<Service<T>*>(it->second.get());

            return nullptr;
        }

        Context*    m_parent;
        ScopableMap m_entries;
        Scopable*   m_current = nullptr;
    };

    namespace Detail
    {
        template <typename Owner>
        template <typename T, typename>
        Resolver<Owner>::operator T& () const
        {
            return Ctx.Require<T>();
        }

        template <typename Owner>
        template <typename T, typename>
        Resolver<Owner>::operator T&& () const
        {
            return std::move(Ctx.Require<T>());
        }

        template <typename Owner>
        template <typename T, typename, typename>
        Resolver<Owner>::operator T () const
        {
            return Ctx.Require<T>();
        }
    }
}
