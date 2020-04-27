#ifndef _CSP_HH_H_
#define _CSP_HH_H_

#ifndef __cplusplus
#   error "C++ header should be included by C++ build only"
#endif

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>

#include "csp/csp.h"
#include "csp/csp_cmp.h"

namespace csp
{
    using exception = std::runtime_error;

    /**< Service handler callback. Takes a libcsp connection
     * and handles it. Typically there are two types of handlers:
     * 1) Short lived handlers that call `auto packet = ::csp_read(conn, 0);`
     *    and pass the first packet to a `dport` specific function.
     * 2) Long lived connection handler (like ftp) that will do a series
     *    of transmissions over time before closing the connection.
     */
    using csp_service_handler_fn = std::function<bool(csp_conn_t *, csp_packet_t *&)>;

    namespace
    {
        static std::once_flag csp_init_once;

    } /* end anon namespace */

    struct buffer_config
    {
        unsigned int num_buffers;
        unsigned int buffer_size;
    };

    struct router_config
    {
        unsigned int stack_size;
        unsigned int prio;
    };

    struct rdp_config
    {
        unsigned int window_size;
        unsigned int conn_timeout_ms;
        unsigned int packet_timeout_ms;
        unsigned int delayed_acks;
        unsigned int ack_timeout;
        unsigned int ack_delay_count;
    };

    struct config
    {
        std::uint8_t addr;
        std::string hostname;
        std::string model;
        buffer_config buffer_conf;
        router_config router_conf;
        rdp_config rdp_conf;
    };

    /**
     * @brief Proxy object for `cmp ident`
     * @details This exists to have safer storage since libcsp
     *  sometimes references global memory. We simply create this
     *  so there are copy targets for the values.
     */
    struct ident_response
    {
        std::string hostname;
        std::string model;
        std::string revision;
        std::string date;
        std::string _time;
    };

    template<typename = void>
        void set_debug_level(csp_debug_level_t lvl)
        {
            ::csp_debug_set_level(CSP_ERROR,    lvl >= CSP_ERROR    ? true : false);
            ::csp_debug_set_level(CSP_WARN,     lvl >= CSP_WARN     ? true : false);
            ::csp_debug_set_level(CSP_INFO,     lvl >= CSP_INFO     ? true : false);
            ::csp_debug_set_level(CSP_BUFFER,   lvl >= CSP_BUFFER   ? true : false);
            ::csp_debug_set_level(CSP_PACKET,   lvl >= CSP_PACKET   ? true : false);
            ::csp_debug_set_level(CSP_PROTOCOL, lvl >= CSP_PROTOCOL ? true : false);
            ::csp_debug_set_level(CSP_LOCK,     lvl >= CSP_LOCK     ? true : false);
        }

    static inline std::string error_to_str(int err)
    {
#       define STR_CASE(X) case X: return #X
        switch (err)
        {
            STR_CASE(CSP_ERR_NONE    ); /* No error */
            STR_CASE(CSP_ERR_NOMEM   ); /* Not enough memory */
            STR_CASE(CSP_ERR_INVAL   ); /* Invalid argument */
            STR_CASE(CSP_ERR_TIMEDOUT); /* Operation timed out */
            STR_CASE(CSP_ERR_USED    ); /* Resource already in use */
            STR_CASE(CSP_ERR_NOTSUP  ); /* Operation not supported */
            STR_CASE(CSP_ERR_BUSY    ); /* Device or resource busy */
            STR_CASE(CSP_ERR_ALREADY ); /* Connection already in progress */
            STR_CASE(CSP_ERR_RESET   ); /* Connection reset */
            STR_CASE(CSP_ERR_NOBUFS  ); /* No more buffer space available */
            STR_CASE(CSP_ERR_TX      ); /* Transmission failed */
            STR_CASE(CSP_ERR_DRIVER  ); /* Error in driver layer */
            STR_CASE(CSP_ERR_AGAIN   ); /* Resource temporarily unavailable */
            STR_CASE(CSP_ERR_HMAC    ); /* HMAC failed */
            STR_CASE(CSP_ERR_XTEA    ); /* XTEA failed */
            STR_CASE(CSP_ERR_CRC32   ); /* CRC32 failed */
        };

        return "UNKNOWN";
#       undef STR_CASE
    }

