// ╔══════════════════════════════════════════════════════════════╗
// ║         HEARTLINE — DEVICE B (Ashu's Board)                ║
// ║         Flash this on ASHU'S board                         ║
// ╚══════════════════════════════════════════════════════════════╝

#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ════════════════════════════════════════════════════════════════
//  ⚠️  CHANGE ONLY THESE 2 LINES — everything else leave as-is
// ════════════════════════════════════════════════════════════════
const char* WIFI_SSID = "Ashu_";        // ← your WiFi name
const char* WIFI_PASS = "12345678";    // ← your WiFi password

// ════════════════════════════════════════════════════════════════
//  MQTT CONFIG — do not change
//  ⚠️  NOTICE: TOPIC_SEND and TOPIC_RECEIVE are SWAPPED vs Device A
// ════════════════════════════════════════════════════════════════
const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;
const char* TOPIC_SEND    = "heartline/A_yogesh/gesture";   // B sends TO A
const char* TOPIC_RECEIVE = "heartline/B_ashu/gesture";     // B listens for A

// ════════════════════════════════════════════════════════════════
//  PIN DEFINITIONS — same as Device A
// ════════════════════════════════════════════════════════════════
#define BUZZER_PIN   13
#define LED_PIN      14
#define TOUCH_PIN    27
#define BUTTON_PIN   26

// ════════════════════════════════════════════════════════════════
//  OLED
// ════════════════════════════════════════════════════════════════
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ════════════════════════════════════════════════════════════════
//  NETWORK
// ════════════════════════════════════════════════════════════════
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastReconnect = 0;

// ════════════════════════════════════════════════════════════════
//  NOTE FREQUENCIES
// ════════════════════════════════════════════════════════════════
#define REST 0
#define C4  262
#define E4  330
#define G4  392
#define A4  440
#define B4  494
#define C5  523
#define D5  587
#define E5  659
#define G5  784
#define A5  880

// ════════════════════════════════════════════════════════════════
//  MELODIES
// ════════════════════════════════════════════════════════════════
int startMel[]  = { C4, E4, G4, C5 };
int startDur[]  = { 100, 100, 100, 250 };
int loveMel[]   = { C5, B4, A4, G4, A4, B4, C5, E5, D5, C5 };
int loveDur[]   = { 280, 280, 280, 560, 280, 280, 280, 280, 280, 560 };
int surprMel[]  = { C4, E4, G4, C5, E5, G5, C5, REST, C5, E5 };
int surprDur[]  = { 80, 80, 80, 160, 80, 80, 200, 80, 120, 400 };

int* melNotes   = nullptr;
int* melDurs    = nullptr;
int   melLen     = 0;
int   melIdx     = 0;
bool  melPlaying = false;
unsigned long noteTimer = 0;

// ════════════════════════════════════════════════════════════════
//  DISPLAY STATES
// ════════════════════════════════════════════════════════════════
#define NUM_STATES   7
#define ST_HEART     0
#define ST_TEXT      1
#define ST_EYES      2
#define ST_STARFIELD 3
#define ST_XO        4
#define ST_FIREWORKS 5
#define ST_MATRIX    6

// ════════════════════════════════════════════════════════════════
//  LOVE MESSAGES — from Ashu's perspective
// ════════════════════════════════════════════════════════════════
const char* msgs[] = {
  "Good morning,\nYogesh.",
  "You are my\nfavorite.",
  "Thinking of\nyou always.",
  "My heart\nbeats for\nyou.",
  "Love you\nforever.",
  "Just for you.",
  "You make my\nworld bright.",
  "You = my\nhappiness.",
  "Missing you\nalready.",
  "You + Me =\nForever."
};
const int numMsgs = sizeof(msgs) / sizeof(msgs[0]);

// ════════════════════════════════════════════════════════════════
//  TIMING & STATE
// ════════════════════════════════════════════════════════════════
unsigned long prevDisplay   = 0;
unsigned long prevTyping    = 0;
unsigned long lastActivity  = 0;
unsigned long fireworkTimer = 0;
const long displayInterval  = 6000;
const long typingInterval   = 75;
const long screensaverDelay = 60000;

int  curState     = ST_HEART;
int  msgIndex     = 0;
int  charIndex    = 0;
bool screensaver  = false;
bool surpriseLock = false;

