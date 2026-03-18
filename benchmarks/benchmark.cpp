#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #define CLOSE_SOCKET close
#endif

struct BenchmarkStats {
    std::atomic<uint64_t> total_messages{0};
    std::atomic<uint64_t> total_latency_ns{0};
    std::atomic<uint64_t> min_latency{UINT64_MAX};
    std::atomic<uint64_t> max_latency{0};
    std::atomic<uint64_t> errors{0};
};

class BenchmarkClient {
private:
    int socket_fd_;
    BenchmarkStats* stats_;
    std::string symbol_;
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<> price_dist_;
    std::uniform_int_distribution<> qty_dist_;
    
public:
    BenchmarkClient(BenchmarkStats* stats, const std::string& symbol = "AAPL")
        : stats_(stats), symbol_(symbol), gen_(rd_()), 
          price_dist_(10000, 20000), qty_dist_(1, 1000) {
        
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }
    }
    
    ~BenchmarkClient() {
        if (socket_fd_ >= 0) {
            CLOSE_SOCKET(socket_fd_);
        }
    }
    
    bool connect(const std::string& host, uint16_t port) {
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
            return false;
        }
        
        if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr), 
                     sizeof(server_addr)) < 0) {
            return false;
        }
        
        return true;
    }
    
    void run_benchmark(size_t num_messages) {
        for (size_t i = 0; i < num_messages; ++i) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            if (send_order()) {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time - start_time).count();
                
                stats_->total_messages.fetch_add(1);
                stats_->total_latency_ns.fetch_add(latency);
                
                // Update min/max latency
                uint64_t current_min = stats_->min_latency.load();
                while (latency < current_min && 
                       !stats_->min_latency.compare_exchange_weak(current_min, latency)) {
                    // Retry if value changed
                }
                
                uint64_t current_max = stats_->max_latency.load();
                while (latency > current_max && 
                       !stats_->max_latency.compare_exchange_weak(current_max, latency)) {
                    // Retry if value changed
                }
            } else {
                stats_->errors.fetch_add(1);
            }
        }
    }
    
private:
    bool send_order() {
        // Simplified order message (in real implementation, use proper binary protocol)
        std::string order = "NEW_ORDER " + symbol_ + " " + 
                           (gen_() % 2 == 0 ? "BUY" : "SELL") + " " +
                           std::to_string(qty_dist_(gen_)) + " " +
                           std::to_string(price_dist_(gen_));
        
        ssize_t sent = send(socket_fd_, order.c_str(), order.length(), 0);
        return sent == static_cast<ssize_t>(order.length());
    }
};

void print_stats(const BenchmarkStats& stats, double duration_seconds) {
    uint64_t total_msgs = stats.total_messages.load();
    uint64_t total_latency = stats.total_latency_ns.load();
    uint64_t errors = stats.errors.load();
    uint64_t min_lat = stats.min_latency.load();
    uint64_t max_lat = stats.max_latency.load();
    
    double throughput = total_msgs / duration_seconds;
    double avg_latency = total_msgs > 0 ? static_cast<double>(total_latency) / total_msgs : 0.0;
    
    std::cout << "\n=== Benchmark Results ===" << std::endl;
    std::cout << "Duration: " << duration_seconds << " seconds" << std::endl;
    std::cout << "Messages sent: " << total_msgs << std::endl;
    std::cout << "Errors: " << errors << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(0) 
              << throughput << " msg/sec" << std::endl;
    std::cout << "Average latency: " << std::fixed << std::setprecision(2) 
              << (avg_latency / 1000.0) << " μs" << std::endl;
    std::cout << "Min latency: " << (min_lat / 1000.0) << " μs" << std::endl;
    std::cout << "Max latency: " << (max_lat / 1000.0) << " μs" << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
              << (total_msgs > 0 ? (100.0 * (total_msgs - errors) / total_msgs) : 0.0) 
              << "%" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <num_clients> [messages_per_client]" << std::endl;
        return 1;
    }
    
    std::string host = argv[1];
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[2]));
    size_t num_clients = static_cast<size_t>(std::atoi(argv[3]));
    size_t messages_per_client = argc > 4 ? static_cast<size_t>(std::atoi(argv[4])) : 1000;
    
    std::cout << "Starting benchmark: " << num_clients << " clients, " 
              << messages_per_client << " messages each" << std::endl;
    std::cout << "Target: " << host << ":" << port << std::endl;
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
#endif
    
    BenchmarkStats stats;
    std::vector<std::thread> clients;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Launch client threads
    for (size_t i = 0; i < num_clients; ++i) {
        clients.emplace_back([&stats, &host, port, messages_per_client, i]() {
            try {
                BenchmarkClient client(&stats, i % 2 == 0 ? "AAPL" : "GOOGL");
                
                if (client.connect(host, port)) {
                    client.run_benchmark(messages_per_client);
                } else {
                    std::cerr << "Client " << i << " failed to connect" << std::endl;
                    stats.errors.fetch_add(messages_per_client);
                }
            } catch (const std::exception& e) {
                std::cerr << "Client " << i << " error: " << e.what() << std::endl;
                stats.errors.fetch_add(messages_per_client);
            }
        });
    }
    
    // Wait for all clients to complete
    for (auto& thread : clients) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    print_stats(stats, duration / 1000.0);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    return 0;
}
