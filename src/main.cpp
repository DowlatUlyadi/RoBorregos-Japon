///////////Librerias////////////
#include <Arduino.h>
#include <Mapear.h>

//////////Encoders//////////////
#define ENCODER_A 4
#define ENCODER_B 5

/////////Variables, mapa y mover//////////
uint8_t iRow = 4, iCol = 4, iPiso = 0;
char cDir = 'n';
Tile tMapa[3][10][10];
Movimiento *mover;

///////////Apuntadores constantes a las variables/////////
uint8_t *const iR = &iRow;
uint8_t *const iC = &iCol;
uint8_t *const iP = &iPiso;
char *const cD = &cDir;

//////////////Funciones de encoders//////////////////
void encoder1() {
	mover->encoder1();
}

void encoder2() {
	mover->encoder2();
}


void setup() {
	//Serial
	Serial.begin(9600);
	Serial2.begin(115200);
	while(Serial2.available()) {
		Serial2.read();
	}

	//Interrupciones
	attachInterrupt(ENCODER_A, encoder1, RISING);
	attachInterrupt(ENCODER_B, encoder2, RISING);

	//Resto de los objetos
	SensarRealidad sensarr;
	SensarRealidad *const sensar = &sensarr;
	Movimiento robot(180, 180, sensar, cD, iC, iR, iP);
	mover = &robot;
	Mapear mapa(sensar, mover);

	//El Mariachi
	sensar->apantallanteLCD("      El", "    MARIACHI");
	mover->stop();

	/*while(true) {
		// sensar->escribirLCD(String(sensar->getDistanciaDerecha()) + "    " + String(sensar->getDistanciaAtras()) + "    " + String(sensar->getDistanciaIzquierda()), "      " + String(sensar->getDistanciaEnfrente()));
		mover->avanzar(tMapa);
		delay(10000);
	}*/

	//Inicializamos el tile actual
	tMapa[iPiso][iCol][iRow].inicio(true);
	tMapa[iPiso][iCol][iRow].visitado(true);
	tMapa[iPiso][iCol][iRow].existe(true);
	if(sensar->caminoAtras()) {
		tMapa[iPiso][iRow + 1][iCol].existe(true);
	} else {
		tMapa[iPiso][iRow][iCol].abajo(true, &tMapa[iPiso][iRow + 1][iCol]);
	}
	mapa.llenaMapaSensor(tMapa, cDir, iCol, iRow, iPiso);

	//Loop en el cual recorre todo el mapa
	while (mover->decidir(tMapa)) {
		mover->stop();
		mapa.llenaMapaSensor(tMapa, cDir, iCol, iRow, iPiso);
	}

	//Se regresa al inicio
	sensar->apantallanteLCD("Let's go home");
	while(!tMapa[iPiso][iCol][iRow].inicio()) {
		mover->goToVisitado(tMapa, 'i');
	}

	//Regresó al incio
	mover->stop();
	sensar->apantallanteLCD("      HE","    LLEGADO");
	delay(3500);
	sensar->apantallanteLCD("    V I V A", "  M E X I C O");
}

void loop() {
}
