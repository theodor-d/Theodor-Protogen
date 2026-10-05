#include <MaxMatrix.h>
#include <avr/pgmspace.h>

bool eyeToggle = false;
int touchPin = 2;
#define DIN 6
#define CLK 7
#define CS 8
#define MAX_DEVICES 6

MaxMatrix m(DIN, CS, CLK, MAX_DEVICES);

byte L_eye0[8] = {
  0b11100000,
  0b11111000,
  0b11111110,
  0b00000111,
  0b00000001,
  0b00000000,
  0b00000000,
  0b00000000
};


byte L_eye1[8] = {
  0b00001111,
  0b00111111,
  0b11111111,
  0b11111110,
  0b11111100,
  0b01111000,
  0b00000000,
  0b00000000
};

byte RR_eye0[8] = {
  B11110000,
  B00011110,
  B00000011,
  B00000000,
  B00000000,
  B00000011,
  B00011110,
  B11110000
};

byte R_eye0[8] = {
 0b11110000,
 0b11111100,
 0b11111111,
 0b01111111,
 0b00111111,
 0b00011110,
 0b00000000,
 0b00000000
};

byte RRR_eye1[8] = {
  0b00011100,
  0b00111111,
  0b01111111,
  0b11111111,
  0b11111111,
  0b01111100,
  0b00000000,
  0b00000000
};
byte R_eye1[8] = {
 0b00000111,
 0b00011111,
 0b01111111,
 0b11100000,
 0b10000000,
 0b00000000,
 0b00000000,
 0b00000000
};

byte RR_eye1[8] = {
  0b00000000,
  0b00000000,
  0b11000000,
  0b01110000,
  0b01111000,
  0b11000000,
  0b00000000,
  0b00000000
};

byte RRR_eye0[8] = {
  0b00111000,
  0b11111100,
  0b11111110,
  0b11111111,
  0b11111111,
  0b00111110,
  0b00000000,
  0b00000000
};

byte nose[8] = {
 0b11111111,
 0b01111110,
 0b00111100,
 0b00011000,
 0b00000000,
 0b00000000,
 0b00000000,
 0b00000000
};

byte R_mouth0[8] = {
 0b11110000,
 0b10111000,
 0b10001110,
 0b11111111,
 0b01111111,
 0b00011111,
 0b00000000,
 0b00000000

};

byte R_mouth1[8] = {
 0b00001111,
 0b01111111,
 0b11111001,
 0b11110000,
 0b10000000,
 0b00000000,
 0b00000000,
 0b00000000
};

byte R_mouth2[8] = {
 0b00000000,
 0b11000000,
 0b11110000,
 0b01111100,
 0b00011111,
 0b00000111,
 0b00000001,
 0b00000000
};

byte R_mouth3[8] = {
 0b00000001,
 0b00000011,
 0b00001111,
 0b00011110,
 0b00111100,
 0b11111000,
 0b11100000,
 0b00000000
};

byte L_mouth3[8] = {
  0b00001111,
  0b00011101,
  0b01110001,
  0b11111111,
  0b11111110,
  0b11110000,
  0b00000000,
  0b00000000
};



byte L_mouth2[8] = {
  0b11110000,
  0b11111110,
  0b10011111,
  0b00001111,
  0b00000001,
  0b00000000,
  0b00000000,
  0b00000000
};

byte L_mouth1[8] = {
  0b00000000,
  0b00000011,
  0b00001111,
  0b00111110,
  0b11110000,
  0b11000000,
  0b10000000,
  0b00000000
};


byte L_mouth0[8] = {
  0b10000000,
  0b11000000,
  0b11110000,
  0b01111000,
  0b00111100,
  0b00011111,
  0b00000111,
  0b00000000
};

byte R_blink_eye1[8] = {
  B00000000,
  B00001111,
  B00111111,
  B00111111,
  B00111000,
  B00000000,
  B00000000,
  B00000000
};

byte R_blink_eye0[8] = {
  B00000000,
  B11110000,
  B11111000,
  B01111100,
  B00111100,
  B00000000,
  B00000000,
  B00000000
};

byte L_blink_eye0[8] = {
  B00000000,
  B11110000,
  B11111100,
  B11111100,
  B00011100,
  B00000000,
  B00000000,
  B00000000
};

byte L_blink_eye1[8] = {
  B00000000,
  B00001111,
  B00011111,
  B00111110,
  B00111100,
  B00000000,
  B00000000,
  B00000000
};

byte O_eye01[8] = {
  B00000011,
  B00000110,
  B00001100,
  B00001000,
  B00001000,
  B00001100,
  B00000110,
  B00000011
};

byte O_eye02[8] = {
  B11000000,
  B01100000,
  B00110000,
  B00010000,
  B00010000,
  B00110000,
  B01100000,
  B11000000
};

void drawBitmap(byte bitmap[8], int moduleIndex) {
  int startCol = moduleIndex * 8;

  for (int row = 0; row < 8; row++) {
    m.setColumn(startCol + row, bitmap[row]);
  }
}


void setup() {
  m.init();
  m.setIntensity(8);
  m.clear();
  pinMode(touchPin, INPUT);

}

void loop() {

  int touchState = digitalRead(touchPin);

  drawBitmap(R_mouth0, 2);
  drawBitmap(R_mouth1, 3);
  drawBitmap(R_mouth2, 4);
  drawBitmap(R_mouth3, 5);

  if (touchState == HIGH) {
    drawBitmap(RR_eye0, 0);
    drawBitmap(RR_eye1, 1);
  } else {
    drawBitmap(R_eye0, 1);
    drawBitmap(R_eye1, 0);

    delay(3000);
    drawBitmap(R_blink_eye0,1);
    drawBitmap(R_blink_eye1,0);
    delay(250);
  }

  delay(50);
}
