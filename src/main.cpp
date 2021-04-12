#include <Arduino.h>
#include "Timemark.h"
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Chrono.h>
#include <EEPROM.h>

//prototipos de funciones
void delta(void);
void filtroRC(int);

//*******************************************************************
//para el rfid
#define RST_PIN         9
#define SS_PIN          10
MFRC522 mfrc522(SS_PIN, RST_PIN);

class RFID{
  private:
    byte llave1[4] = { 0x09, 0xC9, 0x15, 0xB4 };  // Ejemplo de clave valida
    byte llave2[4] = {0x99, 0xF0, 0xF6, 0xB9};
    bool dbg =  false;    //para debug
    
    //Función para comparar dos vectores
    bool isEqualArray(byte* arrayA, byte* arrayB, int length)
    {
      for (int index = 0; index < length; index++)
      {
        if (arrayA[index] != arrayB[index]) return false;
      }
      return true;
    }

  public:
    bool a = false;

    void setup(){
      SPI.begin();			// Init SPI bus
      mfrc522.PCD_Init();	
    }

    void debug(){
      dbg = true;

      //debug de rfid
      SPI.begin();			// Init SPI bus
      mfrc522.PCD_Init();		// Init MFRC522
      delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
      mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
      Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
    }

      bool validar(){

      // Detectar tarjeta
      if (mfrc522.PICC_IsNewCardPresent())
      {
        //Seleccionamos una tarjeta
        if (mfrc522.PICC_ReadCardSerial())
        {
          // Comparar ID con las claves válidas
          if (isEqualArray(mfrc522.uid.uidByte, llave1, 4)){
            Serial.println("Tarjeta valida");
            a = true;
          }else if(isEqualArray(mfrc522.uid.uidByte, llave2, 4)){
            Serial.println("Tarjeta valida");
            a = true;
          }else{
            Serial.println("Tarjeta invalida");
            a = false;
          }
    
          // Finalizar lectura actual
          mfrc522.PICC_HaltA();
        }
      }
      return a;
    }
  
    void infoTarjeta(){
      // Look for new cards
      if ( ! mfrc522.PICC_IsNewCardPresent()) {
        return;
      }

      // Select one of the cards
      if ( ! mfrc522.PICC_ReadCardSerial()) {
        return;
      }

      // Dump debug info about the card; PICC_HaltA() is automatically called
      mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
    }
};
RFID rfid;

//*******************************************************************
//Crear el objeto lcd  dirección  0x27 y 20 columnas y 4 filas
LiquidCrystal_I2C lcd(0x27, 20, 4);
Timemark tiempoFlashCorto(400);
Timemark tiempoFlashLargo(800);
Timemark tiempoAlternar1;
Timemark tiempoAlternar2;

class LCDI2C{
  private:
    //lcd de 20x4
    int lineas = 4;
    int caracteres = 20;

    //para "limpiar" una linea. No se usa clear() porque es para borrar toda la pantalla
    //se usa en lineaVacia y limpiarLinea()
    String textoVacio;

    String textoLinea[4];
    int tiempoInt = 1500;

    //genera la linea de espacios para usar como limpiador de lineas de acuerdo al numero de caracteres de la pantalla
    void lineaVacia(){
      for(int j = 0; j < caracteres; j++){
        textoVacio = textoVacio + " ";
      }
    }

    void imprimirLinea(String texto, int linea){
      lcd.setCursor(0, linea);
      lcd.print(texto);
    }

    void imprimirLinea(String texto, int linea, int posicion){
      lcd.setCursor(posicion, linea);
      lcd.print(texto);
    }
  
  public:

    void setup(){
      lcd.init();
      lcd.clear();
      lcd.backlight();
      lineaVacia();

      tiempoAlternar1.limitMillis(tiempoInt);
      tiempoAlternar2.limitMillis(tiempoInt);
    }

    void limpiarLinea(int linea){
      if(linea >= 0 && linea < lineas){
        lcd.setCursor(0, linea);
        lcd.print(textoVacio);
        lcd.setCursor(0, linea);
      }
    }    //fin limpiarLinea

    void mostrarTexto(String texto, int linea){
      if(linea >= 0 && linea < lineas){
        if(textoLinea[linea] != texto){    //si ha cambiado el texto lo actualiza, si no, no hace nada
          imprimirLinea(texto, linea);
          textoLinea[linea] = texto;
        }
      }
    }

