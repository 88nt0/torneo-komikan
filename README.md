# Torneo de Komikan Concurrente

Tarea 2 de Sistemas Operativos, Universidad Diego Portales.

Este proyecto simula un torneo de Komikan, Puma contra Perros, en donde los clientes llegan en momentos aleatorios, un Matchmaker los empareja y cada partida se juega en una mesa con su propio juez. Está implementado en C++17 con hilos POSIX, y la sincronización se hace con `pthread_mutex_t`, `pthread_cond_t` y `sem_t`.

## Requisitos

- Linux (probado en Ubuntu)
- g++ con soporte C++17
- make

En Ubuntu se instalan con:

```bash
sudo apt install build-essential
```

## Cómo compilar

```bash
git clone https://github.com/88nt0/torneo-komikan.git
cd torneo-komikan
make
```

`make` ejecuta `g++ -Wall -Wextra -std=c++17 -pthread -O2 -o torneo torneo.cpp` y genera el ejecutable `torneo`. La compilación no entrega warnings.

## Cómo ejecutar

```bash
./torneo N M [config.txt]
```

- `N`: cantidad de clientes. La mitad juega como Puma y la otra mitad como Perros. Si N es impar, un cliente queda sin oponente y se cuenta como no emparejado.
- `M`: cantidad máxima de mesas activas al mismo tiempo.
- `config.txt`: archivo de configuración opcional. Si no se entrega, se usan los valores por defecto.

Ejemplos:

```bash
./torneo 20 3 config_rapido.txt   # partidas rápidas
./torneo 40 4 config_lento.txt    # tiempos normales, útil para probar Ctrl+C
./torneo 30 6 config_fallos.txt   # con fallos de jugadores simulados
```

Con el primer ejemplo, al final se imprime una línea de resumen como esta (los valores cambian en cada corrida):

```
RESUMEN: partidas=10 | gana puma=9 | ganan perros=1 | abandonos=0 | empates=0 | interrumpidas=0 | no emparejados=0 | max mesas simultaneas=3 (M=3)
```

Ahí se puede comprobar que `partidas` es N/2 y que `max mesas simultaneas` nunca supera M.

## Qué muestra el programa

- En consola se imprimen las capturas, el tablero cada cierto número de movimientos, el resultado de cada mesa y el resumen final.
- Todos los movimientos quedan registrados en `torneo.log`.
- El código de salida es `0` si todo terminó normal, `130` si se interrumpió con Ctrl+C y `2` si hubo un error de uso o de configuración.

## Formato del archivo de configuración

Cada línea tiene el formato `clave=valor`. Lo que va después de `#` es comentario, y los parámetros que no se indican toman su valor por defecto.

| Clave | Qué hace | Por defecto |
|---|---|---|
| `min_ms`, `max_ms` | retardo mínimo y máximo entre movimientos de una ficha (ms) | 50, 300 |
| `seed` | semilla aleatoria | 12345 |
| `max_mov` | tope de movimientos por partida, si se alcanza es empate | 1500 |
| `print_cada` | imprime el tablero cada K movimientos (0 = nunca) | 25 |
| `llegada_ms` | cada cliente llega en un instante aleatorio entre 0 y este valor | 200 |
| `fallo_prob` | probabilidad de que un jugador falle al decidir | 0 |
| `log` | archivo de registro | `torneo.log` |
| `trono` | las 3 casillas del trono: punta, izquierda, derecha (`P`, `D`, `.`) | `P..` |
| `fila0` a `fila4` | posición inicial en la grilla 5x5 (`P` puma, `D` perro, `.` vacío) | ver abajo |

Posición inicial por defecto:

```
trono=P..
fila0=.....
fila1=.....
fila2=D...D
fila3=DDDDD
fila4=DDDDD
```

El archivo se valida al cargarlo: debe haber un solo `P` y entre 1 y 12 `D`, `min_ms` no puede ser mayor que `max_ms` y cada fila debe tener 5 caracteres. Si hay un error, el programa indica la línea y el motivo, y termina con código 2.

## Cómo probar Ctrl+C

```bash
./torneo 40 4 config_lento.txt
```

Al presionar `Ctrl + C` pasados un par de segundos, las mesas activas terminan como `INTERRUMPIDA`, se imprime el resumen y el programa se cierra por sí solo con código `130`.

## Cómo probar con sanitizers

