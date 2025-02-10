#include <Arduino.h>
#include <math.h>

// ================== Пины и настройки ==================
const int buttonPin = 13;      // пин кнопки (с внутренней подтяжкой)
const int redPin    = 6;       // пин красного канала светодиода
const int bluePin   = 3;       // пин синего канала светодиода
const int pwmPin    = 9;       // пин для управления MOSFET и генерации сигнала

const unsigned long debounceDelay = 50; // задержка для антидребезга (мс)

// ================== Переменные для кнопки ==================
int buttonState = HIGH;        // текущее состояние кнопки
int lastButtonState = HIGH;    // предыдущее состояние кнопки
unsigned long lastDebounceTime = 0; // время для антидребезга
bool isRed = false;            // состояние системы: false – выключена (синий свет), true – включена (красный свет)
bool firstPressIgnored = false; // первый клик только активирует систему (без переключения)

// ================== Параметры генерации сигнала ==================
float frequency = 50.0; // начальная частота сигнала (Гц)
int signalType = 0;     // тип сигнала: 0 – прямоугольный, 1 – пилообразный, 2 – синусоидальный

// Для синусоидального сигнала
const int resolution = 360;  // число шагов в одном периоде синусоиды
int sineTable[resolution];   // таблица значений синусоиды

// ================== Функция установки цвета светодиода ==================
void setColor(bool red) {
  /*
   * При использовании общего катода:
   * - HIGH включает канал
   * - LOW выключает
   */
  if (red) {
    analogWrite(redPin, 255);  // красный – максимальная яркость
    analogWrite(bluePin, 0);    // синий выключен
  } else {
    analogWrite(redPin, 0);     // красный выключен
    analogWrite(bluePin, 128);  // синий – половинная яркость
  }
}

// ================== Функции генерации сигналов ==================

// Прямоугольный сигнал
void generateSquareWave() {
  unsigned long period = 1000000UL / frequency; // период сигнала в микросекундах
  unsigned long halfPeriod = period / 2;
  
  digitalWrite(pwmPin, HIGH);
  delayMicroseconds(halfPeriod);
  if (!isRed) return;  // если во время генерации система выключили, выходим
  digitalWrite(pwmPin, LOW);
  delayMicroseconds(halfPeriod);
}

// Пилообразный сигнал
void generateSawtoothWave() {
  unsigned long stepDelay = 1000000UL / (frequency * 256); // задержка на шаг
  for (int i = 0; i < 256; i++) {
    if (!isRed) break;  // проверка состояния системы
    analogWrite(pwmPin, i);
    delayMicroseconds(stepDelay);
  }
}

// Синусоидальный сигнал
void generateSineWave() {
  unsigned long stepDelay = 1000000UL / (frequency * resolution);
  for (int i = 0; i < resolution; i++) {
    if (!isRed) break;
    analogWrite(pwmPin, sineTable[i]);
    delayMicroseconds(stepDelay);
  }
}

// ================== setup() ==================
void setup() {
  // Инициализация таблицы синусоиды
  for (int i = 0; i < resolution; i++) {
    // Функция radians переводит градусы в радианы, sin() возвращает значение от -1 до 1.
    sineTable[i] = (int)((sin(radians(i)) + 1.0) * 127.5); // преобразуем в диапазон [0, 255]
  }

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(pwmPin, OUTPUT);

  Serial.begin(9600);
  Serial.println("Система запущена");
  Serial.println("Доступные команды:");
  Serial.println("  0 - Прямоугольный сигнал");
  Serial.println("  1 - Пилообразный сигнал");
  Serial.println("  2 - Синусоидальный сигнал");
  Serial.println("  Для изменения частоты введите: f<значение> (например, f100 для 100 Гц)");

  // Устанавливаем начальное состояние: система выключена (синий свет) и сигнал на пине 9 отсутствует
  setColor(isRed);
  digitalWrite(pwmPin, LOW);
  Serial.println("Начальный цвет: Синий (Система выключена)");
}

// ================== loop() ==================
void loop() {
  // ===== Обработка кнопки =====
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { // кнопка нажата
        if (!firstPressIgnored) {
          firstPressIgnored = true;
          Serial.println("Система активирована. Готов к переключениям!");
        } else {
          // Переключаем состояние системы
          isRed = !isRed;
          setColor(isRed);
          if (isRed) {
            Serial.println("Система включена. MOSFET активирован.");
          } else {
            Serial.println("Система выключена. MOSFET деактивирован.");
            digitalWrite(pwmPin, LOW); // сразу отключаем сигнал
          }
        }
      }
    }
  }
  lastButtonState = reading;

  // ===== Обработка команд через Serial =====
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      char firstChar = input.charAt(0);
      if (firstChar == '0' || firstChar == '1' || firstChar == '2') {
        signalType = firstChar - '0';
        Serial.print("Выбран сигнал: ");
        if (signalType == 0) Serial.println("Прямоугольный");
        else if (signalType == 1) Serial.println("Пилообразный");
        else if (signalType == 2) Serial.println("Синусоидальный");
      }
      else if (firstChar == 'f' || firstChar == 'F') {
        float newFrequency = input.substring(1).toFloat();
        if (newFrequency > 0) {
          frequency = newFrequency;
          Serial.print("Частота изменена на: ");
          Serial.print(frequency);
          Serial.println(" Гц");
        } else {
          Serial.println("Неверное значение частоты.");
        }
      }
      else {
        Serial.println("Неверная команда.");
      }
    }
  }

  // ===== Генерация сигнала (на пине 9) =====
  if (isRed) {  // если система включена
    switch (signalType) {
      case 0:
        generateSquareWave();
        break;
      case 1:
        generateSawtoothWave();
        break;
      case 2:
        generateSineWave();
        break;
      default:
        break;
    }
  }
  else {
    // Если система выключена, убеждаемся, что на пине 9 ноль.
    digitalWrite(pwmPin, LOW);
    // Короткая задержка для уменьшения нагрузки на процессор.
    delay(10);
  }
}
