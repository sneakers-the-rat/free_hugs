/*
Arduino Inflatable Needs

When everything is first turned on, inflation relay (relay 1) turns on for 12 seconds

After this first inflation command is finished, relay 1 should begin a cycle of waiting for 1 minute and then turning on for 3 seconds 

If it’s not too difficult, it would be a cherry on top to have relay 3 (music relay) turn on for 10 seconds every time this 5 second inflation cycle is triggered.

Then

When the motion sensor senses someone closer than 6 feet for longer than 1 second, deflation relay (relay 4) is turned on for 10 seconds, regardless of where relay 1 is in its cycle. Then everything should halt for 1 minute and then the cycle begins again with inflation relay turning on for 12 seconds.

The ultrasonic sensor is hooked up to port 2 and 3
Relay 1 - Deflation - port 8
Relay 3 - Music - port 10
Relay 4 - Inflation - port 11

 */

/////////////////
// constants
////////////////

// Pins to control pumps and music
int INFLATION = 11;
int DEFLATION = 8;
int MUSIC     = 10;

// program parameters
// duration of initial inflation in seconds
int INITIAL_INFLATE = 3;
// period of time to wait between automatic inflations in seconds
long AUTOINFLATE_PERIOD = 5;
// duration of time to inflate during autoinflates in seconds
int AUTOINFLATE_DURATION = 1;
// duration of audio signal
long AUDIO_DURATION = 2;

// Pins for ultrasonic sensor
int DIST_TRIG = 3;
int DIST_ECHO = 2;

// distance thresholds and average
// see the following for details on smoothing
// https://www.arduino.cc/en/tutorial/smoothing
// number of readings to average over

const int N_READINGS = 10;

// distance threshold to trigger deflation
int DEFLATION_THRESHOLD = 100;

// duration should stay deflated in seconds -- 
// once a deflation is started, it should always go to full time
long DEFLATION_DURATION = 7;

// time to wait after a deflation has ended to do a full reinflation (in seconds)
long REINFLATION_DELAY = 5;

// should we initialize a serial connection and read back values?
bool DEBUG = true;



////////////////
// variables
////////////////

// state of program
// OFF 
// AUTOINFLATE
// SENSORINFLATE


// store distance values
long dist_duration;
int distance;

// store time of last inflation
long time_last_inflation;
long time_current;

// during inflations, store time that inflation was started
long time_inflation_started;
bool inflation_ongoing;

// during deflations, ''
bool deflation_ongoing;
long time_deflation_started;
bool deflated = false;

// two different types of inflation
// 'autoinflate' - 1 -- short to keep inflation in normal circumstances
// 'refill'      - 2 -- long for when initializing and after deflations
int inflation_type = 1;

// store if we're waiting to do a re-inflation after a deflation
bool refill_delay;
long time_refill_delay;

// array to store distance readings
int readings[N_READINGS];       // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average

long time_audio_start; // time audio started playing
bool audio_ongoing; // audio is currently playing


void setup() {
  // put your setup code here, to run once:

  // setup pins for pump control
  pinMode(INFLATION, OUTPUT);
  pinMode(DEFLATION, OUTPUT);
  pinMode(MUSIC,     OUTPUT);

  // pins for ultrasonic sensor
  pinMode(DIST_TRIG, OUTPUT);
  pinMode(DIST_ECHO, INPUT);
  
  // on startup, turn on inflation for 12 seconds
  digitalWrite(INFLATION, HIGH);
  delay(INITIAL_INFLATE*1000);
  digitalWrite(INFLATION, LOW);

  // Initialize distances with high numbers
  for (int i=0; i<N_READINGS; i++){
    update_average(500);
  }

  
  time_last_inflation = millis();

  if (DEBUG){
    Serial.begin(9600);
  }
  
}

