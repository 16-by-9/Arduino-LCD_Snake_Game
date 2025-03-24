#include <LiquidCrystal_I2C.h>

#include <FastIO.h>
#include <I2CIO.h>
#include <LCD.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidCrystal_I2C_ByVac.h>
#include <LiquidCrystal_SI2C.h>
#include <LiquidCrystal_SR.h>
#include <LiquidCrystal_SR1W.h>
#include <LiquidCrystal_SR2W.h>
#include <LiquidCrystal_SR3W.h>
#include <SI2CIO.h>
#include <SoftI2CMaster.h>

enum DisplayItem {GRAPHIC_ITEM_NONE, GRAPHIC_ITEM_A, GRAPHIC_ITEM_B,
GRAPHIC_ITEM_NUM};
#include <LiquidCrystal_I2C.h> //Install LiquidCrystal_I2C.h
#include <stdlib.h> //Install stdlib.h
#include <limits.h> //Install limits.h
#define GRAPHIC_WIDTH 16
#define GRAPHIC_HEIGHT 4
LiquidCrystal_I2C lcd(0x27,16,2);

byte block[3] = {
B01110,
B01110,
B01110,
};
byte apple[3] = {
B00100,
B01010,
B00100,
};
#define DEBOUNCE_DURATION 20
//Return true if the actual output value is true
bool debounce_activate(unsigned long* debounceStart)
{
if(*debounceStart == 0)
*debounceStart = millis();
else if(millis()-*debounceStart>DEBOUNCE_DURATION)
return true;
return false;
}
//Return true if it's rising edge/falling edge
bool debounce_activate_edge(unsigned long* debounceStart)
{

if(*debounceStart == ULONG_MAX){
return false;
} 

else if(*debounceStart == 0){
*debounceStart = millis();
} 

else if(millis()-*debounceStart>DEBOUNCE_DURATION){
*debounceStart = ULONG_MAX;
return true;
}

return false;
}

void debounce_deactivate(unsigned long* debounceStart){
*debounceStart = 0;
}

#define BUTTON_LEFT 6 //LEFT BUTTON
#define BUTTON_RIGHT 7 // RIGHT BUTTON
unsigned long debounceCounterButtonLeft = 0;
unsigned long debounceCounterButtonRight = 0;
struct Pos {
uint8_t x=0, y=0;
};
struct Pos snakePosHistory[GRAPHIC_HEIGHT*GRAPHIC_WIDTH]; 
//first element is the head.
size_t snakeLength = 0;
enum {SNAKE_LEFT,SNAKE_UP,SNAKE_RIGHT,SNAKE_DOWN} snakeDirection;
struct Pos applePos;
unsigned long lastGameUpdateTick = 0;
unsigned long gameUpdateInterval = 1000;
bool thisFrameControlUpdated = false;
enum {GAME_MENU, GAME_PLAY, GAME_LOSE, GAME_WIN} gameState;
uint8_t graphicRam[GRAPHIC_WIDTH*2/8][GRAPHIC_HEIGHT];
void graphic_generate_characters()
{
/*
space: 0 0
0: 0 A

1: 0 B
2: A 0
3: A A
4: A B
5: B 0
6: B A
7: B B
*/
for (size_t i=0; i<8; i++) {
byte glyph[8];
int upperIcon = (i+1)/3;
int lowerIcon = (i+1)%3;
memset(glyph, 0, sizeof(glyph));
if(upperIcon==1)
memcpy(&glyph[0], &block[0], 3);
else if(upperIcon==2)
memcpy(&glyph[0], &apple[0], 3);
if(lowerIcon==1)
memcpy(&glyph[4], &block[0], 3);
else if(lowerIcon==2)
memcpy(&glyph[4], &apple[0], 3);
lcd.createChar(i, glyph);
}

delay(1); 
//Wait for the CGRAM to be written
}

void graphic_clear(){
memset(graphicRam, 0, sizeof(graphicRam));
}

void graphic_add_item(uint8_t x, uint8_t y, enum DisplayItem item) {
graphicRam[x/(8/2)][y] |= (uint8_t)item*(1<<((x%(8/2))*2));
}

void graphic_flush(){
lcd.clear();
for(size_t x=0; x<16; x++){
for(size_t y=0; y<2; y++){
enum DisplayItem upperItem =
(DisplayItem)((graphicRam[x/(8/2)][y*2]>>((x%(8/2))*2))&0x3);
enum DisplayItem lowerItem =
(DisplayItem)((graphicRam[x/(8/2)][y*2+1]>>((x%(8/2))*2))&0x3);
if(upperItem>=GRAPHIC_ITEM_NUM)
upperItem = GRAPHIC_ITEM_B;

if(lowerItem>=GRAPHIC_ITEM_NUM)
lowerItem = GRAPHIC_ITEM_B;
lcd.setCursor(x, y);
if(upperItem==0 && lowerItem==0)
lcd.write(' ');
else
lcd.write(byte((uint8_t)upperItem*3+(uint8_t)lowerItem-1));
}

}

}
void game_new_apple_pos()
{
bool validApple = true;
do {
applePos.x = rand()%GRAPHIC_WIDTH;
applePos.y = rand()%GRAPHIC_HEIGHT;
validApple = true;
for(size_t i=0; i<snakeLength; i++)
{
if(applePos.x==snakePosHistory[i].x && applePos.y==snakePosHistory[i].y){
validApple = false;
break;
}

}

} 

while(!validApple);

}
void game_init(){
 
srand(micros());
gameUpdateInterval = 1000;
gameState = GAME_PLAY;
snakePosHistory[0].x=3; snakePosHistory[0].y=1;
snakePosHistory[1].x=2; snakePosHistory[1].y=1;
snakePosHistory[2].x=1; snakePosHistory[2].y=1;
snakePosHistory[3].x=0; snakePosHistory[3].y=1;
snakeLength = 4;
snakeDirection = SNAKE_RIGHT;
game_new_apple_pos();
thisFrameControlUpdated = false;
}

