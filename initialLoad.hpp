#ifndef __INITIAL_LOAD
#define __INITIAL_LOAD

#include <unordered_map>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <climits>
#include "global.hpp"
#include "utility.hpp"
namespace fs = std::filesystem;

std::string getCurrentDatabase(std::string &metaFile)
{
    std::ifstream file(metaFile);
    if (!file.is_open())
        throw std::runtime_error("Unable to open meta file");

    std::string line;
    std::getline(file, line);  

    size_t keyPos = line.find("\"current_db\"");
    if (keyPos == std::string::npos)
        throw std::runtime_error("Key 'current_db' not found");

    size_t colonPos = line.find(":", keyPos);
    if (colonPos == std::string::npos)
        throw std::runtime_error("Invalid format in meta file");

    size_t firstQuote = line.find("\"", colonPos);
    if (firstQuote == std::string::npos)
        throw std::runtime_error("Invalid format in meta file");

    size_t secondQuote = line.find("\"", firstQuote + 1);
    if (secondQuote == std::string::npos)
        throw std::runtime_error("Invalid format in meta file");

    return line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}


bool checkDBexist(const std::string &name)
{
    std::stringstream file;
    file << dbDirectoryPath;
    file << name;
    file << ".shivam.db";
    return MyUtility::checkIfFileExist(file.str());
}

