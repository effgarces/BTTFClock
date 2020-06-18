#include <Arduino.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <JQ6500_Serial.h>
#include <TM1637Display.h>
#include <Wire.h>
#include "RTClib.h"

// Definição de pinos para os displays
#define CLK 2
#define DIO1 3
#define DIO2 4
#define DIO3 5
#define DIO4 6
#define DIO5 7
#define DIO6 8
#define DIO7 9
#define DIO8 10
#define DIO9 11

//definição de pinos para o shift register que controla os leds AM/PM
#define CLKPIN 13
#define LATCHPIN A0
#define DATAPIN A1

//Pino do botão de efeitos sonoros
#define REDBUTTON 12

//Define endereço de EEPROM para volume
#define addr 0

//variavel que contém que leds AM/PM devem acender e apgar
byte leds = 0;

//variavel que recebe comandos
String command = "";

//declara um rtc do tipo ds1307 (obrigatorio porta A4 e A5 | SCA e SCL)
RTC_DS1307 rtc;

//declara um objeto do tipo jq6500 para controlar os efeitos sonoros
JQ6500_Serial mp3(A3,A2);

//define o volume
int volume = 0;

//variaveis para o temporizador, provavelmente pode ser otimizado
unsigned long startTime = 0;
unsigned long elapsedTime = 0;
unsigned long previousTime = 0;

//array com todos os displays
TM1637Display display[] = {TM1637Display(CLK, DIO1), TM1637Display(CLK, DIO2), TM1637Display(CLK, DIO3), TM1637Display(CLK, DIO4), TM1637Display(CLK, DIO5), TM1637Display(CLK, DIO6), TM1637Display(CLK, DIO7), TM1637Display(CLK, DIO8), TM1637Display(CLK, DIO9)};

void setup()
{
  //inicia a porta de serie
  Serial.begin (9600);

  // inicia o relógio
  rtc.begin();

  //inicializa os pinos do shift register
  pinMode(LATCHPIN, OUTPUT);
  pinMode(DATAPIN, OUTPUT);
  pinMode(CLKPIN, OUTPUT);
  digitalWrite(LATCHPIN, LOW);

  updateShiftRegister();
  
  //inicializa o pino do notão
  pinMode(REDBUTTON, INPUT_PULLUP);

  //inicializa a comunicação com o gerador de efeitos sonoros e define um volume
  volume = EEPROM.read(addr);
  mp3.begin(9600);
  mp3.reset();
  mp3.setVolume(volume);

  //define o brilho de todos os displays
  for(int k = 0; k < 9; k++) {
      display[k].setBrightness(0x0f);
  }
  //inicializa a função random
  randomSeed(rtc.now().unixtime());
}

