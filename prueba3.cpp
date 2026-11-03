#include <cstdio>
#include "tablero.hpp"

int main() {
	const Topologia& t = topo();
	int total = 0;
	for (int i = 0; i < NCASILLAS; i++) total += (int)t.ady[i].size();
	printf("casillas: %d, aristas: %d\n", NCASILLAS, total / 2);

	for (int i : {0, 12, 2, 25, 26}) {
		printf("casilla %d (col %d, fila %d) vecinas:", i, t.col[i], t.fila[i]);
		for (int q : t.ady[i]) printf(" %d", q);
		printf("\n");
	}
	for (int i : {12, 25, 26}) {
		printf("saltos desde %d:", i);
		for (Salto s : t.saltos[i]) printf(" (sobre %d -> %d)", s.sobre, s.hasta);
		printf("\n");
	}
}
