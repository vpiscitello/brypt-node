
//----------------------------------------------------------------------------------------------------------------------
// File: SignalService.hpp
// Description: A boost::asio extension service to support Awaitable::ExclusiveSignal. Each instance represents a 
// one-to-one relationship between a notifier and waiter. 
// Reasoning: The aim of this extension is to provide a more intuitive signaling mechanism compared to the 
// boost::asio::steady_timer alternative. This type of extension is required to support coroutines not based on 
// boost::asio::awaitable<T>. Meaning any coroutine that needs custom state between await_ready(), await_suspend(), 
// and await_resume(). 
// Special Note: This is an exercise in understanding boost::asio internals, by nature of using detail classes it 
// is inherently unstable in terms of versioning and supported use cases. But, should be akin to how boost::beast
// is implemented on top of boost::asio. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Utilities/Awaitable.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <compare>
#include <cstddef>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detail/io_object_impl.hpp>
#include <boost/asio/detail/non_const_lvalue.hpp>
#include <boost/asio/detail/throw_error.hpp>
#include <boost/asio/detail/wait_handler.hpp>
#include <boost/asio/error.hpp>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network::TCP {
//----------------------------------------------------------------------------------------------------------------------

template<typename Signal>
concept AllowableSignal = requires()
{
    std::is_member_function_pointer_v<decltype(&Signal::Ready)>;
    std::is_member_function_pointer_v<decltype(&Signal::Signaled)>;
    std::is_member_function_pointer_v<decltype(&Signal::Canceled)>;
    std::is_member_function_pointer_v<decltype(&Signal::Waiting)>;
    std::is_member_function_pointer_v<decltype(&Signal::Notify)>;
    std::is_member_function_pointer_v<decltype(&Signal::Cancel)>;
    std::is_member_function_pointer_v<decltype(&Signal::AsyncWait)>;
};

template<AllowableSignal Signal, typename Executor = boost::asio::any_io_executor> 
class SignalService;

using ExclusiveSignalService = SignalService<Awaitable::ExclusiveSignal, boost::asio::any_io_executor>;

//----------------------------------------------------------------------------------------------------------------------
} // Network::TCP namespace
//----------------------------------------------------------------------------------------------------------------------

template<Network::TCP::AllowableSignal Signal, typename Executor>
class Network::TCP::SignalService
{
public:
    using executor_type = Executor;
    template<typename OtherExecutor> struct rebind_executor { using other = SignalService<OtherExecutor>; };

    explicit SignalService(executor_type const& executor) : m_impl(0, executor) { }

    SignalService(SignalService const& other) = delete;
    SignalService(SignalService&& other) = delete;
    SignalService& operator=(SignalService const& other) = delete;
    SignalService& operator=(SignalService&& other) = delete;

    ~SignalService() = default;

    [[nodiscard]] executor_type get_executor() noexcept { return m_impl.get_executor(); }
    
    [[nodiscard]] bool is_ready() const noexcept
    {
        return m_impl.get_service(m_impl.get_implementation()).is_ready();
    }
    
    [[nodiscard]] bool is_signaled() const noexcept
    {
        return m_impl.get_service(m_impl.get_implementation()).is_signaled();
    }
    
    [[nodiscard]] bool is_canceled() const noexcept
    {
        return m_impl.get_service(m_impl.get_implementation()).is_canceled();
    }

    // Note: notify() will only apply if there is an existing waiter. The waiter will be resumed in  this call. 
    std::size_t notify()
    {
        boost::system::error_code error;
        std::size_t result = m_impl.get_service().notify(m_impl.get_implementation(), error);
        boost::asio::detail::throw_error(error, "notify");
        return result;
    }

    // Note: notify_next() applies regardless of an existing waiter. If there is a waiter, it will be resumed in this 
    // call. Otherwise, the next async_wait() call will not be suspended and will resume immediately in that call. 
    std::size_t notify_next()
    {
        boost::system::error_code error;
        std::size_t result = m_impl.get_service().notify_before(m_impl.get_implementation(), error);
        boost::asio::detail::throw_error(error, "notify");
        return result;
    }

    // Note: cancel applies regardless of an existing waiter. The resumption outcomes are the same as notify_next().
    std::size_t cancel()
    {
        boost::system::error_code error;
        std::size_t result = m_impl.get_service().cancel(m_impl.get_implementation(), error);
        boost::asio::detail::throw_error(error, "cancel");
        return result;
    }

    template<boost::asio::completion_token_for<void(boost::system::error_code)> 
        WaitHandler = boost::asio::default_completion_token<executor_type>::type>
    [[nodiscard]] auto async_wait(
        WaitHandler&& handler = typename boost::asio::default_completion_token<executor_type>::type())
    {
        return boost::asio::async_initiate<WaitHandler, void(boost::system::error_code)>(
            initiate_async_wait(this), handler);
    }

private:
    class initiate_async_wait
    {
    public:
        using executor_type = Executor;
        explicit initiate_async_wait(SignalService* self) : m_self(self) { }
        ~initiate_async_wait() = default;

        [[nodiscard]] executor_type get_executor() const noexcept { return m_self->get_executor(); }

        template<typename WaitHandler> void operator()(WaitHandler&& handler) const
        {
            BOOST_ASIO_WAIT_HANDLER_CHECK(WaitHandler, handler) type_check;
            boost::asio::detail::non_const_lvalue<WaitHandler> other(handler);
            m_self->m_impl.get_service().async_wait(
                m_self->m_impl.get_implementation(), other.value, m_self->m_impl.get_executor());
        }