// ════════════════════════════════════════════════════════════════
//  BUTTON STATE
// ════════════════════════════════════════════════════════════════
bool          lastButtonState  = HIGH;
unsigned long pressStart       = 0;
int           pressCount       = 0;
unsigned long pressWindowStart = 0;
bool          waitingForWindow = false;

// ════════════════════════════════════════════════════════════════
//  TOUCH STATE
// ════════════════════════════════════════════════════════════════
int lastTouchState = LOW;

// ════════════════════════════════════════════════════════════════
//  STARFIELD
// ════════════════════════════════════════════════════════════════
#define NUM_STARS 35
int starX[NUM_STARS], starY[NUM_STARS], starSpd[NUM_STARS];

// ════════════════════════════════════════════════════════════════
//  XO BOUNCE
// ════════════════════════════════════════════════════════════════
float xX=20, xY=10, xDX=2.1, xDY=1.3;
float oX=80, oY=40, oDX=-1.7, oDY=1.8;

// ════════════════════════════════════════════════════════════════
//  FIREWORKS
// ════════════════════════════════════════════════════════════════
struct Particle { float x,y,vx,vy; bool active; };
#define MAX_PARTS 24
Particle parts[MAX_PARTS];

// ════════════════════════════════════════════════════════════════
//  MATRIX RAIN
// ════════════════════════════════════════════════════════════════
#define MATRIX_COLS 10
int matrixY[MATRIX_COLS];
int matrixSpd[MATRIX_COLS];
unsigned long matrixTimer = 0;
const char matrixChars[] = "ASHULOVE<3";
const int  numMC = sizeof(matrixChars) - 1;

// ════════════════════════════════════════════════════════════════
//  SCREENSAVER
// ════════════════════════════════════════════════════════════════
#define NUM_SHEARTS 6
float shX[NUM_SHEARTS], shY[NUM_SHEARTS], shSpd[NUM_SHEARTS];

// ════════════════════════════════════════════════════════════════
//  EYE VARIABLES
// ════════════════════════════════════════════════════════════════
float left_eye_x=32, left_eye_y=32, left_eye_width=28, left_eye_height=28;
float right_eye_x=96, right_eye_y=32, right_eye_width=28, right_eye_height=28;
float ref_eye_height=28, ref_eye_width=28;
int   ref_corner_radius=8;

#define EYE_IDLE    0
#define EYE_HAPPY   1
#define EYE_SAD     2
#define EYE_CURIOUS 3
#define EYE_SHOCKED 4
#define EYE_LOVE    5

int  currentEyeMood     = EYE_IDLE;
unsigned long eyeMoodTimer   = 0;
const long    eyeMoodDuration= 5000;
unsigned long lastEyeMove    = 0;
unsigned long lastBlink      = 0;
unsigned long lastWink       = 0;
unsigned long eyeBlinkInterval = 3200;

// ════════════════════════════════════════════════════════════════
//  FUNCTION PROTOTYPES (Fix for Arduino IDE compiler bugs)
// ════════════════════════════════════════════════════════════════
void draw_eyes(bool update);
void center_eyes(bool update);
void blink_eyes(int speed);
void wink_right();
void sleep_eyes();
void wakeup_eyes();
void saccade_return();
void startupAnimation();
void connectWiFi();
void connectMQTT();
void showIdle();
void reconnectMQTT();
void checkButton();
void checkTouch();
void updateMelody();
void updateLED();
void drawScreensaver();
void updateDisplay();
void onMessageReceived(char* topic, byte* payload, unsigned int length);
void advanceState();


// ════════════════════════════════════════════════════════════════
//                         S E T U P
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("DEVICE B — Ashu's board booting...");

  pinMode(BUZZER_PIN,  OUTPUT);
  pinMode(LED_PIN,     OUTPUT);
  pinMode(TOUCH_PIN,   INPUT);
  pinMode(BUTTON_PIN,  INPUT_PULLUP);
  noTone(BUZZER_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 FAILED — check wiring"));
    for (;;);
  }
  display.clearDisplay(); display.display();

  randomSeed(analogRead(34));

  for (int i=0;i<NUM_STARS;i++){starX[i]=random(0,SCREEN_WIDTH);starY[i]=random(0,SCREEN_HEIGHT);starSpd[i]=random(1,4);}
  for (int i=0;i<MATRIX_COLS;i++){matrixY[i]=random(0,SCREEN_HEIGHT);matrixSpd[i]=random(1,3);}
  for (int i=0;i<MAX_PARTS;i++) parts[i].active=false;
  for (int i=0;i<NUM_SHEARTS;i++){shX[i]=random(8,SCREEN_WIDTH-8);shY[i]=SCREEN_HEIGHT+i*14;shSpd[i]=0.4+random(0,8)/10.0;}

  startupAnimation();
  sleep_eyes(); delay(600); wakeup_eyes(); blink_eyes(12);

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessageReceived);
  connectMQTT();

  showIdle();
  lastActivity = millis();
}


