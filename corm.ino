#include <GyverTimers.h>

#define PERIOD 1 //в минутах
#define FREQ 1 //частота

volatile int multiplier;
volatile int feeding_Period = 5; //время кормления секундах - зависит от размера кормушки
volatile int count_TimerA = 0;
volatile int count_TimerB = 0;
// volatile int phase=1;

void setup() {
  Serial.begin(9600);

  // phase = PERIOD / FREQ;
  // phase = TIMER / phase;
  multiplier = 60;
  // multiplier = 3600 * FREQ;
  count_TimerB = multiplier * PERIOD - feeding_Period * FREQ;
  Timer1.setFrequencyFloat(FREQ);
  Timer1.enableISR(CHANNEL_A);
  Timer1.enableISR(CHANNEL_B);
  // Timer1.phaseShift(CHANNEL_B, phase);

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(0, btnIsr, FALLING);
  pinMode(13, OUTPUT); 
  pinMode(8, OUTPUT); 
  //digitalWrite(11, HIGH);

}


void loop() {
  //wifi для esp
}


void btnIsr() {
  //digitalWrite(13, HIGH);
  count_TimerA=multiplier*PERIOD-1;
  count_TimerB=multiplier*PERIOD-1-feeding_Period * FREQ;  // + нажатие
}


//таймер включения кормления
ISR(TIMER1_A) {
  if (count_TimerA++ >= multiplier*PERIOD)
  {
    digitalWrite(13, HIGH); // заменить на функцию включения кормления
    count_TimerA=0;
  }
}


//таймер выключения кормления
ISR(TIMER1_B) {
  if (count_TimerB++ >= multiplier*PERIOD)
  {
    digitalWrite(13, LOW); //заменить на функцию отключения кормления
    count_TimerB=0;
  }
}

