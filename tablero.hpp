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

const int MAX_PERROS = 12;
const int PUMA = 0;
const int VACIO = -1;
const int CAPTURAS_VICTORIA = 6;

enum Resultado {
	EN_CURSO = 0, GANA_PUMA_CAPTURAS, GANAN_PERROS_ACORRALADO, GANA_PUMA_ABANDONO,
	GANAN_PERROS_ABANDONO, ABANDONO_AMBOS, EMPATE_LIMITE, INTERRUMPIDA
};

inline const char* nombre_resultado(int r) {
	switch (r) {
		case EN_CURSO: return "en curso";
		case GANA_PUMA_CAPTURAS: return "GANA EL PUMA (capturo 6 perros)";
		case GANAN_PERROS_ACORRALADO: return "GANAN LOS PERROS (puma acorralado)";
		case GANA_PUMA_ABANDONO: return "GANA EL PUMA (perros abandonaron)";
		case GANAN_PERROS_ABANDONO: return "GANAN LOS PERROS (puma abandono)";
		case ABANDONO_AMBOS: return "SIN GANADOR (ambos abandonaron)";
		case EMPATE_LIMITE: return "EMPATE (limite de movimientos)";
		case INTERRUMPIDA: return "INTERRUMPIDA (SIGINT)";
	}
	return "?";
}

struct OpcionPuma {
	int sobre;
	int destino;
};

struct Instantanea {
	int cel[NCASILLAS];
	int capturas;
	long movs;
	int resultado;
};

class Tablero {
public:
	Tablero() : res_(EN_CURSO), capturas_(0), movs_(0), ndogs_(0) {
		for (int i = 0; i <= MAX_PERROS; i++) pos_[i].store(-1);
		for (int i = 0; i < NCASILLAS; i++) cel_[i].store(VACIO);
	}

	void inicializar(const Config& c) {
		string plano = c.trono;
		for (int r = 0; r < 5; r++) plano += c.filas[r];
		int id = 0;
		for (int i = 0; i < NCASILLAS; i++) {
			int idx = (i < 3) ? APEX + i : i - 3;
			char ch = plano[i];
			if (ch == 'P') {
				cel_[idx].store(PUMA);
				pos_[PUMA].store(idx);
			} else if (ch == 'D') {
				id++;
				cel_[idx].store(id);
				pos_[id].store(idx);
			}
		}
		ndogs_ = id;
	}

	int perros() const {return ndogs_;}
	int pos(int pieza) const {return pos_[pieza].load();}
	long movimientos() const {return movs_.load();}
	int resultado() const {return res_.load();}

	void copiar(int salida[NCASILLAS]) const {
		for (int i = 0; i < NCASILLAS; i++) salida[i] = cel_[i].load();
	}

	bool mover(int pieza, int destino) {
		int desde = pos_[pieza].load();
		if (desde < 0 || !topo().adyacente(desde, destino)) return false;

		int menor = min(desde, destino), mayor = max(desde, destino);
		cl_[menor].lock();
		cl_[mayor].lock();

		bool ok = res_.load() == EN_CURSO && pos_[pieza].load() == desde &&
			cel_[desde].load() == pieza && cel_[destino].load() == VACIO;
		if (ok) {
			cel_[destino].store(pieza);
			cel_[desde].store(VACIO);
			pos_[pieza].store(destino);
			movs_.fetch_add(1);
		}

		cl_[mayor].unlock();
		cl_[menor].unlock();
		return ok;
	}

	int capturar(int sobre, int destino) {
		int desde = pos_[PUMA].load();
		if (desde < 0) return 0;
		bool valido = false;
		for (const Salto& s : topo().saltos[desde])
			if (s.sobre == sobre && s.hasta == destino) valido = true;
		if (!valido) return 0;

		int idx[3] = {desde, sobre, destino};
		sort(idx, idx + 3);
		for (int i = 0; i < 3; i++) cl_[idx[i]].lock();

		int victima = cel_[sobre].load();
		bool ok = res_.load() == EN_CURSO && cel_[desde].load() == PUMA && victima > 0 &&
			cel_[destino].load() == VACIO;
		if (ok) {
			cel_[sobre].store(VACIO);
			pos_[victima].store(-1);
			cel_[destino].store(PUMA);
			cel_[desde].store(VACIO);
			pos_[PUMA].store(destino);
			movs_.fetch_add(1);
			if (capturas_.fetch_add(1) + 1 >= CAPTURAS_VICTORIA) fijar(GANA_PUMA_CAPTURAS);
		}

		for (int i = 2; i >= 0; i--) cl_[idx[i]].unlock();
		return ok ? victima : 0;
	}

	Instantanea instantanea() {
		bloquear_todo();
		Instantanea s = leer_locked();
		desbloquear_todo();
		return s;
	}

	Instantanea evaluar(long max_mov) {
		bloquear_todo();
		Instantanea s = leer_locked();
		if (res_.load() == EN_CURSO) {
			if (s.capturas >= CAPTURAS_VICTORIA) fijar(GANA_PUMA_CAPTURAS);
			else if (opciones_puma(s.cel, nullptr) == 0) fijar(GANAN_PERROS_ACORRALADO);
			else if (s.movs >= max_mov) fijar(EMPATE_LIMITE);
		}
		s.resultado = res_.load();
		desbloquear_todo();
		return s;
	}

	void fijar_global(Resultado r) {
		bloquear_todo();
		fijar(r);
		desbloquear_todo();
	}

	static int opciones_puma(const int cel[NCASILLAS], vector<OpcionPuma>* salida) {
		int p = -1;
		for (int i = 0; i < NCASILLAS; i++)
			if (cel[i] == PUMA) p = i;
		if (p < 0) return 0;
		int n = 0;
		for (int q : topo().ady[p])
			if (cel[q] == VACIO) {n++; if (salida) salida->push_back({-1, q});}
		for (const Salto& s : topo().saltos[p])
			if (cel[s.sobre] > 0 && cel[s.hasta] == VACIO) {n++; if (salida) salida->push_back({s.sobre, s.hasta});}
		return n;
	}

private:
	bool fijar(int r) {
		int esperado = EN_CURSO;
		return res_.compare_exchange_strong(esperado, r);
	}
	void bloquear_todo() {for (int i = 0; i < NCASILLAS; i++) cl_[i].lock();}
	void desbloquear_todo() {for (int i = NCASILLAS - 1; i >= 0; i--) cl_[i].unlock();}
	Instantanea leer_locked() const {
		Instantanea s;
		for (int i = 0; i < NCASILLAS; i++) s.cel[i] = cel_[i].load();
		s.capturas = capturas_.load();
		s.movs = movs_.load();
		s.resultado = res_.load();
		return s;
	}

	Mutex cl_[NCASILLAS];
	atomic<int> cel_[NCASILLAS];
	atomic<int> pos_[MAX_PERROS + 1];
	atomic<int> res_;
	atomic<int> capturas_;
	atomic<long> movs_;
	int ndogs_;
};
