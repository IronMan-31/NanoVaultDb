#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#include "SQL_LEXER.hpp"
#include "SQL_PARSER.hpp"
#include "generator.hpp"
#include "logging.hpp"
#include "global.hpp"

std::string handleSQL(const std::string &sql) {
    try {
        logging(sql);
        Lexer lexer(sql);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        parser.parse();
        clear_log();
        return "OK\n";
    }
    catch (const std::exception &e) {
        return std::string("ERROR: ") + e.what() + "\n";
    }
}

void handleClient(int client_fd) {
    std::cout << "[Client connected]\n";

    while (true) {
        char buffer[4096] = {0};
        int bytesRead = read(client_fd, buffer, sizeof(buffer));

        if (bytesRead <= 0) {
            std::cout << "[Client disconnected]\n";
            break;
        }

        std::string sql(buffer, bytesRead);

        std::string trimmed = sql;
        trimmed.erase(
            std::remove_if(trimmed.begin(), trimmed.end(), ::isspace),
            trimmed.end()
        );

        if (trimmed == "exit" || trimmed == "quit") {
            std::cout << "[Client closed session]\n";
            break;
        }

        std::string response = handleSQL(sql);

        send(client_fd, response.c_str(), response.size(), 0);
    }

    close(client_fd);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6969);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);

    std::cout << "NanoDB server running on port 6969...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);

        std::thread clientThread(handleClient, client_fd);
        clientThread.detach();
    }

    close(server_fd);
    return 0;
}
