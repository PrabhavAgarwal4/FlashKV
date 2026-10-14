#include <iostream>
#include <string>
#include <cassert>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <filesystem>
#include "storage_engine.hpp"
#include "wal.hpp"
#include "tcp_server.hpp"

using namespace std;
using namespace flashkv;

const int TEST_PORT = 19876;
const string TEST_SERVER_WAL = "test_server_wal.log";

void cleanup_wal() {
    if (filesystem::exists(TEST_SERVER_WAL)) {
        filesystem::remove(TEST_SERVER_WAL);
    }
}

string send_and_receive(SOCKET sock, const string& msg) {
    send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);

    char buf[512];
    int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
    if (bytes <= 0) return "";
    buf[bytes] = '\0';
    return string(buf);
}

void test_tcp_server_protocol() {
    cleanup_wal();
    cout << "[NETWORK TEST 1] Starting TCP Server on port " << TEST_PORT << "..." << endl;

    {
        StorageEngine engine(1000);
        WriteAheadLog wal(TEST_SERVER_WAL);

        TcpServer server(TEST_PORT, engine, wal, 2);
        bool started = server.start();
        assert(started && server.is_running());

        // Give server thread a few milliseconds to bind & listen
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Connect a client socket
        cout << "[NETWORK TEST 2] Connecting client socket over TCP..." << endl;
        SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        assert(client != INVALID_SOCKET);

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TEST_PORT);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        int conn = connect(client, (sockaddr*)&server_addr, sizeof(server_addr));
        assert(conn == 0);
        cout << "  -> Connected to server!" << endl;

        // Test 1: PING
        cout << "  -> Testing PING..." << endl;
        string resp = send_and_receive(client, "PING\n");
        assert(resp == "+PONG\r\n");

        // Test 2: SET
        cout << "  -> Testing SET..." << endl;
        resp = send_and_receive(client, "SET greeting Hello Distributed World\n");
        assert(resp == "+OK\r\n");

        // Test 3: GET
        cout << "  -> Testing GET..." << endl;
        resp = send_and_receive(client, "GET greeting\n");
        assert(resp == "$Hello Distributed World\r\n");

        // Test 4: GET (Non-existent)
        resp = send_and_receive(client, "GET missing_key\n");
        assert(resp == "(nil)\r\n");

        // Test 5: STATS
        cout << "  -> Testing STATS..." << endl;
        resp = send_and_receive(client, "STATS\n");
        assert(resp.find("+STATS") != string::npos);

        // Test 6: DEL
        cout << "  -> Testing DEL..." << endl;
        resp = send_and_receive(client, "DEL greeting\n");
        assert(resp == ":1\r\n");

        // Test 7: QUIT
        cout << "  -> Testing QUIT..." << endl;
        resp = send_and_receive(client, "QUIT\n");
        assert(resp == "+BYE\r\n");

        closesocket(client);

        cout << "[NETWORK TEST 3] Shutting down TCP Server..." << endl;
        server.stop();
        assert(!server.is_running());
        cout << "  -> Server stopped cleanly!" << endl;
    }

    cleanup_wal();
}

int main() {
    cout << "===============================================" << endl;
    cout << "    FLASHKV MILESTONE 4: TCP NETWORKING TESTS  " << endl;
    cout << "===============================================" << endl;

    test_tcp_server_protocol();

    cout << "===============================================" << endl;
    cout << "   ALL TCP SOCKET & PROTOCOL TESTS PASSED!     " << endl;
    cout << "===============================================" << endl;

    return 0;
}