void loop() {
  // put your main code here, to run repeatedly:

  // store the current time of the loop
  time_current = millis();

  // account for time resets on the arduino
  if (time_current < time_last_inflation || 
      time_current < time_inflation_started ||
      time_current < time_deflation_started ||
      time_current < time_refill_delay ||
      time_current < time_audio_start){
    time_last_inflation = 0;
    time_inflation_started = 0;
    time_deflation_started = 0;
    time_refill_delay = 0;
    time_audio_start = 0;
  }


  // if the autoinflate period has elapsed, turn on the pump!
  if (time_current-time_last_inflation > AUTOINFLATE_PERIOD*1000){
    if (!deflation_ongoing && !inflation_ongoing && !refill_delay){
      time_last_inflation = time_current;
      //time_inflation_started = time_current;
      digitalWrite(INFLATION, HIGH);
      inflation_ongoing = true;
      inflation_type = 1;

      // audio stuff
      digitalWrite(MUSIC, HIGH);
      audio_ongoing = true;
      time_audio_start = time_current;
      
    }
  }

  // audio only checker
  if (audio_ongoing){
    if (time_current - time_audio_start > AUDIO_DURATION*1000){
      digitalWrite(MUSIC, LOW);
      audio_ongoing = false;
    }
  }

  // if we're waiting on a refill after a deinflation, check for it 
  if (refill_delay){
    if (time_current - time_refill_delay > REINFLATION_DELAY*1000){
      time_last_inflation = time_current;
      digitalWrite(INFLATION, HIGH);
      inflation_ongoing = true;
      inflation_type = 2;
      refill_delay = false;
 
    }
  }

  // if the pump is turned on, and the autoinflate duration has elapsed, turn it off.
  if (inflation_ongoing){
    switch (inflation_type) {
      case 1:
        if (time_current-time_last_inflation > AUTOINFLATE_DURATION*1000){
          digitalWrite(INFLATION, LOW);
          inflation_ongoing = false;
        }
      case 2:
        if (time_current-time_last_inflation > INITIAL_INFLATE*1000){
          digitalWrite(INFLATION, LOW);
          inflation_ongoing = false;
          deflated = false;
        }
        
    }

  }

  // measure distance and update average readings
  distance = read_distance(DIST_TRIG, DIST_ECHO);
  // updates the average distance measurement
  update_average(distance);
  
  if (average < DEFLATION_THRESHOLD){
    if (!deflated && !deflation_ongoing){
      time_deflation_started = time_current;
      digitalWrite(DEFLATION, HIGH);
      digitalWrite(INFLATION, LOW);
      refill_delay = false;
    } else {
      time_refill_delay = time_current;
    }
    deflation_ongoing = true;
    inflation_ongoing = false;



    
  } 

  if (deflation_ongoing) {
    if (time_current - time_deflation_started > DEFLATION_DURATION*1000){
      // end the deflation
      digitalWrite(DEFLATION, LOW);
      deflation_ongoing = false;

      // store the time we started waiting to reinflate after we have stopped deflating
      time_refill_delay = time_current;
      refill_delay = true;
      deflated = true;
    }
  }


  // debugging stuff
  if (DEBUG) {
  Serial.print(time_current);
  
  Serial.print(" Average: ");
  Serial.print(average);

    Serial.print(" Distance: ");
  Serial.print(distance);

  Serial.print(" Inflation: ");
  Serial.print(inflation_ongoing);

  Serial.print(" Deflation: ");
  Serial.print(deflation_ongoing);

  Serial.print(" Deflated: ");
  Serial.print(deflated);

  Serial.print(" Refill Delay: ");
  Serial.print(refill_delay);

  Serial.print(" Time Inflation: ");
  Serial.print(time_inflation_started);

  Serial.print(" Time Refill Delay: ");
   Serial.print(time_refill_delay);

  Serial.print(" Inflation Pin: ");
  Serial.print(digitalRead(INFLATION));

  Serial.print(" Deflation Pin: ");
  Serial.print(digitalRead(DEFLATION));

  Serial.print(" Audio Pin: ");
  Serial.println(digitalRead(MUSIC));




  }

}


// Function to measure distance
int read_distance(int trigPin, int echoPin)
{
  // clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  long duration = pulseIn(echoPin, HIGH);
  
  // Calculating the distance
  int distance= duration*0.034/2;
  
  return distance;
}

int update_average(int new_distance){
  total = total - readings[readIndex];
  // read from the sensor:
  readings[readIndex] = new_distance;
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= N_READINGS) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }

  // calculate the average:
  average = total / N_READINGS;
}