void loop()
{
  //lê um novo comando enviado a partir do esp8266
  if (Serial.available()){
    command = Serial.readString();
  }
  //Não havendo nenhum e se o botão estiver pressionado gera um efeito sonoro aleatorio
  if(command == "" && digitalRead(REDBUTTON) != HIGH){
      command = "S" + String(random(4) + 1);
  }
  //interpreta os comandos enviados, neste momento apenas dois T e S
  if(command[0] == 'T'){
    //comando T, define uma nova data/hora os diferentes argumentos são separados e por fim o rtc ajustado
    int cyear = getValue(command, '|', 3).toInt();
    int cmonth = getValue(command, '|', 1).toInt();
    int cday = getValue(command, '|', 2).toInt();
    int chour = getValue(command, '|', 4).toInt();
    int cminutes = getValue(command, '|', 5).toInt();
    int cseconds = getValue(command, '|', 6).toInt();
    rtc.adjust(DateTime(cyear, cmonth, cday, chour , cminutes, cseconds));
  } else if(command[0] == 'V') {
    if(command[1] == '1') {
      mp3.setVolume(10);
      EEPROM.write(addr, 10);
    } else if (command[1] == '2') {
      mp3.setVolume(15);
      EEPROM.write(addr, 15);
    } else if (command[1] == '3') {
      mp3.setVolume(20);
      EEPROM.write(addr, 20);
    } else if (command[1] == '4') {
      mp3.setVolume(25);
      EEPROM.write(addr, 25);
    } else if (command[1] == '5') {
      mp3.setVolume(30);
      EEPROM.write(addr, 30);
    }
  } else if(command[0] == 'S') {
    // se o comando for efeito sonoro e o temporizador não estiver a correr toca o efeito escolhido
    if(command[1] == '1') {
      mp3.playFileByIndexNumber(1);
    } else if(command[1] == '2') {
      mp3.playFileByIndexNumber(2);
    }else if(command[1] == '3') {
      mp3.playFileByIndexNumber(3);
    }else if(command[1] == '4') {
      mp3.playFileByIndexNumber(4);
    }else if(command[1] == '5') {
      mp3.playFileByIndexNumber(5);
    }
    //inicia o temporizador
    startTime = millis();
    previousTime = 0;
  }
  //verifica se existe um temporizador a corre e depois verifica o tempo passado e quando chegar os 10s, desativa o temporizador
  if(startTime != 0) {
    if(elapsedTime == 10) {
      elapsedTime = 0;
      startTime = 0;
      previousTime = 0;
      } else {
        elapsedTime = (millis() - startTime)/1000;
        if(previousTime != elapsedTime) {
          previousTime = elapsedTime;
        }
      }
  }
  //limpa os comandos
  command = "";
  //obtém a data/hora atual
  DateTime now = rtc.now();
  //coloca a data 09/26 1985 09:00 na primera fila de displays e para 11/20 1955 06:38 na terceira fila
  display[0].showNumberDecEx(926, 0b01000000, true);
  display[1].showNumberDec(1985,true);
  display[6].showNumberDecEx(1120, 0b01000000, true);
  display[7].showNumberDec(1955,true);
  //apenas pisca os dois pontos quando os segundos não são pares
  
  if(now.second() % 2){
    display[2].showNumberDecEx(900, 0b01000000, true); 
    display[8].showNumberDecEx(638, 0b01000000, true);
  } else {
    display[2].showNumberDec(900, true);
    display[8].showNumberDec(638, true);
  }
  // se o temporizador estiver a decorrer muda a data/hora dos displays centrais para a data/hora 09/21 2015 07:28
  if(startTime != 0){
    display[3].showNumberDecEx(921, 0b01000000, true);
    display[4].showNumberDec(2015,true);
    if(now.second() % 2){
      display[5].showNumberDecEx(728, 0b01000000, true); 
    } else {
      display[5].showNumberDec(728, true);
    }
  } else {
    // se não houver temporizador a correr, mostra a data/hora do rtc
    display[3].showNumberDec(now.day(), true, 2, 2);
    display[3].showNumberDecEx(now.month(), 0b01000000, true, 2, 0);
    display[4].showNumberDec(now.year(),true);
    //converte as horas para formato de AM/PM
      if (now.hour() > 12){
        if(now.second() % 2){
          //apenas pisca os dois pontos quando os segundos não são pares
          display[5].showNumberDecEx(now.hour() - 12, 0b01000000, true, 2, 0);
        } else {
          //coloca as horas no display
          display[5].showNumberDec(now.hour() - 12, true, 2, 0);
        }
      } else {
        if(now.second() % 2){
          //apenas pisca os dois pontos quando os segundos não são pares
          display[5].showNumberDecEx(now.hour(), 0b01000000, true, 2, 0);
        } else {
          //coloca as horas no display
          display[5].showNumberDec(now.hour(), true, 2, 0);
        }
      }
      //coloca is minutos no display
      display[5].showNumberDec(now.minute(), true, 2, 2);
  }

  //liga os leds AM/PM corretos, de acordo com a hora
  leds = 0;
  updateShiftRegister();
  bitSet(leds, 1);
  if (now.hour() >= 12) {
    bitSet(leds, 4);
  } else {
    bitSet(leds, 3);
  }
  bitSet(leds, 5);
  updateShiftRegister();
}

//funcção auxiliar para enviar atualizar a informação enviada para o shift register
void updateShiftRegister()
{
  digitalWrite(LATCHPIN, LOW);
  shiftOut(DATAPIN, CLKPIN, LSBFIRST, leds);
  digitalWrite(LATCHPIN, HIGH);
}

//função auxiliar para separar os comandos enviados
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
