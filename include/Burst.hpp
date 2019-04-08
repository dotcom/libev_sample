#pragma once
#include <ev++.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <atomic>
#define BUFFER_SIZE 1024

namespace Burst{
    void signal_h(struct ev_loop *loop, struct ev_signal *watcher, int revents){
        ev_break(loop);
    }
    class Server{
        std::atomic<bool> m_quit;
        unsigned m_port;
        in_addr_t m_in_addr;
        static struct ev_loop *m_loop;
        struct ev_io *w_accept;
        struct ev_signal *w_signal;
        int m_sd;
    
    public:

        Server(in_addr_t in_addr, unsigned port) : m_quit(false), m_port(port), m_in_addr(in_addr){}

        ~Server(){
            delete w_accept;
            delete w_signal;
            close(m_sd);
            std::cout << "server stoped..." << std::endl;
        }
        
        int init(){
            struct sockaddr_in addr;
            int addr_len = sizeof(addr);
            w_accept = (struct ev_io *) malloc(sizeof(struct ev_io));
            w_signal = (struct ev_signal *) malloc(sizeof(struct ev_signal));

            m_loop = ev_default_loop(0);

            if((m_sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
            {
                std::cerr << "socket error" << std::endl;
                return -1;
            }

            bzero(&addr, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(m_port);
            addr.sin_addr.s_addr = htonl(m_in_addr);

            if (bind(m_sd, (struct sockaddr*) &addr, sizeof(addr)) != 0)
            {
                std::cerr << "bind error" << std::endl;
                return -1;
            }

            if (listen(m_sd, 10) < 0)
            {
                perror("listen error");
                return -1;
            }

            ev_signal_init(w_signal, signal_h, SIGINT);
            ev_signal_start(m_loop, w_signal);

            ev_io_init(w_accept, accept_cb, m_sd, EV_READ);
            ev_io_start(m_loop, w_accept);
        }

        void run(){
            ev_loop(m_loop, 0);
        }

    private:
        static void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
        static void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
        static void protocol(int fd, char *data, size_t size);
        static void send_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
        static void async_send(int fd, char *data, size_t size);
    };

    struct ev_loop *Server::m_loop;

    struct my_io{
        ev_io io;
        char* buffer;
        size_t size;
    };

    void Server::protocol(int fd, char *data, size_t size){
        std::cout << ":: echo ::" << std::endl;
        std::string s(data, size);
        std::cout << s << std::endl;
        std::cout << static_cast<int>(data[size-1]) << std::endl;
        async_send(fd, data, size);
    }

    void Server::async_send(int fd, char *data, size_t size){
        struct my_io *w_sender = (struct my_io*) malloc (sizeof(struct my_io));

        // Send message bach to the client
        //send(watcher->fd, buffer, read_size, 0);
        w_sender->buffer = data;
        w_sender->size = size;

        ev_io_init(&w_sender->io, send_cb, fd, EV_WRITE);
        ev_io_start(m_loop, &w_sender->io);
    }

    void Server::send_cb(struct ev_loop *loop, struct ev_io *watcher, int revents){
        struct my_io *w = (struct my_io *) watcher;
        send(watcher->fd, w->buffer, w->size, 0);
        ev_io_stop(loop,watcher);
        free(w->buffer);
        free(w);
        return;
    }

    void Server::accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sd;
        struct ev_io *w_client = (struct ev_io*) malloc (sizeof(struct ev_io));

        if(EV_ERROR & revents)
        {
            perror("got invalid event");
            return;
        }

        // Accept client request
        client_sd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_sd < 0)
        {
            perror("accept error");
            return;
        }

        // Initialize and start watcher to read client requests
        ev_io_init(w_client, Server::read_cb, client_sd, EV_READ);
        ev_io_start(loop, w_client);
    }

    void Server::read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents){
        auto buffer = new char[BUFFER_SIZE];
        ssize_t read_size;
        bzero(buffer, BUFFER_SIZE);

        if(EV_ERROR & revents){
            std::cerr << "got invalid event" << std::endl;
            return;
        }

        read_size = recv(watcher->fd, buffer, BUFFER_SIZE, 0);
        if(read_size < 0){
            close(watcher->fd);
            ev_io_stop(loop,watcher);
            free(watcher);
            free(buffer);
            std::cerr << "read error" << std::endl;
            return;
        }
        else if(read_size == 0){
            close(watcher->fd);
            ev_io_stop(loop,watcher);
            free(watcher);
            std::cerr << "peer might closing" << std::endl;
            return;
        }

        protocol(watcher->fd, buffer, read_size);
    }
}

