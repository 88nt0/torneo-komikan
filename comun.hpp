#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <set>
#include <string>
#include "config.hpp"
#include "sync.hpp"
#include "tablero.hpp"
using namespace std;

class Logger {
public:
	Logger() : arch_(nullptr), t0_(chrono::steady_clock::now()) {}
	void abrir(const string& ruta) {
		arch_ = fopen(ruta.c_str(), "w");
		if (!arch_) perror("no se pudo abrir el log");
	}
	void cerrar() {
		if (arch_) fclose(arch_);
		arch_ = nullptr;
	}
	void escribir(const string& s, bool consola) {
		Guard g(m_);
		double t = chrono::duration<double>(chrono::steady_clock::now() - t0_).count();
		char pre[32];
		snprintf(pre, sizeof pre, "[%8.3f] ", t);
		if (consola) {
			fputs(pre, stdout); fputs(s.c_str(), stdout); fputc('\n', stdout); fflush(stdout);
		}
		if (arch_) {
			fputs(pre, arch_); fputs(s.c_str(), arch_); fputc('\n', arch_);
		}
	}
private:
	Mutex m_;
	FILE* arch_;
	chrono::steady_clock::time_point t0_;
};

Logger LOG;
Config CFG;
atomic<bool> STOP(false);
Sem SLOTS;
Mutex GM;
Cond GCV;

atomic<int> ACTIVAS(0), MAX_ACTIVAS(0);
atomic<int> ST_PARTIDAS(0), ST_PUMA(0), ST_PERROS(0), ST_ABANDONO(0), ST_EMPATE(0),
	ST_INTERR(0), ST_SIN_PAR(0);

struct Mesa;
Mutex REG;
set<Mesa*>* REGISTRO = nullptr;

string casilla(int i) {
	if (i == APEX) return "T";
	if (i == TRI_IZQ) return "TI";
	if (i == TRI_DER) return "TD";
	return "(" + to_string(topo().fila[i]) + "," + to_string(topo().col[i]) + ")";
}

string render(const int cel[NCASILLAS]) {
	auto sim = [&](int i) {return cel[i] == VACIO ? '.' : (cel[i] == PUMA ? 'P' : 'D');};
	string s = "        ";
	s += sim(APEX);
	s += "\n      ";
	s += sim(TRI_IZQ);
	s += "   ";
	s += sim(TRI_DER);
	s += "   <- trono\n";
	for (int r = 0; r < 5; r++) {
		s += "    ";
		for (int c = 0; c < 5; c++) {
			s += sim(r * 5 + c);
			if (c < 4) s += ' ';
		}
		if (r < 4) s += '\n';
	}
	return s;
}
