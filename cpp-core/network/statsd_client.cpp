#include "statsd_client.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #define closesocket close
    #define INVALID_SOCKET -1
#endif

namespace trading {

StatsDClient::StatsDClient(const Host& host, Port port) 
    : m_host(host), m_port(port), m_sock(-1) {
    
#ifdef _WIN32
    // Инициализация Winsock (безопасно вызывать многократно)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

StatsDClient::~StatsDClient() {
    if (m_sock != -1) {
        closesocket(m_sock);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void StatsDClient::timing(const Key& key, Value value) {
    send_message(key + ":" + std::to_string(value) + "|ms");
}

void StatsDClient::increment(const Key& key) {
    send_message(key + ":1|c");
}

void StatsDClient::send_message(const Message& message) {
    if (m_sock == -1) return;

    struct sockaddr_in dest;
    std::memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(m_port);
    inet_pton(AF_INET, m_host.c_str(), &dest.sin_addr);

    sendto(m_sock, message.c_str(), static_cast<int>(message.length()), 0, 
           (struct sockaddr*)&dest, sizeof(dest));
}

}