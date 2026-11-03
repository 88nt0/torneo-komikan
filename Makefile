CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread -O2
FUENTES  = torneo.cpp sync.hpp config.hpp tablero.hpp comun.hpp mesa.hpp piezas.hpp juez.hpp

torneo: $(FUENTES)
	$(CXX) $(CXXFLAGS) -o $@ torneo.cpp

tsan: $(FUENTES)
	$(CXX) -Wall -Wextra -std=c++17 -pthread -g -O1 -fsanitize=thread -o torneo_tsan torneo.cpp

asan: $(FUENTES)
	$(CXX) -Wall -Wextra -std=c++17 -pthread -g -O1 -fsanitize=address,undefined -o torneo_asan torneo.cpp

clean:
	rm -f torneo torneo_tsan torneo_asan prueba? prueba?t *.log

.PHONY: tsan asan clean
