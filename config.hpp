#pragma once
#include <cstdlib>
#include <fstream>
#include <string>
using namespace std;

struct Config {
	int min_ms = 50;
	int max_ms = 300;
	unsigned seed = 12345;
	long max_mov = 1500;
	long print_cada = 25;
	int llegada_ms = 200;
	double fallo_prob = 0.0;
	string log = "torneo.log";

	string trono = "P..";
	string filas[5] = {".....", ".....", "D...D", "DDDDD", "DDDDD"};
};

inline string recortar(const string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

inline bool a_entero(const string& v, long& salida) {
	char* fin = nullptr;
	salida = strtol(v.c_str(), &fin, 10);
	return !v.empty() && *fin == '\0';
}

inline bool a_decimal(const string& v, double& salida) {
	char* fin = nullptr;
	salida = strtod(v.c_str(), &fin);
	return !v.empty() && *fin == '\0';
}

inline bool validar_config(const Config& c, string& err) {
	if (c.min_ms < 0 || c.max_ms < c.min_ms) {err = "se requiere 0 <= min_ms <= max_ms"; return false;}
	if (c.max_mov < 1) {err = "max_mov debe ser >= 1"; return false;}
	if (c.fallo_prob < 0.0 || c.fallo_prob >= 1.0) {err = "fallo_prob debe estar en [0,1)"; return false;}
	int pumas = 0, perros = 0;
	if (c.trono.size() != 3) {
		err = "trono debe tener exactamente 3 caracteres (punta, izquierda, derecha)";
		return false;
	}
	string todo = c.trono;
	for (int r = 0; r < 5; r++) {
		if (c.filas[r].size() != 5) {
			err = "fila" + to_string(r) + " debe tener exactamente 5 caracteres";
			return false;
		}
		todo += c.filas[r];
	}
	for (char ch : todo) {
		if (ch == 'P') pumas++;
		else if (ch == 'D') perros++;
		else if (ch != '.') {err = "caracter invalido en el tablero (use P, D o .)"; return false;}
	}
	if (pumas != 1) {err = "el tablero debe tener exactamente 1 puma (P)"; return false;}
	if (perros < 1 || perros > 12) {err = "el tablero debe tener entre 1 y 12 perros (D)"; return false;}
	return true;
}

inline bool cargar_config(const string& ruta, Config& c, string& err) {
	ifstream f(ruta);
	if (!f) {err = "no se pudo abrir '" + ruta + "'"; return false;}
	string linea;
	int n = 0;
	while (getline(f, linea)) {
		n++;
		size_t h = linea.find('#');
		if (h != string::npos) linea.erase(h);
		linea = recortar(linea);
		if (linea.empty()) continue;

		size_t eq = linea.find('=');
		if (eq == string::npos) {err = "linea " + to_string(n) + ": se esperaba clave=valor"; return false;}
		string k = recortar(linea.substr(0, eq));
		string v = recortar(linea.substr(eq + 1));
		long x = 0;
		double d = 0;
		bool ok = true;

		if (k == "min_ms") {ok = a_entero(v, x); c.min_ms = (int)x;}
		else if (k == "max_ms") {ok = a_entero(v, x); c.max_ms = (int)x;}
		else if (k == "seed") {ok = a_entero(v, x); c.seed = (unsigned)x;}
		else if (k == "max_mov") {ok = a_entero(v, x); c.max_mov = x;}
		else if (k == "print_cada") {ok = a_entero(v, x); c.print_cada = x;}
		else if (k == "llegada_ms") {ok = a_entero(v, x); c.llegada_ms = (int)x;}
		else if (k == "fallo_prob") {ok = a_decimal(v, d); c.fallo_prob = d;}
		else if (k == "log") {c.log = v;}
		else if (k == "trono") {ok = (v.size() == 3); if (ok) c.trono = v;}
		else if (k.size() == 5 && k.compare(0, 4, "fila") == 0 && k[4] >= '0' && k[4] <= '4') {
			c.filas[k[4] - '0'] = v;
		} else {
			err = "linea " + to_string(n) + ": clave desconocida '" + k + "'";
			return false;
		}
		if (!ok) {err = "linea " + to_string(n) + ": valor invalido para '" + k + "'"; return false;}
	}
	return validar_config(c, err);
}