// ════════════════════════════════════════════════════════════════
//                          L O O P
// ════════════════════════════════════════════════════════════════
void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();

  checkButton();
  checkTouch();
  updateMelody();
  updateLED();

  if (screensaver) {
    drawScreensaver();
  } else {
    updateDisplay();
    if (millis()-lastActivity>screensaverDelay) screensaver=true;
  }
}


// ════════════════════════════════════════════════════════════════
//  WIFI
// ════════════════════════════════════════════════════════════════
void connectWiFi() {
  showStatus("Connecting\nto WiFi...");
  Serial.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts=0;
  while (WiFi.status()!=WL_CONNECTED&&attempts<40){delay(500);Serial.print(".");attempts++;}
  if (WiFi.status()==WL_CONNECTED) {
    Serial.println("\nWiFi OK: "+WiFi.localIP().toString());
    showStatus("WiFi OK!\n"+WiFi.localIP().toString()); delay(800);
  } else {
    Serial.println("\nWiFi FAILED");
    showStatus("WiFi FAILED!\nCheck name &\npassword"); delay(2000);
  }
}


// ════════════════════════════════════════════════════════════════
//  MQTT
// ════════════════════════════════════════════════════════════════
void connectMQTT() {
  showStatus("Connecting\nto server...");
  Serial.print("Connecting MQTT...");
  String clientId="heartlineB-"+String(random(0xffff),HEX);
  if (mqtt.connect(clientId.c_str())) {
    mqtt.subscribe(TOPIC_RECEIVE);
    Serial.println("MQTT OK!");
    Serial.println("Listening on: "+String(TOPIC_RECEIVE));
    showStatus("Connected!\nReady."); delay(800);
  } else {
    Serial.println("MQTT FAILED rc="+String(mqtt.state()));
    showStatus("Server FAILED\nrc="+String(mqtt.state())+"\nRetrying..."); delay(2000);
  }
}

void reconnectMQTT() {
  if (millis()-lastReconnect<5000) return;
  lastReconnect=millis();
  Serial.println("Reconnecting MQTT...");
  connectMQTT();
}


// ════════════════════════════════════════════════════════════════
//  BUTTON
// ════════════════════════════════════════════════════════════════
void checkButton() {
  bool currentState=digitalRead(BUTTON_PIN);
  if (currentState==LOW&&lastButtonState==HIGH) {
    pressStart=millis();
    if (!waitingForWindow){pressWindowStart=millis();waitingForWindow=true;pressCount=0;}
    pressCount++; lastActivity=millis();
  }
  if (currentState==HIGH&&lastButtonState==LOW) {
    long heldMs=millis()-pressStart;
    if (heldMs>2000){waitingForWindow=false;pressCount=0;triggerSurprise();lastButtonState=currentState;return;}
  }
  if (waitingForWindow&&(millis()-pressWindowStart>600)) {
    waitingForWindow=false;
    if      (pressCount==1) sendGesture("1");
    else if (pressCount==2) sendGesture("2");
    else if (pressCount>=3) sendGesture("3");
    pressCount=0;
  }
  lastButtonState=currentState;
}


// ════════════════════════════════════════════════════════════════
//  TOUCH
// ════════════════════════════════════════════════════════════════
void checkTouch() {
  int ts=digitalRead(TOUCH_PIN);
  if (ts==HIGH&&lastTouchState==LOW) {
    lastActivity=millis();
    if (screensaver){screensaver=false;display.clearDisplay();display.display();}
    else{tone(BUZZER_PIN,E5,30);advanceState();}
  }
  lastTouchState=ts;
}


