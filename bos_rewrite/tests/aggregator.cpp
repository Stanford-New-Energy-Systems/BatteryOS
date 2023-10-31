#include "BOS.hpp"

int main() {
    using namespace std::chrono_literals;
    
    BOS bos;
    bos.startAggregator(65429, 65430);

    LOG() << "SHUTTING DOWN!" << std::endl;
}
