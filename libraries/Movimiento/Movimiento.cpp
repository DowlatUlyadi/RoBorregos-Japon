#include <Arduino.h>
#include <Movimiento.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <utility/Adafruit_MS_PWMServoDriver.h>
#include <PID_v1.h>

//////////////////////Define constants///////////////////////////
const uint8_t kToleranciaBumper = 10;
const double kPrecisionImu = 4.85;
const uint8_t kMapSize = 10;
const uint8_t kRampaLimit = 17;
const double kI_Front_Pared = 0.06;
const double kP_Front_Pared = 0.25;
const int kEncoder30 = 2120;
const int kEncoder15 = kEncoder30 / 2;
const uint8_t kSampleTime = 35;
const double kP_Vueltas = 1.4;
const int kDistanciaEnfrente = 40;
const int kMapearPared = 5;
const int kParedDeseadoIzq = 56; // 105 mm
const int kParedDeseadoDer = 56; // 105 mm

//////////////////////Define pins and motors//////////////////////////
#define pin_Servo 10
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *myMotorRightF = AFMS.getMotor(1);
Adafruit_DCMotor *myMotorRightB = AFMS.getMotor(4);
Adafruit_DCMotor *myMotorLeftF = AFMS.getMotor(3);
Adafruit_DCMotor *myMotorLeftB = AFMS.getMotor(2);//al revés
Servo myservo;
//1 Enfrente derecha
//2 Atras derecha
//3 Enfrente izquierda
//4 Atras izquierda

//////////////////////PID FRONT///////////////////////////
double inIzqIMU, outIzqIMU, inDerIMU, outDerIMU, fSetPoint, outIzqPARED, outDerPARED;

PID PID_IMU_izq(&inIzqIMU, &outIzqIMU, &fSetPoint, 1.15, 0, 0, DIRECT);
PID PID_IMU_der(&inDerIMU, &outDerIMU, &fSetPoint, 1.15, 0, 0, REVERSE);

/*
   cDir (dirección)
   n = norte
   e = este
   s = sur
   w = oeste
   sMov
   d = derecha
   e = enfrente
   i = izquierda
   a = atrás
 */
Movimiento::Movimiento(uint8_t iPowd, uint8_t iPowi, SensarRealidad *r, char *c, uint8_t *ic, uint8_t *ir, uint8_t *ip) {
	////////////////////////Inicializamos los motores y Servo/////////////////////////
	iPowI = iPowi;
	iPowD = iPowd;
	AFMS.begin();
	velocidad(iPowI, iPowD);
	stop();
	myservo.attach(pin_Servo);
	myservo.write(90);
	//////////////////Inicializamos variables en 0////////////////////////////////
	eCount1 = eCount2 = cVictima = cParedes = iTerm = fSetPoint = resetIMU = 0;
	//////////////////////////Inicializamos el apuntador a los sensores, posición y LED//////////////////////
	real = r, iCol = ic, iRow = ir, iPiso = ip, cDir = c;
	//////////////////////////Inicializamos del PID de irse derecho
	PID_IMU_izq.SetMode(AUTOMATIC);
	PID_IMU_der.SetMode(AUTOMATIC);
}

void Movimiento::velocidad(uint8_t powIzq, uint8_t powDer) {
	if(powIzq < 0) powIzq = 0;
	if(powDer < 0) powDer = 0;
	if(powIzq > 255) powIzq = 255;
	if(powDer > 255) powDer = 255;

	myMotorLeftF->setSpeed(powIzq);
	myMotorLeftB->setSpeed(powIzq);

	myMotorRightF->setSpeed(powDer);
	myMotorRightB->setSpeed(powDer);
}


void Movimiento::stop() {
	// velocidad(255, 255);

	myMotorLeftF->run(BREAK);
	//myMotorLeftB->run(BREAK);
	//myMotorRightF->run(BREAK);
	myMotorRightB->run(BREAK);

	delay(35);

	myMotorLeftF->run(RELEASE);
	myMotorLeftB->run(RELEASE);
	myMotorRightF->run(RELEASE);
	myMotorRightB->run(RELEASE);

	eCount1 = eCount2 = 0;
}

