#include <cstdio>
#include "config.hpp"

int main(int argc, char** argv) {
	Config c;
	if (argc == 2) {
		string err;
		if (!cargar_config(argv[1], c, err)) {printf("ERROR: %s\n", err.c_str()); return 1;}
	}
	printf("retardo %d-%d ms, seed %u, max_mov %ld\n", c.min_ms, c.max_ms, c.seed, c.max_mov);
	for (int r = 0; r < 5; r++) printf("fila%d: %s\n", r, c.filas[r].c_str());
}

