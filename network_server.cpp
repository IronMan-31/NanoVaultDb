#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <mutex>
#include <atomic>
#include <csignal>

#include "SQL_LEXER.hpp"
#include "SQL_PARSER.hpp"
#include "generator.hpp"
#include "logging.hpp"
#include "global.hpp"

std::mutex dbMutex;
std::atomic<bool> serverRunning(true);

void handleSignal(int) {
    serverRunning = false;
}

std::string handleSQL(const std::string &sql) {
    try {
        std::lock_guard<std::mutex> lock(dbMutex);

        Lexer lexer(sql);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        parser.parse();

        return "OK\n";
    }
    catch (const std::exception &e) {
        return std::string("ERROR: ") + e.what() + "\n";
    }
}

bool sendAll(int fd, const std::string &data) {
    size_t totalSent = 0;

    while (totalSent < data.size()) {
        ssize_t sent = send(
            fd,
            data.data() + totalSent,
            data.size() - totalSent,
            0
        );

        if (sent <= 0) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}


void handleClient(int client_fd) {
    std::cout << "[Client connected]\n";

    std::string clientBuffer;

    while (true) {
        char buffer[4096] = {0};
        int bytesRead = read(client_fd, buffer, sizeof(buffer));

        if (bytesRead <= 0) {
            std::cout << "[Client disconnected]\n";
            break;
        }

        clientBuffer.append(buffer, bytesRead);

        size_t pos;
        while ((pos = clientBuffer.find(';')) != std::string::npos) {

            std::string sql = clientBuffer.substr(0, pos + 1);
            clientBuffer.erase(0, pos + 1);

            std::string trimmed = sql;
            trimmed.erase(
                std::remove_if(trimmed.begin(), trimmed.end(), [](unsigned char c) { return std::isspace(c); }
),
                trimmed.end()
            );

            if (trimmed == "exit;" || trimmed == "quit;") {
                std::cout << "[Client closed session]\n";
                close(client_fd);
                return;
            }

            std::string response = handleSQL(sql);
            if (!sendAll(client_fd, response)) {
                std::cout << "[Send failed, closing client]\n";
                close(client_fd);
                return;
            }

        }
    }

    close(client_fd);
}


int main() {
    signal(SIGINT, handleSignal);
    initialDatabseLoad();
    try
    {
       initializePrimaryIndexBtrees();
    }
    catch(const std::exception& e)
    {
        
    }
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6969);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "NanoDB server running on port 6969...\n";

    while (serverRunning) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (!serverRunning) break;
            perror("accept");
            continue;
        }

        std::thread clientThread(handleClient, client_fd);
        clientThread.detach();
    }

    std::cout << "Shutting down server...\n";
    close(server_fd);
    return 0;
}

