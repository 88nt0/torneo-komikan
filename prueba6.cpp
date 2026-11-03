#include <cstdio>
#include "juez.hpp"

int main() {
	Config cfg;
	cfg.min_ms = 2;
	cfg.max_ms = 10;
	cfg.print_cada = 0;
	LOG.abrir("prueba6.log");
	SLOTS.init(1);
	SLOTS.wait();
	ACTIVAS++;
	set<Mesa*> registro;
	REGISTRO = &registro;

	Cliente a, b;
	a.id = 1; a.rol = ROL_PUMA;
	b.id = 2; b.rol = ROL_PERROS;
	Mesa me(1, cfg, &a, &b);
	a.mesa = &me;
	b.mesa = &me;
	registro.insert(&me);

	thread tj(juez, &me), ta(jugar, &a), tb(jugar, &b);
	tj.join();
	ta.join();
	tb.join();
	printf("resultado final: %s\n", nombre_resultado(me.tab.resultado()));
	LOG.cerrar();
}
