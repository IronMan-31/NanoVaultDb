#ifndef __SELECT_AST_PARSER
#define __SELECT_AST_PARSER
#include <variant>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include "global.hpp"
#include <vector>
namespace AstParser
{

    Value getValueFromRow(const Expression *expr, const Row &row)
    {
        if (!expr)
            throw std::runtime_error("Null expression");

        switch (expr->getType())
        {
        case ASTNodeType::IDENTIFIER:
        {
            const auto *id = static_cast<const Identifier *>(expr);
            auto it = row.columns.find(id->name);
            if (it == row.columns.end())
                throw std::runtime_error("Unknown column: " + id->name);
            return it->second;
        }
        case ASTNodeType::INT_LITERAL:
        {
            const auto *lit = static_cast<const IntLiteral *>(expr);
            return int64_t(lit->value);
        }
        case ASTNodeType::STRING_LITERAL:
        {
            const auto *lit = static_cast<const StringLiteral *>(expr);
            return lit->value;
        }
        case ASTNodeType::BOOLEAN_LITERAL:
        {
            const auto *lit = static_cast<const BoolLiteral *>(expr);
            return lit->value;
        }
        case ASTNodeType::PARENTHESIZED_EXPRESSION:
        {
            const auto *paren = static_cast<const ParenthesizedExpression *>(expr);
            return getValueFromRow(paren->expression.get(), row);
        }
        default:
            throw std::runtime_error("Invalid expression in getValueFromRow");
        }
    }

    bool compareValues(const Value &lhs, const Value &rhs, ComparisonOperator op)
    {
        if (lhs.index() != rhs.index())
            throw std::runtime_error("Type mismatch in comparison");

        if (std::holds_alternative<int64_t>(lhs))
        {
            int64_t l = std::get<int64_t>(lhs);
            int64_t r = std::get<int64_t>(rhs);
            switch (op)
            {
            case ComparisonOperator::EQUAL:
                return l == r;
            case ComparisonOperator::NOT_EQUAL:
                return l != r;
            case ComparisonOperator::GREATER:
                return l > r;
            case ComparisonOperator::LESS:
                return l < r;
            case ComparisonOperator::GREATER_EQUAL:
                return l >= r;
            case ComparisonOperator::LESS_EQUAL:
                return l <= r;
            }
        }
        else if (std::holds_alternative<std::string>(lhs))
        {
            const auto &l = std::get<std::string>(lhs);
            const auto &r = std::get<std::string>(rhs);
            switch (op)
            {
            case ComparisonOperator::EQUAL:
                return l == r;
            case ComparisonOperator::NOT_EQUAL:
                return l != r;
            case ComparisonOperator::GREATER:
                return l > r;
            case ComparisonOperator::LESS:
                return l < r;
            case ComparisonOperator::GREATER_EQUAL:
                return l >= r;
            case ComparisonOperator::LESS_EQUAL:
                return l <= r;
            }
        }
        else if (std::holds_alternative<bool>(lhs))
        {
            bool l = std::get<bool>(lhs);
            bool r = std::get<bool>(rhs);
            switch (op)
            {
            case ComparisonOperator::EQUAL:
                return l == r;
            case ComparisonOperator::NOT_EQUAL:
                return l != r;
            default:
                throw std::runtime_error("Invalid operator for bool");
            }
        }

        throw std::runtime_error("Unsupported comparison type");
    }

    bool evaluateExpression(const Expression *expr, const Row &row)
    {
        if (!expr)
            return false;

        switch (expr->getType())
        {
        case ASTNodeType::LOGICAL_EXPRESSION:
        {
            const auto *log = static_cast<const LogicalExpression *>(expr);
            bool left = evaluateExpression(log->left.get(), row);
            bool right = evaluateExpression(log->right.get(), row);
            return (log->op == LogicalOperator::AND) ? (left && right) : (left || right);
        }
        case ASTNodeType::COMPARISON_EXPRESSION:
        {
            const auto *comp = static_cast<const ComparisonExpression *>(expr);
            Value lhs = getValueFromRow(comp->left.get(), row);
            Value rhs = getValueFromRow(comp->right.get(), row);
            return compareValues(lhs, rhs, comp->op);
        }
        case ASTNodeType::PARENTHESIZED_EXPRESSION:
        {
            const auto *paren = static_cast<const ParenthesizedExpression *>(expr);
            return evaluateExpression(paren->expression.get(), row);
        }
        case ASTNodeType::IDENTIFIER:
        case ASTNodeType::INT_LITERAL:
        case ASTNodeType::STRING_LITERAL:
        case ASTNodeType::BOOLEAN_LITERAL:
            return std::get<bool>(getValueFromRow(expr, row));
        default:
            throw std::runtime_error("Unsupported expression type in evaluateExpression");
        }
    }

