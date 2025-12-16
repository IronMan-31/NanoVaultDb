#include <fstream>
#include <iostream>
#include <cstdint>

int main() {
    std::ifstream in("/home/shivam/Desktop/learning/advanceCpp/distributed_Database/db/tables/school/students.index", std::ios::binary);
    std::ofstream out("output.txt");

    if (!in || !out) {
        std::cerr << "File open error\n";
        return 1;
    }

    uint64_t value;
    int count = 0;

    while (in.read(reinterpret_cast<char*>(&value), sizeof(value))) {
        out << value;
        count++;

        if (count % 5 == 0)
            out << '\n';   
        else
            out << ' ';   
    }

    out << '\n';

    in.close();
    out.close();
    return 0;
}