```bash
make tsan && ./torneo_tsan 20 3 config_rapido.txt
make asan && ./torneo_asan 20 3 config_rapido.txt
```

El primero detecta data races y el segundo errores de memoria y fugas. En ambos casos no deberían aparecer `WARNING` ni `ERROR`. Si TSan falla al iniciar con `unexpected memory mapping`, se soluciona con `sudo sysctl vm.mmap_rnd_bits=28`.

`make clean` borra los ejecutables y los logs.

## Estructura del código

- `sync.hpp`: clases `Mutex`, `Guard`, `Cond` y `Sem` sobre pthread
- `config.hpp`: lectura y validación del archivo de configuración
- `tablero.hpp`: tablero, conexiones, reglas y locks
- `comun.hpp`: log, variables globales y funciones de impresión
- `mesa.hpp`: `Cliente` y `Mesa`
- `piezas.hpp`: hilos del Puma y de los Perros
- `juez.hpp`: hilo del juez de cada mesa
- `torneo.cpp`: `main`, Matchmaker, clientes y manejo de señales
- `prueba1.cpp` a `prueba6.cpp`: pruebas que fui haciendo por etapas

## Decisiones de sincronización

**Tablero.** Tiene 28 casillas (la grilla 5x5 y las 3 del trono) y cada una tiene su propio mutex. Cada ficha mira el tablero sin bloquear y decide su movimiento. Luego bloquea solo las casillas involucradas (2 si camina, 3 si captura) y vuelve a revisar que todo siga igual, porque otra ficha pudo moverse mientras tanto. Si dos fichas quieren la misma casilla, una la obtiene y la otra falla sin efectos y reintenta en su siguiente turno.

**Deadlock.** Las casillas siempre se bloquean de menor a mayor índice, por lo que no puede formarse una espera circular. El juez bloquea las 28 en ese mismo orden cuando necesita una vista consistente del tablero.

**Resultado de la partida.** Se fija con compare-and-swap, de modo que solo el primer evento (victoria, abandono o interrupción) decide el resultado.

**Límite de mesas.** Se usa un `sem_t` que parte en M. El Matchmaker hace `sem_wait` antes de abrir una mesa, esperando sin consumir CPU, y el juez hace `sem_post` cuando la partida termina. Por esto nunca hay más de M mesas activas.

**Piezas y juez.** Cada mesa tiene un mutex, una variable de condición y un contador. Las fichas avisan cuando algo cambia y el juez duerme hasta recibir el aviso, o hasta que pasen 100 ms. Los retardos de las fichas también son esperas con variable de condición, para poder cortarlos apenas termina la partida.

**Fallos.** Si un jugador falla, el juez le da la victoria al oponente por abandono, detiene las piezas y libera los recursos de la mesa. Los fallos ocurren fuera de las secciones críticas y los locks se liberan automáticamente con `Guard`, por lo que ningún hilo termina reteniendo un mutex.

**Ctrl+C.** SIGINT y SIGTERM se bloquean al inicio y los recibe un hilo dedicado con `sigwait`. Al llegar la señal se deja de aceptar clientes, las mesas activas terminan de forma ordenada y se liberan los recursos. Solo se usan hilos y semáforos sin nombre, por lo que no quedan semáforos ni memoria compartida en el sistema.

## Supuestos

El enunciado no define todos los detalles, por lo que tomé las siguientes decisiones. El tablero completo está en la clase `Topologia` de `tablero.hpp`.

- La grilla es de 5x5, solo con líneas horizontales y verticales. Las casillas van de 0 a 24, con índice `fila*5 + columna`.
- El trono tiene 3 casillas: la punta (25) y dos abajo, izquierda (26) y derecha (27). En total el tablero tiene 28 casillas y 48 conexiones.
- El Puma captura saltando sobre un perro vecino y cayendo en la casilla vacía que sigue en línea recta, con un salto por movimiento. Gana al lograr 6 capturas. Los Perros ganan si el Puma se queda sin movimientos.
- La posición inicial por defecto es el Puma en la punta y 12 perros en la parte baja. Se puede cambiar con `trono` y `fila0` a `fila4`.
- Agregué `max_mov` para evitar partidas infinitas. Si se alcanza, la partida termina en empate.
- La semilla fija las decisiones aleatorias de cada ficha, pero el sistema operativo decide el orden de ejecución de los hilos, por lo que dos corridas con la misma semilla no son idénticas movimiento a movimiento.

## Autor

Renato Sandoval
