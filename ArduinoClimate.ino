/* 
 * for Arduino NANO v 2.0 
 * добавлен DHT22 ванная 
 * добавлено управление Реле с пинов A0-А3 
 * изменена нумерация дискретных контактов для DHT и реле: D2-D5 для DHT, D6-D13 для реле
 *
 * D2  - DHT22 кухня-гостинная
 * D3  - DHT22 детская
 * D4  - DHT22 спальня
 * D5  - DHT22 ванная
 * 
 * D6  - теплый пол (кухня)
 * D7  - теплый пол (гостинная)
 * D8  - теплый пол (детская)
 * D9  - теплый пол (спальня)
 * D10 - теплый пол (ванная, туалет, коридор)
 * 
 * D11 - батареи кухня-гостинная
 * D12 - батареи детская
 * D13 - батареи спальня
 * 
 * A0  - управление котлом (сухие контакты)
 * A1  - пин питания датчиков DHT22 
 * A2  - реле насоса теплого пола
 * A3  - реле насоса ГВС
 * 
 * A4, A5 - i2c slave mode
 * 
 * =================================================
 * 
 * v21 - 24/03/2020
 * добавлена функция перезагрузки при зависании DHT22
 * добавлен dht_bath
 * отлажен с NodeMCU в УКБП
 * 
 * v22 - 30/03/2020
 * исправлено управление аналоговым выходом D6 для управления питанием dht22
 * 
 * v23 - 08/04/2020
 * раскомментированы функции для датчика DTH bath
 */


#include <Wire.h>
#include "DHT.h"                                
DHT dht_kit(2, DHT22); 
DHT dht_det(3, DHT22);
DHT dht_bed(4, DHT22);
DHT dht_bath(5, DHT22);

byte OutState[13]; // 14 цифр
bool flag_receive_data = 0;

int temp1, temp2, temp3, temp4, hum1, hum2, hum3, hum4;
byte isp = 0, isp_1, isp_10;
byte err1, err2, err3, err4;

const byte ERR_LIMIT = 20;
const int  DHT_READ_PERIOD = 5*1000;
unsigned long Last_DHT_read_time;

char calc_text[25];
int calc_data[25];

//==========================================================================

void setup() {
  // настройка COM-порта
  Serial.begin(9600); 
  Serial.println("Arduino NANO has been started (My climate v 2.1)");

  // настройка DHT
  dht_kit.begin();  
  dht_det.begin(); 
  dht_bed.begin();
  dht_bath.begin();
  
  // настройка i2c
  Wire.begin(8);                
  Wire.onReceive(get_data);
  Wire.onRequest(send_request_data);

  // настройка дискретных пинов управления реле
  for (byte pin = 6; pin < 14; pin++) { 
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  // настройка аналоговых пинов как дискрентные
  pinMode(A0, OUTPUT); digitalWrite(A0, LOW);
  pinMode(A1, OUTPUT); digitalWrite(A1, HIGH); //питание DHT22
  pinMode(A2, OUTPUT); digitalWrite(A2, LOW);
  pinMode(A3, OUTPUT); digitalWrite(A3, LOW);
}

//==========================================================================

void loop() {
  delay(1);
  if (flag_receive_data) { 
    flag_receive_data = false;
   
    for (byte out = 0; out <= 7; out++)  {       
      digitalWrite(out+6, OutState[out]);         
    }
    digitalWrite(A0, OutState[8]);  
    digitalWrite(A2, OutState[10]);
    digitalWrite(A3, OutState[11]);   
    
    Read_DHT(); 
    Last_DHT_read_time = millis();
    Calc_request_data();        
    DHT_Reboot();    
  }
}

//==========================================================================
// получение данных по i2c  (14 бит: 8 + 4 + 2)
void get_data(void) {   
  byte byte_counter = 0;
  flag_receive_data = 0; 
  Serial.print("Get_data: ");
  while (0 < Wire.available())  {    
    OutState[byte_counter] = Wire.read() -'0'; 
    Serial.print(OutState[byte_counter]);  
    byte_counter++;
  }  
  Serial.print(" ... received "); Serial.print(byte_counter); Serial.print(" bite");
  
  if (byte_counter == 14) {
    flag_receive_data = true; 
    Serial.println(" ... flag_receive_data = true");
  }
  else {
    flag_receive_data = false;
    Serial.println(" ... flag_receive_data = false");  
  }
}  
 

// функция передачи данных dht-датчиков в nodemcu по запросу
void send_request_data(void) {   
  char c;
  for (int i=0; i<=23; i++) {
    c = calc_data[i] + '0';  Wire.write(c);
  }
  c = isp_10 + '0'; Wire.write(c); 
  c = isp_1 + '0';  Wire.write(c);  
}


//==========================================================================
// функция чтения данных с датчика DHT22

void Read_DHT(void) {
  Serial.println("Read_DHT:"); 
  float t1 = dht_kit.readTemperature();  temp1 = t1*10; Serial.println(t1); delay(100); 
  float h1 = dht_kit.readHumidity();     hum1  = h1*10; Serial.println(h1); delay(100);
  float t2 = dht_det.readTemperature();  temp2 = t2*10; Serial.println(t2); delay(100);
  float h2 = dht_det.readHumidity();     hum2  = h2*10; Serial.println(h2); delay(100);
  float t3 = dht_bed.readTemperature();  temp3 = t3*10; Serial.println(t3); delay(100);
  float h3 = dht_bed.readHumidity();     hum3  = h3*10; Serial.println(h3); delay(100);
  float t4 = dht_bath.readTemperature(); temp4 = t4*10; Serial.println(t3); delay(100);
  float h4 = dht_bath.readHumidity();    hum4  = h4*10; Serial.println(h3); delay(100);

  // считаем ошибки
  if (!temp1) err1++; else err1 = 0;
  if (!temp2) err2++; else err2 = 0;
  if (!temp3) err3++; else err3 = 0;
  if (!temp4) err4++; else err4 = 0;
}


// функция вычисления текста для передачи
void Calc_request_data (void) {
  sprintf(calc_text,"%03d%03d%03d%03d%03d%03d%03d%03d%1d%1d",temp1, hum1, temp2, hum2, temp3, hum3, temp4, hum4, isp_1, isp_10); 
  for (int i=0; i<=23; i++) { calc_data[i] = calc_text[i] - '0'; } 
  isp = isp+1; if (isp >= 100 ) { isp = 0; }
  isp_1 = isp % 10;    isp_10 = isp/10;
}

void DHT_Reboot(void) {
  if ((err1 > ERR_LIMIT) 
    || (err2 > ERR_LIMIT)
    || (err3 > ERR_LIMIT)
    || (err4 > ERR_LIMIT)
    ) {
      // сбрасываем счетчики ошибок
      err1 = 0; err2 = 0; err3 = 0; err4 = 0;
      // передергиваем питание у DHT22
      Serial.println("Rebooting DHT power...");
      digitalWrite(A1, LOW);
      delay(2000);
      digitalWrite(A1, HIGH);
      dht_kit.begin();  
      dht_det.begin(); 
      dht_bed.begin();
      dht_bath.begin();
    }      
}
