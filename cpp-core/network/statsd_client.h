#pragma once
#include <string>
#include <cstdint>

namespace trading {

class StatsDClient {
public:
    using Host = std::string;
    using Port = uint16_t;
    using Key = std::string;
    using Value = uint64_t;

    StatsDClient(const Host& host, Port port);
    ~StatsDClient();

    // Отправка времени выполнения (в микросекундах/миллисекундах)
    void timing(const Key& key, Value value);
    // Инкремент счетчика
    void increment(const Key& key);

private:
    using Message = std::string;
    using SocketFd = int;

    void send_message(const Message& message);

    Host m_host;
    Port m_port;
    SocketFd m_sock; // socket handle
};

}