//----------------------------------------------------------------------------------------------------------------------
// File: PackageSynchronizer.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "CipherPackage.hpp"
#include "SecurityTypes.hpp"
#include "SecurityDefinitions.hpp"
#include "SynchronizerContext.hpp"
#include "Components/Configuration/Options.hpp"
#include "Interfaces/Synchronizer.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace Node { class ServiceProvider; }
namespace Configuration::Options { class SupportedAlgorithms; }
namespace Security::Detail { class SynchronizerContext; }

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

class PackageSynchronizer : public ISynchronizer
{
public:
    PackageSynchronizer(ExchangeRole role, std::weak_ptr<Configuration::Options::SupportedAlgorithms> const& wpSupportedAlgorithms);

    PackageSynchronizer(PackageSynchronizer&& other) noexcept;
    PackageSynchronizer& operator=(PackageSynchronizer&& other) noexcept;

    PackageSynchronizer(PackageSynchronizer const& other) = delete;
    PackageSynchronizer& operator=(PackageSynchronizer const& other) = delete;

    // ISynchronizer {
    [[nodiscard]] virtual ExchangeRole GetExchangeRole() const override;

    [[nodiscard]] virtual std::uint32_t GetStages() const override;
    [[nodiscard]] virtual SynchronizationStatus GetStatus() const override;
    [[nodiscard]] virtual bool Synchronized() const override;

    [[nodiscard]] virtual SynchronizationResult Initialize() override;
    [[nodiscard]] virtual SynchronizationResult Synchronize(ReadableView buffer) override;
    [[nodiscard]] virtual std::unique_ptr<CipherPackage> Finalize() override;
    // } ISynchronizer

    static void PackAndCacheSupportedAlgorithms(Configuration::Options::SupportedAlgorithms const& supportedAlgorithms);

private:
    class ISynchronizationRoleExecutor
    {
    public:
        using OptionalInitializationResult = std::optional<std::pair<Buffer, Buffer>>;

        virtual ~ISynchronizationRoleExecutor() = default;

        [[nodiscard]] virtual std::uint32_t GetStages() const = 0;
        [[nodiscard]] virtual SynchronizationStatus GetStatus() const = 0;
        [[nodiscard]] virtual bool Synchronized() const = 0;

        [[nodiscard]] virtual SynchronizationResult Initialize() = 0;
        [[nodiscard]] virtual SynchronizationResult Synchronize(ReadableView buffer) = 0;

        virtual void SetContext(std::reference_wrapper<Detail::SynchronizerContext>&& context) = 0;
    };

    class InitiatingRoleExecutor;
    class AcceptingRoleExecutor;

    static inline auto CachedSupportedAlgorithmsBuffer = Buffer{};

    Detail::SynchronizerContext m_context;
    std::unique_ptr<ISynchronizationRoleExecutor> m_upExecutor;
};

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------