void Movimiento::front() {
	myMotorLeftF->run(BACKWARD);
	myMotorLeftB->run(BACKWARD);

	myMotorRightF->run(BACKWARD);
	myMotorRightB->run(FORWARD);
}

void Movimiento::back() {
	myMotorLeftF->run(FORWARD);
	myMotorLeftB->run(FORWARD);

	myMotorRightF->run(FORWARD);
	myMotorRightB->run(BACKWARD);
}

void Movimiento::right() {
	myMotorLeftF->run(BACKWARD);
	myMotorLeftB->run(BACKWARD);

	myMotorRightF->run(FORWARD);
	myMotorRightB->run(BACKWARD);
}

void Movimiento::left() {
	myMotorLeftF->run(FORWARD);
	myMotorLeftB->run(FORWARD);

	myMotorRightF->run(BACKWARD);
	myMotorRightB->run(FORWARD);
}

void Movimiento::separarPared() {
	unsigned long ahora = millis();
	uint8_t iActual = real->getDistanciaEnfrente(), iPowDD = 130;
	real->escribirLCD("SEPARA");
	while ((iActual < kDistanciaEnfrente - 8 || iActual > kDistanciaEnfrente + 8 ) && ahora+2000 > millis()) {
		iActual = real->getDistanciaEnfrente();
		if(iActual < kDistanciaEnfrente) { //Muy cerca
			back();
			velocidad(iPowDD, iPowDD);
		}

		else if(iActual > kDistanciaEnfrente) { //Muy lejos
			front();
			velocidad(iPowDD, iPowDD);
		}
	}
	stop();
}

void Movimiento::corregirIMU() {
	if(!(real->caminoAtras()) && (resetIMU > 10)) {
		real->escribirLCD("corregir IMU");
		double fRef = 0;
		back();
		velocidad(80, 80);
		while(real->getDistanciaAtras() > 20) {
			real->escribirLCD(String(real->getDistanciaAtras()));
		}
		delay(500); // TODO: Checar con sensor
		stop();

		for(int i = 0; i < 5; i++)
			fRef += real->getAngulo();
		fRef /= 5;
		fSetPoint = fRef;

		front();
		velocidad(80, 80);
		while(real->getDistanciaAtras() < 20) {
			real->escribirLCD(String(real->getDistanciaAtras()));
		}
		stop();

		resetIMU = 0;
	}
}


void Movimiento::vueltaIzq(Tile tMapa[3][10][10]) {
	real->escribirLCD("Vuelta IZQ");
	resetIMU += 2;
	iTerm = 0;
	int potIzq, potDer, dif;
	//unsigned long inicio = millis();
	double posInicial, limInf, limSup;

	fSetPoint -= 90;
	if(fSetPoint < 0) fSetPoint += 360;

	limInf = fSetPoint - kPrecisionImu;
	if(limInf < 0) limInf += 360;

	limSup = fSetPoint + kPrecisionImu;
	if(limSup >= 360.0) limSup -= 360;

	posInicial = real->getAngulo();
	potIzq = potDer = 100;
	left();
	velocidad(225, 225);

	if(limSup > limInf) {
		while(posInicial < limInf || posInicial > limSup) {
			posInicial = real->getAngulo();
			if(posInicial < 90 && fSetPoint > 270)
				dif = abs(fSetPoint - 360 - posInicial);
			else
				dif = abs(fSetPoint - posInicial);
			if(dif > 90) dif = 90;
			dif *= kP_Vueltas;
			velocidad(potIzq + dif, potDer + dif);
			/*if (millis() >= inicio + 10000) {
			    velocidad(160, 160);
			   } else if (millis() >= inicio + 5000) {
			    velocidad(170, 170);
			   }*/
		}
	} else {
		while(posInicial < limInf && posInicial > limSup) {
			posInicial = real->getAngulo();
			if(posInicial < 90 && fSetPoint > 270)
				dif = abs(fSetPoint - 360 - posInicial);
			else
				dif = abs(fSetPoint - posInicial);
			if(dif > 90) dif = 90;
			dif *= kP_Vueltas;
			velocidad(potIzq + dif, potDer + dif);
			/*x = 85 + abs(fSetPoint - posInicial) * 1.2 / 90;
			   y = 85 + abs(fSetPoint - posInicial) * 1.2 / 90;
			   velocidad(x,y);*/
			/*if (millis() >= inicio + 10000) {
			   velocidad(160, 160);
			   } else if (millis() >= inicio + 5000) {
			    velocidad(170, 170);
			   }*/
		}
	}
	stop();
	velocidad(iPowI, iPowD);
}

