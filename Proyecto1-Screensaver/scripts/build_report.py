#!/usr/bin/env python3
"""Genera docs/Informe.pdf desde docs/Informe.md (Paged.js + Chrome headless; ver README)."""

import base64
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

try:
    import markdown
    from markdown.extensions.toc import slugify as toc_slugify
except ImportError:
    sys.exit("Falta el paquete 'markdown' (pip install markdown).")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs")
SOURCE = os.path.join(DOCS, "Informe.md")
HTML_OUT = os.path.join(DOCS, "Informe.html")
PDF_OUT = os.path.join(DOCS, "Informe.pdf")
PAGED_JS = os.path.join(ROOT, "scripts", "vendor", "paged.polyfill.js")

GREEN = "#0b5134"  # verde institucional UVG

CSS = """
@page {
  size: letter;
  margin: 2.5cm;
}

body { font-family: Arial, "Liberation Sans", Helvetica, sans-serif; font-size: 12pt;
       line-height: 1.5; color: #111; text-align: justify; hyphens: auto; }
h2, h3 { font-family: Arial, "Liberation Sans", Helvetica, sans-serif; color: #000;
         text-align: left; break-after: avoid; line-height: 1.25; }
h2 { font-size: 15pt; margin: 1.3em 0 0.6em; }
h3 { font-size: 12.5pt; margin: 1.1em 0 0.4em; }
p, li { orphans: 3; widows: 3; }
/* Evita que Paged.js estire la ultima linea de elementos cortados entre paginas. */
div, h2, h3, li, p:not([data-split-to]) { text-align-last: auto !important; }
code { font: inherit; background: none; padding: 0; }

/* Secciones que inician en pagina nueva */
h2[id="sec-1-introduccion"], h2[id="sec-5-resultados-y-discusion"], h2[id="sec-7-referencias"] { break-before: page; }
.anexo { break-before: page; }
.sin-corte { break-inside: avoid; }

/* Tablas con titulo abajo (p.tabla-titulo), filas sin cortar */
table { border-collapse: collapse; width: 100%; margin: 0.8em 0 0.2em; font-size: 12pt;
        line-height: 1.3; text-align: left; }
tr { break-inside: avoid; }
th { background: none; color: #000; font-weight: bold; }
th, td { border: 0.6pt solid #000; padding: 3px 4px; vertical-align: top; overflow-wrap: break-word; }
.tabla-pequena td:first-child { white-space: nowrap; }
.tabla-titulo { text-align: center; font-style: italic; font-size: 12pt; margin: 0.3em 0 1em; break-before: avoid; }
.tabla-pequena table { font-size: 12pt; }
.tabla-pequena th, .tabla-pequena td { padding: 1.5px 3px; }

/* Figuras con titulo abajo, centrado e italico */
figure { margin: 0.8em 0; text-align: center; break-inside: avoid; }
figure img { max-width: 100%; }
figure img.diagrama { max-height: 18.8cm; width: auto; }
figcaption { font-size: 12pt; font-style: italic; line-height: 1.3; color: #222; margin-top: 4px; }
.formula { text-align: center; font-size: 12pt; margin: 0.6em 0; }

/* Caratula */
.caratula { text-align: center; font-family: "Times New Roman", "Liberation Serif", Times, serif; }
.caratula p { text-align: center; }
.caratula .logo { width: 4.2cm; margin-top: 0.5cm; }
.caratula .institucion { font-size: 13pt; line-height: 1.6; margin-top: 0.8cm; }
.caratula .institucion::first-line { font-weight: bold; }
.caratula .titulo { font-family: Arial, "Liberation Sans", sans-serif; font-size: 20pt; font-weight: bold;
                    color: #000; line-height: 1.35; margin: 2.2cm 0 0.3cm; }
.caratula .tipo { font-size: 13pt; font-style: italic; margin-bottom: 2.2cm; }
.caratula .autores { font-size: 13pt; line-height: 1.8; }
.caratula .fecha { margin-top: 2.2cm; font-size: 12pt; }

/* Indice */
.indice { break-before: page; }
.indice-titulo { font-family: Arial, Helvetica, sans-serif; font-size: 15pt; font-weight: bold; color: #000;
                 text-align: left; }
.indice .toc ul { list-style: none; padding-left: 1.3em; margin: 0; }
.indice .toc > ul { padding-left: 0; }
.indice .toc li { line-height: 1.32; font-size: 12pt; }
.indice .toc > ul > li > a { font-weight: bold; }
.indice .toc a { color: #111; text-decoration: none; display: flex; }


.captura { font-size: 12pt; line-height: 1.3; white-space: pre-wrap; text-align: left; }
""".replace("GREEN", GREEN)


