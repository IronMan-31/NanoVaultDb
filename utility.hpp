#ifndef MYUTILITY_UTILITY_HPP
#define MYUTILITY_UTILITY_HPP

#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>
#include <filesystem> // Include for std::filesystem
#include <fstream>
#include <sys/stat.h>
#include "json.hpp"   // Assuming this is a necessary include
#include "global.hpp" // Assuming this is a necessary include
#include <utility>
#include "databaseSchemaReader.hpp"
namespace MyUtility
{                                   // Define a namespace called MyUtility
    namespace fs = std::filesystem; // Shorthand for std::filesystem


    std::string extractBaseName(const std::string& filename) {
    fs::path p(filename);
    std::string stem = p.stem().string();  // first .stem() call removes ".db"
    while (fs::path(stem).extension() != "") {
        stem = fs::path(stem).stem().string();  // repeat to strip ".something"
    }
    return stem;  // returns "hello"
}

    void createFile(const std::string &filePath, const std::string &content)
    {
        fs::path parentDir = fs::path(filePath).parent_path();
        if (!parentDir.empty() && !fs::exists(parentDir))
        {
            std::error_code ec;
            if (fs::create_directories(parentDir, ec))
            {
                std::cout << "Created directory: " << parentDir << std::endl;
            }
            else
            {
                throw std::runtime_error("Error creating directory: " + ec.message());
            }
        }

        std::ofstream outfile(filePath); // Create and open the file
        if (outfile.is_open())
        {
            outfile << content; // Write the content to the file
            outfile.close();    // Close the file
        }
        else
        {
            throw std::runtime_error("Error: Could not create or open file '" + filePath + "' for writing");
        }
    }

    bool checkIfFileExist(const std::string &filePath)
    {
        if (fs::exists(filePath))
        {
            return true;
        }
        return false;
    }

    void changeCurrentDb(const std::string &newDbName)
    {

        std::ofstream outfile(currentDbPath);
        if (outfile.is_open())
        {
            std::string content = "{\"current_db\":\"" + newDbName + "\"}";
            outfile << content;
            outfile.close();
        }
        else
        {
            throw std::runtime_error("Error: Could not open file '" + currentDbPath);
        }
    }

    std::pair<bool, std::string> checkIfTableExist(const std::string &table)
    {
        std::stringstream s;
        s << "./db/";
        s << currentDatabase;
        s << ".shivam.db";
        PythonLikeJSONParser parser;

        if (checkIfFileExist(s.str()))
        {
            std::cout << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
            parser.loadFromFile(s.str());
            JSONArrayWrapper columns = parser[0][std::string("tables")].asArray();
            for(size_t i = 0;i<columns.size();i++){
                std::string tableName = columns[i][std::string("name")];
                if( tableName== table){
                    return std::make_pair(true,"");
                }
            }
            
        }
        std::stringstream err;
        err << "the table name "<<table <<" not exist";
        return std::make_pair(false, err.str());
    }

} // namespace MyUtility



namespace PagerHandler
{

    struct RowIndex
    {
        int64_t row_start, row_end;
    };

    std::mutex tableIndexMutex;
    std::mutex tableDataMutex;
    int64_t getFileSize(const std::string &filename)
    {
        struct stat st;
        if (stat(filename.c_str(), &st) != 0)
            return 0;
        return st.st_size;
    }

    void insertRow(std::string primaryName, std::vector<std::pair<std::string, std::pair<std::string, bool>>> data, std::string tableName)
    {
        if (data.empty())
        {
            throw std::runtime_error("Cannot insert empty row data");
        }

        std::stringstream indexFileName;
        indexFileName << tableDirectory << "/" << currentDatabase << "/" << tableName << ".index";

        std::stringstream dataFileName;
        dataFileName << tableDirectory << "/" << currentDatabase << "/" << tableName << ".data";

        if (!MyUtility::checkIfFileExist(indexFileName.str()))
        {
            throw std::runtime_error("Table " + tableName + " does not exist. Create it first.");
        }

        if (!MyUtility::checkIfFileExist(dataFileName.str()))
        {
            throw std::runtime_error("Data file for table " + tableName + " does not exist.");
        }

        int64_t currentIndexFileSize = getFileSize(indexFileName.str());
        int64_t colsize = data.size();

        std::lock_guard<std::mutex> indexLock(tableIndexMutex);
        std::lock_guard<std::mutex> dataLock(tableDataMutex);

        std::fstream indexFile(indexFileName.str(), std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
        std::fstream dataFile(dataFileName.str(), std::ios::in | std::ios::out | std::ios::binary | std::ios::app);

        if (!indexFile.is_open())
        {
            throw std::runtime_error("Failed to open index file: " + indexFileName.str());
        }
        if (!dataFile.is_open())
        {
            throw std::runtime_error("Failed to open data file: " + dataFileName.str());
        }

        int64_t newRowId;

        if (currentIndexFileSize == 0)
        {
            newRowId = 1;
            std::cout << "Inserting first row (ID: " << newRowId << ")\n";
        }
        else
        {
            int64_t rowEntrySize = sizeof(int64_t) + colsize * sizeof(RowIndex);
            int64_t lastRowIdPos = currentIndexFileSize - rowEntrySize;

            indexFile.seekg(lastRowIdPos);
            int64_t lastRowId;
            indexFile.read(reinterpret_cast<char *>(&lastRowId), sizeof(int64_t));

            if (indexFile.fail())
            {
                throw std::runtime_error("Failed to read last row ID from index file");
            }

            newRowId = lastRowId + 1;
            std::cout << "Inserting new row (ID: " << newRowId << ")\n";
        }

        indexFile.seekp(0, std::ios::end);
        dataFile.seekp(0, std::ios::end);

        indexFile.write(reinterpret_cast<const char *>(&newRowId), sizeof(int64_t));
        if (indexFile.fail())
        {
            throw std::runtime_error("Failed to write row ID to index file");
        }

        for (size_t i = 0; i < data.size(); i++)
        {
            const std::string &columnData = data[i].second.first;

            int64_t start = dataFile.tellp();

            dataFile.write(columnData.c_str(), columnData.size());
            if (dataFile.fail())
            {
                throw std::runtime_error("Failed to write column data to data file");
            }

            int64_t end = dataFile.tellp();

            RowIndex entry{start, end};
            indexFile.write(reinterpret_cast<const char *>(&entry), sizeof(RowIndex));
            if (indexFile.fail())
            {
                throw std::runtime_error("Failed to write index entry");
            }

            std::cout << "Column " << i << ": '" << columnData
                      << "' stored at [" << start << "-" << end << "]\n";
        }

        dataFile.flush();
        indexFile.flush();

        std::cout << "Successfully inserted row with ID: " << newRowId << "\n";
    }

};

// for this does not cahnge the function name just make it thread safe
#endif