void Movimiento::vueltaDer(Tile tMapa[3][10][10]) {
	real->escribirLCD("Vuelta DER");
	resetIMU += 2;
	iTerm = 0;
	int potIzq, potDer, dif;
	//unsigned long inicio = millis();
	double posInicial, limInf, limSup;

	fSetPoint += 90;
	if(fSetPoint >= 360) fSetPoint -= 360;

	limInf = fSetPoint - kPrecisionImu;
	if(limInf < 0) limInf += 360;

	limSup = fSetPoint + kPrecisionImu;
	if(limSup >= 360.0) limSup -= 360;

	posInicial = real->getAngulo();
	potIzq = potDer = 100;
	right();
	velocidad(225, 225);

	if(limSup > limInf) {
		while(posInicial < limInf || posInicial > limSup) {
			posInicial = real->getAngulo();
			if(posInicial > 270 && fSetPoint < 90)
				dif = abs(fSetPoint + 360 - posInicial);
			else
				dif = abs(fSetPoint - posInicial);
			if(dif > 90) dif = 90;
			dif *= kP_Vueltas;
			velocidad(potIzq + dif, potDer + dif);
			/*if (millis() >= inicio + 10000) {
			    velocidad(160, 160);
			   } else if (millis() >= inicio + 5000) {
			    velocidad(170, 170);
			   }*/
		}
	} else {
		while(posInicial < limInf && posInicial > limSup) {
			posInicial = real->getAngulo();
			if(posInicial > 270 && fSetPoint < 90)
				dif = abs(fSetPoint + 360 - posInicial);
			else
				dif = abs(fSetPoint - posInicial);
			if(dif > 90) dif = 90;
			dif *= kP_Vueltas;
			velocidad(potIzq + dif, potDer + dif);
			/*if (millis() >= inicio + 10000) {
			   velocidad(160, 160);
			   } else if (millis() >= inicio + 5000) {
			    velocidad(170, 170);
			   }*/
		}
	}
	stop();
	velocidad(iPowI, iPowD);
}