def expand_includes(text):
    """Reemplaza cada {{include ruta}} por el contenido del archivo (datos de results/)."""
    def replace(match):
        path = os.path.normpath(os.path.join(DOCS, match.group(1).strip()))
        if not os.path.isfile(path):
            sys.exit(f"No existe el archivo incluido: {path}")
        with open(path, encoding="utf-8") as f:
            return f.read()
    return re.sub(r"^\{\{include (.+?)\}\}$", replace, text, flags=re.M)


def find_chrome():
    for name in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser", "chrome"):
        path = shutil.which(name)
        if path:
            return path
    return None


def main():
    with open(SOURCE, encoding="utf-8") as f:
        text = f.read()
    text = expand_includes(text)
    # Bloques de texto plano: sin salto inicial ni sangria, para que las columnas queden parejas.
    text = re.sub(r'<div class="captura">\s*(.*?)\s*</div>',
                  lambda m: '<div class="captura">' + '\n'.join(l.strip() for l in m.group(1).split('\n')) + '</div>',
                  text, flags=re.S)
    body = markdown.markdown(
        text,
        extensions=["tables", "toc", "md_in_html", "attr_list", "sane_lists"],
        # Prefijo "sec-": un id que empieza con digito rompe los selectores de Paged.js.
        extension_configs={"toc": {"title": "", "toc_depth": "2-3",
                                   "slugify": lambda value, sep: "sec-" + toc_slugify(value, sep)}},
    )
    paged = os.path.relpath(PAGED_JS, DOCS)
    html = ("<!doctype html><html lang='es'><head><meta charset='utf-8'>"
            "<title>Proyecto 1 - Screensaver Galaxia</title><style>" + CSS + "</style>"
            "<script>window.PagedConfig = { after: function () { window.pagedDone = true; } };</script>"
            f"<script src='{paged}'></script></head><body>" + body + "</body></html>")
    with open(HTML_OUT, "w", encoding="utf-8") as f:
        f.write(html)
    print("HTML:", HTML_OUT)

    chrome = find_chrome()
    if chrome is None:
        sys.exit("No se encontro Chrome/Chromium: abra Informe.html en Chrome y use 'Imprimir a PDF'.")
    print_with_devtools(chrome, "file://" + HTML_OUT, PDF_OUT)
    print("PDF:", PDF_OUT)


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def print_with_devtools(chrome, url, pdf_path, timeout=120):
    """Abre la pagina en Chrome, espera a Paged.js y la imprime por DevTools."""
    try:
        import websocket
    except ImportError:
        sys.exit("Falta el paquete 'websocket-client' (pip install websocket-client).")

    port = free_port()
    profile = tempfile.mkdtemp(prefix="informe-chrome-")
    proc = subprocess.Popen([chrome, "--headless=new", "--disable-gpu", f"--remote-debugging-port={port}",
                             "--remote-allow-origins=*", "--allow-file-access-from-files",
                             f"--user-data-dir={profile}", "about:blank"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        ws_url = None
        for _ in range(100):
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{port}/json/list") as resp:
                    pages = [t for t in json.load(resp) if t.get("type") == "page"]
                if pages:
                    ws_url = pages[0]["webSocketDebuggerUrl"]
                    break
            except OSError:
                pass
            time.sleep(0.1)
        if ws_url is None:
            sys.exit("No se pudo conectar con Chrome (DevTools).")

        conn = websocket.create_connection(ws_url, timeout=timeout)
        next_id = [0]

        def call(method, **params):
            next_id[0] += 1
            conn.send(json.dumps({"id": next_id[0], "method": method, "params": params}))
            while True:
                msg = json.loads(conn.recv())
                if msg.get("id") == next_id[0]:
                    if "error" in msg:
                        sys.exit(f"Error de Chrome en {method}: {msg['error']}")
                    return msg.get("result", {})

        call("Page.enable")
        call("Page.navigate", url=url)
        deadline = time.time() + timeout
        while time.time() < deadline:
            done = call("Runtime.evaluate", expression="window.pagedDone === true", returnByValue=True)
            if done.get("result", {}).get("value"):
                break
            time.sleep(0.5)
        else:
            sys.exit("Paged.js no termino de paginar a tiempo.")

        result = call("Page.printToPDF", printBackground=True, preferCSSPageSize=True,
                      displayHeaderFooter=False, marginTop=0, marginBottom=0, marginLeft=0, marginRight=0)
        with open(pdf_path, "wb") as f:
            f.write(base64.b64decode(result["data"]))
        conn.close()
    finally:
        proc.terminate()
        proc.wait(timeout=10)
        shutil.rmtree(profile, ignore_errors=True)


if __name__ == "__main__":
    main()
