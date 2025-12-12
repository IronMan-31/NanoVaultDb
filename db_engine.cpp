// db_engine.cpp


/*

(base) shivam@shivam-Nitro-ANV15-51:~/Desktop/learning/advanceCpp/distributed_Database$ ar rcs libdb_engine.a db_engine.o^C
(base) shivam@shivam-Nitro-ANV15-51:~/Desktop/learning/advanceCpp/distributed_Database$ g++ -c db_engine.cpp -o db_engine.o
ar rcs libdb_engine.a db_engine.o

commands to genrate
*/
#include <iostream>
#include <string>
#include <vector>

#include "SQL_LEXER.hpp"
#include "SQL_PARSER.hpp"
#include "initialLoad.hpp"

static std::string last_error;

extern "C" {

void initialize_database() {
    try {
        initialDatabseLoad();
    } catch (const std::exception& e) {
        last_error = e.what();
    }
}

int execute_sql(const char* sql) {
    try {
        Lexer lexer(sql);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        parser.parse();
        return 0;
    } catch (const std::exception& e) {
        last_error = e.what();
        return 1;
    }
}

const char* get_last_error() {
    return last_error.c_str();
}

}
