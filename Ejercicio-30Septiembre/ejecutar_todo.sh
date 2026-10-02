#!/bin/bash
# Compila y ejecuta los 4 programas mostrando los comandos (para las capturas de pantalla)
cd "$(dirname "$0")"
run() { echo -e "\e[1;32m\$ $*\e[0m"; "$@"; }
for f in Ejercicio1a Ejercicio1a_modificado Ejercicio1b Ejercicio1b_modificado; do
  b=$(echo $f | tr A-Z a-z | sed 's/modificado/mod/')
  echo; echo "==================== $f.c ===================="
  run mpicc $f.c -o $b
  run mpirun -np 4 ./$b
  [ "$1" = "-p" ] && read -p $'\n[Enter para continuar al siguiente programa]'
done
