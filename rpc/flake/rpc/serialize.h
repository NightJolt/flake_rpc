#pragma once

#include "../globals.h"
#include "../bytes.h"
#include "../color.h"
#include "defs.h"
#include "stub.h"
#include "scope.h"

#define NESTED_TEMPLATE_ARGS(TYPE, TEMPLATED_TYPE, TEMPLATED_TYPE_VARIADIC)\
    template <class TYPE, template <class...> class TEMPLATED_TYPE, template <class...> class... TEMPLATED_TYPE_VARIADIC>

namespace fun::rpc {
    template <class, template <class...> class...>
    struct nested_t;

    template <class A>
    struct nested_t<A> {
        using type = A;
    };

    NESTED_TEMPLATE_ARGS(T, P, V)
    struct nested_t<T, P, V...> {
        using type = P<typename nested_t<T, V...>::type>;
    };



    template <template <class...> class, template <class...> class...>
    struct is_same_template_t {
        static constexpr bool value = false;
    };

    template <template <class...> class T>
    struct is_same_template_t<T, T> {
        static constexpr bool value = true;
    };



    template <class T>
    concept STR_T = std::is_same_v<T, std::string>;

    template <class T>
    concept INT_T = std::is_integral_v<T>;

    template <class T>
    concept BYTES_T = std::is_same_v<T, bytes_t>;

    template <template <class...> class V>
    concept VEC_T = is_same_template_t<V, std::vector>::value;

    template <class T>
    concept COL_T = std::is_same_v<T, rgb_t> || std::is_same_v<T, rgba_t>;

    template <template <class...> class V>
    concept UPTR_T = is_same_template_t<V, std::unique_ptr>::value;

    template <template <class...> class V>
    concept STUB_T = is_same_template_t<V, stub_t>::value;



    class serializer_t {
    public:
        serializer_t();
        ~serializer_t() = default;

        serializer_t(const serializer_t&) = delete;
        serializer_t& operator=(const serializer_t&) = delete;

        serializer_t(serializer_t&&) noexcept = delete;
        serializer_t& operator=(serializer_t&&) noexcept = delete;

        template <INT_T INT>
        void serialize(INT value) {
            *(INT*)cursor = value;
            cursor += sizeof(INT);
        }
        
        template <STR_T>
        void serialize(const std::string& value) {
            serialize<uint32_t>(value.size());
            memcpy(cursor, value.data(), value.size());
            cursor += value.size();
        }

        template <BYTES_T>
        void serialize(const bytes_t& value) {
            serialize<uint32_t>(value.get_size());
            value.copy_out(cursor, value.get_size());
            cursor += value.get_size();
        }

        template <COL_T col_t>
        void serialize(const col_t& value) {
            serialize<uint8_t>(value.r);
            serialize<uint8_t>(value.g);
            serialize<uint8_t>(value.b);

            if constexpr (requires { value.a; }) {
                serialize<uint8_t>(value.a);
            }
        }

        NESTED_TEMPLATE_ARGS(T, P, V)
        requires VEC_T<P>
        void serialize(const std::vector<typename nested_t<T, V...>::type>& value) {
            serialize<uint32_t>(value.size());

            for (const auto& item : value) {
                serialize<T, V...>(item);
            }
        }

        template <class T>
        requires std::is_base_of_v<i_hollow_t, T>
        void serialize(const T& value) {
            addr_t owner_addr = get_rpc_scope().get_connection_provider().get_pub_addr();
            const stub_t<T>* stub = dynamic_cast<const stub_t<T>*>(&value);

            if (stub) {
                owner_addr = stub->owner_addr;
            }

            serialize<iid_t>(value.iid);
            serialize<ip_t>(owner_addr.ip);
            serialize<port_t>(owner_addr.port);
            serialize<oid_t>((oid_t)(i_hollow_t*)&value);
        }

        NESTED_TEMPLATE_ARGS(T, P, V)
        requires UPTR_T<P>
        void serialize(const std::unique_ptr<typename nested_t<T, V...>::type>& value) {
            serialize<uint8_t>(value != nullptr);

            if (value != nullptr) {
                serialize<T, V...>(*value);
            }
        }

        uint8_t* get_data();
        uint32_t get_size();

    private:
        uint8_t data[max_packet_size];
        uint8_t* cursor;
    };

    class deserializer_t {
    public:
        deserializer_t(
            uint8_t*
        );

        template <INT_T INT>
        INT deserialize() {
            INT value = *(INT*)cursor;
            cursor += sizeof(INT);

            return value;
        }

        template <STR_T>
        std::string deserialize() {
            uint32_t size = deserialize<uint32_t>();
            std::string value((char*)cursor, size);
            cursor += size;

            return value;
        }

        template <BYTES_T>
        bytes_t deserialize() {
            uint32_t size = deserialize<uint32_t>();
            bytes_t value(bytes_t::create(cursor, size));
            cursor += size;

            return value;
        }

        template <COL_T col_t>
        col_t deserialize() {
            col_t value;
            
            value.r = deserialize<uint8_t>();
            value.g = deserialize<uint8_t>();
            value.b = deserialize<uint8_t>();

            if constexpr (requires { value.a; }) {
                value.a = deserialize<uint8_t>();
            }

            return value;
        }

        NESTED_TEMPLATE_ARGS(T, P, V)
        requires VEC_T<P>
        std::vector<typename nested_t<T, V...>::type> deserialize() {
            uint32_t size = deserialize<uint32_t>();
            std::vector<typename nested_t<T, V...>::type> value;

            for (uint32_t i = 0; i < size; i++) {
                value.push_back(deserialize<T, V...>());
            }

            return value;
        }

        template <class T>
        requires std::is_base_of_v<i_hollow_t, T>
        T* deserialize() {
            iid_t iid;
            addr_t addr;
            oid_t oid;

            iid = deserialize<iid_t>();
            addr.ip = deserialize<ip_t>();
            addr.port = deserialize<port_t>();
            oid = deserialize<oid_t>();

            return (T*)get_rpc_scope().get_stub_factory().create(iid, addr, oid);
        }

        NESTED_TEMPLATE_ARGS(T, P, V)
        requires UPTR_T<P>
        std::unique_ptr<typename nested_t<T, V...>::type> deserialize() {
            bool is_not_null = deserialize<uint8_t>();

            if (is_not_null) {
                return std::unique_ptr<typename nested_t<T, V...>::type>(deserialize<T, V...>());
            }
            
            return nullptr;
        }

    private:
        uint8_t* data;
        uint8_t* cursor;
    };
}