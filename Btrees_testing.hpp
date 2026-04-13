#pragma once
#include"global.hpp"
#include<iostream>

void test_b_trees(){
    for (const auto &dbPair : globalTableCache)
    {
        const std::string &dbName = dbPair.first;
        const auto &tables = dbPair.second;

        for (const auto &tablePair : tables)
        {
            if (tablePair.second.first!="-1") continue;
            const auto &columns= tablePair.second.second;
            std::cout<<"Traversing Btree of "<<tablePair.first<<"\n";
            for (const auto &col:columns){
                if (col->isPrimary || col->isUnique){
                    auto tree=dbBtrees[dbName][tablePair.first][col->name].first;
                    std::visit([](auto &treePtr) {
                        if (!treePtr) return;
                        treePtr->print();
                    }, tree);
                }
            }
        }
    }
}