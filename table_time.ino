
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include "Chewy_Regular_45.h"
#include <time.h>

#define TOUCH_THRESHOLD 25
#define TOUCH1 4 // GPIO 4 for touch 1
#define TOUCH2 27 // GPIO 27 for touch 2

#define ONBOARD_LED 2

#define LOOP_DELAY 50 // 50 ms
#define TIME_UPDATE_LOOP_COUNT 20 //update time after 20 loops

#define NO_OF_DIFF_CLOCK 2 // no of different clocks in FRAME 0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1



int time_update_loop= 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid     = "14_A";
const char* password = "fourteena";

bool showWindowStatus_show= true; // if window status is shown for next frame;

// const char* ssid     = "Droid";
// const char* password = "bubublabla";

int currentFrame= 1; // 0, 1 or 2 as starting frame...
int frameShift_X= 0, frameShift_y= 0;
const int frameJmp= 10; // if changed, have to change code in loop to fix

const long utcOffsetInSeconds = 19800;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


int date= 0, month= 0, year= 0;
int hours= 0, minutes= 88, seconds= 0;
char days[7][4]= {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
int today= 0;
char ampm[3]= "am";
time_t rawtime;
struct tm *ti;


bool touchOngoing1= false, touchOngoing2= false;
bool fstRandomTouched1= false, fstRandomTouched2= false;

int frame0_clock_no= 0;

int current_blink= 1; // counter for blinking clock

/*******************************************
***** Show WARNING through onboard led *****
*******************************************/
void warn() {
	digitalWrite(ONBOARD_LED, HIGH);
	delay(100);
	digitalWrite(ONBOARD_LED, LOW);
	delay(100);
	digitalWrite(ONBOARD_LED, HIGH);
	delay(100);
	digitalWrite(ONBOARD_LED, LOW);
	delay(200);
}

/*******************************************
***** Show ERROR through onboard led *****
*******************************************/

void error() {
	while(1) {
		digitalWrite(ONBOARD_LED, HIGH);
		delay(500);
		digitalWrite(ONBOARD_LED, LOW);
		delay(100);
	}
}


void setup() {

	/**************************************
	********* General SETUP ***************
	**************************************/

	Serial.begin(115200);
	pinMode(ONBOARD_LED,OUTPUT);
	delay(10);

	/**************************************
	********* Display Setup ***************
	**************************************/

	if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
		Serial.println(F("SSD1306 allocation failed"));
		error();
	}
	display.display();
	delay(2000);
	display.clearDisplay();

	display.setTextSize(1);
	display.setTextColor(WHITE);

	/**************************************
	********* Connect to WIFI *************
	**************************************/

	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);

	display.setCursor(0,SCREEN_HEIGHT/4);
	display.print("Connecting WIFI");
	display.display();

	while(WiFi.status() != WL_CONNECTED) {
		warn();
		Serial.print(".");

		display.print(".");
		display.display();
	}
	display.println(" ");
	display.println("WIFI Connected");
	display.display();

	/*******************************************
	********** Setup for NTP *******************
	*******************************************/

	timeClient.begin();

	display.println("NTP started");
	display.display();



	/******************************************
	********** END of setup *******************
	******************************************/
	delay(1000);

}


void loop() {

	// display.setTextSize(2);

	display.clearDisplay();

	if(--time_update_loop < 0) {
		updateTime();
		time_update_loop= TIME_UPDATE_LOOP_COUNT;
	}

	runTouchInterupt();


	/********************************************************
	*** For showing animation between window/frame change ***
	********************************************************/
	if(currentFrame == 0) {
		if(frameShift_y > 5) {
			frame0(frameShift_X, frameShift_y);
			frame2(frameShift_X, frameShift_y- 64);
			frameShift_y-= frameJmp;
		} else if(frameShift_y < -5) {
			frame0(frameShift_X, frameShift_y);
			frame1(frameShift_X, 64+frameShift_y);
			frameShift_y+= frameJmp;
		} else {
			frame0(0, 0);
		}
		if(frameShift_y < 5 && frameShift_y > -5) {
			frameShift_y= 0;
		}

	} else if(currentFrame == 1) {
		if(frameShift_y > 5) {
			frame1(frameShift_X, frameShift_y);
			frame0(frameShift_X, frameShift_y- 64);
			frame0_clock_no= 0;
			frameShift_y-= frameJmp;
		} else if(frameShift_y < -5) {
			frame1(frameShift_X, frameShift_y);
			frame2(frameShift_X, 64+frameShift_y);
			// frame0_clock_no= 0;
			frameShift_y+= frameJmp;
		} else {
			frame1(0, 0);
		}
		if(frameShift_y < 5 && frameShift_y > -5) {
			frameShift_y= 0;
		}
	} else if(currentFrame == 2) {
		if(frameShift_y > 5) {
			frame2(frameShift_X, frameShift_y);
			frame1(frameShift_X, frameShift_y- 64);
			frameShift_y-= frameJmp;
		} else if(frameShift_y < -5) {
			frame2(frameShift_X, frameShift_y);
			frame0(frameShift_X, 64+frameShift_y);
			frame0_clock_no= 0;
			frameShift_y+= frameJmp;
		} else {
			frame2(0, 0);
		}
		if(frameShift_y < 5 && frameShift_y > -5) {
			frameShift_y= 0;
		}
	}

	if(currentFrame) { // dont show at frame 0...
		showWindowStatus(currentFrame);
	}

	//************ End animation **********************

	if(WiFi.status() != WL_CONNECTED) {
		warn();
	}

	display.display();

	delay(50);


}