    bool evaluateWhere(const WhereClause *whereClause, const Row &row)
    {
        if (!whereClause || !whereClause->condition)
            return true;

        return evaluateExpression(whereClause->condition.get(), row);
    }

} // namespace AstParser

namespace SelectQueryHandler
{

    std::string valueToJson(const Value &v)
    {
        return std::visit([](auto &&arg) -> std::string
                          {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + arg + "\"";          // Strings get quotes
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";    // Booleans are true/false
        } else {
            return std::to_string(arg);       // int64_t as number
        } }, v);
    }

    std::string handle(const std::unique_ptr<SelectStatement> &stmt)
    {
        const std::string tableName = stmt->table;
        std::stringstream indexfilename;
        std::stringstream datafilename;

        std::vector<std::string> columns;
        std::vector<TableGlobalColumnNode> allColumns;
        std::vector<std::string> allColumnName;
        {
            auto &colPtrs = globalTableCache[currentDatabase][tableName];
            // allColumns.reserve(colPtrs.size());
            for (const auto &p : colPtrs)
            {
                if (!p)
                    throw std::runtime_error("Null column metadata in globalTableCache");
                // allColumns.push_back(*p);
                allColumnName.push_back(p->name);
            }
        }
        // sort(allColumns.begin(), allColumns.end());
        sort(allColumnName.begin(), allColumnName.end());
        if (stmt->columns.size() > 0 && stmt->columns[0] == "*")
        {
            columns.insert(columns.end(), allColumnName.begin(), allColumnName.end());
        }
        else
        {

            columns.insert(columns.end(), stmt->columns.begin(), stmt->columns.end());
        }

        sort(columns.begin(), columns.end());

        indexfilename << tableDirectory << "/" << currentDatabase << "/" << tableName << ".index";
        datafilename << tableDirectory << "/" << currentDatabase << "/" << tableName << ".data";

        std::fstream indexFile(indexfilename.str(), std::ios::in | std::ios::out | std::ios::binary);
        std::fstream dataFile(datafilename.str(), std::ios::in | std::ios::out | std::ios::binary);
        if (indexFile.fail())
        {
            throw std::runtime_error("Failed to read primary key  from index file");
        }

        if (dataFile.fail())
        {
            throw std::runtime_error("Failed to read data   from dataFile file");
        }

        int64_t indexFileSize = PagerHandler::getFileSize(indexfilename.str());
        int64_t dataFileSize = PagerHandler::getFileSize(datafilename.str());
        int64_t size = allColumnName.size();
        int64_t divider = 8 * (2 * size - 1);
        int64_t getTotalNoOfRows = (int64_t)(indexFileSize / divider);

        std::stringstream json;
        json << "[";

        bool firstRow = true; // Track first row to handle commas

        while (getTotalNoOfRows--)
        {
            Row row;
            int64_t id;
            indexFile.read(reinterpret_cast<char *>(&id), sizeof(int64_t));
            row.columns[std::string(allColumnName[0])] = id;

            for (int i = 1; i < allColumnName.size(); i++)
            {
                int64_t start, end;
                indexFile.read(reinterpret_cast<char *>(&start), sizeof(int64_t));
                indexFile.read(reinterpret_cast<char *>(&end), sizeof(int64_t));

                uint64_t strSize = end - start ;
                std::string value(strSize, '\0');

                dataFile.seekg(start, std::ios::beg);
                dataFile.read(value.data(), strSize);
                std::cout<<allColumnName[i] << " value  "<<value<<"\n";
                row.columns[allColumnName[i]] = value;
            }

            if (AstParser::evaluateWhere(stmt->whereClause.get(), row))
            {
                if (!firstRow)
                    json << ','; // Only add comma if not first
                firstRow = false;

                json << '{';
                bool firstCol = true;
                for (const auto &[key, val] : row.columns)
                {
                    if (!firstCol)
                        json << ',';
                    firstCol = false;
                    json << '"' << key << "\":" << valueToJson(val);
                }
                json << '}';
            }
        }

        json << "]";

        return std::move(json.str());
    }
}

#endif