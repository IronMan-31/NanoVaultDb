// g++ -c db_engine.cpp -o db_engine.o
// ar rcs libdb_engine.a db_engine.o


#include <iostream>
#include <string>
#include <vector>
#include "UDPReceiver.hpp"
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
//     CREATE DATABASE school;
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
// UPDATE StudentRolls SET roll_no=12 WHERE roll_no<5;
// )",
// R"(
// DELETE FROM StudentRolls WHERE roll_no=12;
// )",
R"(
CREATE HFT TABLE eth_ticks (
    timestamp  DOUBLE PRECISION 0,          
    price      DOUBLE PRECISION 10,           
    volume     DOUBLE PRECISION 2,            
    side       DOUBLE PRECISION 0              
) SYMBOL 2 TOP;
)",
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
            // parser.parse(); // Par/se the SQL
        }
        catch (const std::exception &e)
        {
            cerr << "Error: " << e.what() << endl;
        }
        cout << "\n";
    }

        NetFeed::run_receiver();
    return 0;
}

