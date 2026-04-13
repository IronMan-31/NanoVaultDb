#pragma once
#include <string>
#include "SQL_LEXER.hpp"
#include "SQL_PARSER.hpp"
#include "UDPReceiver.hpp"
#include "hft.hpp"
#include "initialLoad.hpp"
#include "logging.hpp"
#include "strategyHandler.hpp"
#include "utils/cpu_affinity.hpp"
#include <iostream>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "IndicatorHandler.hpp"
#include "Btrees_testing.hpp"
#include "web_socks_og.hpp"

class NanoVaultDB{
    private:
        void setup() {
            std::thread packet_receiver(NetFeed::run_receiver);
            std::thread packet_parser(NetFeed::run_packet_parser);
            std::thread strategy_parser(NetFeed::run_strategy_parser);
            std::thread web_socks(init_web_sockets);
            pin_thread_to_cpu(packet_receiver, 0);
            pin_thread_to_cpu(packet_parser, 1);
            pin_thread_to_cpu(strategy_parser,2);
            pin_thread_to_cpu(web_socks,3);

            packet_receiver.detach();
            packet_parser.detach();
            strategy_parser.detach();
            web_socks.detach();
        }
        string readLineWithHistory(vector<string> &history, int &historyIndex) {
            termios oldt, newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);

            string input;
            vector<char> buffer;

            while (true) {
                int ch = getchar();
                if (ch == EOF || ch == 3) { 
                    cout << "\n";
                    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                    return "exit";
                }
                char c = ch;

                if (c == '\n') {
                cout << "\n";
                break;
                }

                if (c == 27) {
                char c1 = getchar();
                char c2 = getchar();

                if (c1 == '[') {
                    if (c2 == 'A') {
                    if (!history.empty() && historyIndex > 0) {
                        historyIndex--;

                        cout << "\33[2K\rnanoVaultDb> ";
                        input = history[historyIndex];
                        cout << input;
                    }
                    } else if (c2 == 'B') {
                    if (!history.empty() && historyIndex < (int)history.size() - 1) {
                        historyIndex++;
                        cout << "\33[2K\rnanoVaultDb> ";
                        input = history[historyIndex];
                        cout << input;
                    } else {
                        historyIndex = history.size();
                        cout << "\33[2K\rnanoVaultDb> ";
                        input.clear();
                    }
                    }
                }
                continue;
                }

                if (c == 127 || c == 8) {
                if (!input.empty()) {
                    input.pop_back();
                    cout << "\b \b";
                }
                continue;
                }

                input.push_back(c);
                cout << c;
            }

            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return input;
        }
    public:
        NanoVaultDB() = default;

        void init(){
            std::streambuf* og_buf=std::cout.rdbuf();
            std::cout.rdbuf(NULL);
            initialDatabseLoad();
            HFT::InitalStorage::initialIndicatorLoad();
            HFT::InitalStorage::initialStrategyLoad();
            runVacuum();
            initializePrimaryIndexBtrees("abcd",true);
            setup();
            IndicatorHandler::registerAllIndicators(IndicatorHandler::indicatorRegistry);
            StrategyHandler::registerAllStrategy(StrategyHandler::strategyRegistry);
            std::cout.rdbuf(og_buf);
        }
        std::string execute(const std::string &query){
            std::streambuf* og_buf=std::cout.rdbuf();
            try {
                std::cout.rdbuf(NULL);
                logging(query); 
                Lexer lexer(query);
                vector<Token *> tokens = lexer.tokenize();
                Parser parser(tokens);
                std::string output=parser.parse();
                clear_log();
                std::cout.rdbuf(og_buf);
                return output; 
            } catch (const std::exception &e) {
                std::cout.rdbuf(og_buf);
                cerr << "Error: " << e.what() << endl;
                return "Error\n";
            }
        }
        void enter_shell(){
            vector<string> history;
            vector<string> rem_sqls = exec_rem_sqls();
            for (string s : rem_sqls) {
                try {
                Lexer lexer(s);
                vector<Token *> tokens = lexer.tokenize();
                for (Token *token : tokens) {
                    cout << typeToString(token->TYPE) << " : " << token->VALUE << endl;
                }
                
                Parser parser(tokens);
                std::string output = parser.parse();
                std::cout << output << "\n";
                } catch (const std::exception &e) {
                    cerr << "Warning: Failed to execute recovered SQL -> " << e.what()<< endl;
                }
            }
            streambuf* og_buf;
            int historyIndex = 0;
            while (true) {
                og_buf=std::cout.rdbuf();
                cout << "nanoVaultDb> ";
                string sql = readLineWithHistory(history, historyIndex);

                if (sql == "exit" || sql == "quit")
                break;

                if (!sql.empty()) {
                history.push_back(sql);
                historyIndex = history.size();
                }

                while (sql.find(';') == string::npos) {
                cout << " ...> ";
                string more = readLineWithHistory(history, historyIndex);

                if (!more.empty()) {
                    history.push_back(more);
                    historyIndex = history.size();
                }

                sql += "\n" + more;
                }

                try {
                    cout.rdbuf(NULL);
                    logging(sql);
                    Lexer lexer(sql);
                    vector<Token *> tokens = lexer.tokenize();
                    for (Token *token : tokens) {
                        cout << typeToString(token->TYPE) << " : " << token->VALUE << endl;
                    }
                    Parser parser(tokens);
                    std::string output = parser.parse();
                    cout.rdbuf(og_buf);
                    std::cout << output << "\n";
                    clear_log();
                } catch (const std::exception &e) {
                    cout.rdbuf(og_buf);
                    cerr << "Error: " << e.what() << endl;
                }
            }
        }
};