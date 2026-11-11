// http_server -- the viewer's HttpServer (app/webviewer/HttpServer.h) with a
// connection open that never sends, as a browser's spare connection does: other
// requests are still served, and stop() returns and closes it.

#include "app/webviewer/HttpServer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

int g_port = 0;

// Exits rather than throws: unwinding past a blocked stop() would hang.
void require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::fflush(stderr);
    std::_Exit(1);
}

int dial() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    timeval tv{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)g_port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    require(connect(fd, (sockaddr*)&addr, sizeof addr) == 0, "connect() failed");
    return fd;
}

// The reply to a request sent as `first`, then `rest` 200 ms later.
std::string get(const std::string& first, const std::string& rest = "") {
    int fd = dial();
    send(fd, first.data(), first.size(), 0);
    if (!rest.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        send(fd, rest.data(), rest.size(), 0);
    }
    std::string reply;
    char buf[4096];
    for (ssize_t n; (n = recv(fd, buf, sizeof buf, 0)) > 0;) reply.append(buf, (size_t)n);
    close(fd);
    return reply;
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

}  // namespace

int main() {
    HttpServer server;
    server.route("/ping", [](const HttpRequest& r) {
        return HttpResponse::text(200, "pong " + r.get("n"));
    });
    for (g_port = 47000; g_port < 47100; g_port++) {
        try {
            server.start("127.0.0.1", g_port);
            break;
        } catch (const std::exception&) {
        }
    }
    require(g_port < 47100, "no free port in 47000-47099");

    int idle = dial();
    require(has(get("GET /ping?n=1 HTTP/1.1\r\n\r\n"), "pong 1"),
            "a request behind an idle connection was not served");
    require(has(get("GET /ping?n=2 HTTP/1.1\r\n", "Host: x\r\n\r\n"), "pong 2"),
            "a request head split across two sends was not served");
    require(has(get("GET /missing HTTP/1.0\r\n\r\n"), " 404 "), "an unknown path was not a 404");

    auto stopped = std::async(std::launch::async, [&] { server.stop(); });
    require(stopped.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
            "stop() blocked on an idle connection");
    char c;
    require(recv(idle, &c, 1, 0) == 0, "stop() left the idle connection open");
    close(idle);

    std::printf("http_server_test: OK\n");
    return 0;
}