void Movimiento::potenciasDerecho(uint8_t &potenciaIzq, uint8_t &potenciaDer) {
	unsigned long now = millis();
	if((now - lastTime) >= kSampleTime) {
		double angle = real->getAngulo();
		if((*cDir) == 'n' && angle > 270 && fSetPoint < 90) {
			inIzqIMU = angle - 360;
			inDerIMU = angle - 360;
		} else if((*cDir) == 'n' && angle < 90 && fSetPoint > 270) {
			inIzqIMU = angle + 360;
			inDerIMU = angle + 360;
		} else {
			inIzqIMU = angle;
			inDerIMU = angle;
		}
		PID_IMU_izq.Compute();
		PID_IMU_der.Compute();

		//real->escribirLCD(String(angle) + " " + String(inIzqIMU) + " " + String(fSetPoint), String(outDerIMU) + "    " + String(outIzqIMU));
		// 7 adelante
		// 6 atras

		int distanciaIzq = real->getDistanciaIzquierda(), distanciaDer = real->getDistanciaDerecha(), iError;
		if(distanciaIzq < 180 && distanciaDer < 180) {
			iError = distanciaDer - distanciaIzq;
			iTerm += iError;
			if(iTerm > 150) iTerm = 150;
			else if(iTerm < -150) iTerm = -150;

			contadorIzq++;
			contadorDer++;

			if(iError <= -1) {
				// Se tiene que mover a la izquierda
				outIzqPARED = -iError * kP_Front_Pared;
				outDerPARED = iError * kP_Front_Pared + iTerm * kI_Front_Pared;
			} else if(iError >= 1) {
				// Se tiene que mover a la derecha
				outIzqPARED = iError * kP_Front_Pared;
				outDerPARED = -iError * kP_Front_Pared + iTerm * kI_Front_Pared;
			} else {
				// Alinearse con las dos paredes
				// TODO
			}
		} else if(distanciaIzq < 180) {
			contadorIzq++;
			iError = kParedDeseadoIzq - distanciaIzq;

			// debe ser negativo, creo
			iTerm -= iError;
			if(iTerm > 150) iTerm = 150;
			else if(iTerm < -150) iTerm = -150;

			outIzqPARED = iError * kP_Front_Pared;
			outDerPARED = -iError * kP_Front_Pared + iTerm * kI_Front_Pared;
		} else if(distanciaDer < 180) {
			contadorDer++;
			iError = kParedDeseadoDer - distanciaDer;

			iTerm += iError;
			if(iTerm > 150) iTerm = 150;
			else if(iTerm < -150) iTerm = -150;

			outIzqPARED = -iError * kP_Front_Pared;
			outDerPARED = iError * kP_Front_Pared + iTerm * kI_Front_Pared;
		} else {
			iTerm = 0;
		}
		potenciaIzq = iPowI + outIzqIMU + outIzqPARED;
		potenciaDer = iPowD + outDerIMU + outDerPARED;
		real->escribirLCD(String(distanciaDer) + "     " + String(distanciaIzq));
		// real->escribirLCD(String(outDerIMU) + "     " + String(outIzqIMU), String(outDerPARED) + "     " + String(outIzqPARED));
		lastTime = now;
	}
}

void Movimiento::pasaRampa() {
	resetIMU += 10;
	while(Serial2.available())
		cVictima = (char)Serial2.read();
	real->escribirLCD("Rampa");
	uint8_t iPowII, iPowDD;
	while(real->sensarRampa() < -kRampaLimit || real->sensarRampa() > kRampaLimit) {
		potenciasDerecho(iPowII, iPowDD);
		front();
		velocidad(iPowII, iPowDD);
	}
	cParedes = 0;
	front();
	velocidad(100, 100);
	delay(800);
	stop();
	if(real->getDistanciaEnfrente() < 200) { //Si hay pared enfrente
		separarPared();
	}
	stop();
}

void Movimiento::dejarKit(Tile tMapa[3][10][10], uint8_t iCase) {
	stop();
	tMapa[*iPiso][*iRow][*iCol].victima(true);
	if(real->sensarRampa() < kRampaLimit && real->sensarRampa() > -kRampaLimit) {
		switch(iCase) {
		case 1:
			real->apantallanteLCD("VICTIMA con KIT", "derecha");
			myservo.write(70);
			delay(500);
			myservo.write(110);
			delay(500);
			myservo.write(0);
			break;
		case 2:
			real->apantallanteLCD("VICTIMA con KIT", "izquierda");
			myservo.write(110);
			delay(500);
			myservo.write(70);
			delay(500);
			myservo.write(180);
			break;
		}
		delay(1000);
		myservo.write(90);
	}
	while(Serial2.available())
		cVictima = (char)Serial2.read();
}


