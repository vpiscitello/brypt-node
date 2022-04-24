//----------------------------------------------------------------------------------------------------------------------
// File: Auxiliary.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageHandler.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Route::Auxiliary {
//----------------------------------------------------------------------------------------------------------------------

class ExternalHandler;

//----------------------------------------------------------------------------------------------------------------------
} // Route::Auxiliary namespace
//----------------------------------------------------------------------------------------------------------------------

class Route::Auxiliary::ExternalHandler : public Route::IMessageHandler
{
public:
    using ExternalCallback = std::function<bool(Message::Application::Parcel const& message, Peer::Action::Next& next)>;

    explicit ExternalHandler(ExternalCallback const& callback);

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider) override;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const& message, Peer::Action::Next& next) override;
    // }IMessageHandler
   
private:
    ExternalCallback const m_callback;
};

//----------------------------------------------------------------------------------------------------------------------
