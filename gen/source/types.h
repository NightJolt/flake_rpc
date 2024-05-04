#pragma once

#include <flake/std/types/string.h>
#include <flake/std/types/string_view.h>
#include <flake/types.h>

namespace fl {
    struct rpc_type_element_t {
        str_t type;
        bool generated = false;
    };

    struct rpc_type_t {
        rpc_type_element_t elements[4];
        uint32_t elements_count = 0;
    };

    struct cpp_type_element_t {
        str_t type;
        bool is_trivially_copyable = true;
    };

    struct cpp_type_t {
        cpp_type_element_t elements[8];
        uint32_t elements_count = 0;

        str_t as_type;
        str_t as_ref;
        str_t as_template_args;
    };

    cpp_type_t str_to_cpp_type(const str_t&);
    cpp_type_t str_to_cpp_type(str_view_t);
}