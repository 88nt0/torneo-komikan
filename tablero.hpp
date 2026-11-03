#pragma once
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>
#include "config.hpp"
#include "sync.hpp"
using namespace std;

const int NCASILLAS = 28;
const int APEX = 25;
const int TRI_IZQ = 26;
const int TRI_DER = 27;

struct Salto {
	int desde, sobre, hasta;
};

class Topologia {
public:
	int fila[NCASILLAS];
	int col[NCASILLAS];
	vector<int> ady[NCASILLAS];
	vector<Salto> saltos[NCASILLAS];
	vector<Salto> porSobre[NCASILLAS];

	bool adyacente(int a, int b) const {
		for (int x : ady[a])
			if (x == b) return true;
		return false;
	}

	Topologia() {
		for (int i = 0; i < 25; i++) {fila[i] = i / 5; col[i] = i % 5;}
		fila[APEX] = -2; col[APEX] = 2;
		fila[TRI_IZQ] = -1; col[TRI_IZQ] = 1;
		fila[TRI_DER] = -1; col[TRI_DER] = 3;

		for (int r = 0; r < 5; r++) {
			for (int c = 0; c < 5; c++) {
				int i = r * 5 + c;
				if (c + 1 < 5) arista(i, i + 1);
				if (r + 1 < 5) arista(i, i + 5);
			}
		}

		arista(APEX, TRI_IZQ);
		arista(APEX, TRI_DER);
		arista(APEX, 2);
		arista(TRI_IZQ, TRI_DER);
		arista(TRI_IZQ, 0);
		arista(TRI_IZQ, 1);
		arista(TRI_DER, 4);
		arista(TRI_DER, 3);

		for (int p = 0; p < NCASILLAS; p++) {
			for (int q : ady[p]) {
				int dx = col[q] - col[p], dy = fila[q] - fila[p];
				for (int r : ady[q]) {
					int ex = col[r] - col[q], ey = fila[r] - fila[q];
					if (dx * ey - dy * ex == 0 && dx * ex + dy * ey > 0) {
						saltos[p].push_back({p, q, r});
						porSobre[q].push_back({p, q, r});
					}
				}
			}
		}
	}

private:
	void arista(int a, int b) {
		if (!adyacente(a, b)) {
			ady[a].push_back(b);
			ady[b].push_back(a);
		}
	}
};

inline const Topologia& topo() {
	static const Topologia t;
	return t;
}