    void mostrarTexto(String texto, int linea, int posicion){
      if(linea >= 0 && linea < lineas){
        if(textoLinea[linea] != texto){    //si ha cambiado el texto lo actualiza, si no, no hace nada
          imprimirLinea(texto, linea, posicion);
          textoLinea[linea] = texto;
        }
      }
    }

    void mostrarSinBorrar(String texto, int linea){
      if(linea >= 0 && linea < lineas){
        if(textoLinea[linea] != texto){
          int numeroEspacios = caracteres - texto.length();
          String espacios = "";
          for(int i = 0; i < numeroEspacios; i++){
            espacios = espacios + " ";
          }
          lcd.setCursor(0, linea);
          lcd.print(texto + espacios);
          textoLinea[linea] = texto;
        }
      }
    }

    //"parpadea" el texto
    void flash(String texto, int linea){
      if(!tiempoFlashLargo.running() && !tiempoFlashCorto.running()){
        tiempoFlashLargo.start();
        mostrarTexto(texto, linea);
      }
      if(tiempoFlashLargo.expired()){
        tiempoFlashLargo.stop();
        tiempoFlashCorto.start();
        mostrarTexto(" ", linea);
      }
      if(tiempoFlashCorto.expired()){
        tiempoFlashCorto.stop();
        tiempoFlashLargo.start();
        mostrarTexto(texto, linea);
      }
    }

    void alternar(String texto1, String texto2, int linea){
      if(!tiempoAlternar1.running() && !tiempoAlternar2.running()){
        tiempoAlternar1.start();
        mostrarTexto(texto1, linea);
      }
      if(tiempoAlternar1.expired()){
        tiempoAlternar1.stop();
        tiempoAlternar2.start();
        mostrarTexto(texto2, linea);
      }
      if(tiempoAlternar2.expired()){
        tiempoAlternar2.stop();
        tiempoAlternar1.start();
        mostrarTexto(texto1, linea);
      }
    }

    /*
    Alineacion
    0 - alinea el texto a la izquierda
    1 - alinea al centro
    2 - alinea a la derecha
    */
    void alinear(String texto, int linea, int alineacion){
      switch (alineacion)
      {
      case 0:    //justificado a la izquierda (comportamiento normal)
        mostrarTexto(texto, linea);
        break;
      case 1:    //centrado
        if(texto.length() >= (unsigned int)caracteres){
          mostrarTexto(texto, linea);
        }else{
          int espaciosEnBlanco = caracteres - texto.length();
          int espaciosAlInicio = espaciosEnBlanco/2;
          mostrarTexto(texto, linea, espaciosAlInicio);
        }
        break;
      case 2:    //a la derecha
        if(texto.length() >= (unsigned int)caracteres){
          mostrarTexto(texto, linea);
        }else{
          int espaciosAlInicio = caracteres - texto.length();
          mostrarTexto(texto, linea, espaciosAlInicio);
        }
      
      default:
        break;
      }
    }

    void extremos(String texto1, String texto2, int linea){
      if(texto1.length() + texto2.length() > (unsigned int)(caracteres - 1)){    //-1 asegura por lo menos un espacio en blanco
        alternar(texto1, texto2, linea);
      }else{
        int espaciosEnBlanco = caracteres - texto1.length() - texto2.length();
        String texto = texto1;
        for(int i = 0; i < espaciosEnBlanco; i++){
          texto = texto + ' ';
        }
        texto = texto + texto2;
        mostrarTexto(texto, linea);
      }
    }

    void ajustarTiempoIntercambio(int t){
      tiempoInt = t;
      tiempoAlternar1.limitMillis(t);
      tiempoAlternar2.limitMillis(t);
    }
};
LCDI2C pantalla;

//*******************************************************************
//no se implementara antirebote por software porque se usara hardware para resolver ese problema
class EntradasDigitales{
  private:
    byte pin;

  public:
    //constructor
    EntradasDigitales(byte pin) {
      this->pin = pin;
    }

    void setup(){
      pinMode(pin, INPUT);
    }

    int estado(){
      return digitalRead(pin);
    }
};

