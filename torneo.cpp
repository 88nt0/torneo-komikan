#include <signal.h>
#include <cstdlib>
#include <deque>
#include <list>
#include <memory>
#include "juez.hpp"
using namespace std;

struct Matchmaker {
	Mutex m;
	Cond cv;
	deque<Cliente*> q[2];
	int pendientes = 0;
} MM;

void cancelar(Cliente* c) {
	Guard g(c->m);
	c->cancelado = true;
	c->cv.signal();
}

bool llegar(Cliente* c) {
	Guard g(MM.m);
	MM.pendientes--;
	bool ok = !STOP.load();
	if (ok) MM.q[c->rol].push_back(c);
	MM.cv.signal();
	return ok;
}

struct Entrada {
	thread th;
	unique_ptr<Mesa> mesa;
};

void matchmaker() {
	list<Entrada> entradas;
	int contador = 0;
	auto limpiar = [&](bool todas) {
		for (auto it = entradas.begin(); it != entradas.end();) {
			if (todas || it->mesa->terminada.load()) {
				it->th.join();
				it = entradas.erase(it);
			} else {
				++it;
			}
		}
	};

	for (;;) {
		Cliente *a = nullptr, *b = nullptr;
		bool terminar = false;
		{
			Guard g(MM.m);
			while (!STOP.load() && !(!MM.q[0].empty() && !MM.q[1].empty()) && MM.pendientes > 0)
				MM.cv.wait(MM.m);
			if (STOP.load() || MM.q[0].empty() || MM.q[1].empty()) {
				terminar = true;
			} else {
				a = MM.q[ROL_PUMA].front(); MM.q[ROL_PUMA].pop_front();
				b = MM.q[ROL_PERROS].front(); MM.q[ROL_PERROS].pop_front();
			}
		}
		if (terminar) break;

		SLOTS.wait();
		if (STOP.load()) {cancelar(a); cancelar(b); break;}
		limpiar(false);

		auto mesa = make_unique<Mesa>(++contador, CFG, a, b);
		{
			Guard g(REG);
			if (STOP.load()) {cancelar(a); cancelar(b); SLOTS.post(); break;}
			REGISTRO->insert(mesa.get());
		}
		int act = ++ACTIVAS;
		int prev = MAX_ACTIVAS.load();
		while (act > prev && !MAX_ACTIVAS.compare_exchange_weak(prev, act)) {}

		Mesa* mp = mesa.get();
		entradas.push_back({thread(juez, mp), move(mesa)});
		for (Cliente* c : {a, b}) {
			Guard g(c->m);
			c->mesa = mp;
			c->cv.signal();
		}
	}

	{
		Guard g(MM.m);
		for (int r = 0; r < 2; r++)
			while (!MM.q[r].empty()) {
				Cliente* c = MM.q[r].front();
				MM.q[r].pop_front();
				ST_SIN_PAR++;
				cancelar(c);
			}
	}
	limpiar(true);
}

void cliente(Cliente* c) {
	mt19937 rng(CFG.seed * 31u + (unsigned)c->id);
	int espera = CFG.llegada_ms > 0 ? uniform_int_distribution<int>(0, CFG.llegada_ms)(rng) : 0;
	{
		timespec limite = Cond::deadline_ms(espera);
		Guard g(GM);
		while (!STOP.load())
			if (!GCV.wait_until(GM, limite)) break;
	}
	if (!llegar(c)) return;
	{
		Guard g(c->m);
		while (!c->mesa && !c->cancelado) c->cv.wait(c->m);
		if (c->cancelado) return;
	}
	jugar(c);
}

void hilo_senales() {
	sigset_t mascara;
	sigemptyset(&mascara);
	sigaddset(&mascara, SIGINT);
	sigaddset(&mascara, SIGTERM);
	sigaddset(&mascara, SIGUSR1);
	for (;;) {
		int sig = 0;
		if (sigwait(&mascara, &sig) != 0) continue;
		if (sig == SIGUSR1) return;
		if (STOP.exchange(true)) continue;
		LOG.escribir("SIGINT: se detiene la llegada de jugadores y se cierran las partidas...", true);
		{Guard g(GM); GCV.broadcast();}
		{Guard g(MM.m); MM.cv.broadcast();}
		SLOTS.post();
		Guard g(REG);
		for (Mesa* me : *REGISTRO) me->interrumpir();
	}
}

bool numero_positivo(const char* s, long& salida) {
	char* fin = nullptr;
	salida = strtol(s, &fin, 10);
	return *s != '\0' && *fin == '\0' && salida >= 1;
}

int main(int argc, char** argv) {
	sigset_t mascara;
	sigemptyset(&mascara);
	sigaddset(&mascara, SIGINT);
	sigaddset(&mascara, SIGTERM);
	sigaddset(&mascara, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &mascara, nullptr);

	long N = 0, M = 0;
	if (argc < 3 || argc > 4 || !numero_positivo(argv[1], N) || !numero_positivo(argv[2], M)) {
		fprintf(stderr, "Uso: %s N M [config.txt]\n  N: clientes (>=1), M: mesas maximas (>=1)\n", argv[0]);
		return 2;
	}
	if (argc == 4) {
		string err;
		if (!cargar_config(argv[3], CFG, err)) {
			fprintf(stderr, "Error en configuracion: %s\n", err.c_str());
			return 2;
		}
	}

	LOG.abrir(CFG.log);
	SLOTS.init((unsigned)M);
	set<Mesa*> registro;
	REGISTRO = &registro;
	MM.pendientes = (int)N;

	LOG.escribir("Torneo: N=" + to_string(N) + " clientes, M=" + to_string(M) + " mesas, retardo " +
		to_string(CFG.min_ms) + "-" + to_string(CFG.max_ms) + " ms, seed " +
		to_string(CFG.seed), true);

	thread senales(hilo_senales);

	vector<Rol> roles;
	for (long i = 0; i < N; i++) roles.push_back(i % 2 == 0 ? ROL_PUMA : ROL_PERROS);
	mt19937 rng(CFG.seed);
	shuffle(roles.begin(), roles.end(), rng);

	vector<unique_ptr<Cliente>> clientes;
	for (long i = 0; i < N; i++) {
		clientes.push_back(make_unique<Cliente>());
		clientes.back()->id = (int)i + 1;
		clientes.back()->rol = roles[(size_t)i];
	}

	thread mm(matchmaker);
	vector<thread> hilos;
	for (auto& c : clientes) hilos.emplace_back(cliente, c.get());

	for (auto& h : hilos) h.join();
	mm.join();

	pthread_kill(senales.native_handle(), SIGUSR1);
	senales.join();

	bool interrumpido = STOP.load();
	char resumen[512];
	snprintf(resumen, sizeof resumen,
		"RESUMEN: partidas=%d | gana puma=%d | ganan perros=%d | abandonos=%d | empates=%d | "
		"interrumpidas=%d | no emparejados=%d | max mesas simultaneas=%d (M=%ld)",
		ST_PARTIDAS.load(), ST_PUMA.load(), ST_PERROS.load(), ST_ABANDONO.load(),
		ST_EMPATE.load(), ST_INTERR.load(), ST_SIN_PAR.load(), MAX_ACTIVAS.load(), M);
	LOG.escribir(resumen, true);
	LOG.cerrar();
	REGISTRO = nullptr;
	return interrumpido ? 130 : 0;
}
