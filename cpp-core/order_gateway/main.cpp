#include "gateway_server.h"
#include <iostream>
#include <signal.h>

using namespace trading;

GatewayServer* gateway = nullptr;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (gateway) {
        gateway->stop();
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    try {
        std::cout << "Starting Trading Gateway on port " << port << std::endl;
        
        GatewayServer server(port);
        gateway = &server;
        
        if (!server.start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
        std::cout << "Server running. Press Ctrl+C to stop." << std::endl;
        
        // Wait for shutdown
        while (server.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Server stopped gracefully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
