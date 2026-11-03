#pragma once
#include <random>
#include <thread>
#include <vector>
#include "mesa.hpp"

string prefijo(const Mesa* me) {return "Mesa " + to_string(me->id) + ": ";}

bool sorteo_fallo(const Config& c, mt19937& rng) {
	if (c.fallo_prob <= 0.0) return false;
	return uniform_real_distribution<double>(0.0, 1.0)(rng) < c.fallo_prob;
}

void hilo_puma(Mesa* me) {
	const Config& c = me->cfg;
	Tablero& t = me->tab;
	mt19937 rng(c.seed * 7919u + (unsigned)me->id * 131u + 1u);
	uniform_int_distribution<int> retardo(c.min_ms, c.max_ms);

	while (!me->parar(ROL_PUMA)) {
		me->dormir(ROL_PUMA, retardo(rng));
		if (me->parar(ROL_PUMA)) break;
		if (sorteo_fallo(c, rng)) {
			me->declarar_fallo(ROL_PUMA);
			break;
		}

		int cel[NCASILLAS];
		t.copiar(cel);
		vector<OpcionPuma> ops;
		Tablero::opciones_puma(cel, &ops);
		vector<OpcionPuma> caps, pasos;
		for (const OpcionPuma& o : ops) (o.sobre >= 0 ? caps : pasos).push_back(o);
		const vector<OpcionPuma>& elegidas = !caps.empty() ? caps : pasos;
		if (elegidas.empty()) continue;
		const OpcionPuma& o =
			elegidas[uniform_int_distribution<int>(0, (int)elegidas.size() - 1)(rng)];

		int desde = t.pos(PUMA);
		if (o.sobre >= 0) {
			int v = t.capturar(o.sobre, o.destino);
			if (v > 0) {
				LOG.escribir(prefijo(me) + "Puma " + casilla(desde) + " captura al perro " +
					to_string(v) + " en " + casilla(o.sobre) + " -> " +
					casilla(o.destino), true);
				me->notificar();
			}
		} else if (t.mover(PUMA, o.destino)) {
			LOG.escribir(prefijo(me) + "Puma " + casilla(desde) + " -> " + casilla(o.destino), false);
			me->notificar();
		}
	}
}

void hilo_perro(Mesa* me, int id) {
	const Config& c = me->cfg;
	Tablero& t = me->tab;
	mt19937 rng(c.seed * 7919u + (unsigned)me->id * 131u + 17u * (unsigned)id + 2u);
	uniform_int_distribution<int> retardo(c.min_ms, c.max_ms);
	uniform_real_distribution<double> u01(0.0, 1.0);
	const Topologia& tp = topo();

	while (!me->parar(ROL_PERROS)) {
		me->dormir(ROL_PERROS, retardo(rng));
		if (me->parar(ROL_PERROS)) break;
		if (sorteo_fallo(c, rng)) {
			me->declarar_fallo(ROL_PERROS);
			break;
		}
		int desde = t.pos(id);
		if (desde < 0) break;

		int cel[NCASILLAS];
		t.copiar(cel);
		int puma = -1;
		for (int i = 0; i < NCASILLAS; i++)
			if (cel[i] == PUMA) puma = i;

		vector<int> seguros, inseguros;
		for (int q : tp.ady[desde]) {
			if (cel[q] != VACIO) continue;
			int cel2[NCASILLAS];
			copy(cel, cel + NCASILLAS, cel2);
			cel2[desde] = VACIO;
			cel2[q] = id;
			bool expuesto = false;
			for (const Salto& s : tp.porSobre[q])
				if (cel2[s.desde] == PUMA && cel2[s.hasta] == VACIO) expuesto = true;
			(expuesto ? inseguros : seguros).push_back(q);
		}

		int destino = -1;
		if (!seguros.empty()) {
			if (puma >= 0 && u01(rng) < 0.7) {
				auto dist = [&](int q) {
					int dr = tp.fila[q] - tp.fila[puma], dc = tp.col[q] - tp.col[puma];
					return dr * dr + dc * dc;
				};
				destino = *min_element(seguros.begin(), seguros.end(),
					[&](int a, int b) {return dist(a) < dist(b);});
			} else {
				destino = seguros[uniform_int_distribution<int>(0, (int)seguros.size() - 1)(rng)];
			}
		} else if (!inseguros.empty() && u01(rng) < 0.2) {
			destino = inseguros[uniform_int_distribution<int>(0, (int)inseguros.size() - 1)(rng)];
		}

		if (destino >= 0 && t.mover(id, destino)) {
			LOG.escribir(prefijo(me) + "Perro " + to_string(id) + " " + casilla(desde) + " -> " +
				casilla(destino), false);
			me->notificar();
		}
	}
}

void jugar(Cliente* c) {
	Mesa* me = c->mesa;
	me->esperar_listo();
	vector<thread> hs;
	if (c->rol == ROL_PUMA) {
		hs.emplace_back(hilo_puma, me);
	} else {
		for (int id = 1; id <= me->tab.perros(); id++) hs.emplace_back(hilo_perro, me, id);
	}
	for (auto& h : hs) h.join();
	me->jugador_termino();
}