void Movimiento::retroceder(Tile tMapa[3][10][10]) {
	real->escribirLCD("CUADRO NEGRO");
	uint8_t iPowDD, iPowII;
	switch(*cDir)
	{
	case 'n':
		(*iRow)++;
		break;
	case 'e':
		(*iCol)--;
		break;
	case 's':
		(*iRow)--;
		break;
	case 'w':
		(*iCol)++;
		break;
	}
	while(eCount1 + eCount2 < kEncoder30) {
		potenciasDerecho(iPowII, iPowDD);
		back();
		velocidad(iPowII, iPowDD);
	}
	eCount1 = eCount2 = 0;
}

void Movimiento::acomodaChoque(uint8_t switchCase) {
	real->escribirLCD("LIMIT");
	stop();
	uint16_t encoderTemp1 = eCount1;
	uint16_t encoderTemp2 = eCount2;
	switch(switchCase) {
	case 1:
		back();
	   velocidad(100, 100);
	   delay(400);
	   stop();
	   velocidad(0, 255);
	   delay(400);
	   break;
	case 2:
		back();
		velocidad(0, 255);
		delay(400);
		velocidad(255, 0);
		delay(400);
		break;
	}
	stop();
	encoderTemp1 -= eCount1;
	encoderTemp2 -= eCount2;
	eCount1 = encoderTemp1;
	eCount2 = encoderTemp2;
}

void Movimiento::avanzar(Tile tMapa[3][10][10]) {
	real->escribirLCD("AVANZAR");
	cParedes = 0;
	resetIMU++;
	double bumperMin = 0.0, bumperMax = 0.0;
	uint8_t iPowII, iPowDD, switchCase, iCase;

	while(Serial2.available())
		cVictima = (char)Serial2.read();

	corregirIMU();
	eCount1 = eCount2 = 0;

	velocidad(iPowI, iPowD);
	front();
	while(eCount1 + eCount2 < kEncoder15 && real->getDistanciaEnfrente() >= kDistanciaEnfrente) {
		potenciasDerecho(iPowII, iPowDD);
		velocidad(iPowII, iPowDD);

		//Calor
		while(Serial2.available())
			cVictima = (char)Serial2.read();

		if(cVictima&0b00000010 && !tMapa[*iPiso][*iRow][*iCol].victima()) {
			iCase = (cVictima&0b00000001) ? 1 : 2;
			stop();
			dejarKit(tMapa, iCase);
			front();
		}
		switchCase = real->switches();
		   if(switchCase > 0 && real->caminoEnfrente()) {
		   acomodaChoque(switchCase);
		   }

		//bumper
		bumperMin = real->sensarRampa() < bumperMin ? real->sensarRampa() : bumperMin;
		bumperMax = real->sensarRampa() > bumperMax ? real->sensarRampa() : bumperMax;
		if (bumperMax - bumperMin >= kToleranciaBumper)
			tMapa[*iPiso][*iRow][*iCol].bumper(true);
		cVictima = 0;
	}

	switch(*cDir) {
	case 'n':
		(*iRow)--;
		break;
	case 'e':
		(*iCol)++;
		break;
	case 's':
		(*iRow)++;
		break;
	case 'w':
		(*iCol)--;
		break;
	}

	contadorIzq = contadorDer = 0;

	while(eCount1 + eCount2 < kEncoder30  && real->getDistanciaEnfrente() > kDistanciaEnfrente) {
		potenciasDerecho(iPowII, iPowDD);
		front();
		velocidad(iPowII, iPowDD);

		//Calor
		while(Serial2.available())
			cVictima = (char)Serial2.read();

		if(cVictima&0b00000010 && !tMapa[*iPiso][*iRow][*iCol].victima()) {
			iCase = (cVictima&0b00000001) ? 1 : 2;
			dejarKit(tMapa, iCase);
			front();
		}
		switchCase = real->switches();
		if(switchCase > 0 && real->caminoEnfrente()) {
			acomodaChoque(switchCase);
		}

		//bumper
		bumperMin = real->sensarRampa() < bumperMin ? real->sensarRampa() : bumperMin;
		bumperMax = real->sensarRampa() > bumperMax ? real->sensarRampa() : bumperMax;
		if (bumperMax - bumperMin >= kToleranciaBumper)
			tMapa[*iPiso][*iRow][*iCol].bumper(true);
		cVictima = 0;
	}
	stop();
	if(contadorIzq > kMapearPared)
		cParedes |= 0b00000100;

	if(contadorDer > kMapearPared)
		cParedes |= 0b00000001;

	eCount1 = eCount2 = 0;
	if( !real->color() && real->sensarRampa() < kRampaLimit && real->sensarRampa() > -kRampaLimit && real->getDistanciaEnfrente() < 20) {
		cParedes |= 0b00000010;
		separarPared();
		iCase = 0;
		stop();
	}
}

