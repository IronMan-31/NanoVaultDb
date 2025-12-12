#ifndef __GENERATOR

#define __GENERATOR

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

#include <filesystem> // Include for std::filesystem
#include <fstream>
#include "json.hpp"
#include "utility.hpp"
#include "global.hpp"
namespace fs = std::filesystem;


namespace CommandRunner
{

    void generateDropStatement(const std::unique_ptr<DropStatement> &stmt) {
    using namespace std;

    bool is_table = stmt->istable;
    std::string name = stmt->name;

    if (is_table) {

        if (currentDatabase.empty()) {
            throw std::runtime_error("No database selected");
        }

        auto it_db = globalTableCache.find(currentDatabase);
        if (it_db == globalTableCache.end()) {
            throw std::runtime_error("Database not found: " + currentDatabase);
        }

        auto it_table = it_db->second.find(name);
        if (it_table == it_db->second.end()) {
            throw std::runtime_error("Table '" + name + "' does not exist");
        }

        std::string base = tableDirectory + "/" + currentDatabase + "/";
        std::string dataFile  = base + name + ".data";
        std::string indexFile = base + name + ".index";

        if (fs::exists(dataFile))  fs::remove(dataFile);
        if (fs::exists(indexFile)) fs::remove(indexFile);

        globalTableCache[currentDatabase].erase(name);

        std::string filePath = "./db/" + currentDatabase + ".shivam.db";
        JSONParser parser(filePath);

        if (!parser.loadFromFile())
            throw std::runtime_error("Failed to load DB file: " + filePath);

        JSONParser::JSONValue root = parser.getObject(0);
        auto &dbObj = std::get<JSONParser::JSONObject>(root.value);

        if (dbObj.find("tables") != dbObj.end()) {
            auto &tables = std::get<JSONParser::JSONArray>(dbObj["tables"].value);

            for (size_t i = 0; i < tables.size(); i++) {
                const auto &tblVal = tables[i];
                const auto &tblObj = std::get<JSONParser::JSONObject>(tblVal.value);

                std::string tblName = std::get<std::string>(tblObj.at("name").value);

                if (tblName == name) {
                    tables.erase(tables.begin() + i);
                    break;
                }
            }
        }

        parser.clear();
        parser.appendValue(JSONParser::JSONValue(dbObj));
        if (!parser.saveToFile()) {
            throw std::runtime_error("Failed to save updated DB metadata");
        }

        std::cout << "Table '" << name << "' dropped successfully.\n";
    }
    else {
        std::string metaFile = "./db/" + name + ".shivam.db";

        if (!fs::exists(metaFile)) {
            throw std::runtime_error("Database '" + name + "' does not exist.");
        }

        std::string dbDir = tableDirectory + "/" + name;

        if (fs::exists(dbDir)) {
            fs::remove_all(dbDir);
        }

        fs::remove(metaFile);

        if (globalTableCache.find(name) != globalTableCache.end()) {
            globalTableCache.erase(name);
        }

        if (currentDatabase == name) {
            currentDatabase.clear();
        }

        std::cout << "Database '" << name << "' dropped successfully.\n";
    }
}


