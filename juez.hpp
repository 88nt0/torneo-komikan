#pragma once
#include "piezas.hpp"

void juez(Mesa* me) {
	Tablero& t = me->tab;
	const string pre = prefijo(me);

	t.inicializar(me->cfg);
	{
		Guard g(me->m);
		me->listo = true;
		me->cv_listo.broadcast();
	}
	Instantanea s0 = t.instantanea();
	LOG.escribir(pre + "inicia: Puma=cliente " + to_string(me->cli[ROL_PUMA]->id) +
		", Perros=cliente " + to_string(me->cli[ROL_PERROS]->id) + "\n" +
		render(s0.cel), true);

	long visto = -1, bucket = 0;
	for (;;) {
		long v;
		{
			Guard g(me->m);
			bool especial = me->interrumpida.load() || me->fallo[0].load() || me->fallo[1].load();
			if (me->version == visto && !especial) me->cv.wait_ms(me->m, 100);
			v = me->version;
		}
		bool cambio = (v != visto);
		visto = v;

		if (me->interrumpida.load()) {
			t.fijar_global(INTERRUMPIDA);
		} else if (me->fallo[0].load() || me->fallo[1].load()) {
			bool fp = me->fallo[ROL_PUMA].load(), fd = me->fallo[ROL_PERROS].load();
			t.fijar_global(fp && fd ? ABANDONO_AMBOS
				: (fp ? GANAN_PERROS_ABANDONO : GANA_PUMA_ABANDONO));
		} else {
			Instantanea s = t.evaluar(me->cfg.max_mov);
			if (cambio) {
				string txt = pre + "mov " + to_string(s.movs) + ", capturas " +
					to_string(s.capturas) + "\n" + render(s.cel);
				bool consola = false;
				if (me->cfg.print_cada > 0 && s.movs / me->cfg.print_cada > bucket) {
					bucket = s.movs / me->cfg.print_cada;
					consola = true;
				}
				LOG.escribir(txt, consola);
			}
		}
		if (t.resultado() != EN_CURSO) break;
	}
	me->despertar_piezas();

	{
		Guard g(me->m);
		while (me->terminados < 2) me->cv.wait(me->m);
	}

	int r = t.resultado();
	Instantanea s = t.instantanea();
	LOG.escribir(pre + "TERMINA: " + nombre_resultado(r) + " (movs " + to_string(s.movs) +
		", capturas " + to_string(s.capturas) + ")\n" + render(s.cel), true);
	ST_PARTIDAS++;
	switch (r) {
		case GANA_PUMA_CAPTURAS: ST_PUMA++; break;
		case GANAN_PERROS_ACORRALADO: ST_PERROS++; break;
		case GANA_PUMA_ABANDONO:
		case GANAN_PERROS_ABANDONO:
		case ABANDONO_AMBOS: ST_ABANDONO++; break;
		case EMPATE_LIMITE: ST_EMPATE++; break;
		case INTERRUMPIDA: ST_INTERR++; break;
		default: break;
	}

	{
		Guard g(REG);
		REGISTRO->erase(me);
	}
	ACTIVAS--;
	SLOTS.post();
	me->terminada.store(true);
}