//Aquí obviamente hay que poner las cosas de moverse con los motores, hasta ahorita es solamente modificar mapa
void Movimiento::derecha(Tile tMapa[3][10][10]) {                                                                                                              //Modificarse en realidad
	real->escribirLCD("DER");
	uint8_t iCase;
	while(Serial2.available())
		cVictima = (char)Serial2.read();
	if(cVictima&0b00000010 && !tMapa[*iPiso][*iRow][*iCol].victima()) {
		iCase = (cVictima&0b00000001) ? 1 : 2;
		dejarKit(tMapa, iCase);
	}
	switch(*cDir) {
	case 'n':
		*cDir='e';
		break;
	case 'e':
		(*cDir) = 's';
		break;
	case 's':
		(*cDir) = 'w';
		break;
	case 'w':
		(*cDir) = 'n';
		break;
	}
	vueltaDer(tMapa);
}

//Aquí obviamente hay que poner las cosas de moverse con los motores, hasta ahorita es solamente modificar mapa
void Movimiento::izquierda(Tile tMapa[3][10][10]) {
	real->escribirLCD("IZQ");
	uint8_t iCase;
	while(Serial2.available())
		cVictima = (char)Serial2.read();
	if(cVictima&0b00000010 && !tMapa[*iPiso][*iRow][*iCol].victima()) {
		iCase = (cVictima&0b00000001) ? 1 : 2;
		dejarKit(tMapa, iCase);
	}
	switch(*cDir) {
	case 'n':
		(*cDir) = 'w';
		break;
	case 'e':
		(*cDir) = 'n';
		break;
	case 's':
		(*cDir) = 'e';
		break;
	case 'w':
		(*cDir) = 's';
		break;
	}
	vueltaIzq(tMapa);
}

//Recibe el string de a dónde moverse y ejecuta las acciones llamando a las funciones de arriba
void Movimiento::hacerInstrucciones(Tile tMapa[3][10][10], String sMov) {
	real->escribirLCD("PATH");
	///////////////////////////TEST/////////////////////////////////
	/*for(int i = sMov.length()-1; i >= 0; i--) {
	        Serial.print(sMov[i]);
	   }
	   Serial.println(" ");
	   delay(5000);
	   cDir = (char)Serial.read();*/
	for(int i = sMov.length()-1; i >= 0; i--) {
		switch(sMov[i]) {
		case 'r':
			derecha(tMapa);
			avanzar(tMapa);
			break;
		case 'u':
			avanzar(tMapa);
			break;
		case 'l':
			izquierda(tMapa);
			avanzar(tMapa);
			break;
		case 'd':
			derecha(tMapa);
			derecha(tMapa);
			avanzar(tMapa);
			break;
		}
	}
	//stop();
}