    /**
     * @brief Identifies a remote node
     * @details This is invokes csp_cmp_ident. This is a rather large reply
     *  as it holds ASCII string data. You should not invoke this frequently on
     *  resource constrained nodes.
     * @param node The remote node to identify
     * @param timeout_ms The timeout for cmp send + reply
     * @param i The identification response
     * @return std::errc::success on success
     * @return std::errc::timed_out if the request was not fulfilled
     */
    template<typename = void>
        std::error_code identify(std::uint8_t node, std::uint32_t timeout_ms, ident_response & i)
        {
            struct csp_cmp_message msg;

            if (CSP_ERR_TIMEDOUT == csp_cmp_ident(node, timeout_ms, &msg))
            {
                return std::make_error_code(std::errc::timed_out);
            }

            i.hostname = std::string{msg.ident.hostname};
            i.model = std::string{msg.ident.model};
            i.revision = std::string{msg.ident.revision};
            i.date = std::string{msg.ident.date};
            i._time = std::string{msg.ident.time};

            return {};
        }

    /**
     * @brief Pings a host
     * @param node The host/node to ping
     * @param timeout_ms The timeout for the ping send + response
     * @param size The size in bytes of the ping
     * @param flags The libcsp connection flags to apply
     * @return True if the ping was successful
     * @return False otherwise
     */
    template<typename = void>
        bool ping(std::uint8_t node, std::uint32_t timeout_ms, std::uint8_t size = 1,
                  std::uint32_t flags = 0, std::uint32_t * response_ms = nullptr)
        {
            auto res = ::csp_ping(node, timeout_ms, size, flags);

            if (response_ms) {
                if (res != -1)
                    *response_ms = static_cast<std::uint32_t>(res);
            }

            return (res != -1);
        }

    template<typename = void>
        void reboot(std::uint8_t node)
        {
            ::csp_reboot(node);
        }

    namespace detail
    {
        template<typename = void>
            void init_once(config const & conf)
            {
                static config conf_copy = conf;

                set_debug_level(CSP_WARN);

                ::csp_set_hostname(conf_copy.hostname.c_str());
                ::csp_set_model(conf_copy.model.c_str());
                ::csp_set_revision(VCS_REV);

                ::csp_buffer_init(conf_copy.buffer_conf.num_buffers,
                                  conf_copy.buffer_conf.buffer_size);

                if (CSP_ERR_NONE != ::csp_init(conf_copy.addr)) {
                    throw csp::exception("Failed to initialize CSP");
                }

                ::csp_rdp_set_opt(conf_copy.rdp_conf.window_size,
                                  conf_copy.rdp_conf.conn_timeout_ms,
                                  conf_copy.rdp_conf.packet_timeout_ms,
                                  conf_copy.rdp_conf.delayed_acks,
                                  conf_copy.rdp_conf.ack_timeout,
                                  conf_copy.rdp_conf.ack_delay_count);
                
                ::csp_route_start_task(conf_copy.router_conf.stack_size,
                                       conf_copy.router_conf.prio);
            }

    } /* end namespace detail */

    /**
     * @brief libcsp server/service handler
     * @details libcsp's runtime is required for initializing transactions
     *  and connections. But to be reactive to csp traffic, you need a service
     *  handler much like you'd need a listening socket when reacting to IP traffic.
     */
    struct server final
    {
    private:
        std::atomic_bool m_local_stop{false};

    public:
        server() = default;
        ~server() { m_local_stop = true; }

        /**
         * @brief The server functor, does the handling of traffic
         * @param service_handler The function to be invoked for every incoming packet
         *  to determine if the user needs to handle it. If the function returns false, then
         *  the packet is ran through libcsp's default handler for extra filtering.
         * @param global_stop External loop terminator
         */
        void operator()(csp_service_handler_fn user_service_handler, std::atomic_bool const & global_stop)
        {
            int csp_err;
            csp_socket_t * sock = ::csp_socket(0);
            csp_conn_t * conn = nullptr;
            csp_packet_t * packet = nullptr;

            if (CSP_ERR_NONE !=
                (csp_err = ::csp_bind(sock, CSP_ANY)))
            {
                throw csp::exception("Failed to invoke ::csp_bind: " + error_to_str(csp_err));
            }

            if (CSP_ERR_NONE !=
                (csp_err = ::csp_listen(sock, CSP_CONN_QUEUE_LENGTH)))
            {
                throw csp::exception("Failed to invoke ::csp_listen: " + error_to_str(csp_err));
            }

            while (! m_local_stop && ! global_stop)
            {
                conn = ::csp_accept(sock, 1000);

                if (NULL == conn)
                    continue;

                if (user_service_handler && ! user_service_handler(conn, packet))
                {
                    /* fork off to the default handler. We know it closes the connection
                     * after one response, so we can close this side of the connection after the
                     * first packet response. */
                    packet = ::csp_read(conn, 500);
                    ::csp_service_handler(conn, packet);
                    ::csp_close(conn);
                }
            }
        }
    };

    template<typename = void>
        void init(config const & _conf)
        {
            std::call_once(csp_init_once, detail::init_once<>, std::cref(_conf));
        }

} /* end namespace csp */

#endif
