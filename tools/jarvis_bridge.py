#!/usr/bin/env python3
"""
Pont JARVIS : relie le port serie de MikeaOS (COM1, expose par QEMU sur
TCP 127.0.0.1:4555 via "make run-ai") a un modele de langage LOCAL
(Ollama, http://127.0.0.1:11434).

Tout reste sur votre machine : aucun service cloud n'est contacte.

Usage :
    1. ollama pull llama3.2:3b          (ou un autre modele local)
    2. python3 tools/jarvis_bridge.py   (laisser tourner)
    3. make run-ai                      (dans un autre terminal)
    4. dans MikeaOS :  jarvis ask Explique-moi ce qu'est un pare-feu

Protocole (une ligne par message, ASCII) :
    MikeaOS -> hote :  "P:"        -> hote repond "OK"
    MikeaOS -> hote :  "Q:<texte>" -> hote repond "A:<reponse>"

SECURITE : la reponse n'est qu'un TEXTE affiche par MikeaOS. Elle n'est
jamais executee comme commande.

Variables d'environnement : JARVIS_MODEL, JARVIS_HOST, JARVIS_PORT, OLLAMA_URL
"""

import json
import os
import socket
import sys
import time
import unicodedata
import urllib.request

MODEL = os.environ.get("JARVIS_MODEL", "llama3.2:3b")
HOST = os.environ.get("JARVIS_HOST", "127.0.0.1")
PORT = int(os.environ.get("JARVIS_PORT", "4555"))
OLLAMA_URL = os.environ.get("OLLAMA_URL", "http://127.0.0.1:11434/api/generate")

MAX_ANSWER = 230  # l'ecran de MikeaOS est petit : reponses courtes

SYSTEM_PROMPT = (
    "Tu es JARVIS, l'assistant IA local du systeme d'exploitation MikeaOS. "
    "Reponds en francais, en une a trois phrases courtes, sans markdown, "
    "sans listes, sans emojis, avec uniquement des caracteres ASCII "
    "(pas d'accents). Sois precis et utile, sur un ton calme et poli."
)


def to_ascii(text: str) -> str:
    text = unicodedata.normalize("NFKD", text)
    text = text.encode("ascii", "ignore").decode("ascii")
    text = " ".join(text.split())
    return text[:MAX_ANSWER]


def ask_model(question: str) -> str:
    payload = json.dumps({
        "model": MODEL,
        "system": SYSTEM_PROMPT,
        "prompt": question,
        "stream": False,
        "options": {"num_predict": 120, "temperature": 0.3},
    }).encode("utf-8")

    request = urllib.request.Request(
        OLLAMA_URL, data=payload, headers={"Content-Type": "application/json"}
    )

    try:
        with urllib.request.urlopen(request, timeout=110) as response:
            data = json.loads(response.read().decode("utf-8"))
            answer = to_ascii(data.get("response", ""))
            return answer or "Je n'ai pas de reponse a donner."
    except Exception as error:  # noqa: BLE001
        return to_ascii(
            f"Modele local indisponible ({type(error).__name__}). "
            f"Verifiez qu'Ollama tourne et que le modele {MODEL} est installe."
        )


def serve(connection: socket.socket) -> None:
    buffer = b""

    while True:
        chunk = connection.recv(1024)
        if not chunk:
            return
        buffer += chunk

        while b"\n" in buffer:
            raw, buffer = buffer.split(b"\n", 1)
            line = raw.decode("ascii", "ignore").strip()

            if line.startswith("P:"):
                connection.sendall(b"OK\n")
            elif line.startswith("Q:"):
                question = line[2:].strip()
                print(f"[jarvis] question : {question}")
                answer = ask_model(question)
                print(f"[jarvis] reponse  : {answer}")
                connection.sendall(("A:" + answer + "\n").encode("ascii", "ignore"))


def main() -> None:
    print(f"[jarvis] pont IA local, modele '{MODEL}', cible {HOST}:{PORT}")
    print("[jarvis] en attente de MikeaOS (lancez 'make run-ai')...")

    while True:
        try:
            with socket.create_connection((HOST, PORT), timeout=3) as connection:
                connection.settimeout(None)
                print("[jarvis] MikeaOS connecte.")
                serve(connection)
                print("[jarvis] connexion fermee.")
        except (ConnectionRefusedError, OSError):
            time.sleep(1.5)
        except KeyboardInterrupt:
            sys.exit(0)


if __name__ == "__main__":
    main()
