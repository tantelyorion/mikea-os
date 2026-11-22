import subprocess, time, sys, os, hashlib, signal

def start(cert, key, port, extra=()):
    p = subprocess.Popen(["openssl","s_server","-accept",str(port),"-cert",cert,"-key",key,"-tls1_3",
                          "-ciphersuites","TLS_AES_128_GCM_SHA256","-www","-quiet",*extra],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
    time.sleep(1.0)
    return p

def client(port, pin="-", corrupt=None):
    args=["./h",str(port),pin]
    if corrupt is not None: args.append(str(corrupt))
    r=subprocess.run(args,capture_output=True,text=True,timeout=40)
    return r.returncode, r.stdout

def spki_pin(pem):
    der=subprocess.run(f"openssl x509 -in {pem} -pubkey -noout | openssl pkey -pubin -outform DER",shell=True,capture_output=True).stdout
    return hashlib.sha256(der).hexdigest()

results=[]
def check(name, ok, detail=""):
    results.append(ok); print(("[OK]    " if ok else "[ECHEC] ")+name, detail)

srv=start("rsa.pem","rsa.key",4443)
try:
    rc,out=client(4443)
    line=[l for l in out.splitlines() if l.startswith("pin=")]
    got=line[0][4:] if line else ""
    check("poignee de main TLS 1.3 + echange HTTP (RSA-2048, TOFU)", rc==0 and "HTTP/1.0 200" in out, out.splitlines()[0] if out else "")
    check("empreinte de cle identique a celle calculee par OpenSSL", got==spki_pin("rsa.pem"))
    rc,out=client(4443,spki_pin("rsa.pem"))
    check("bonne empreinte attendue -> accepte", rc==0)
    rc,out=client(4443,"00"*32)
    check("MAUVAISE empreinte -> refuse", rc!=0 and "EMPREINTE" in out, out.splitlines()[0] if out else "")
    # falsification d'un octet dans le flux serveur (apres le ServerHello, dans la partie chiffree)
    for off in (200, 600, 900):
        rc,out=client(4443,"-",off)
        check(f"octet falsifie a l'offset {off} -> refuse", rc!=0, out.splitlines()[0] if out else "")
finally:
    srv.terminate(); srv.wait(timeout=5)

srv=start("rsa4k.pem","rsa4k.key",4444)
try:
    rc,out=client(4444)
    check("serveur RSA-4096", rc==0 and "HTTP/1.0 200" in out, out.splitlines()[0] if out else "")
finally:
    srv.terminate(); srv.wait(timeout=5)

srv=start("ec.pem","ec.key",4445)
try:
    rc,out=client(4445)
    check("serveur ECDSA P-256 : poignee de main + echange HTTP", rc==0 and "HTTP/1.0 200" in out, out.splitlines()[0] if out else "")
    rc,out=client(4445,"00"*32)
    check("ECDSA : mauvaise empreinte -> refuse", rc!=0 and "EMPREINTE" in out)
    rc,out=client(4445,"-",700)
    check("ECDSA : octet falsifie -> refuse", rc!=0, out.splitlines()[0] if out else "")
finally:
    srv.terminate(); srv.wait(timeout=5)

# serveur qui n'accepte QUE ed25519 / sigalgs incompatibles -> refus propre
subprocess.run("openssl req -x509 -newkey ed25519 -nodes -keyout ed.key -out ed.pem -days 2 -subj /CN=localhost 2>/dev/null",shell=True)
srv=start("ed.pem","ed.key",4446)
try:
    rc,out=client(4446)
    check("certificat Ed25519 (non gere) -> refus propre", rc!=0 and "algorithmes" in out, out.splitlines()[0] if out else "")
finally:
    srv.terminate(); srv.wait(timeout=5)

print(f"\n{sum(results)}/{len(results)} tests reussis")
sys.exit(0 if all(results) else 1)
