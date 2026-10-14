#include <iostream>
#include <string>
#include "storage_engine.hpp"
#include "wal.hpp"
#include "tcp_server.hpp"

using namespace std;
using namespace flashkv;

int main(int argc, char* argv[]) {
    int port = 6379;
    size_t capacity = 50000;
    string wal_path = "wal.log";

    if (argc >= 2) {
        port = stoi(argv[1]);
    }

    cout << "==========================================================" << endl;
    cout << "      ______ _           _     _  ___      __             " << endl;
    cout << "     |  ____| |         | |   | |/ / \\    / /             " << endl;
    cout << "     | |__  | | __ _ ___| |__ | ' / \\ \\  / /              " << endl;
    cout << "     |  __| | |/ _` / __| '_ \\|  <   \\ \\/ /               " << endl;
    cout << "     | |    | | (_| \\__ \\ | | | . \\   \\  /                " << endl;
    cout << "     |_|    |_|\\__,_|___/_| |_|_|\\_\\   \\/                 " << endl;
    cout << "==========================================================" << endl;
    cout << " [FlashKV] Starting In-Memory Engine & TCP Daemon...       " << endl;
    cout << " [FlashKV] Port:        " << port << endl;
    cout << " [FlashKV] Max Keys:    " << capacity << endl;
    cout << " [FlashKV] Persistence: " << wal_path << endl;

    StorageEngine engine(capacity);
    WriteAheadLog wal(wal_path);

    // Step 1: Automatic Crash Recovery
    cout << " [FlashKV] Checking for existing WAL records..." << endl;
    RecoveryStats stats = wal.recover(engine);
    if (stats.replayed_operations > 0) {
        cout << " [FlashKV] RESTORED " << stats.replayed_operations 
             << " operations from disk in " << stats.elapsed_ms << " ms!" << endl;
    } else {
        cout << " [FlashKV] No previous WAL records. Initialized fresh memory." << endl;
    }

    // Step 2: Start Network Listener
    TcpServer server(port, engine, wal, 4);
    if (!server.start()) {
        cerr << " [FlashKV] FATAL: Could not start TCP server on port " << port << endl;
        return 1;
    }

    cout << " [FlashKV] Server running! Connect via:" << endl;
    cout << "           telnet localhost " << port << "  OR  nc localhost " << port << endl;
    cout << " [FlashKV] Press ENTER to stop server gracefully..." << endl;

    cin.get();

    cout << " [FlashKV] Graceful shutdown initiated..." << endl;
    server.stop();
    cout << " [FlashKV] Stopped. Total keys preserved: " << engine.size() << endl;

    return 0;
}
