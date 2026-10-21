#include <memory>

#include "server.h"

int main() {
    auto server = std::make_unique<Server>(9000);

    if (!server->Init()) {
        return 1;
    }

    server->Start();
    return 0;
}