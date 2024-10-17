
#include "TLSSocket.hpp"

SSL_CTX* TLSSocket::context = nullptr;

SSL_CTX* server_init() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();                       // load and register crypto
    SSL_load_error_strings();                           // load all error messages
    const SSL_METHOD* method = TLS_server_method(); // create new server-method instance
    SSL_CTX* ctx = SSL_CTX_new(method);                 // create new context from server-method

    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

SSL_CTX* client_init() {
    OpenSSL_add_all_algorithms();                   // Load crypto
    SSL_load_error_strings();                       // bring in and register error messages
    const SSL_METHOD* method = TLS_client_method(); // create new client-method instance
    SSL_CTX* ctx = SSL_CTX_new(method);             // create new context

    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

void load_certificates(SSL_CTX* ctx, const char* certFile, const char* keyFile) {
    if (SSL_CTX_use_certificate_file(ctx, certFile, SSL_FILETYPE_PEM) <= 0)
        ERR_print_errors_fp(stderr);
    else if (SSL_CTX_use_PrivateKey_file(ctx, keyFile, SSL_FILETYPE_PEM) <= 0)
        ERR_print_errors_fp(stderr);
    else if (!SSL_CTX_check_private_key(ctx))
        ERR_print_errors_fp(stderr);
    else
        return;
    abort();
}

void TLSSocket::InitializeServer(const std::string& ca_path, const std::string& cert_path, const std::string& key_path) {
    TLSSocket::context = server_init();

    // TODO: client init...
    if (SSL_CTX_load_verify_locations(TLSSocket::context, ca_path.c_str(), NULL) != 1) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    load_certificates(TLSSocket::context, cert_path.c_str(), key_path.c_str()); /* load certs */
    SSL_CTX_set_verify(TLSSocket::context, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL); 
}

void TLSSocket::InitializeClient(const std::string& ca_path, const std::string& cert_path, const std::string& key_path) {
    TLSSocket::context = client_init();

    // TODO: client init...
    if (SSL_CTX_load_verify_locations(TLSSocket::context, ca_path.c_str(), NULL) != 1) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    load_certificates(TLSSocket::context, cert_path.c_str(), key_path.c_str()); /* load certs */
    SSL_CTX_set_verify(TLSSocket::context, SSL_VERIFY_PEER, NULL); 
}


TLSSocket::TLSSocket(int fd) : Socket(fd) {
    this->ssl = SSL_new(TLSSocket::context);
    SSL_set_fd(this->ssl, fd);
}

TLSSocket::TLSSocket(int fd, std::function<void()> readHandler) : TLSSocket(fd) {
    this->readHandler = readHandler; 
} 

TLSSocket::~TLSSocket() {}

TLSSocket::TLSSocket(TLSSocket&& other) noexcept : Socket(std::move(other)) {
    this->ssl = std::exchange(other.ssl, nullptr);
}

TLSSocket& TLSSocket::operator=(TLSSocket&& other) {
    this->fd = other.fd;
    this->readHandler = other.readHandler;
    this->ssl = other.ssl;

    other.fd = 0;
    other.readHandler = std::function<void()>();
    other.ssl = nullptr;
    
    return *this;
}

size_t TLSSocket::read(char* buffer, size_t len) {
    return SSL_read(this->ssl, buffer, len);
}

size_t TLSSocket::write(char* buffer, size_t len) {
    return SSL_write(this->ssl, buffer, len);
}

size_t TLSSocket::read_exact(char* buffer, size_t bytes) {
    return util::SSL_read_exact(this->ssl, buffer, bytes);
}

size_t TLSSocket::write_exact(char* buffer, size_t bytes) {
    return util::SSL_write_exact(this->ssl, buffer, bytes);
}

std::unique_ptr<TLSSocket> TLSSocket::connect(int fd, in_addr_t addr, int port) {
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = addr; 

    int status = ::connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
    if (status == -1) {
        ERROR() << "could not connect to server!" << std::endl;
        exit(1);
    }

    std::unique_ptr<TLSSocket> socket = std::make_unique<TLSSocket>(fd);
    int err = SSL_connect(socket->ssl);

    /*Check for error in connect.*/
    if (err < 1) {
       err=SSL_get_error(socket->ssl, err);
       printf("SSL error #%d in accept,program terminated\n",err);
       exit(0);
    }

    return socket;
}

TLSSocket* TLSSocket::accept(int fd) {
    TLSSocket* socket = new TLSSocket(fd);

    /*Do the SSL Handshake*/
    int err=SSL_accept(socket->ssl);

    /* Check for error in handshake*/
    if (err<1) {
      err=SSL_get_error(socket->ssl,err);
      printf("SSL error #%d in SSL_accept,program terminated\n",err);
      std::cout << "SSL Error: " << ERR_error_string(err, NULL) << std::endl;
      exit(0);
    }

    /* Check for Client authentication error */
    if (SSL_get_verify_result(socket->ssl) != X509_V_OK) {
        printf("SSL Client Authentication error\n");
        exit(0);
    }

    return socket; 
}
