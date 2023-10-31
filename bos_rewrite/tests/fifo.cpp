#include "BOS.hpp"

int main() {
    using namespace std::chrono_literals;

    std::unique_ptr<BOS> bos = std::make_unique<BOS>("batteries", 0755);

    bos->startFifos(0777);

    LOG() << "SHUTTING DOWN!" << std::endl;
}