/***************************************************
************* Function if touched ****************
***************************************************/

// onTouch executes once if being touched... 
void onTouch1() {

	// display scroll down
	if(!frameShift_y) {
		frameShift_y= 64;
		currentFrame= (currentFrame+1)% 3;
		reset();
	}

	// display scroll up
	// if(!frameShift_y) {
	// 	frameShift_y= -64;
	// 	currentFrame--;
	// 	currentFrame= currentFrame<0? 2: currentFrame;
	// }
}

void onTouch2() {
	if(currentFrame == 0) {
		frame0_clock_no= (frame0_clock_no+1)%NO_OF_DIFF_CLOCK;
		reset();
	}
}

void runTouchInterupt() {

	int val1= touchRead(TOUCH1);
	int val2= touchRead(TOUCH2);

	Serial.print("TOUCH of- ");
	Serial.print(TOUCH1);
	Serial.print(" - ");
	Serial.println(val1);

	Serial.print("TOUCH of- ");
	Serial.print(TOUCH2);
	Serial.print(" - ");
	Serial.println(val2);

	if(val1<TOUCH_THRESHOLD) {
		if(!fstRandomTouched1) {
			fstRandomTouched1= true;
		} else {
			if(!touchOngoing1) {
				onTouch1();
				touchOngoing1= true;
			}
		}
	} else {
		touchOngoing1= false;
		fstRandomTouched1= false;
	}

	if(val2<TOUCH_THRESHOLD) {
		if(!fstRandomTouched2) {
			fstRandomTouched2= true;
		} else {
			if(!touchOngoing2) {
				onTouch2();
				touchOngoing2= true;
			}
		}
	} else {
		touchOngoing2= false;
		fstRandomTouched2= false;
	}
}


/***************************************************
***************** Change Frame *********************
***************************************************/


/***************************************************
************* UPDATE TIME VARIABLES ****************
***************************************************/
void updateTime() {
	// return;
	timeClient.update();
	hours= timeClient.getHours()>12? timeClient.getHours()-12 : timeClient.getHours();
	minutes= timeClient.getMinutes();
	seconds= timeClient.getSeconds();
	ampm[0]= timeClient.getHours()>12? 'p' : 'a';
	today= timeClient.getDay();

	time_t rawtime = timeClient.getEpochTime();
	ti = localtime (&rawtime);
	year= ti->tm_year + 1900;
	month= ti->tm_mon+1;
	date= ti->tm_mday;

}

/***************************************************
************ Get remaining day of GATE *************
***************************************************/

int getRemainingDay(int d, int m, int y) {
	int td= date, tm= month, ty= year;
	int res= 0;
	int i;
	for(i=ty; i< y; i++) {
		res+= i%4 == 0? 366 : 365;
	}
	res-= daysInMonth(tm,ty);
	res-= td;
	res+= daysInMonth(m, y);
	res += d;
	return  res;
}

int daysInMonth(int a,int yy) {
	int x=0,c;
	int mon[12]={31,28,31,30,31,30,31,31,30,31,30,31};
	for(c=0;c<a-1;c++) {
		if(c==1) {
			if(yy%4==0)
				x+=29;
			else
				x+=28;
		}
		else
			x+=mon[c];
	}
	return(x);
}


/***************************************************
************ Main frame of display******************
********* Shows time and days remaining ************
***************************************************/


