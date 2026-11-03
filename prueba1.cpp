#include <thread>
#include <vector>
#include <cstdio>
#include "sync.hpp"
using namespace std;

int contador = 0;
Mutex m;

void sumar_sin_lock() {for (int i = 0; i < 1000000; i++) contador++;}
void sumar_con_lock() {for (int i = 0; i < 1000000; i++) {Guard g(m); contador++;}}

int main() {
	for (auto f : {sumar_sin_lock, sumar_con_lock}) {
        	contador = 0;
        	vector<thread> hs;
        	for (int i = 0; i < 4; i++) hs.emplace_back(f);
        	for (auto& h : hs) h.join();
        	printf("resultado: %d (esperado 4000000)\n", contador);
    	}
}
