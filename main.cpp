// g++ -c db_engine.cpp -o db_engine.o
// ar rcs libdb_engine.a db_engine.o


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
    runVacuum();
    try
    {
        initializePrimaryIndexBtrees();
    }
    catch(const std::exception& e)
    {}

    std::cout<<"#### \n\n\n finished b+ \n\n\n\n ###";


    std::vector<std::string> testSQLs = {
// R"(
//     CREATE DATABASE test;
// )",
// R"(
// CREATE TABLE StudentRolls (
//     id INT PRIMARY KEY AUTO_INCREMENT,
//     roll_no INT NOT NULL
// );
// )",
// R"(
// INSERT INTO StudentRolls (roll_no)
// VALUES (2);
// )",
// R"(
// INSERT INTO StudentRolls (roll_no)
// VALUES (5);
// )",
// R"(
// DELETE FROM studentrolls WHERE roll_no < "4";
// )",
// R"(
//     CREATE DATABASE test1;
// )",
// R"(
// CREATE TABLE testing (
//     id INT PRIMARY KEY AUTO_INCREMENT,
//     roll_no INT NOT NULL
// );
// )",
// R"(
// INSERT INTO testing (roll_no)
// VALUES (15);
// )",
// R"(
// INSERT INTO testing (roll_no)
// VALUES (20);
// )",
// R"(
// DELETE FROM testing WHERE roll_no < "16";
// )",
// R"(
// SELECT * FROM StudentRolls;
// )",
// R"(
// SELECT * FROM testing;
// )",
// R"(
// DROP TABLE StudentRolls;
// )",
// R"(
// DROP DATABASE test;
// )",
// R"(
// USE school;
// )",
// R"(
// MEMORY KEY=a VALUES=123 TTL=5;
// )",
// R"(
// MEMORY GET KEY=a;
// )",
};


// for (int i = 1; i <= 10; i++)
// {
//     std::string insertSQL =
//         "INSERT INTO testing (rollno, name, age) VALUES (" +
//         std::to_string(i) + ", \"Student" +
//         std::to_string(i) + "\", " +
//         std::to_string(18 + i) + ");";

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