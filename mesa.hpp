#pragma once
#include "comun.hpp"

enum Rol {ROL_PUMA = 0, ROL_PERROS = 1};

struct Cliente {
	int id;
	Rol rol;
	Mutex m;
	Cond cv;
	Mesa* mesa = nullptr;
	bool cancelado = false;
};

struct Mesa {
	int id;
	const Config& cfg;
	Cliente* cli[2];
	Tablero tab;

	Mutex m;
	Cond cv;
	Cond cv_listo;
	long version = 0;
	bool listo = false;
	int terminados = 0;

	atomic<bool> fallo[2];
	atomic<bool> interrumpida;
	atomic<bool> terminada;

	Mutex md;
	Cond cd;

	Mesa(int id_, const Config& c, Cliente* puma, Cliente* perros)
		: id(id_), cfg(c), interrumpida(false), terminada(false) {
		cli[ROL_PUMA] = puma;
		cli[ROL_PERROS] = perros;
		fallo[0] = false;
		fallo[1] = false;
	}

	void notificar() {
		Guard g(m);
		version++;
		cv.signal();
	}
	void despertar_piezas() {
		Guard g(md);
		cd.broadcast();
	}
	bool parar(int rol) const {return tab.resultado() != EN_CURSO || fallo[rol].load();}

	void dormir(int rol, int ms) {
		timespec limite = Cond::deadline_ms(ms);
		Guard g(md);
		while (!parar(rol))
			if (!cd.wait_until(md, limite)) break;
	}

	void declarar_fallo(int rol) {
		fallo[rol].store(true);
		LOG.escribir("Mesa " + to_string(id) + ": FALLO interno del jugador " +
			(rol == ROL_PUMA ? "Puma" : "Perros"), true);
		despertar_piezas();
		notificar();
	}
	void interrumpir() {
		interrumpida.store(true);
		notificar();
	}
	void esperar_listo() {
		Guard g(m);
		while (!listo) cv_listo.wait(m);
	}
	void jugador_termino() {
		Guard g(m);
		terminados++;
		cv.signal();
	}
};
