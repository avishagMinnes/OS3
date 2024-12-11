#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <mutex>
#include "graph.hpp"
#include "proactor.hpp"

Graph* g = nullptr;
std::mutex graphMutex;

void* handleClient(int sockfd) {
    int fd = sockfd;
    char buffer[1024];
    while (true) {
        int nbytes = read(fd, buffer, sizeof(buffer) - 1);
        if (nbytes <= 0) {
            if (nbytes == 0) {
                std::cout << "Connection closed by client" << std::endl;
            } else {
                perror("recv");
            }
            close(fd);
            return nullptr;
        }
        buffer[nbytes] = '\0';
        std::string command(buffer);

        if (command.find("Newgraph") == 0) {
            int n, m;
            sscanf(command.c_str(), "Newgraph %d,%d", &n, &m);
            std::lock_guard<std::mutex> lock(graphMutex);
            delete g;
            g = new Graph(n, m);
            std::string response = "New graph created\n";
            send(fd, response.c_str(), response.size(), 0);

            for (int i = 0; i < m; ++i) {
                nbytes = read(fd, buffer, sizeof(buffer) - 1);
                if (nbytes <= 0) {
                    if (nbytes == 0) {
                        std::cout << "Connection closed by client" << std::endl;
                    } else {
                        perror("recv");
                    }
                    close(fd);
                    return nullptr;
                }
                buffer[nbytes] = '\0';
                int u, v;
                sscanf(buffer, "%d %d", &u, &v);
                g->addEdge(u, v);
            }
        } else if (command.find("Kosaraju") == 0) {
            std::lock_guard<std::mutex> lock(graphMutex);
            if (g != nullptr) {
                auto sccs = g->kosaraju();
                std::string response = "Strongly connected components:\n";
                for (const auto& component : sccs) {
                    for (int vertex : component) {
                        response += std::to_string(vertex) + " ";
                    }
                    response += "\n";
                }
                send(fd, response.c_str(), response.size(), 0);
            }
        } else if (command.find("Newedge") == 0) {
            int u, v;
            sscanf(command.c_str(), "Newedge %d,%d", &u, &v);
            std::lock_guard<std::mutex> lock(graphMutex);
            if (g != nullptr) {
                g->addEdge(u, v);
                std::string response = "Edge added\n";
                send(fd, response.c_str(), response.size(), 0);
            }
        } else if (command.find("Removeedge") == 0) {
            int u, v;
            sscanf(command.c_str(), "Removeedge %d,%d", &u, &v);
            std::lock_guard<std::mutex> lock(graphMutex);
            if (g != nullptr) {
                g->removeEdge(u, v);
                std::string response = "Edge removed\n";
                send(fd, response.c_str(), response.size(), 0);
            }
        } else {
            std::string response = "Unknown command\n";
            send(fd, response.c_str(), response.size(), 0);
        }
    }
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(9034);

    if (bind(listener, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
        perror("bind");
        close(listener);
        return 1;
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        close(listener);
        return 1;
    }

    std::cout << "Server is running on port 9034" << std::endl;

    pthread_t proactorThread = startProactor(listener, handleClient);
    if (proactorThread == 0) {
        std::cerr << "Failed to start proactor" << std::endl;
        close(listener);
        return 1;
    }

    // Wait for proactor to run
    pthread_join(proactorThread, nullptr);

    close(listener);
    delete g;
    return 0;
}



