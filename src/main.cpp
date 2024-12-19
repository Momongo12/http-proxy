#include "proxy_app.hpp"

int main(int argc, char** argv) {
    ProxyApp app;
    app.parseArgs(argc, argv);
    if (app.showHelp()) {
        app.printHelp();
        return 0;
    }
    app.init();
    app.run();
    app.shutdown();
    return 0;
}