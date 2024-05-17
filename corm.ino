#include <GyverTimers.h>
#include <GyverStepper.h>

#define PERIOD 12//в часах
#define FREQ 1 //частота

volatile int feed_speed = 300; // скорость кормления (моторчика)
volatile unsigned int multiplier;
volatile unsigned int speed;
volatile unsigned int feeding_Period = 3; // время кормления секундах - зависит от размера кормушки
volatile unsigned int count_TimerA = 0;
volatile unsigned int count_TimerB = 0;

GStepper<STEPPER4WIRE> stepper(2048, 3, 5, 4, 6); //6, 3 - поменяны местами, 2048 - полушаг, дескриптор моторчика


void setup() {
  Serial.begin(9600);

  stepper.setRunMode(KEEP_SPEED); // режим работы
  stepper.setMaxSpeed(feed_speed);
  stepper.setAcceleration(2*feeding_Period*feed_speed); // 10 град в сек ускорение
  // stepper.autoPower(true);

  //wifi для esp

  // multiplier = 1;
  multiplier = 3600 / FREQ;
  count_TimerB = multiplier * PERIOD - feeding_Period * FREQ;
  Timer1.setFrequencyFloat(FREQ);
  Timer1.enableISR(CHANNEL_A);
  Timer1.enableISR(CHANNEL_B);
  
  pinMode(3, INPUT);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(0, btnIsr, FALLING);
  pinMode(13, OUTPUT); 
  pinMode(8, OUTPUT); 
  //digitalWrite(11, HIGH);

}


// Функция устранения засора (моторчик остановился)
void clearBlock() {
}


void loop() {
  if (!stepper.tick() && speed !=0) {
    // условие для устранения засора
    stepper.setSpeed(speed);
  }
}


void btnIsr() {
  count_TimerA=(int)(multiplier*PERIOD-1);
  count_TimerB=multiplier*PERIOD-1-feeding_Period * FREQ;  // + нажатие
}


//таймер кормления
ISR(TIMER1_A) {
  if (count_TimerA++ >= multiplier*PERIOD) {
    digitalWrite(13, HIGH);
    // stepper.setSpeed(FEED_SPEED); // заменить на функцию включения кормления
    speed = feed_speed;
    // feed_speed = -feed_speed;
    count_TimerA = 0;
  }
}


// таймер выключения кормления
ISR(TIMER1_B) {
  if (count_TimerB++ >= multiplier*PERIOD) {
    speed = 0;
    stepper.brake();
    digitalWrite(13, LOW);
    count_TimerB = 0;
  }
}
