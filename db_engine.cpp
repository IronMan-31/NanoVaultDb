// db_engine.cpp

#include <string>
#include <cstring>
#include <filesystem>

#include "SQL_LEXER.hpp"
#include "SQL_PARSER.hpp"
#include "initialLoad.hpp"

static std::string last_error;
static std::string base_directory;

extern "C" {

// --------------------------------------------------
// Initialize database with explicit base directory
// --------------------------------------------------
void initialize_database(const char* base_path) {
    try {
        base_directory = base_path;

        // Set working directory explicitly
        std::filesystem::current_path(base_directory);

        initialDatabseLoad();
    } catch (const std::exception& e) {
        last_error = e.what();
    }
}

// --------------------------------------------------
// Execute SQL
//   - returns heap-allocated C string on success
//   - returns nullptr on error
// --------------------------------------------------
const char* execute_sql(const char* sql) {
    try {
        Lexer lexer(sql);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);

        std::string output = parser.parse();

        char* result = new char[output.size() + 1];
        std::strcpy(result, output.c_str());
        return result;
    } catch (const std::exception& e) {
        last_error = e.what();
        return nullptr;
    }
}

// --------------------------------------------------
// Free string allocated by execute_sql
// --------------------------------------------------
void free_string(const char* s) {
    delete[] s;
}

// --------------------------------------------------
// Get last error message
// --------------------------------------------------
const char* get_last_error() {
    return last_error.c_str();
}

} // extern "C"