    void generateInsertTableStatement(const std::unique_ptr<InsertStatement> &stmt)
    {

        std::string tableName = stmt->tableName;
        // column , <value isUnique>
        std::vector<std::pair<std::string, std::pair<std::string,bool>>> column;
        std::vector<std::pair<std::string,bool>>actualColumn ;
        std::string primaryColName = "";
        for (int i = 0; i < stmt->columns.size(); i++)
        {

            column.push_back(make_pair(stmt->columns[i], make_pair(stmt->values[i],false)));
        }

        std::sort(column.begin(), column.end(), [](const auto &a, const auto &b)
                  { return a.first < b.first; });

        auto it_db = globalTableCache.find(currentDatabase);
        if (it_db != globalTableCache.end())
        {
            auto it_table = it_db->second.find(tableName);
            if (it_table != it_db->second.end())
            {
                std::vector<std::shared_ptr<TableGlobalColumnNode>> &columns = it_table->second;

                // Use `columns` here
                for ( auto &col : columns)
                {
                    if(!col->isPrimary){

                        actualColumn.push_back(make_pair(col->name,col->isUnique));
                    }else{
                        primaryColName = col->name;
                    }
                    // Example: assuming TableGlobalColumnNode has a `name` field

                    //std::cout << col->name << '\n';
                }

                sort(actualColumn.begin(),actualColumn.end(),[](const auto &a, const auto &b)
                  { return a.first < b.first; });
                if(actualColumn.size()!=column.size()){
                    throw std::runtime_error("the given column size does not match with the actual column size");
                }
                for(int i = 0;i<actualColumn.size();i++){

                    if(actualColumn[i].first!=column[i].first){
                        std::stringstream s;
                        s<<"error "<<" get column Name "<<column[i].first<<" instead of  "<<actualColumn[i].first<<"\n";
                        throw std::runtime_error(s.str());
                    }
                    else{
                        if(actualColumn[i].second){
                            column[i].second.second = true;
                        }
                    }


                }

                PagerHandler::insertRow(std::move(primaryColName),std::move(column),std::move(tableName));
            }
            else
            {
                std::stringstream s ;
                s << "Table not found: " << tableName << '\n';
                throw std::runtime_error(s.str());
            }
        }
        else
        {
            std::stringstream s;
            s << "Database not found: " << currentDatabase << '\n';
            throw std::runtime_error(s.str());
        }
    }
    void generateCreateTableStatement(const std::unique_ptr<CreateStatement> &stmt)
    {
        // Step 1: Convert column definitions to JSON
        JSONParser::JSONArray columnArray;
        std::vector<std::shared_ptr<TableGlobalColumnNode>> newTableCache;

        for (const auto &col : stmt->columns)
        {
            std::shared_ptr<TableGlobalColumnNode> node = std::make_shared<TableGlobalColumnNode>();
            node->name = col.name;
            node->type = col.type;

            int length = INT_MAX;
            bool isUnique = false;
            bool isPrimary = false;
            bool autoIncrement = false;
            bool createIndex = false;
            std::vector<std::string> ActualTableConstraint;
            
            JSONParser::JSONObject colJson = {
                {"name", JSONParser::JSONValue(col.name)},
                {"type", JSONParser::JSONValue(col.type)}};

            // Extract length if it's a VARCHAR with size, e.g., varchar(255)
            std::cout<<"getting col type\n";
            std::cout<<col.type<<"\n";
            if (col.type.find("varchar(") != std::string::npos)
            {
                size_t start = col.type.find("(") + 1;
                size_t end = col.type.find(")");
                if (start != std::string::npos && end != std::string::npos && end > start)
                {
                    std::string lengthStr = col.type.substr(start, end - start);
                    try
                    {
                        int length = std::stoi(lengthStr);
                        node->length = length;
                        colJson["length"] = JSONParser::JSONValue(length);
                        colJson["type"] = JSONParser::JSONValue("varchar"); // strip size from type
                    }
                    catch (...)
                    {
                        throw std::runtime_error("Invalid VARCHAR length");
                    }
                }
            }

            // Convert constraints
            JSONParser::JSONArray constraintArray;
            for (const auto &c : col.constraints)
            {
                switch (c)
                {
                case ColumnConstraint::NOT_NULL:
                    constraintArray.push_back(JSONParser::JSONValue("not_null"));
                    ActualTableConstraint.push_back("not_null");
                    break;
                case ColumnConstraint::PRIMARY_KEY:
                    constraintArray.push_back(JSONParser::JSONValue("primary_key"));
                    isPrimary = true;
                    ActualTableConstraint.push_back("primary_key");
                    break;
                case ColumnConstraint::UNIQUE:
                    constraintArray.push_back(JSONParser::JSONValue("unique"));
                    isUnique = true;
                    ActualTableConstraint.push_back("unique");
                    break;
                case ColumnConstraint::AUTO_INCREMENT:
                    autoIncrement = true;
                    ActualTableConstraint.push_back("auto_increment");
                    constraintArray.push_back(JSONParser::JSONValue("auto_increment"));
                    
                    break;
                default:
                    break;
                }
            }

            colJson["constraints"] = JSONParser::JSONValue(constraintArray);
            columnArray.push_back(JSONParser::JSONValue(colJson));
            node->autoIncrement = autoIncrement;
            node->isUnique = isUnique;
            node->createIndex = createIndex;
            node->isPrimary = isPrimary;
            newTableCache.push_back(node);

        }

        // Step 0: Check if table already exists in globalTableCache
        if (globalTableCache[currentDatabase].find(stmt->name) != globalTableCache[currentDatabase].end())
        {
            throw std::runtime_error(" Table '" + stmt->name + "' already exists in DB '" + currentDatabase + "'");
        }

        // Step 2: Create table JSON
        JSONParser::JSONObject tableJson = {
            {"name", JSONParser::JSONValue(stmt->name)},
            {"columns", JSONParser::JSONValue(columnArray)}};

        // Step 3: Load existing JSON file
        std::string filePath = "./db/" + currentDatabase + ".shivam.db";
        JSONParser parser(filePath);

        if (!parser.loadFromFile())
        {
            throw std::runtime_error(" Failed to load DB file: " + filePath);
        }

        JSONParser::JSONValue root = parser.getObject(0);
        if (!std::holds_alternative<JSONParser::JSONObject>(root.value))
        {
            throw std::runtime_error("Root of DB JSON must be an object");
        }

        auto &dbObj = std::get<JSONParser::JSONObject>(root.value);

        // Step 4: Add table to the JSON
        if (dbObj.find("tables") != dbObj.end() &&
            std::holds_alternative<JSONParser::JSONArray>(dbObj["tables"].value))
        {

            auto &tables = std::get<JSONParser::JSONArray>(dbObj["tables"].value);
            tables.push_back(JSONParser::JSONValue(tableJson));
        }
        else
        {
            dbObj["tables"] = JSONParser::JSONValue(JSONParser::JSONArray{
                JSONParser::JSONValue(tableJson)});
        }

        // Step 5: Save back to file
        parser.clear();
        parser.appendValue(JSONParser::JSONValue(dbObj));

        if (!parser.saveToFile())
        {
            throw std::runtime_error(" Failed to save DB JSON file");
        }

        // Optional: Update in-memory cache too
        
        globalTableCache[currentDatabase][stmt->name] = newTableCache; // You can populate columns later
                                            
        std::cout << " Table '" << stmt->name << "' added to DB '" << currentDatabase << "' successfully.\n";
        std::string tablename = stmt->name;
        std::stringstream indexFile, dataFile;

        indexFile << tableDirectory << "/" << currentDatabase << "/" << tablename << ".index";
        dataFile << tableDirectory << "/" << currentDatabase << "/" << tablename << ".data";

        MyUtility::createFile(indexFile.str(), "");
        MyUtility::createFile(dataFile.str(), "");
    }

    void generateInsertStatement()
    {
    }

};
#endif