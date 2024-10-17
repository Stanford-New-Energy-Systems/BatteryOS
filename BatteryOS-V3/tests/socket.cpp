#include "BOS.hpp"

int main() {
    using namespace std::chrono_literals;
    
    BOS bos;
    bos.startSockets(65432, 65431);

    LOG() << "SHUTTING DOWN!" << std::endl;
}
