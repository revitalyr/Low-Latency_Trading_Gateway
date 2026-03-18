#include <boost/ut.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "../order_gateway/gateway_server.h"

using namespace boost::ut;
using namespace trading;
using namespace std::chrono_literals;

// Вспомогательная структура сообщения (должна совпадать с протоколом сервера)
struct __attribute__((packed)) TestNewOrderMsg {
    uint8_t type;       // 1 = NewOrder
    uint64_t order_id;
    uint64_t price;
    uint32_t quantity;
    char symbol[8];
    char side;          // 'B' or 'S'
};

suite gateway_tests = [] {
    "Full Flow: Connect and Send Order"_test = [] {
        uint16_t port = 9090;
        
        // 1. Запуск GatewayServer
        GatewayServer gateway(port);
        std::thread gateway_thread([&] {
            gateway.start(); 
        });

        // Даем серверу время на инициализацию
        std::this_thread::sleep_for(200ms);
        expect(gateway.is_running()) << "Gateway failed to start";

        // 2. Настройка клиента
        #ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        #else
            int sock = socket(AF_INET, SOCK_STREAM, 0);
        #endif

        expect(sock != -1);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        // 3. Подключение
        int conn_res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        expect(conn_res == 0) << "Failed to connect to Gateway";

        if (conn_res == 0) {
            // 4. Формирование сообщения
            TestNewOrderMsg msg{};
            msg.type = 1; 
            msg.order_id = 1001;
            msg.price = 50000;
            msg.quantity = 10;
            std::strncpy(msg.symbol, "BTCUSD", 6);
            msg.side = 'B';

            // 5. Отправка
            send(sock, (const char*)&msg, sizeof(msg), 0);

            // 6. Ожидание ответа (Acknowledgement)
            char buffer[256];
            // int bytes = recv(sock, buffer, sizeof(buffer), 0);
            // expect(bytes > 0);
            // Здесь можно разобрать ответ и проверить, что пришел OrderAck
        }

        // 7. Завершение
        #ifdef _WIN32
            closesocket(sock);
            WSACleanup();
        #else
            close(sock);
        #endif

        gateway.stop();
        gateway_thread.join();
    };
};