//*******************************************************************
class ShiftRegisters{
  private:
    /* data */
    const int latch = 4;
    const int clk = 5;
    const int dataIn = 6;
    const int dataOut = 7;

  public:
    byte entradas = 0;
    byte salidas = 0;
    byte cuartos, luzIzq, luzDer, bajas, altas;

    void setup(){
      pinMode(latch, OUTPUT);
      pinMode(clk, OUTPUT);
      pinMode(dataOut, OUTPUT);
      pinMode(dataIn, INPUT);

      actualizar(LSBFIRST);
    }

    void actualizar(uint8_t bitOrder){
      
      uint8_t i;

      //cargar datos al registro de los Shift registers
      digitalWrite(latch, LOW);
      //para el correcto funcionamiento del shiftIn
      digitalWrite(clk, LOW);

      for (i = 0; i < 8; i++)  {
        //shiftOut
        if (bitOrder == LSBFIRST)
          digitalWrite(dataOut, !!(salidas & (1 << i)));
        else	
          digitalWrite(dataOut, !!(salidas & (1 << (7 - i))));
          
        digitalWrite(clk, HIGH);
        digitalWrite(clk, LOW);		
      }
      digitalWrite(clk, LOW);
      digitalWrite(latch, HIGH);
      delayMicroseconds(5);
      entradas = 0;
      for (i = 0; i < 8; i++)  {
        digitalWrite(clk, HIGH);
        //shiftIn
        if (bitOrder == LSBFIRST)
          entradas |= digitalRead(dataIn) << i;
        else
          entradas |= digitalRead(dataIn) << (7 - i);
        digitalWrite(clk, LOW);
      }

      luzDer = bitRead(entradas, 1);
      luzIzq = bitRead(entradas, 2);
      bajas = bitRead(entradas, 3);
      altas = bitRead(entradas, 4);
      cuartos = bitRead(entradas, 5);
    }

    void encender(int pin){
      bitSet(salidas, pin);
      actualizar(1);
    }

    void apagar(int pin){
      bitClear(salidas, pin);
      actualizar(1);
    }

    int estado(int pin){
      int i = bitRead(entradas, pin);
      return i;
    }
};
ShiftRegisters sr;

//*************************************************************
class BarraDeEstado{
  private:
    //Faro
    byte faroBajo[8] = {
      0b00000,
      0b00111,
      0b01111,
      0b11111,
      0b11111,
      0b01111,
      0b00111,
      0b00000
    };
    byte faroAlto[8] = {
      0b00000,
      0b01000,
      0b10000,
      0b00000,
      0b11111,
      0b00000,
      0b10000,
      0b01000
    };

    //luz derecha
    byte luzDerecha[8] = {
      0b00000,
      0b01000,
      0b01100,
      0b11110,
      0b11111,
      0b11110,
      0b01100,
      0b01000
    };

    //luz izquierda
    byte luzIzquierda[8] = {
      0b00000,
      0b00010,
      0b00110,
      0b01111,
      0b11111,
      0b01111,
      0b00110,
      0b00010
      };

    byte barra[8] = {
      0b00000,
      0b00000,
      0b00000,
      0b11111,
      0b11111,
      0b11111,
      0b00000,
      0b00000
    };
  //Para cuartos usar P o C
  byte entradasAnterior = 0;

  public:
    void setup(){
      lcd.createChar(3, faroBajo);
      lcd.createChar(4, faroAlto);
      lcd.createChar(5, luzDerecha);
      lcd.createChar(6, luzIzquierda);
      lcd.createChar(7, barra);
    }

    void mostrar(){
      //logica para mostrar iconos
      
      if(sr.entradas != entradasAnterior){
        entradasAnterior = sr.entradas;

        //flecha derecha
        lcd.setCursor(18,0);
        if(sr.luzDer){
          lcd.write(5);
          lcd.write(5);
        }else{
          lcd.print("  ");
        }

        //flecha izquierda
        lcd.setCursor(0,0);
        if(sr.luzIzq){
          lcd.write(6);
          lcd.write(6);
        }else{
          lcd.print("  ");
        }

        //cuartos
        lcd.setCursor(6,0);
        if(sr.cuartos){
          lcd.print("C");
        }else{
          lcd.print(" ");
        }

        //bajas
        lcd.setCursor(12,0);
        if(sr.bajas){
          lcd.write(3);
        }else{
          lcd.print(" ");
        }
        
        //altas
        lcd.setCursor(13,0);
        if(sr.altas){
          lcd.write(4);
        }
        else{
          lcd.print(" ");
        }
      }
    }
};
BarraDeEstado barra;

