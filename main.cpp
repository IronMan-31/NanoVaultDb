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

    std::cout<<"#### \n\n\n finished b+ \n\n\n\n ###";


    std::vector<std::string> testSQLs = {

// R"(
// CREATE DATABASE  school;
// )",

// R"(
// SELECT id, rollno, name
// FROM students
// WHERE rollno = 'CS101';

// )"


R"(
use testing;

)"

// ,
// R"(
// SELECT rollno, name, age FROM testing
// WHERE name = "Student3";

// )"
// ,
// R"(
// SELECT * FROM testing
// WHERE age > 1;

// )"

// R"(
// INSERT INTO students (rollno)
// VALUES ("23");
// )",


// R"(
// CREATE DATABASE  school;
// )",

// R"(
// DROP TABLE students;
// )"

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
