#ifndef SensarMapa_h
#define SensarMapa_h

#include "Arduino.h"
#include <Tile.h>

class SensarMapa{
	public:
		////////////////////////////Constructores//////////////////////////////////////
		SensarMapa();				//Constructor default
		SensarMapa(uint8_t iT);		//Constructor con parametro, realmente no nos sirve
		////////////////////////////"Getters"/////////////////////////////////////////
		bool sensa_Pared(Tile tMapa[3][10][10], uint8_t iCol, uint8_t iRow, char cDir, char cCase, uint8_t &iPiso);
		bool sensaVisitado(Tile tMapa[3][10][10], uint8_t iCol, uint8_t iRow, char cDir, char cCase, uint8_t &iPiso);
		bool sensaExiste(Tile tMapa[3][10][10], uint8_t iCol, uint8_t iRow, char cDir, char cCase, uint8_t &iPiso);
		////////////////////////////Para la función de Go_to//////////////////////////
		void llenaMapa(uint8_t iMapa[10][10], Tile tMapa[3][10][10], uint8_t iCol, uint8_t iRow, uint8_t &iPiso);
		bool comparaMapa(uint8_t iMapa[10][10], Tile tMapa[3][10][10], uint8_t iCol, uint8_t iRow, uint8_t &iNCol, uint8_t &iNRow, char cD, uint8_t &iPiso);
		String getInstrucciones(uint8_t iMapa[10][10], Tile tMapa[3][10][10], uint8_t iNCol, uint8_t iNRow, uint8_t &iPiso);

	private:
		uint8_t iTamano;
};

#endif