//Busca dónde está el no visitado usando funciones de aquí y de sensar mapa.
//Aquí imprime el mapa de int
//La funcion comparaMapa se podría modificar para depender si quiero ir al inicio o a un cuadro no visitado
bool Movimiento::goToVisitado(Tile tMapa[3][10][10], char cD) {
	//Declara un mapa de int, debe ser del tamaño que el otro mapa. Será mejor declararlo desde un principio del código?
	uint8_t iMapa[10][10];
	char cMapa[10][10];
	//Llena el mapa de 0
	for (uint8_t i = 0; i < kMapSize; i++)
		for(uint8_t j = 0; j < kMapSize; j++) {
			iMapa[i][j] = 0;
			cMapa[i][j] = 'n';
		}
	//Pone 1 en donde vamos a iniciar
	iMapa[*iRow][*iCol] = 1;
	cMapa[*iRow][*iCol] = 'i';
	//LA FUNCION RECURSIVA
	mapa.llenaMapa(iMapa, cMapa, tMapa, *cDir, *iCol, *iRow, *iPiso);
	///////////////Imprime el mapa//////////////////////////////
	/*for (uint8_t i = 0; i < kMapSize; ++i) {
	        for(uint8_t j=0; j<kMapSize; j++) {
	                Serial.print(iMapa[i][j]); Serial.print(" ");
	        }
	        Serial.println();
	   }
	   Serial.println();
	   Serial.println();
	   for (uint8_t i = 0; i < kMapSize; ++i) {
	        for(uint8_t j=0; j<kMapSize; j++) {
	                Serial.print(cMapa[i][j]); Serial.print(" ");
	        }
	        Serial.println();
	   }
	   delay(5000);*/
	//Nuevas coordenadas a dónde moverse
	uint8_t iNCol = 100, iNRow = 100;
	//Compara las distancias para escoger la más pequeña
	if(mapa.comparaMapa(iMapa, tMapa, cD, *iCol, *iRow, iNCol, iNRow, *iPiso)) { //Hace las instrucciones que recibe de la función en forma de string
		hacerInstrucciones(tMapa, mapa.getInstrucciones(iMapa, cMapa, tMapa, iNCol, iNRow, *iPiso));
		*iCol = iNCol, *iRow = iNRow;
		return true;
	}
	return false;
}


//La hice bool para que de una forma estuviera como condición de un loop, pero aún no se me ocurre cómo
bool Movimiento::decidir(Tile tMapa[3][10][10]) {
	stop();
	if(tMapa[*iPiso][*iRow][*iCol].cuadroNegro()) {
		retroceder(tMapa);
	}
	if(real->getDistanciaEnfrente() < 200) {
		separarPared();
	}
	uint8_t iCase;
	while(Serial2.available())
		cVictima = (char)Serial2.read();
	if(cVictima&0b00000010 && !tMapa[*iPiso][*iRow][*iCol].victima()) {
		iCase = (cVictima&0b00000001) ? 1 : 2;
		dejarKit(tMapa, iCase);
	}
	//Esto ya no debe de ser necesario con la clase Mapear y SensarRealidad
	tMapa[*iPiso][*iRow][*iCol].existe(true);
	//Esto, no sé si sea mejor tenerlo aquí o en la clase Mapear
	tMapa[*iPiso][*iRow][*iCol].visitado(true);
	//Todos estos sensados los hace con el mapa virtual, por eso dependemos en que el robot sea preciso.
	//Si no hay pared a la derecha Y no está visitado, muevete hacia allá
	if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'r') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'r')) {
		derecha(tMapa);
		avanzar(tMapa);
		stop();
		return true;
	}
	//Si no hay pared enfrente Y no está visitado, muevete hacia allá
	else if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'u') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'u')) {
		avanzar(tMapa);
		stop();
		return true;
	}
	//Si no hay pared a la izquierda y no está visitado, muevete hacia allá
	else if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'l') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'l')) {
		izquierda(tMapa);
		avanzar(tMapa);
		stop();
		return true;
	}
	//Si no hay pared atrás y no está visitado, muevete hacia allá
	//Sólo entraría a este caso en el primer movimiento de la ronda (si queda mirando a un deadend)
	else if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'd') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'd')) {
		derecha(tMapa);
		derecha(tMapa);
		avanzar(tMapa);
		stop();
		return true;
	}
	//Aquí es cuando entra a lo recursivo
	else{
		//Llama la función recursiva
		return goToVisitado(tMapa, 'n');
	}
}

