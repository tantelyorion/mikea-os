# Tests d'interoperabilite TLS (sur la machine hote, avec OpenSSL)

Verifient `net/tls.c` + `crypto/` contre un VRAI serveur `openssl s_server` TLS 1.3 :
RSA-2048/4096, ECDSA P-256, mauvaise empreinte, octets falsifies, certificat non gere.

Prerequis : gcc, openssl, python3 (Linux/WSL/MSYS2). Depuis la racine du projet :

    mkdir -p /tmp/tlst && cd /tmp/tlst
    openssl req -x509 -newkey rsa:2048 -nodes -keyout rsa.key -out rsa.pem -days 2 -subj "/CN=localhost"
    openssl req -x509 -newkey rsa:4096 -nodes -keyout rsa4k.key -out rsa4k.pem -days 2 -subj "/CN=localhost"
    openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -keyout ec.key -out ec.pem -days 2 -subj "/CN=localhost"
    cd - && gcc -w -I. -Iinclude tools/tests/tls_harness.c net/tls.c crypto/sha256.c crypto/aes_gcm.c \
        crypto/x25519.c crypto/rsa.c crypto/random.c libc/string.c -o /tmp/tlst/h
    cp tools/tests/tls_interop.py /tmp/tlst/run_tests.py && cd /tmp/tlst && python3 run_tests.py