// ════════════════════════════════════════════════════════════════
//  SEND GESTURE
// ════════════════════════════════════════════════════════════════
void sendGesture(const char* gesture) {
  Serial.println("Sending gesture: "+String(gesture));
  if (!mqtt.connected()){showStatus("No server!\nCannot send.");delay(1500);showIdle();return;}

  mqtt.publish(TOPIC_SEND, gesture);
  tone(BUZZER_PIN,880,80); delay(110); tone(BUZZER_PIN,1100,80);

  display.clearDisplay();
  display.drawRect(0,0,128,64,SSD1306_WHITE);
  display.setTextSize(1); display.setCursor(4,6); display.println("Sent to Yogesh:");
  display.drawFastHLine(4,16,120,SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(4,22);
  if      (String(gesture)=="1"){display.println("Thinking");display.println("of you");}
  else if (String(gesture)=="2"){display.println("Missing"); display.println("you");}
  else if (String(gesture)=="3"){display.println("Love");    display.println("you <3");}
  display.display();
  delay(2500); showIdle();
}


// ════════════════════════════════════════════════════════════════
//  RECEIVE — fires when Yogesh sends a gesture
// ════════════════════════════════════════════════════════════════
void onMessageReceived(char* topic, byte* payload, unsigned int length) {
  String msg="";
  for (int i=0;i<length;i++) msg+=(char)payload[i];
  Serial.println("Received from Yogesh: "+msg);

  lastActivity=millis(); screensaver=false;
  hapticPattern(msg.toInt());

  if      (msg=="1") setEyeMood(EYE_CURIOUS);
  else if (msg=="2") setEyeMood(EYE_SAD);
  else if (msg=="3") setEyeMood(EYE_LOVE);
  else if (msg=="4") setEyeMood(EYE_SHOCKED);

  curState=ST_EYES; charIndex=0; prevDisplay=millis();

  display.clearDisplay();
  display.drawRect(0,0,128,64,SSD1306_WHITE);
  display.drawRect(2,2,124,60,SSD1306_WHITE);
  display.setTextSize(1); display.setCursor(4,6); display.println("From Yogesh:");
  display.drawFastHLine(4,15,120,SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(4,22);
  if      (msg=="1"){display.println("Thinking");display.println("of you");}
  else if (msg=="2"){display.println("Missing"); display.println("you");}
  else if (msg=="3"){display.println("Love you");display.println("<3");}
  else if (msg=="4"){display.println("Call me"); display.println("now!");}
  display.display();
  delay(3000);
}


// ════════════════════════════════════════════════════════════════
//  HAPTIC
// ════════════════════════════════════════════════════════════════
void hapticPattern(int type) {
  switch(type){
    case 1: tone(BUZZER_PIN,440,200); break;
    case 2: tone(BUZZER_PIN,440,150);delay(200);tone(BUZZER_PIN,440,150); break;
    case 3: tone(BUZZER_PIN,523,100);delay(150);tone(BUZZER_PIN,659,100);delay(150);tone(BUZZER_PIN,784,200); break;
    case 4: for(int i=0;i<4;i++){tone(BUZZER_PIN,880,100);delay(150);} break;
  }
}


// ════════════════════════════════════════════════════════════════
//  SURPRISE
// ════════════════════════════════════════════════════════════════
void triggerSurprise() {
  surpriseLock=true;
  for(int i=0;i<5;i++){analogWrite(LED_PIN,255);delay(60);analogWrite(LED_PIN,0);delay(60);}
  display.clearDisplay();
  display.drawRect(0,0,128,64,SSD1306_WHITE);
  display.drawRect(2,2,124,60,SSD1306_WHITE);
  display.setTextSize(2);display.setCursor(8,8);display.println("I LOVE");
  display.setCursor(8,28);display.println("YOU,");
  display.setTextSize(1);display.setCursor(8,52);display.println("always & forever");
  display.display();
  playBlocking(surprMel,surprDur,10);
  delay(3000);
  surpriseLock=false;curState=ST_HEART;charIndex=0;
  display.clearDisplay();prevDisplay=millis();lastActivity=millis();
}


// ════════════════════════════════════════════════════════════════
//  ADVANCE STATE
// ════════════════════════════════════════════════════════════════
void advanceState() {
  curState++;
  if(curState>=NUM_STATES){curState=ST_HEART;msgIndex=(msgIndex+1)%numMsgs;startMelody(loveMel,loveDur,10);}
  charIndex=0;prevDisplay=millis();lastActivity=millis();display.clearDisplay();
}


// ════════════════════════════════════════════════════════════════
//  DISPLAY MANAGER
// ════════════════════════════════════════════════════════════════
void updateDisplay() {
  if(surpriseLock) return;
  if(millis()-prevDisplay>=(unsigned long)displayInterval) advanceState();
  switch(curState){
    case ST_HEART:     drawHeart();       break;
    case ST_TEXT:      drawTypewriter();  break;
    case ST_EYES:      animateEyesTick(); break;
    case ST_STARFIELD: drawStarfield();   break;
    case ST_XO:        drawXO();          break;
    case ST_FIREWORKS: drawFireworks();   break;
    case ST_MATRIX:    drawMatrix();      break;
  }
}


// ════════════════════════════════════════════════════════════════
//  LED
// ════════════════════════════════════════════════════════════════
void updateLED() {
  unsigned long t=millis()%900; int b=0;
  if      (t< 80) b=map(t,  0, 80,  0,255);
  else if (t<160) b=map(t, 80,160,255, 80);
  else if (t<240) b=map(t,160,240, 80,255);
  else if (t<320) b=map(t,240,320,255,  0);
  analogWrite(LED_PIN,b);
}


// ════════════════════════════════════════════════════════════════
//  MELODY ENGINE
// ════════════════════════════════════════════════════════════════
void startMelody(int* notes,int* durs,int len){melNotes=notes;melDurs=durs;melLen=len;melIdx=0;melPlaying=true;noteTimer=millis();if(notes[0]!=REST)tone(BUZZER_PIN,notes[0],durs[0]);}
void updateMelody(){if(!melPlaying)return;if(millis()-noteTimer>=(unsigned long)(melDurs[melIdx]+25)){melIdx++;if(melIdx>=melLen){melPlaying=false;noTone(BUZZER_PIN);return;}noteTimer=millis();if(melNotes[melIdx]==REST)noTone(BUZZER_PIN);else tone(BUZZER_PIN,melNotes[melIdx],melDurs[melIdx]);}}
void playBlocking(int* notes,int* durs,int len){for(int i=0;i<len;i++){if(notes[i]==REST)noTone(BUZZER_PIN);else tone(BUZZER_PIN,notes[i],durs[i]);delay(durs[i]+25);noTone(BUZZER_PIN);}}


// ════════════════════════════════════════════════════════════════
//  STARTUP ANIMATION
// ════════════════════════════════════════════════════════════════
void startupAnimation() {
  for(int s=1;s<=12;s++){display.clearDisplay();int x=SCREEN_WIDTH/2,y=SCREEN_HEIGHT/2-8;display.fillCircle(x-s,y-s,s,SSD1306_WHITE);display.fillCircle(x+s,y-s,s,SSD1306_WHITE);display.fillTriangle(x-s*2,y-s+1,x+s*2,y-s+1,x,y+s*2,SSD1306_WHITE);display.display();delay(35);}
  delay(300);
  display.clearDisplay();display.drawRect(0,0,128,64,SSD1306_WHITE);
  display.setTextSize(2);display.setTextColor(SSD1306_WHITE);display.setCursor(4,8);display.print("Heartline");
  display.setTextSize(1);display.setCursor(22,36);display.print("made with love");display.setCursor(34,50);display.print("for Yogesh");
  display.display();playBlocking(startMel,startDur,4);delay(1200);
  display.clearDisplay();display.display();
}


// ════════════════════════════════════════════════════════════════
//  DISPLAY HELPERS
// ════════════════════════════════════════════════════════════════
void showStatus(String msg){display.clearDisplay();display.setTextSize(1);display.setTextColor(SSD1306_WHITE);display.setCursor(0,16);display.println(msg);display.display();}

void showIdle() {
  display.clearDisplay();
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);display.print("Heartline  B");
  display.setCursor(100,0);display.print(WiFi.isConnected()?"W+":"W-");
  display.setCursor(116,0);display.print(mqtt.connected()?"M+":"M-");
  display.drawFastHLine(0,10,128,SSD1306_WHITE);
  display.setCursor(0,14);display.println("1x = thinking of you");
  display.setCursor(0,24);display.println("2x = missing you");
  display.setCursor(0,34);display.println("3x = love you");
  display.setCursor(0,44);display.println("Hold = surprise!");
  display.display();
}


// ════════════════════════════════════════════════════════════════
//  ALL ANIMATIONS — identical to Device A
// ════════════════════════════════════════════════════════════════
void drawHeart(){display.clearDisplay();int size=7+(int)(3*sin(millis()/250.0));int x=SCREEN_WIDTH/2,y=SCREEN_HEIGHT/2-8;display.fillCircle(x-size,y-size,size,SSD1306_WHITE);display.fillCircle(x+size,y-size,size,SSD1306_WHITE);display.fillTriangle(x-size*2,y-size+1,x+size*2,y-size+1,x,y+size*2,SSD1306_WHITE);int sparkle=(millis()/250)%4;int sx[]={x-28,x+26,x-32,x+30},sy[]={y-22,y-22,y+8,y+8};for(int i=0;i<4;i++){if(i==sparkle){display.drawPixel(sx[i],sy[i],SSD1306_WHITE);display.drawPixel(sx[i]+1,sy[i],SSD1306_WHITE);display.drawPixel(sx[i],sy[i]+1,SSD1306_WHITE);}}display.setTextSize(1);display.setCursor(28,56);display.print("Yogesh <3");display.display();}

void drawTypewriter(){unsigned long now=millis();if(now-prevTyping>=(unsigned long)typingInterval){prevTyping=now;if(charIndex<(int)strlen(msgs[msgIndex]))charIndex++;}display.clearDisplay();display.drawFastHLine(0,0,128,SSD1306_WHITE);display.drawFastHLine(0,2,128,SSD1306_WHITE);display.setTextSize(2);display.setTextColor(SSD1306_WHITE);display.setCursor(4,8);for(int i=0;i<charIndex;i++)display.print(msgs[msgIndex][i]);if(charIndex<(int)strlen(msgs[msgIndex])&&(millis()/350)%2==0){int lines=0,col=0;for(int i=0;i<charIndex;i++){if(msgs[msgIndex][i]=='\n'){lines++;col=0;}else col++;}display.fillRect(4+col*12,8+lines*16,3,14,SSD1306_WHITE);}if(charIndex>=(int)strlen(msgs[msgIndex])){int hx=118,hy=54,hs=3;display.fillCircle(hx-hs,hy-hs,hs,SSD1306_WHITE);display.fillCircle(hx+hs,hy-hs,hs,SSD1306_WHITE);display.fillTriangle(hx-hs*2,hy-hs+1,hx+hs*2,hy-hs+1,hx,hy+hs*2,SSD1306_WHITE);}display.display();}

// Eyes
void draw_eyes(bool update=true){display.clearDisplay();int lx=int(left_eye_x-left_eye_width/2),ly=int(left_eye_y-left_eye_height/2);int rx=int(right_eye_x-right_eye_width/2),ry=int(right_eye_y-right_eye_height/2);display.fillRoundRect(lx,ly,(int)left_eye_width,(int)left_eye_height,ref_corner_radius,SSD1306_WHITE);display.fillRoundRect(rx,ry,(int)right_eye_width,(int)right_eye_height,ref_corner_radius,SSD1306_WHITE);if(update)display.display();}
void center_eyes(bool update=true){left_eye_x=32;left_eye_y=32;right_eye_x=96;right_eye_y=32;left_eye_width=ref_eye_width;left_eye_height=ref_eye_height;right_eye_width=ref_eye_width;right_eye_height=ref_eye_height;ref_corner_radius=8;draw_eyes(update);}
void blink_eyes(int speed=12){for(int h=ref_eye_height;h>1;h-=speed){left_eye_height=h;right_eye_height=h;draw_eyes();delay(10);}left_eye_height=1;right_eye_height=1;draw_eyes();delay(40);for(int h=1;h<ref_eye_height;h+=speed){left_eye_height=h;right_eye_height=h;draw_eyes();delay(10);}left_eye_height=ref_eye_height;right_eye_height=ref_eye_height;draw_eyes();}
void wink_right(){for(int h=ref_eye_height;h>1;h-=10){right_eye_height=h;draw_eyes();delay(12);}right_eye_height=1;draw_eyes();delay(80);for(int h=1;h<ref_eye_height;h+=10){right_eye_height=h;draw_eyes();delay(12);}right_eye_height=ref_eye_height;draw_eyes();}
void sleep_eyes(){center_eyes(false);left_eye_height=6;right_eye_height=6;ref_corner_radius=3;draw_eyes();}
void wakeup_eyes(){for(int h=6;h<=ref_eye_height;h+=3){left_eye_height=h;right_eye_height=h;draw_eyes();delay(30);}center_eyes();}
void saccade_return(){for(int i=0;i<6;i++){left_eye_x+=(32.0-left_eye_x)/6.0;right_eye_x+=(96.0-right_eye_x)/6.0;left_eye_y+=(32.0-left_eye_y)/6.0;right_eye_y+=(32.0-right_eye_y)/6.0;draw_eyes();delay(20);}center_eyes();}
void happy_eyes(){center_eyes(false);left_eye_height=14;right_eye_height=14;left_eye_width=32;right_eye_width=32;ref_corner_radius=7;draw_eyes();}
void sad_eyes(){center_eyes(false);left_eye_height=20;right_eye_height=20;left_eye_y=36;right_eye_y=36;left_eye_width=24;right_eye_width=24;ref_corner_radius=4;draw_eyes();}
void shocked_eyes(){center_eyes(false);left_eye_height=ref_eye_height+10;right_eye_height=ref_eye_height+10;left_eye_width=ref_eye_width+8;right_eye_width=ref_eye_width+8;ref_corner_radius=10;draw_eyes();}
void curious_eyes(){center_eyes(false);left_eye_height=ref_eye_height+4;left_eye_width=ref_eye_width+4;right_eye_height=ref_eye_height-4;right_eye_width=ref_eye_width-4;left_eye_y=30;right_eye_y=30;ref_corner_radius=8;draw_eyes();}
void love_eyes(){for(int i=0;i<3;i++){center_eyes(false);left_eye_height=10;right_eye_height=10;left_eye_width=34;right_eye_width=34;ref_corner_radius=5;draw_eyes();delay(300);left_eye_height=ref_eye_height;right_eye_height=ref_eye_height;draw_eyes();delay(150);}left_eye_height=10;right_eye_height=10;left_eye_width=34;right_eye_width=34;draw_eyes();}
void setEyeMood(int mood){currentEyeMood=mood;eyeMoodTimer=millis();center_eyes(false);switch(mood){case EYE_HAPPY:happy_eyes();break;case EYE_SAD:sad_eyes();break;case EYE_SHOCKED:shocked_eyes();break;case EYE_CURIOUS:curious_eyes();break;case EYE_LOVE:love_eyes();break;default:center_eyes();break;}}
void idle_eyes_tick(){unsigned long now=millis();if(now-lastEyeMove>1800){lastEyeMove=now;int dx=random(-10,11),dy=random(-6,7);left_eye_x=constrain(left_eye_x+dx,18,46);right_eye_x=constrain(right_eye_x+dx,82,110);left_eye_y=constrain(left_eye_y+dy,18,46);right_eye_y=constrain(right_eye_y+dy,18,46);draw_eyes();delay(500);saccade_return();}if(now-lastBlink>eyeBlinkInterval){lastBlink=now;eyeBlinkInterval=2500+random(0,2000);blink_eyes(14);}if(now-lastWink>8000){lastWink=now;wink_right();}}
void animateEyesTick(){if(currentEyeMood!=EYE_IDLE){if(millis()-eyeMoodTimer>eyeMoodDuration){currentEyeMood=EYE_IDLE;wakeup_eyes();}else{static unsigned long moodSub=0;if(millis()-moodSub>900){moodSub=millis();if(currentEyeMood==EYE_LOVE){int h=(left_eye_height==10)?14:10;left_eye_height=h;right_eye_height=h;draw_eyes();}if(currentEyeMood==EYE_SHOCKED){blink_eyes(20);shocked_eyes();}if(currentEyeMood==EYE_SAD){blink_eyes(5);sad_eyes();}}}return;}idle_eyes_tick();}

void drawStarfield(){display.clearDisplay();for(int i=0;i<NUM_STARS;i++){starX[i]-=starSpd[i];if(starX[i]<0){starX[i]=SCREEN_WIDTH;starY[i]=random(0,SCREEN_HEIGHT-8);starSpd[i]=random(1,4);}if(starSpd[i]==1)display.drawPixel(starX[i],starY[i],SSD1306_WHITE);else if(starSpd[i]==2){display.drawPixel(starX[i],starY[i],SSD1306_WHITE);display.drawPixel(starX[i]+1,starY[i],SSD1306_WHITE);}else display.fillRect(starX[i],starY[i],2,2,SSD1306_WHITE);}display.setTextSize(1);display.setCursor(8,56);display.print("To infinity, Ashu!");display.display();}

void drawXO(){static unsigned long lastXO=0;if(millis()-lastXO<30)return;lastXO=millis();xX+=xDX;xY+=xDY;oX+=oDX;oY+=oDY;if(xX<=0||xX>=SCREEN_WIDTH-16)xDX=-xDX;if(xY<=0||xY>=SCREEN_HEIGHT-20)xDY=-xDY;if(oX<=0||oX>=SCREEN_WIDTH-16)oDX=-oDX;if(oY<=0||oY>=SCREEN_HEIGHT-20)oDY=-oDY;display.clearDisplay();display.setTextSize(2);display.setCursor((int)xX,(int)xY);display.print("X");display.setCursor((int)oX,(int)oY);display.print("O");display.drawPixel((int)(xX-xDX*3),(int)(xY-xDY*3),SSD1306_WHITE);display.drawPixel((int)(oX-oDX*3),(int)(oY-oDY*3),SSD1306_WHITE);float dist=sqrt(pow(xX-oX,2)+pow(xY-oY,2));if(dist<25){display.fillCircle((int)((xX+oX)/2),(int)((xY+oY)/2),4,SSD1306_WHITE);tone(BUZZER_PIN,A4,20);}display.display();}

void launchFirework(){int cx=random(20,SCREEN_WIDTH-20),cy=random(8,SCREEN_HEIGHT/2);for(int i=0;i<MAX_PARTS;i++){float angle=(float)i/MAX_PARTS*2.0*PI,spd=0.5+random(5,15)/10.0;parts[i]={(float)cx,(float)cy,cos(angle)*spd,sin(angle)*spd,true};}tone(BUZZER_PIN,A5,40);}
void drawFireworks(){static unsigned long lastFW=0;if(millis()-lastFW<35)return;lastFW=millis();if(millis()-fireworkTimer>2200){launchFirework();fireworkTimer=millis();}display.clearDisplay();for(int i=0;i<MAX_PARTS;i++){if(!parts[i].active)continue;parts[i].x+=parts[i].vx;parts[i].y+=parts[i].vy;parts[i].vy+=0.12;if(parts[i].x<0||parts[i].x>=SCREEN_WIDTH||parts[i].y<0||parts[i].y>=SCREEN_HEIGHT)parts[i].active=false;else display.drawPixel((int)parts[i].x,(int)parts[i].y,SSD1306_WHITE);}display.setTextSize(1);display.setCursor(4,56);display.print("You're my star!");display.display();}

void drawMatrix(){if(millis()-matrixTimer<80)return;matrixTimer=millis();display.clearDisplay();int colW=SCREEN_WIDTH/MATRIX_COLS;for(int c=0;c<MATRIX_COLS;c++){char ch=matrixChars[random(0,numMC)];display.setTextSize(1);display.setCursor(c*colW,matrixY[c]);display.print(ch);matrixY[c]+=8*matrixSpd[c];if(matrixY[c]>SCREEN_HEIGHT){matrixY[c]=random(-20,0);matrixSpd[c]=random(1,3);}}display.display();}

void drawScreensaver(){display.clearDisplay();for(int i=0;i<NUM_SHEARTS;i++){shY[i]-=shSpd[i];if(shY[i]<-10){shY[i]=SCREEN_HEIGHT+random(0,20);shX[i]=random(8,SCREEN_WIDTH-8);}int x=(int)shX[i],y=(int)shY[i],s=3;display.fillCircle(x-s,y-s,s,SSD1306_WHITE);display.fillCircle(x+s,y-s,s,SSD1306_WHITE);display.fillTriangle(x-s*2,y-s+1,x+s*2,y-s+1,x,y+s*2,SSD1306_WHITE);}display.setTextSize(1);display.setCursor(12,56);display.print("Thinking of you...");display.display();}