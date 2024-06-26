#include "rpc/connection.h"

fun::rpc::connection_info_t::connection_info_t() {}
fun::rpc::connection_info_t::connection_info_t(ip_t ip, port_t rp, port_t lp) : ip(ip), remote_port(rp), local_port(lp) {}
fun::rpc::connection_info_t::connection_info_t(sf::TcpSocket* socket) {
    ip = socket->getRemoteAddress().toInteger();
    remote_port = socket->getRemotePort();
    local_port = socket->getLocalPort();
}

fun::rpc::connection_t::connection_t() : socket(nullptr) {}
fun::rpc::connection_t::connection_t(sf::TcpSocket* socket) : socket(socket), info(connection_info_t(socket)) {}
fun::rpc::connection_t::~connection_t() {
    if (socket) {
        socket->disconnect();
    }
}

fun::rpc::connection_t::connection_t(connection_t&& other) noexcept : info(other.info), socket(std::exchange(other.socket, nullptr)) {}
fun::rpc::connection_t& fun::rpc::connection_t::operator=(connection_t&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    info = other.info;
    socket = std::exchange(other.socket, nullptr);

    return *this;
}

fun::rpc::connection_stub_t::connection_stub_t(sf::TcpSocket* socket) : socket(socket) {}

void fun::rpc::connection_stub_t::send(uint8_t* data, uint32_t size) {
    size_t sent = 0;
    while (socket->send(data + sent, size - sent, sent) == sf::Socket::Partial) {}
}

bool fun::rpc::connection_stub_t::is_valid() {
    return socket != nullptr && socket->getRemotePort() != 0;
}

void fun::rpc::connection_provider_t::init(port_t port) {
    assert(connection_listener.listen(port) == sf::Socket::Done);

    public_addr.ip = sf::IpAddress::getPublicAddress().toInteger();
    public_addr.port = connection_listener.getLocalPort();

    local_addr.ip = sf::IpAddress::getLocalAddress().toInteger();
    local_addr.port = connection_listener.getLocalPort();

    connection_listener.setBlocking(false);

    new_connection = std::make_unique<sf::TcpSocket>();
}

void fun::rpc::connection_provider_t::quit() {
    for (auto& [addr, connection] : connections) {
        connection.socket->disconnect();
    }

    connection_listener.close();
}

fun::rpc::addr_t fun::rpc::connection_provider_t::get_pub_addr() {
    return public_addr;
}

fun::rpc::addr_t fun::rpc::connection_provider_t::get_loc_addr() {
    return local_addr;
}

fun::rpc::connection_stub_t fun::rpc::connection_provider_t::get_connection(addr_t addr) {
    if (addr.ip == public_addr.ip) {
        addr.ip = local_addr.ip;
    }

    if (!check_connection(addr)) {
        connection_t connection { new sf::TcpSocket() };

        bool is_connected = connection.socket->connect(sf::IpAddress(addr.ip), addr.port) == sf::Socket::Done;
        
        if (!is_connected) {
            return connection_stub_t { nullptr };
        }

        connection.socket->setBlocking(false);
        connections[addr] = std::move(connection);
    }

    return connection_stub_t { connections[addr].socket.get() };
}

bool fun::rpc::connection_provider_t::check_connection(addr_t addr) {
    auto it = connections.find(addr);

    return it != connections.end() && it->second.socket->getRemotePort() != 0;
}

void fun::rpc::connection_provider_t::check_for_incoming_connections() {
    while (true) {
        auto status = connection_listener.accept(*new_connection);

        if (status == sf::Socket::Done) {
            addr_t addr { new_connection->getRemoteAddress().toInteger(), (port_t)new_connection->getRemotePort() };

            new_connection->setBlocking(false);
            connections[addr] = connection_t { new_connection.release() };

            new_connection = std::make_unique<sf::TcpSocket>();
        } else {
            break;
        }
    }
}

void fun::rpc::connection_provider_t::check_for_incoming_data() {
    uint8_t data[max_packet_size];

    for (auto& [addr, connection] : connections) {
        sf::TcpSocket* socket = connection.socket.get();
        size_t received = 0;
        bool partial = true;

        while (partial) {
            auto result = socket->receive(data + received, sizeof(data) - received, received);

            switch(result) {
                case sf::Socket::Done:
                    partial = false;

                    break;
                    
                case sf::Socket::Disconnected:
                    // todo: remove connection

                    partial = false;

                    break;

                case sf::Socket::NotReady:
                    partial = false;

                    break;

                case sf::Socket::Partial:
                    break;

                case sf::Socket::Error:
                    assert(false);

                    break;
            }
        }

        if (received > 0) {
            packet_storage.push(data, received, addr);
        }
    }
}

fun::rpc::packet_storage_t& fun::rpc::connection_provider_t::get_packet_storage() {
    return packet_storage;
}
