#include "generator.h"
#include "types.h"

#include <flake/utils/string.h>

void add_pragma_once(fl::str_t& cpp) {
    cpp += "#pragma once\n\n";
}

void add_include_essentials(fl::str_t& cpp) {
    cpp += "#include <FunEngine2D/core/include/globals.h>\n";
    cpp += "#include <FunEngine2D/core/include/rpc/rpc.h>\n";
    cpp += "#include <FunEngine2D/core/include/rpc/stub.h>\n";
    cpp += "#include <FunEngine2D/core/include/rpc/interfaces.h>\n";
    cpp += "#include <FunEngine2D/core/include/rpc/scope.h>\n";
    cpp += "#include <FunEngine2D/core/include/bytes.h>\n";
    cpp += "#include <FunEngine2D/core/include/color.h>\n";
    cpp += "\n";
}

struct var_t {
    fl::cpp_type_t type;
    fl::str_t name;
};

struct method_t {
    fl::cpp_type_t type;
    fl::str_t name;
    fl::vec_t<var_t> args;
};

struct interface_t {
    fl::str_t base_name;
    fl::str_t mangled_name;
    fl::vec_t<method_t> methods;
};

bool is_interface(fl::strutil::tokens_iterator_t& tokens) {
    return !tokens.is_empty() && tokens.current() == "interface";
}

method_t get_method(fl::strutil::tokens_iterator_t& tokens) {
    method_t method;

    method.type = fl::str_to_cpp_type(tokens.current());
    tokens.advance();

    method.name = fl::str_t(tokens.current());
    tokens.advance();

    tokens.advance();

    while (true) {
        if (tokens.current() == ",") {
            tokens.advance();
        } else if (tokens.current() == ")") {
            break;
        }

        fl::cpp_type_t type = fl::str_to_cpp_type(tokens.current());
        tokens.advance();
        
        fl::str_t name = fl::str_t(tokens.current());
        tokens.advance();

        method.args.push_back({ type, name });
    }

    tokens.advance();

    return method;
}

interface_t get_interface(fl::strutil::tokens_iterator_t& tokens) {
    interface_t interface;
    
    tokens.advance();

    interface.base_name = fl::str_t(tokens.current());
    tokens.advance();

    interface.mangled_name = "i_" + fl::str_t(interface.base_name) + "_t";

    tokens.advance();

    while (tokens.current() != "}") {
        interface.methods.push_back(get_method(tokens));
    }

    tokens.advance();

    return interface;
}

void def_interface_iid(fl::str_t& cpp, const fl::str_t& name) {
    cpp += "    static const fl::rpc::iid_t iid = " + std::to_string(std::hash<fl::str_t>{}(name)) + "u;\n\n";
}

void impl_method_base(fl::str_t& cpp, const method_t& method) {
    cpp += "    virtual " + method.type.as_type + " " + method.name + "(";

    for (uint32_t i = 0; i < method.args.size(); i++) {
        cpp += method.args[i].type.as_ref + " " + method.args[i].name;

        if (i != method.args.size() - 1) {
            cpp += ", ";
        }
    }

    cpp += ") = 0;\n";
}

void impl_interface_ref_typedef(fl::str_t& cpp, const interface_t& interface) {
    cpp += "typedef fl::rpc::ref_t<" + interface.mangled_name + "> " + interface.base_name + "_ref_t;\n";
}

void impl_interface_base(fl::str_t& cpp, const interface_t& interface) {
    cpp += "struct " + interface.mangled_name + " : public fl::rpc::i_hollow_t {\n";

    cpp += "    virtual ~" + interface.mangled_name + "() = default;\n\n";

    def_interface_iid(cpp, interface.base_name);

    for (const method_t& method : interface.methods) {
        impl_method_base(cpp, method);
    }

    cpp += "};\n";
}

void impl_method_stub_ret(fl::str_t& cpp, const method_t& method) {
    cpp += "        " + method.type.as_type + " ret;\n";
    cpp += "        auto sync_call_data_extractor = [&ret](fl::rpc::deserializer_t& deserializer) {\n";
    cpp += "            ret = deserializer.deserialize<" + method.type.as_template_args + ">();\n";
    cpp += "        };\n\n";
    cpp += "        fl::rpc::wait_for_sync_call_reply(sync_call_data_extractor);\n\n";
    cpp += "        return ret;\n";
}

