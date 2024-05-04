#pragma once

#include <flake/std/types/function.h>
#include <flake/rpc/serialize.h>

namespace fl::rpc {
    void wait_for_sync_call_reply(fn_t<void(deserializer_t&)>);
}