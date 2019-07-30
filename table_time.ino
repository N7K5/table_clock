
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>;
#include <time.h>

#define TOUCH_THRESHOLD 25
#define TOUCH1 4 // GPIO 4 for touch 1

#define ONBOARD_LED 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define TIME_UPDATE_LOOP_COUNT 5 //update time after 5 loops
int time_update_loop= 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid     = "14_A";
const char* password = "fourteena";

// const char* ssid     = "Droid";
// const char* password = "bubublabla";

int currentFrame= 1; // 0, 1 or 2 as starting frame...
int frameShift_X= 0, frameShift_y= 0;
const int frameJmp= 10; // if changed, have to change code in loop to fix

const long utcOffsetInSeconds = 19800;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


int date= 0, month= 0, year= 0;
int hours= 5, minutes= 15, seconds= 0;
char days[7][4]= {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
int today= 0;
char ampm[3]= "am";
time_t rawtime;
struct tm *ti;


bool touchOngoing= false;
bool fstRandomTouched= false;

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
		showWindowStatus(0);
		if(frameShift_y < 5 && frameShift_y > -5) {
			frameShift_y= 0;
		}

	} else if(currentFrame == 1) {
		if(frameShift_y > 5) {
			frame1(frameShift_X, frameShift_y);
			frame0(frameShift_X, frameShift_y- 64);
			frameShift_y-= frameJmp;
		} else if(frameShift_y < -5) {
			frame1(frameShift_X, frameShift_y);
			frame2(frameShift_X, 64+frameShift_y);
			frameShift_y+= frameJmp;
		} else {
			frame1(0, 0);
		}
		showWindowStatus(1);
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
			frameShift_y+= frameJmp;
		} else {
			frame2(0, 0);
		}
		showWindowStatus(2);
		if(frameShift_y < 5 && frameShift_y > -5) {
			frameShift_y= 0;
		}
	}

	//************ End animation **********************

	if(WiFi.status() != WL_CONNECTED) {
		warn();
	}

	display.display();

	delay(200);


}


/***************************************************
************* Function if touched ****************
***************************************************/

// onTouch executes once if being touched... 
void onTouch() {

	// display scroll down
	if(!frameShift_y) {
		frameShift_y= 64;
		currentFrame= (currentFrame+1)% 3;
	}

	// display scroll up
	// if(!frameShift_y) {
	// 	frameShift_y= -64;
	// 	currentFrame--;
	// 	currentFrame= currentFrame<0? 2: currentFrame;
	// }
}

void runTouchInterupt() {

	int val= touchRead(TOUCH1);
	Serial.println(val);
	if(val<TOUCH_THRESHOLD) {
		if(!fstRandomTouched) {
			fstRandomTouched= true;
		} else {
			if(!touchOngoing) {
				onTouch();
				touchOngoing= true;
			}
		}
	} else {
		touchOngoing= false;
		fstRandomTouched= false;
	}
}


/***************************************************
***************** Change Frame *********************
***************************************************/


/***************************************************
************* UPDATE TIME VARIABLES ****************
***************************************************/
void updateTime() {
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

	display.setFont(&FreeSans12pt7b);
	display.setCursor(x+20, y+30);
	display.print(touchRead(27));
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