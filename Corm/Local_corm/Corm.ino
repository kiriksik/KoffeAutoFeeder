#include <GyverTimers.h>
#include <PWMServo.h>

#define PERIOD 10//в часах
#define FREQ 61 //частота

volatile int feed_speed = 180; // скорость кормления (моторчика)
volatile unsigned int multiplier;
volatile unsigned int speed;
volatile unsigned int feeding_Period = 10; // время кормления секундах - зависит от размера кормушки
volatile unsigned int count_TimerA = 0;
volatile unsigned int count_TimerB = 0;
int i = 0;
bool start = false;

// GStepper<STEPPER4WIRE> stepper(2048, 3, 5, 4, 6); //6, 3 - поменяны местами, 2048 - полушаг, дескриптор моторчика
PWMServo servo;


void setup() {
  Serial.begin(9600);

  // stepper.setRunMode(KEEP_SPEED); // режим работы
  // stepper.setMaxSpeed(feed_speed);
  // stepper.setAcceleration(2*feeding_Period*feed_speed); // 10 град в сек ускорение
  // stepper.autoPower(true);

  //wifi для esp

  // multiplier = 1;
  multiplier = 3600 / FREQ;
  count_TimerB = multiplier * PERIOD - feeding_Period / 10 * FREQ;
  Timer2.setFrequency(FREQ);
  Timer2.enableISR(CHANNEL_A);
  Timer2.enableISR(CHANNEL_B);
  
  // pinMode(3, INPUT);
  // pinMode(2, INPUT_PULLUP);
  // attachInterrupt(0, btnIsr, FALLING);
  pinMode(13, OUTPUT); 
  // pinMode(8, OUTPUT); 
  //digitalWrite(11, HIGH);

  servo.attach(9);

}


// Функция устранения засора (моторчик остановился)
// void clearBlock() {
// }


void loop() {
    if (start == true)
    {
      for (i = 0; i < feeding_Period; i += 1)
      {
        servo.write(feed_speed);
        delay (200);
        // stepper.setSpeed(FEED_SPEED); // заменить на функцию включения кормления
        if (feed_speed == 0)
          feed_speed = 180;
        else
          feed_speed = 0;
      }
      start = false;
      digitalWrite(13, LOW);
    }
    // myservo.write(180);
    // delay(100);
    // myservo.write(0);
    // delay(100);
  // if (!stepper.tick() && speed !=0) {
    // условие для устранения засора
    // stepper.setSpeed(speed);
  // }
}


// void btnIsr() {
//   count_TimerA=(int)(multiplier*PERIOD-1);
//   count_TimerB=multiplier*PERIOD-1-feeding_Period * FREQ;  // + нажатие
// }


//таймер кормления
ISR(TIMER2_A) {

  // digitalWrite(13, !digitalRead(13));

  if (count_TimerA++ >= multiplier*PERIOD) {
    digitalWrite(13, HIGH);
    start = true;
    // feed_speed = -feed_speed;
    count_TimerA = 0;
  }
}


// таймер выключения кормления
ISR(TIMER2_B) {
  if (count_TimerB++ >= multiplier*PERIOD) {
    // digitalWrite(13, LOW);
    count_TimerB = 0;
  }
}