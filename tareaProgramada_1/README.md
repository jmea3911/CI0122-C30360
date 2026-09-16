# Papa Caliente — CI-0122 Sistemas Operativos

Simulación del juego de la papa caliente: un anillo de `n` procesos (fork)
que se pasan un valor y le aplican la regla de Collatz, usando la clase
`Buzon` (System V message queue) para los mensajes.

## Compilar

make

Genera el ejecutable `papa`.

## Ejecutar

./papa n v [H|A]


- `n` — cantidad de participantes (obligatorio)
- `v` — valor inicial de la papa (obligatorio)
- `H` — sentido horario, i → i+1 (por defecto)
- `A` — sentido antihorario, i → i-1

### Ejemplos

./papa 6 27 H     # 6 procesos, horario
./papa 5 40 A     # 5 procesos, antihorario
./papa 1 15       # caso borde: un solo proceso


## Limpiar

make clean

