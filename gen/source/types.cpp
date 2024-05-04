#include "types.h"

#include <flake/utils/string.h>
#include <flake/iterator.h>
#include <flake/std/types/unordered_map.h>

namespace {
    fl::umap_t<fl::str_t, fl::cpp_type_element_t> cpp_type_elements = {
        { "void", { "void", true } },
        { "i8", { "int8_t", true } },
        { "i16", { "int16_t", true } },
        { "i32", { "int32_t", true } },
        { "i64", { "int64_t", true } },
        { "u8", { "uint8_t", true } },
        { "u16", { "uint16_t", true } },
        { "u32", { "uint32_t", true } },
        { "u64", { "uint64_t", true } },
        { "rgb", { "fl::rgb_t", true } },
        { "rgba", { "fl::rgba_t", true } },
        { "str", { "str_t", false } },
        { "bytes", { "fl::bytes_t", false } },
        { "vec", { "fl::vec_t", false } },
    };
}

fl::rpc_type_t str_to_rpc_type(const fl::str_t& str_type) {
    fl::rpc_type_t rpc_type;

    fl::strutil::tokens_iterator_t tokens = fl::strutil::tokenize(str_type, { "<", ">" }, false);
    fl::iterator_t<fl::str_view_t> it(tokens.begin(), tokens.end());

    for (auto token : tokens) {
        fl::str_t str_token(token);

        rpc_type.elements[rpc_type.elements_count++] = { str_token, cpp_type_elements.find(str_token) == cpp_type_elements.end() };
    }

    return rpc_type;
}

fl::vec_t<fl::cpp_type_element_t> rpc_to_cpp_type_elements(const fl::rpc_type_element_t& rpc_type_element) {
    if (rpc_type_element.generated) {
        return {
            { "std::unique_ptr", false },
            { "i_" + rpc_type_element.type + "_t", false },
        };
    }

    return { cpp_type_elements[rpc_type_element.type] };
}

fl::str_t cpp_type_as_type(const fl::cpp_type_t& cpp_type) {
    fl::str_t as_type = cpp_type.elements[0].type;
    bool is_trivially_copyable = cpp_type.elements[0].is_trivially_copyable;

    for (uint32_t i = 1; i < cpp_type.elements_count; i++) {
        as_type += "<" + cpp_type.elements[i].type;
    }

    for (uint32_t i = 1; i < cpp_type.elements_count; i++) {
        as_type += ">";
    }

    return as_type;
}

fl::str_t cpp_type_as_ref(const fl::cpp_type_t& cpp_type) {
    return cpp_type.elements[0].is_trivially_copyable ? cpp_type.as_type : "const " + cpp_type.as_type + "&";
}

fl::str_t cpp_type_as_template_args(const fl::cpp_type_t& cpp_type) {
    fl::str_t as_template_args = cpp_type.elements[cpp_type.elements_count - 1].type;

    for (int32_t i = 0; i < cpp_type.elements_count - 1; i++) {
        as_template_args += ", " + cpp_type.elements[i].type;
    }

    return as_template_args;
}

fl::cpp_type_t rpc_to_cpp_type(const fl::rpc_type_t& rpc_type) {
    fl::cpp_type_t cpp_type;

    for (uint32_t i = 0; i < rpc_type.elements_count; i++) {
        auto elements = rpc_to_cpp_type_elements(rpc_type.elements[i]);

        for (auto element : elements) {
            cpp_type.elements[cpp_type.elements_count++] = element;
        }
    }

    cpp_type.as_type = cpp_type_as_type(cpp_type);
    cpp_type.as_ref = cpp_type_as_ref(cpp_type);
    cpp_type.as_template_args = cpp_type_as_template_args(cpp_type);

    return cpp_type;
}

fl::cpp_type_t fl::str_to_cpp_type(const str_t& str_type) {
    return rpc_to_cpp_type(str_to_rpc_type(str_type));
}

fl::cpp_type_t fl::str_to_cpp_type(str_view_t str_type) {
    return str_to_cpp_type(str_t(str_type));
}