class Combustible{
  private:
    byte bat1[8];
    byte bat2[8];
    byte bat3[8];
    int nivelAnterior = 0;
    int sensGas = A0;
    int lectura = 0;

    void icono(){
    //crear los caracteres
    int n = nivelGas*13/100;    //13 pasos

    if(n <= 4){
      //caracter 1 variable
      byte val;
      switch (n){
        case 0:
          val = B10000;
          break;
        case 1:
          val = B11000;
          break;
        case 2:
          val = B11100;
          break;
        case 3:
          val = B11110;
          break;
        case 4:
          val = B11111;
          break;
      }

      bat1[0] = 31;
      for(int i = 1; i < 7; i++){
        bat1[i] = val;
      }
      bat1[7] = 31;

      //caracter 2 y 3 vacios
      bat2[0] = 31;
      bat3[0] = 31;
      for(int i = 1; i < 7; i++){
        bat2[i] = 0;
        bat3[i] = 1;
      }
      bat2[7] = 31;
      bat3[7] = 31;
    }else if(n > 4 && n <= 9){
      //caracter 1 lleno
      for(int i = 0; i < 8; i++){
        bat1[i] = 31;
      }

      //caracter 2 variable
      byte val;
      switch (n){
        case 5:
          val = B10000;
          break;
        case 6:
          val = B11000;
          break;
        case 7:
          val = B11100;
          break;
        case 8:
          val = B11110;
          break;
        case 9:
          val = B11111;
          break;
      }
      bat2[0] = 31;
      for(int i = 1; i < 7; i++){
        bat2[i] = val;
      }
      bat2[7] = 31;

      //caracter 3 vacio
      bat3[0] = 31;
      for(int i = 1; i < 7; i++){
        bat3[i] = 1;
      }
      bat3[7] = 31;
    }else if(n > 9 && n <= 13){
      //caracter 1 y 2 lleno
      for(int i = 0; i < 8; i++){
        bat1[i] = 31;
        bat2[i] = 31;
      }

      //caracter 3 variable
      byte val;
      switch (n){
        case 10:
          val = B10001;
          break;
        case 11:
          val = B11001;
          break;
        case 12:
          val = B11101;
          break;
        case 13:
          val = B11111;
          break;
      }
      bat3[0] = 31;
      for(int i = 1; i < 7; i++){
        bat3[i] = val;
      }
      bat3[7] = 31;
    }else{
      //error
    }
  }

  public:
    int nivelGas = 0;

    //nivel entre 0 y 100
    void nivel(){
      lectura = analogRead(sensGas);
      float nivelF = .0977 * lectura;    //.0977 = 100/1023

      //correccion
      //lectura = lectura - 500;
      //float nivelF = .666 * lectura;    //.666 = 100/150
      nivelGas = (int)nivelF;
    }

    void mostrar(int linea){
      //actualizar el nivel
      nivel();

      //actualizar el icono solo si ha cambiado
      if(nivelGas != nivelAnterior){
        icono();
        nivelAnterior = nivelGas;
        lcd.createChar(0, bat1);
        lcd.createChar(1, bat2);
        lcd.createChar(2, bat3);

        //mostrar el icono
        lcd.setCursor(0, linea);
        lcd.write(0);
        lcd.write(1);
        lcd.write(2);

        //mostrar el porcentaje
        lcd.print(" " + (String)nivelGas + "%");
      }
    }
    
};
Combustible combustible;

//************************************************************
RTC_DS1307 rtc;
class Tiempo{
  private:

    String diaStr, mesStr, anioStr;
    String meses[12] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    String dias[7] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    String horaYFecha, fecha;

  public:

  void setup(){
    if (!rtc.begin()) {    //no responde el RTC
      lcd.print("No hay RTC D:");
    }

    if(!rtc.isrunning()) {
      //lcd.print("RTC is NOT running!");
      // following line sets the RTC to the date & time this sketch was compiled
      //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      //rtc.adjust(DateTime(2019, 2, 26, 11, 9, 30));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    //rtc.adjust(DateTime(2021, 4, 5, 20, 55, 0));
  }
  /*
  * Regresa una string con la fecha
  * 
  * formato = 0 regresa dia/mes/año
  * formato = 1 regresa dia/mes/año(2digitos)
  * formato = 2 regresa dia/mes(con letra)/año
  * formato = 3 regresa dia/mes(con letra)/año(2 digitos)
  * formato = 4 regresa dia/mes
  * formato = 5 regresa dia/mes (con letra)
  * formato = 6 regresa mes/dia, año
  * formato = 7 regresa mes(con letra)/dia, año
  * formato = 8 regresa mes/dia, año(2 digitos)
  * formato = 9 regresa mes(con letra)/dia, año(2 digitos)
  * formato = 10 regresa mes/dia
  * formato = 11 regresa mes(con letra)/año
  * formato = 12 regresa dia(letra) dia/mes
  * formato = 13 regresa dia(letra) mes/dia
  */
  String Fecha(int formato){
    DateTime ahora = rtc.now();

    diaStr = (String) ahora.day();
    if(ahora.day() < 10){
      diaStr = '0' + diaStr;
    }

    mesStr = (String) ahora.month();
    if(ahora.month() < 10){
      mesStr = '0' + mesStr;
    }

    anioStr = (String) ahora.year();

    switch(formato){
      case 0:
        fecha = diaStr + "/" + mesStr + "/" + anioStr;
        break;
      case 1:
        fecha = diaStr + "/" + mesStr + "/" + anioStr.substring(2);
        break;
      case 2:
        fecha = diaStr + "/" + meses[ahora.month() - 1] + "/" + anioStr;
        break;
      case 3:
        fecha = diaStr + "/" + meses[ahora.month() - 1] + "/" + anioStr.substring(2);
        break;
      case 4:
        fecha = diaStr + "/" + mesStr;
        break;
      case 5:
        fecha = diaStr + "/" + meses[ahora.month() - 1];
        break;
      case 6:
        fecha = mesStr + "/" + diaStr +", "+ anioStr;
        break;
      case 7:
        fecha = meses[ahora.month() - 1] + "/" + diaStr +", "+ anioStr;
        break;
      case 8:
        fecha = mesStr + "/" + diaStr +", "+ anioStr.substring(2);
        break;
      case 9:
        fecha = meses[ahora.month() - 1] + "/" + diaStr +", "+ anioStr.substring(2);
        break;
      case 10:
        fecha = mesStr + "/" + diaStr;
        break;
      case 11:
        fecha = meses[ahora.month() - 1] + "/" + diaStr;
        break;
      case 12:
        fecha = dias[ahora.dayOfTheWeek()] + ' ' + diaStr + '/' + mesStr;
        break;
      case 13:
        fecha = dias[ahora.dayOfTheWeek()] + ',' + meses[ahora.month() - 1] + "/" + diaStr;
        break;
    }
    return fecha;
  }

  /*
  * Regresa la hora en formato 24 hrs
  */
  String Hora(int formato){
    //variables locales
    String horaStr, minStr, segStr, hora;
    DateTime ahora = rtc.now();

    horaStr = (String)ahora.hour();
    minStr = (String)ahora.minute();
    segStr = (String)ahora.second();

    switch (formato){
      case 0:    //24 horas sin segundos
        if(ahora.minute() < 10){
        minStr = '0' + minStr;
        }
        if(ahora.hour() < 10){
        horaStr = '0' + horaStr;
        }
        hora = horaStr + ":" + minStr;
        break;
      
      case 1:    //24 hrs con segundos
        if(ahora.minute() < 10){
          minStr = '0' + minStr;
        }
        if(ahora.hour() < 10){
          horaStr = '0' + horaStr;
        }
        if(ahora.second() < 10){
          segStr = '0' + segStr;
        }
        hora = horaStr + ":" + minStr + ":" + segStr;
        break;
      
      default:
        //error
        break;
    }
    return hora;
    
  }

};
Tiempo tiempo;

volatile int t0 = 0;
volatile int deltat;
void delta(){
  unsigned long t = millis();
  deltat = t - t0;
  t0 = t;
}

volatile int cont = 0;
void incrementar(){
  cont++;
}

Chrono timerVelocimetro;
class Velocimetro{
  private:
    float circunferenciaLlanta = 1.34;
    float frecuencia = 0.0;
    float aux = circunferenciaLlanta/2.0;