void impl_method_stub(fl::str_t& cpp, const method_t& method, uint32_t method_id) {
    cpp += "    " + method.type.as_type + " " + method.name + "(";

    for (uint32_t i = 0; i < method.args.size(); i++) {
        cpp += method.args[i].type.as_ref + " " + method.args[i].name;

        if (i != method.args.size() - 1) {
            cpp += ", ";
        }
    }

    cpp += ") override {\n";
    cpp += "        fl::rpc::rpc_scope_lock_t lock(rpc);\n\n";

    cpp += "        fl::rpc::serializer_t serializer;\n\n";

    cpp += "        serializer.serialize<fl::rpc::oid_t>(owner_oid);\n";
    cpp += "        serializer.serialize<fl::rpc::iid_t>(iid);\n";
    cpp += "        serializer.serialize<fl::rpc::mid_t>(" + std::to_string(method_id) + ");\n";

    for (uint32_t i = 0; i < method.args.size(); i++) {
        cpp += "        serializer.serialize<" + method.args[i].type.as_template_args + ">(" + method.args[i].name + ");\n";
    }

    cpp += "\n        auto connection = rpc.get_connection_provider().get_connection(owner_addr);\n";
    cpp += "        if (connection.is_valid()) {\n";
    cpp += "            connection.send(serializer.get_data(), serializer.get_size());\n";
    cpp += "        }\n";

    if (method.type.as_type != "void") {
        cpp += "\n";
        impl_method_stub_ret(cpp, method);
    }

    cpp += "    }\n";
}

void impl_interface_stub(fl::str_t& cpp, const interface_t& interface) {
    cpp += "struct " + interface.base_name + "_stub_t : public fl::rpc::stub_t<" + interface.mangled_name + "> {\n";
    cpp += "    " + interface.base_name + "_stub_t(fl::rpc::addr_t owner_addr, fl::rpc::oid_t owner_oid, fl::rpc::i_rpc_t& rpc) :\n";
    cpp += "        fl::rpc::stub_t<" + interface.mangled_name + ">(owner_addr, owner_oid, rpc) {}\n";

    uint32_t method_id = 0;
    for (const method_t& method : interface.methods) {
        cpp += "\n";
        impl_method_stub(cpp, method, method_id++);
    }

    cpp += "};\n";
}

void impl_interface_stub_factory(fl::str_t& cpp, const interface_t& interface) {
    cpp += "inline fl::rpc::i_hollow_t* rpc__" + interface.base_name + "__stub_factory(fl::rpc::addr_t owner_addr, fl::rpc::oid_t owner_oid, fl::rpc::i_rpc_t& rpc) {\n";
    cpp += "    return new " + interface.base_name + "_stub_t(owner_addr, owner_oid, rpc);\n";
    cpp += "}\n";
}

void impl_interface_invokable_no_ret(fl::str_t& cpp, const interface_t& interface, const method_t& method) {
    cpp += "inline void rpc__" + interface.base_name + "__" + method.name + "(fl::rpc::deserializer_t& deserializer, " + interface.mangled_name + "* " + interface.base_name + "_object) {\n";
    cpp += "    " + interface.base_name + "_object->" + method.name + "(";

    for (uint32_t i = 0; i < method.args.size(); i++) {
        cpp += "deserializer.deserialize<" + method.args[i].type.as_template_args + ">()";

        if (i != method.args.size() - 1) {
            cpp += ", ";
        }
    }

    cpp += ");\n";
    cpp += "}\n\n";
}

void impl_interface_invokable_ret(fl::str_t& cpp, const interface_t& interface, const method_t& method) {
    cpp += "inline void rpc__" + interface.base_name + "__" + method.name + "(fl::rpc::deserializer_t& deserializer, " + interface.mangled_name + "* " + interface.base_name + "_object, fl::rpc::serializer_t& serializer) {\n";
    cpp += "    " + method.type.as_type + " ret = " + interface.base_name + "_object->" + method.name + "(";

    for (uint32_t i = 0; i < method.args.size(); i++) {
        cpp += "deserializer.deserialize<" + method.args[i].type.as_template_args + ">()";

        if (i != method.args.size() - 1) {
            cpp += ", ";
        }
    }

    cpp += ");\n\n";
    
    cpp += "    serializer.serialize<fl::rpc::oid_t>(0);\n";
    cpp += "    serializer.serialize<fl::rpc::mid_t>(1);\n";
    cpp += "    serializer.serialize<" + method.type.as_template_args + ">(ret);\n";
    cpp += "}\n\n";
}