void game_calculate_logic() {

if(gameState!=GAME_PLAY)
return;
//Calculate the movement of the tail
for(size_t i=snakeLength; i>=1; i--){ 
snakePosHistory[i] = snakePosHistory[i-1];
}

//Calculate the head movement
snakePosHistory[0]=snakePosHistory[1];
switch(snakeDirection){
case SNAKE_LEFT: snakePosHistory[0].x--; break;
case SNAKE_UP: snakePosHistory[0].y--; break;
case SNAKE_RIGHT: snakePosHistory[0].x++; break;
case SNAKE_DOWN: snakePosHistory[0].y++; break;
}

//Look for wall collision
if(snakePosHistory[0].x<0||snakePosHistory[0].x>=GRAPHIC_WIDTH||snakePosHistory[0].y
<0||snakePosHistory[0].y>=GRAPHIC_HEIGHT){
gameState = GAME_LOSE;
return;
}

//Look for self collision
for(size_t i=1; i<snakeLength; i++){
if(snakePosHistory[0].x==snakePosHistory[i].x &&
snakePosHistory[0].y==snakePosHistory[i].y){
gameState = GAME_LOSE;
return;
}

}
if(snakePosHistory[0].x==applePos.x && snakePosHistory[0].y==applePos.y){
snakeLength++;
gameUpdateInterval = gameUpdateInterval*9/10;
if(snakeLength>=sizeof(snakePosHistory)/sizeof(*snakePosHistory))
gameState = GAME_WIN;
else
game_new_apple_pos();
}

}
void game_calculate_display() {
  
graphic_clear();
switch(gameState){
case GAME_LOSE:
lcd.clear();
lcd.setCursor(0, 0);
lcd.print(" You lose!");
lcd.setCursor(0, 1);
lcd.print("Length: ");
lcd.setCursor(8, 1);
lcd.print(snakeLength);
break;
case GAME_WIN:
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("You won. Congratz!");
lcd.setCursor(0, 1);
lcd.print("Length: ");
lcd.setCursor(8, 1);
lcd.print(snakeLength);
break;
case GAME_PLAY:
for(size_t i=0; i<snakeLength; i++)
graphic_add_item(snakePosHistory[i].x, snakePosHistory[i].y, GRAPHIC_ITEM_A);
graphic_add_item(applePos.x, applePos.y, GRAPHIC_ITEM_B);
graphic_flush();
break;
case GAME_MENU:
//Do nothing
break;
}

}
void setup() {
lcd.begin(16,2);
lcd.backlight();
lcd.print(" Snake Game");
lcd.setCursor(0, 1);

pinMode(6, INPUT);
pinMode(7, INPUT);
graphic_generate_characters();
gameState = GAME_MENU;
}

void loop() {

lcd.setCursor(0, 0);
if(digitalRead(BUTTON_LEFT)==HIGH){
if(debounce_activate_edge(&debounceCounterButtonLeft)&&!thisFrameControlUpdated){
switch(gameState){
case GAME_PLAY:
switch(snakeDirection){
case SNAKE_LEFT: snakeDirection=SNAKE_DOWN; break;
case SNAKE_UP: snakeDirection=SNAKE_LEFT; break;
case SNAKE_RIGHT: snakeDirection=SNAKE_UP; break;
case SNAKE_DOWN: snakeDirection=SNAKE_RIGHT; break;
}

thisFrameControlUpdated = true;
break;
case GAME_MENU:
game_init();
break;
}

}

}

else{
debounce_deactivate(&debounceCounterButtonLeft);
}

lcd.setCursor(8, 0);
if(digitalRead(BUTTON_RIGHT)==HIGH){
if(debounce_activate_edge(&debounceCounterButtonRight)&&!thisFrameControlUpdated){
switch(gameState){
case GAME_PLAY:

switch(snakeDirection){
case SNAKE_LEFT: snakeDirection=SNAKE_UP; break;
case SNAKE_UP: snakeDirection=SNAKE_RIGHT; break;
case SNAKE_RIGHT: snakeDirection=SNAKE_DOWN; break;
case SNAKE_DOWN: snakeDirection=SNAKE_LEFT; break;
}

thisFrameControlUpdated = true;
break;
case GAME_MENU:
game_init();
break;
}

}

}

else{
debounce_deactivate(&debounceCounterButtonRight);
}

if(millis()-lastGameUpdateTick>gameUpdateInterval){
game_calculate_logic();
game_calculate_display();
lastGameUpdateTick = millis();
thisFrameControlUpdated = false;
}

}