/*bool Movimiento::decidir_Prueba(Tile tMapa[3][10][10]) {
        //Esto ya no debe de ser necesario con la clase Mapear y SensarRealidad
        tMapa[*iPiso][*iRow][*iCol].existe(true);
        //Esto, no sé si sea mejor tenerlo aquí o en la clase Mapear
        tMapa[*iPiso][*iRow][*iCol].visitado(true);
        //Todos estos sensados los hace con el mapa virtual, por eso dependemos en que el robot sea preciso.
        //Si no hay pared a la derecha Y no está visitado, muevete hacia allá
        if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'r') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'r')) {
                Serial.println("DER");
                switch(*cDir) {
                case 'n':
                        (*iCol)++;
                        (*cDir) = 'e';
                        break;
                case 'e':
                        (*iRow)++;
                        (*cDir) = 's';
                        break;
                case 's':
                        (*iCol)--;
                        (*cDir) = 'w';
                        break;
                case 'w':
                        (*iRow)--;
                        (*cDir) = 'n';
                        break;
                }
                return true;
        }
        //Si no hay pared enfrente Y no está visitado, muevete hacia allá
        else if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'u') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'u')) {
                Serial.println("ENF");
                switch(*cDir) {
                case 'n':
                        (*iRow)--;
                        (*cDir) = 'n';
                        break;
                case 'e':
                        (*iCol)++;
                        (*cDir) = 'e';
                        break;
                case 's':
                        (*iRow)++;
                        (*cDir) = 's';
                        break;
                case 'w':
                        (*iCol)--;
                        (*cDir) = 'w';
                        break;
                }
                return true;
        }
        //Si no hay pared a la izquierda y no está visitado, muevete hacia allá
        else if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'l') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'l')) {
                Serial.println("IZQ");
                switch(*cDir) {
                case 'n':
                        (*iCol)--;
                        (*cDir) = 'w';
                        break;
                case 'e':
                        (*iRow)--;
                        (*cDir) = 'n';
                        break;
                case 's':
                        (*iCol)++;
                        (*cDir) = 'e';
                        break;
                case 'w':
                        (*iRow)++;
                        (*cDir) = 's';
                        break;
                }
                return true;
        }
        //Si no hay pared atrás y no está visitado, muevete hacia allá
        //Sólo entraría a este caso en el primer movimiento de la ronda (si queda mirando a un deadend)
        else if(mapa.sensa_Pared(tMapa, *cDir, *iCol, *iRow, *iPiso, 'd') && mapa.sensaVisitado(tMapa, *cDir, *iCol, *iRow, *iPiso, 'd')) {
                Serial.println("ATR");
                switch(*cDir) {
                case 'n':
                        (*iRow)++;
                        (*cDir) = 's';
                        break;
                case 'e':
                        (*iCol)--;
                        (*cDir) = 'w';
                        break;
                case 's':
                        (*iRow)--;
                        (*cDir) = 'n';
                        break;
                case 'w':
                        (*iCol)++;
                        (*cDir) = 'e';
                        break;
                }
                return true;
        }
        //Aquí es cuando entra a lo recursivo
        else{
                //Llama la función recursiva
                return goToVisitado(tMapa, 'n');
        }
   }*/

void Movimiento::encoder1() {
	eCount1++;
}

void Movimiento::encoder2() {
	eCount2++;
}

char Movimiento::getParedes() {
	return cParedes;
}

/*
   void Movimiento::ErrorGradosVuelta(double &grados) {
        int iM;
        grados = real->getAngulo();
        grados = (grados >= 360) ? 0 - fDeseado : grados - fDeseado;
        if(grados > 180) {
                iM = grados/180;
                grados = -( 180 - (grados - (iM + 180)));
        } else if(grados < -180) {
                grados *= -1;
                iM = grados/180;
                grados = ( 180 - (grados - (iM + 180)));
        }
   }
 */
