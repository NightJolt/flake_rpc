#include <flake/rpc/types.h>

fl::rpc::addr_t::addr_t() : ip(0), port(0) {}
fl::rpc::addr_t::addr_t(ip_t ip, port_t port) : ip(ip), port(port) {}
fl::rpc::addr_t::addr_t(const char* ip, const char* port) : ip(sf::IpAddress(ip).toInteger()), port(atoi(port)) {}