void frame1(int x, int y) {
	// display.setTextSize(3);

	/******** Show Time ************/
	display.setFont(&FreeSansBold12pt7b);
	display.setCursor(25+x, 17+y);
	char h[3]= {'0', '0', '\0'};
	char m[3]= {'0', '0', '\0'};
	timeStringify(hours, h);
	display.print(h);
	display.print(":");
	timeStringify(minutes, m);
	display.print(m);


	/********** Show AM / PM ************/
	display.setFont(&FreeMono9pt7b);
	display.setCursor(85+x, 17+y);
	display.print(ampm);
	

	/*********** Show Remaining days *********/
	int rem= getRemainingDay(2, 2, 2020);
	if(rem> 0) {
		display.setFont(&FreeSans12pt7b);
		display.setCursor(35+x, 50+y);
		display.print(rem);
		display.setFont(&FreeMono9pt7b);
		display.print("ds");
		display.drawRect(0+x, 58+y, display.width(), 6, WHITE);
		int remainingTime= display.width() * (float)(365-rem)/(float)365;
		display.fillRect(0+x, 58+y, remainingTime,6 , WHITE);
	}
	else if(rem == 0) {
		display.setFont(&FreeSans12pt7b);
		display.setCursor(35+x, 50+y);
		display.print(".GATE.");
	} else {
		display.drawRect(0+x, 58+y, display.width(), 6, WHITE);
		int remainingTime= display.width() * (float)(365-rem)/(float)365;
		display.fillRect(0+x, 58+y, remainingTime,6 , WHITE);
	}

}

void timeStringify(int n, char *c) {
	if(n<10) {
		c[0]= '0';
		c[1]= n+48;
	} else {
		c[1]= (n%10)+48;
		c[0]= (n/10)+48;
	}
}



/***************************************************
************ 2nd frame of display ******************
********* XXXXXXXXXXXXXXXXXXXXXXXXXXXXX ************
***************************************************/

void frame0(int x, int y) {

	// display.setFont(&FreeSans12pt7b);
	// display.setCursor(x+20, y+30);
	// display.print(touchRead(27));

	switch(frame0_clock_no) {
	case 0:
		clock_big(x, y);
		break;
	case 1:
		clock_blink(x, y);
		break;
	}
}



void frame2(int x, int y) {
	// display.drawRect( x+10, y+10, display.width()-20, display.height()-20, WHITE);
	// display.drawRect( x+20, y+20, display.width()-40, display.height()-40, WHITE);
	// display.fillRect( x, y, display.width(), display.height(), WHITE);
	display.drawCircle(x+32, y+32, 31, WHITE);
}






/***************************************************
***************** The Status icons *****************
************* These are always visible *************
***************************************************/

void showWindowStatus(int curWindow) {

	// ************** Show Window via bubble ***************
	if(showWindowStatus_show) {
		const int X= 124, Y= 23, distance= 9, radius= 2;
		const int numberOfWindow= 3;
		int i;
		for(i=0; i< numberOfWindow; i++) {
			if(curWindow == i) {
				display.fillCircle(X, Y+(i)*distance, radius+1, WHITE);
			} else {
				display.drawCircle(X, Y+(i)*distance, radius, WHITE);
			}
		}
	}
	showWindowStatus_show= true;
}

/***************************************************
*************  resets all variables  ***************
***************************************************/

void reset() {
	display.dim(false);
	current_blink= 1;

}

/***************************************************
***************** Different Clocks *****************
******************  for frame 0  *******************
***************************************************/


#define TIME_CONST_BLINK_ON 50
#define TIME_CONST_BLINK_OFF 200
void clock_blink(int x, int y) {
	int cycle_time= TIME_CONST_BLINK_OFF+ TIME_CONST_BLINK_ON;
	current_blink= (current_blink+1) % cycle_time;
	if(current_blink < TIME_CONST_BLINK_ON) {
		display.dim(false);

		display.setFont(&FreeSans24pt7b);
		display.setCursor(5+x, 48+y);

		char h[3]= {'0', '0', '\0'};
		char m[3]= {'0', '0', '\0'};
		timeStringify(hours, h);
		display.print(h);
		display.print(":");
		timeStringify(minutes, m);
		display.print(m);

	} else {
		display.dim(true);
	}

}

void clock_big(int x, int y) {
	display.setFont(&Chewy_Regular_45);
	display.setCursor(0+x, 34+y);

	char h[3]= {'0', '0', '\0'};
	char m[3]= {'0', '0', '\0'};
	timeStringify(hours, h);
	display.print(h);

	display.setCursor(70+x, 60+y);
	timeStringify(minutes, m);
	display.print(m);

	display.setFont(&FreeMono9pt7b);
	display.setCursor(95+x, 10+y);
	display.print(ampm);

	/*********** Show Remaining days *********/
	int rem= getRemainingDay(2, 2, 2020);
	int boxLen= 64;
	if(rem> 0) {
		display.drawRect(0+x, 58+y, boxLen, 6, WHITE);
		int remainingTime= boxLen * (float)(365-rem)/(float)365;
		if(remainingTime < boxLen) {
			display.fillRect(0+x, 58+y, remainingTime,6 , WHITE);
		}
	}
	else if(rem == 0) {
		display.drawRect(0+x, 58+y, boxLen, 6, WHITE);
	}
}