    private:
        SignalService* m_self;
    };

    class SignalServiceDetail : public boost::asio::detail::execution_context_service_base<SignalServiceDetail>
    {
    public:
        struct implementation_type : private boost::asio::detail::noncopyable { Signal signal; };

        explicit SignalServiceDetail(boost::asio::execution_context& context)
            : boost::asio::detail::execution_context_service_base<SignalServiceDetail>(context)
            , m_scheduler(boost::asio::use_service<boost::asio::detail::scheduler>(context))
        {
            m_scheduler.init_task();
        }

        ~SignalServiceDetail() = default;

        void shutdown() { }
        void construct(implementation_type&) { }
        void destroy(implementation_type& impl)
        {
            boost::system::error_code error;
            [[maybe_unused]] auto result = cancel(impl, error);
            assert(!error && result <= 1);
        }

        void move_construct(implementation_type& impl, implementation_type& other_impl)
        {
            // Copying the other's signal should be safe as the internal state is represented by a std::shared_ptr. 
            // The atomic phase should protect the signal's phase changes. However, it shouldn't be possible to change
            // the phase of either signal at this point. 
            impl.signal.ReferenceMove(other_impl.signal);
        }

        void move_assign(implementation_type& impl, SignalServiceDetail&, implementation_type& other_impl)
        {
            // If there are waiters on this service, cancel it before moving the signals. 
            if (impl.signal.Waiting()) { impl.signal.Cancel(); }
            move_construct(impl, other_impl);
        }

        void converting_move_construct(
            implementation_type& impl, SignalServiceDetail&, implementation_type& other_impl)
        {
            move_construct(impl, other_impl);
        }

        void converting_move_assign(
            implementation_type& impl, SignalServiceDetail& other_service, implementation_type& other_impl)
        {
            move_assign(impl, other_service, other_impl);
        }

        [[nodiscard]] bool is_ready(implementation_type const& impl) const noexcept
        {
            return impl.signal.Ready();
        }
        
        [[nodiscard]] bool is_signaled(implementation_type const& impl) const noexcept
        {
            return impl.signal.Signaled();
        }
        
        [[nodiscard]] bool is_canceled(implementation_type const& impl) const noexcept
        {
            return impl.signal.Canceled();
        }
        
        [[nodiscard]] std::size_t notify(implementation_type& impl, boost::system::error_code& error)
        {
            // Note: notify() only applies if there is a waiter, notify_next() can be used to signal before a wait. 
            if (!impl.signal.Waiting()) { error = boost::system::error_code(); return 0; }

            impl.signal.Notify(); // Cancel the current signal cycle, the waiter will be resumed in this call. 
            error = boost::system::error_code();
            return 1;
        }

        [[nodiscard]] std::size_t notify_next(implementation_type& impl, boost::system::error_code& error)
        {
            // Note: Notification will apply to current and future waiters. 
            impl.signal.Notify(); // Cancel the operation, if there is a waiter it will be resumed in this call. 
            error = boost::system::error_code();
            return 1;
        }

        [[nodiscard]] std::size_t cancel(implementation_type& impl, boost::system::error_code& error)
        {
            // Note: Cancellation will apply to current and future waiters. 
            impl.signal.Cancel(); // Cancel the operation, if there is a waiter it will be resumed in this call. 
            error = boost::system::error_code();
            return 1;
        }

        template<typename WaitHandler, typename WaitExecutor>
        void async_wait(implementation_type& impl, WaitHandler& handler, WaitExecutor const& executor)
        {
            using Operation = boost::asio::detail::wait_handler<WaitHandler, WaitExecutor>;
            typename Operation::ptr operation = 
                { boost::asio::detail::addressof(handler), Operation::ptr::allocate(handler), 0 };
            operation.p = new(operation.v) Operation(handler, executor);
            do_wait(impl.signal, operation.p);
            operation.v = operation.p = nullptr;
        }

    private:
        // Note: This is a basic coroutine task that enables calling co_await in the method body. 
        // Essentially a fire-and-forget task; it does not support external management. The coroutine is only 
        // deallocated on completion (notify() or cancel()). 
        struct BasicWaitTask
        {
            struct promise_type
            {
                [[nodiscard]] auto initial_suspend() const noexcept { return std::suspend_never{}; }
                [[nodiscard]] auto final_suspend() const noexcept { return std::suspend_never{}; }
                BasicWaitTask get_return_object() const noexcept { return {}; }
                void return_void() const noexcept { }
                void unhandled_exception() const noexcept { assert(false); std::terminate(); }
            };
        };

        BasicWaitTask do_wait(Signal const& signal, boost::asio::detail::wait_op* operation)
        {
            // Multiple waiters is not supported in this service. A complete cycle must be completed before waiting
            // again. This can be achieved through a call to notify() or cancel(). 
            // Otherwise, we are able to start the waiting coroutine. 
            if (signal.Waiting()) {
                operation->ec_ = boost::asio::error::operation_not_supported;
            } else {
                auto const result = co_await signal.AsyncWait();
                operation->ec_ = (result == Awaitable::Result::Signaled) ?  
                    boost::system::error_code() : boost::asio::error::operation_aborted;
            }

            m_scheduler.post_immediate_completion(operation, false);
        }

        boost::asio::detail::scheduler& m_scheduler;
    };

    boost::asio::detail::io_object_impl<SignalServiceDetail, executor_type> m_impl;
};

//----------------------------------------------------------------------------------------------------------------------