void initialDatabseLoad()
{
        std::string currentDbMeta = "./db/current_db.meta";
        std::string dbName= getCurrentDatabase(currentDbMeta);
        currentDatabase = dbName;
    for (const auto &entry : fs::directory_iterator(dbDirectoryPath))
    {
        if (fs::is_regular_file(entry.status()))
        {
            std::string filename = entry.path().filename().string();

            if (filename.find(".db") != std::string::npos)
            {
                std::string dbname = MyUtility::extractBaseName(filename);
                std::cout << "dbname " << dbname << "\n";
                std::shared_ptr<PythonLikeJSONParser> parser = std::make_shared<PythonLikeJSONParser>();

                // Store parser in global cache
                globalJsonCache[dbname] = parser;

                std::string fullPath = dbDirectoryPath + "/" + filename;
                std::cout << "FULL PATH " << fullPath << "\n";
                if (!parser->loadFromFile(fullPath))
                {
                    std::cerr << "Failed to load file: " << fullPath << std::endl;
                    std::stringstream err;
                    err << "Failed to load file: " << fullPath;
                    throw std::runtime_error(err.str());
                }

                try
                {
                    JSONArrayWrapper tablesArray = (*parser)[0][std::string("tables")].asArray();
                    for (int64_t i = 0; i < tablesArray.size(); ++i)
                    {
                        std::string tableName = tablesArray[i][std::string("name")].getString();
                        JSONArrayWrapper columnsArray = tablesArray[i][std::string("columns")].asArray();

                        std::vector<std::shared_ptr<TableGlobalColumnNode>> columnNodes;

                        for (int64_t j = 0; j < columnsArray.size(); ++j)
                        {
                            std::shared_ptr<TableGlobalColumnNode> node = std::make_shared<TableGlobalColumnNode>();

                            std::string columnDataName = columnsArray[j][std::string("name")].getString();
                            std::string columnDataType = columnsArray[j][std::string("type")].getString();
                            JSONArrayWrapper constraintArray = columnsArray[j][std::string("constraints")].asArray();

                            int length = INT_MAX;
                            bool isUnique = false;
                            bool isPrimary = false;
                            bool autoIncrement = false;
                            bool createIndex = false;

                            for (int64_t k = 0; k < constraintArray.size(); ++k)
                            {
                                std::string constraint = constraintArray[k].getString();
                                if (constraint == "primary_key")
                                    isPrimary = true;
                                if (constraint == "auto_increment")
                                    autoIncrement = true;
                                if (constraint == "unique")
                                    isUnique = true;
                                if (constraint == "create_index")
                                    createIndex = true;
                            }

                            if (columnsArray[j].hasKey(std::string("length")))
                            {
                                length = columnsArray[j][std::string("length")];
                            }

                            node->constraint = constraintArray.toStringVector();
                            node->length = length;
                            node->name = columnDataName;
                            node->type = columnDataType;
                            node->autoIncrement = autoIncrement;
                            node->isUnique = isUnique;
                            node->createIndex = createIndex;
                            node->isPrimary = isPrimary;

                            columnNodes.push_back(node);

                            if (isPrimary || isUnique)
                            {

                                TreeVariant tree = std::make_shared<BPlusTree<int64_t, IndexNode>>();
                                dbBtrees[currentDatabase][tableName][columnDataName] = std::make_pair(tree,0);
                            }
                        }

                        // Save table columns in globalTableCache
                        globalTableCache[dbname][tableName] = std::move(columnNodes);

                        std::cout << "Loaded table: " << tableName << " from DB: " << dbname << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error accessing 'tables' in " << fullPath << ": " << e.what() << std::endl;
                }
            }
        }
    }
}

void loadAllNodesOfBtreeForPrimaryKey(TreeVariant &tree, int64_t size, std::string columnName)
{
    std::stringstream indexFileName;
    indexFileName << tableDirectory << "/" << currentDatabase <<"/" << columnName << ".index";
    std::cout<<"INDEX FILE NAME "<<indexFileName.str()<<"\n";
    if (MyUtility::checkIfFileExist(indexFileName.str()))
    {
        

        std::fstream indexFile(indexFileName.str(), std::ios::in | std::ios::out | std::ios::binary);
        if (indexFile.fail())
        {
            throw std::runtime_error("Failed to read primary key  from index file");
        }

        int64_t indexFileSize = PagerHandler::getFileSize(indexFileName.str());
        std::cout<<"index file size is "<<indexFileSize<<"\n";
        int64_t divider = 8 * (2 * size - 1);
        int64_t getTotalNoOfRows = (int64_t)(indexFileSize / divider);
        std::cout<<"Total No Of rows "<<getTotalNoOfRows<<"\n";
        while (getTotalNoOfRows--)
        {
            int64_t id, start, end;
            std::streampos pos = indexFile.tellg();

            if (pos != std::streampos(-1))
            {
                int64_t offset = static_cast<int64_t>(pos);
                start = offset;
            }
            else
            {
                break;
            }

            indexFile.read(reinterpret_cast<char *>(&id), sizeof(int64_t));

            // indexFile.seekg(sizeof(int64_t),std::ios::cur);
            
            // indexFile.read(reinterpret_cast<char *>(&start), sizeof(int64_t));

            indexFile.seekg((2 * (size - 1)) * sizeof(int64_t),std::ios::cur);

            std::streampos endpos = indexFile.tellg();

            if (endpos != std::streampos(-1))
            {
                int64_t offset = static_cast<int64_t>(endpos);
                end = offset;
            }
            else
            {
                break;
            }

            IndexNode node{start, end};
            std::cout<<"id "<<id<<" start "<<start<<"end "<<end<<"\n";
            std::visit([id, node](auto &treePtr)
                       {
            using TreeType = std::decay_t<decltype(*treePtr)>;
            if constexpr (std::is_same_v<TreeType, BPlusTree<int64_t, IndexNode>>) {
                treePtr->insert(id, node);
            } else if constexpr (std::is_same_v<TreeType, BPlusTree<std::string, IndexNode>>) {
                treePtr->insert(std::to_string(id), node);
            } }, tree);
        }
    }else{
        throw std::runtime_error("the table does not exist");
    }
}



void initializePrimaryIndexBtrees()
{
    for (const auto &dbPair : globalTableCache)
    {
        const std::string &dbName = dbPair.first;
        const auto &tables = dbPair.second;

        for (const auto &tablePair : tables)
        {
            const std::string &tableName = tablePair.first;
            const auto &columns = tablePair.second;
            int64_t noOfColumns = columns.size();

            for (const auto &columnPtr : columns)
            {
                std::cout<<columnPtr->name <<" "<<columnPtr->isPrimary<<"\n";
                if (columnPtr->isPrimary)
                {
                    const std::string &columnName = columnPtr->name;
                    const std::string &type = columnPtr->type;

                    TreeVariant tree;

                    if (type == "int")
                    {
                        tree = std::make_shared<BPlusTree<int64_t, IndexNode>>();
                    }
                    else if (type == "string" || type == "varchar" || type == "text")
                    {
                        tree = std::make_shared<BPlusTree<std::string, IndexNode>>();
                    }
                    else
                    {
                        std::cerr << "Unsupported primary key type: " << type
                                  << " for column: " << columnName << std::endl;
                        continue;
                    }

                    dbBtrees[dbName][tableName][columnName] = std::make_pair(std::move(tree),noOfColumns);
                    loadAllNodesOfBtreeForPrimaryKey(dbBtrees[dbName][tableName][columnName].first,noOfColumns,tableName);

                    std::cout << "Initialized B+ Tree for " << dbName
                              << "." << tableName << "." << columnName << " " << "and size is " << noOfColumns <<std::endl;
                }
            }
        }
    }
}

#endif // __INITIAL_LOAD
