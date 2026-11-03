#include <cstdio>
#include <thread>
#include <vector>
#include "tablero.hpp"

int main() {
	Config cfg;
	cfg.filas[0] = ".....";
	cfg.filas[1] = "..D..";
	cfg.filas[2] = ".D.D.";
	cfg.filas[3] = "..D..";
	cfg.filas[4] = ".....";
	int violaciones = 0;
	for (int ronda = 0; ronda < 2000; ronda++) {
		Tablero t;
		t.inicializar(cfg);
		atomic<int> exitos(0);
		vector<thread> hs;
		for (int perro : {1, 2, 3, 4}) {
			hs.emplace_back([&t, &exitos, perro]() {if (t.mover(perro, 12)) exitos++;});
		}
		for (auto& h : hs) h.join();
		if (exitos != 1) violaciones++;
	}
	printf("rondas: 2000, violaciones de exclusion mutua: %d\n", violaciones);
}