    Chrono timerDeltat;

  public:
    //variable[0] en mts, variable[1] en kms
    float distancia[2], velocidad[2], aceleracion[2];

    void calcularRapido(){
      //vel = frecuencia * circunferencia (m/s)
      if(timerVelocimetro.hasPassed(500)){
        timerVelocimetro.restart();
        frecuencia = cont/2.0;    //cont * 2(1/2 seg) / 4 (4 pulsos/rev)
        cont = 0;
        //x = velocidad * t + c
        distancia[0] = distancia[0] + frecuencia * aux;    //en mts
        distancia[1] = distancia[0]/1000;    //en km
        velocidad[0] = frecuencia * circunferenciaLlanta;    //en m/s
        velocidad[1] = velocidad[0] * 3.6;    //en km/h
      }
    }

    void calculaPreciso(){
      velocidad[0] = (1000/deltat) * circunferenciaLlanta;    //en m/
      velocidad[1] = velocidad[0] * 3.6;    //en km/h
    }
};
Velocimetro velocimetro;

float T = 0.1;
float frecuenciaDeCorte = 0.5;
float Vout[2] = {0.0, 0.0};

float RC = 1.0/(2.0*PI*frecuenciaDeCorte);
float const1 = T/(T+RC);
float const2 = RC/(T+RC);

void filtroRC(int Vin){
  Vout[1] = const1*Vin + const2*Vout[0];
  Vout[0] = Vout[1];
}

const int sensInd = 2;
Chrono timerSistema;
class Sistema{
  private:

  public:
    void setup(){
      sr.setup();    //para apagar las salidas primero
      rfid.setup();
      pantalla.setup();    //inicializar la lcd
      tiempo.setup();    //inicalizar el RTC
      barra.setup();    //iniciar la barra de estado

      //activar la interrupcion para el velocimetro
      pinMode(sensInd, INPUT);
      attachInterrupt(digitalPinToInterrupt(sensInd), incrementar, RISING);
    }

    void loop(){
      //ejecutar cada 100ms
      if(timerSistema.hasPassed(100)){
        timerSistema.restart();
        //actualizar entradas y salidas
        sr.actualizar(1);

        //actualizar el nivel de gas
        combustible.nivel();

        //aplicar el filtro RC
        filtroRC(combustible.nivelGas);
        combustible.nivelGas = (int)Vout[1];

      }
      //actualizar la velocidad
      velocimetro.calcularRapido();
    }

    void memoria(){
      //usar (int)distancia[1] para que sea en km y solo la parte entera
      int distKm = (int)velocimetro.distancia[1];
    }
};
Sistema sistema;

Chrono timerUI;
class InterfazDeUsuario{
  private:

  public:
    void setup(){
      //Texto mamador de inicio
      pantalla.mostrarTexto("MotoUI v1.0", 1);
      pantalla.mostrarTexto("(C)vladeex software", 2);
      delay(1000);

      pantalla.mostrarTexto("Iniciemos!", 3);
      delay(500);
      //while(!rfid.validar());

      //limpiar la pantalla
      lcd.clear();
    }

  void loop(){
    //mostrar la barra de estado
    barra.mostrar();

    if(timerUI.hasPassed(750)){
      timerUI.restart();

      //mostrar el velocimetro
      pantalla.extremos((String)velocimetro.velocidad[1] + "km/h", (String)velocimetro.distancia[1] + "km", 1);

      //mostrar el combustible
      if(combustible.nivelGas < 10){
        pantalla.flash("Gas: Reserva!", 2);
      }else{
        pantalla.mostrarTexto("Gas: " + (String)combustible.nivelGas + "%  ", 2);
      }

      //mostrar la hora y fecha
      pantalla.extremos(tiempo.Fecha(3), tiempo.Hora(0), 3);
    }
  }
};
InterfazDeUsuario ui;

void setup() {
  //iniciar sistema
  sistema.setup();
  //pantalla de inicio
  ui.setup();
}

void loop() {
  sistema.loop();
  ui.loop();
}