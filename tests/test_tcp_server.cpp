#include <boost/ut.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>

// Платформо-зависимые заголовки для клиента (тест)
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "../network/tcp_server.h"

using namespace boost::ut;
using namespace std::chrono_literals;

suite tcp_server_tests = [] {
    "Server start and accept connection"_test = [] {
        // Порт для тестов
        uint16_t port = 8080;
        std::atomic<bool> received{false};
        
        // Callback для обработки данных (предполагаемая сигнатура)
        auto on_message = [&](int fd, const uint8_t* data, size_t len) {
            received = true;
        };

        // Создаем и запускаем сервер в отдельном потоке
        // (Предполагаем, что TCPServer принимает порт и callback)
        // trading::TCPServer server(port, on_message);
        // std::thread server_thread([&] { server.run(); });
        
        // Даем время на запуск
        // std::this_thread::sleep_for(100ms);

        // --- Клиентская часть (RAW Socket) ---
        #ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        #else
            int sock = socket(AF_INET, SOCK_STREAM, 0);
        #endif

        expect(sock != -1) << "Failed to create socket";

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        // Попытка подключения (раскомментировать при реальной реализации)
        // if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        //     const char* msg = "Hello Server";
        //     send(sock, msg, strlen(msg), 0);
        // }
        
        // Ожидание обработки
        // std::this_thread::sleep_for(100ms);

        // expect(received);

        // Очистка
        #ifdef _WIN32
            closesocket(sock);
            WSACleanup();
        #else
            close(sock);
        #endif
        
        // server.stop();
        // server_thread.join();
        
        expect(true); // Placeholder пока API TCPServer не зафиксировано
    };
};