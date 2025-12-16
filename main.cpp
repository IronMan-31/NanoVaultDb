#include <iostream>
#include <string>
#include <vector>

#include "SQL_LEXER.hpp"
#include "SQL_PARSER.hpp"
#include "initialLoad.hpp"

using namespace std;
std::string typeToString(TokenType TYPE);

int main(int argc, char const *argv[])
{
    initialDatabseLoad();
    initializePrimaryIndexBtrees();



    std::vector<std::string> testSQLs = {

// R"(
// CREATE DATABASE  school;
// )",

// R"(
// CREATE TABLE students (
//     id INT PRIMARY KEY,
//     rollno VARCHAR(255) UNIQUE,
//     name VARCHAR(100),

// );
// )"

// R"(
// INSERT INTO students (rollno)
// VALUES ("23");
// )",

// R"(
// DROP TABLE students;
// )"

};


// for (int i = 1; i <= 10; i++)
// {
//     std::string insertSQL =
//         "INSERT INTO students (rollno, name) VALUES (\"" +
//         std::to_string(i) + "\", \"Student" +
//         std::to_string(i) + "\");";

//     testSQLs.push_back(insertSQL);
// }


    for (const auto &sql : testSQLs)
    {
        cout << "\n=============================\n";
        cout << "SQL:\n"
             << sql << endl;
        cout << "=============================\n";

        try
        {
            Lexer lexer(sql);
            vector<Token *> tokens = lexer.tokenize();

            // Debug: Print tokens
            cout << "Tokens:\n";
            for (Token *token : tokens)
            {
                cout << typeToString(token->TYPE) << " : " << token->VALUE << endl;
            }

            Parser parser(tokens);
            parser.parse(); // Parse the SQL
        }
        catch (const std::exception &e)
        {
            cerr << "Error: " << e.what() << endl;
        }
        cout << "\n";
    }

    return 0;
}