void impl_interface_invokables(fl::str_t& cpp, const interface_t& interface) {
    for (const method_t& method : interface.methods) {
        if (method.type.as_type == "void") {
            impl_interface_invokable_no_ret(cpp, interface, method);
        } else {
            impl_interface_invokable_ret(cpp, interface, method);
        }
    }
}

void impl_method_invoker(fl::str_t& cpp, const interface_t& interface, const method_t& method, uint32_t method_id) {
    cpp += "        case " + std::to_string(method_id) + ": {\n";

    if (method.type.as_type == "void") {
        cpp += "            rpc__" + interface.base_name + "__" + method.name + "(deserializer, " + interface.base_name + "_object);\n";
    } else {
        cpp += "            rpc__" + interface.base_name + "__" + method.name + "(deserializer, " + interface.base_name + "_object, serializer);\n";
    }

    cpp += "            break;\n";
    cpp += "        }\n";
}

void impl_interface_invoker(fl::str_t& cpp, const interface_t& interface) {
    cpp += "inline void rpc__" + interface.base_name + "_invoker(fl::rpc::deserializer_t& deserializer, fl::rpc::i_hollow_t* object, fl::rpc::serializer_t& serializer) {\n";
    cpp += "    fl::rpc::mid_t method_id = deserializer.deserialize<fl::rpc::mid_t>();\n";
    cpp += "    " + interface.mangled_name + "* " + interface.base_name + "_object = static_cast<" + interface.mangled_name + "*>(object);\n\n";
    
    cpp += "    switch(method_id) {\n";

    uint32_t method_id = 0;
    for (const method_t& method : interface.methods) {
        impl_method_invoker(cpp, interface, method, method_id++);
    }

    cpp += "    }\n";
    cpp += "}\n";
}

void impl_interface_registrator(fl::str_t& reg, const interface_t& interface) {
    reg += "    rpc.get_invoker().register_interface(" + interface.mangled_name + "::iid, rpc__" + interface.base_name + "_invoker);\n";
    reg += "    rpc.get_stub_factory().register_interface(" + interface.mangled_name + "::iid, rpc__" + interface.base_name + "__stub_factory);\n";
}

void impl_interface(fl::str_t& cpp, const interface_t& interface) {
    impl_interface_base(cpp, interface); cpp += "\n";
    impl_interface_ref_typedef(cpp, interface); cpp += "\n";
    impl_interface_stub(cpp, interface); cpp += "\n";
    impl_interface_stub_factory(cpp, interface); cpp += "\n";
    impl_interface_invokables(cpp, interface);
    impl_interface_invoker(cpp, interface); cpp += "\n";
}

bool begin_namespace(fl::str_t& cpp, fl::strutil::tokens_iterator_t& tokens) {
    if (tokens.current() != "namespace") {
        return false;
    }

    tokens.advance();

    cpp += "namespace " + fl::str_t(tokens.current()) + " {\n\n";
    tokens.advance();
    
    return true;
}

void end_namespace(fl::str_t& cpp) {
    cpp += "}\n";
}

fl::str_t fl::rpc::generate(str_t& rpc_file) {
    strutil::tokens_iterator_t tokens = strutil::tokenize(rpc_file, { " ", "\n", "{", "}", "(", ")", "," });

    str_t cpp;
    str_t reg;

    add_pragma_once(cpp);
    add_include_essentials(cpp);
    
    bool has_namespace = begin_namespace(cpp, tokens);
    
    reg += "inline void register_rpc_interfaces(fl::rpc::i_rpc_t& rpc) {\n";

    while (is_interface(tokens)) {
        interface_t interface = get_interface(tokens);

        impl_interface(cpp, interface);
        impl_interface_registrator(reg, interface);
    }

    reg += "}\n\n";
    cpp += reg;

    if (has_namespace) {
        end_namespace(cpp);
    }

    return cpp;
}
