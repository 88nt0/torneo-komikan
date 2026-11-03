#include <cstdio>
#include <unistd.h>
#include "piezas.hpp"

int main() {
	Config cfg;
	cfg.min_ms = 20;
	cfg.max_ms = 80;
	LOG.abrir("prueba5.log");
	Mesa me(1, cfg, nullptr, nullptr);
	me.tab.inicializar(cfg);

	vector<thread> hs;
	hs.emplace_back(hilo_puma, &me);
	for (int id = 1; id <= me.tab.perros(); id++) hs.emplace_back(hilo_perro, &me, id);

	sleep(3);
	me.tab.fijar_global(EMPATE_LIMITE);
	me.despertar_piezas();
	for (auto& h : hs) h.join();

	Instantanea s = me.tab.instantanea();
	printf("movimientos: %ld, capturas: %d\n%s\n", s.movs, s.capturas, render(s.cel).c_str());
	LOG.cerrar();
}
