#include <RFduinoBLE.h>
#include <Lazarus.h>

//very good! 
//todo: periodical advertising to save power if not connected?

Lazarus Lazarus;
//pins   
const int button = 4;
const int flow_sensor=5;
const int moisture_sensor = 2;
const int water_switch = 1;
//time constants (in ms)
const int debounce_time = 20; //button debouncing
// reset water timer after 2 seconds button pressing
const int button_reset_time = 2000;
// repeat water time if moisture < x (at most y times)
int maxrepeats=0;//y
int repeated=0;// <= y
int repeat_mlevel=500; //x
// log arrays for temperature, moisture, water
const unsigned int log_len=15*24+1;//that many log values
unsigned long log_ts=0;
unsigned int log_interval=60;
unsigned int log_counter=0;//modulo log_len --> pointer within array
int log_temp[log_len];
int log_water[log_len];
//int log_time[log_len];
int log_moisture[log_len];
//water interval
unsigned long water_counter=0;//counter for flow sensor
unsigned int water_counter_sec=0;//counter for seconds
unsigned int water_duration=0;//intendet duration for current watering [seconds]
unsigned long water_ts=0;//water runs since timestamp
// water auto trigger
unsigned long water_trigger_ts=0;//timestamp of last water trigger
//unsigned long auto_start=0; //next automatic start[ms];
unsigned long water_trigger_ontime=0; //duration of automatic watering [s]
byte water_trigger_interval=0; //auto_interval[h]
byte water_trigger_state=0; //is current watering interval triggered?
//button
byte button_state;
unsigned long button_ts;//timestamp of last pressing
//sensor data
int temp1 = 0;// *10
int moisture = 0;//*10

//data send by ble
char * buf;
int buflen=0;

void water(int sec){
    if(water_duration==0){
      water_duration=sec;
      water_ts=millis();
      digitalWrite(water_switch, LOW); //is on!! 
    }else{
      water_duration+=sec;
    }
    water_counter_sec+=sec;
}
void stop_water(){
  if(water_duration==0){//should already be off 
     digitalWrite(water_switch, HIGH); //but to be save 
     return;
  }
  if(millis()-water_ts<SECONDS(water_duration)){ //interrupt before timeout
    water_counter_sec -= water_duration-(millis()-water_ts)/1000;
    water_trigger_state=0; //escape repeat
  }  
  if(water_trigger_state==1){
    updateSensors(0);//check moisture level: repeat watering?
    if(repeated<maxrepeats && moisture<repeat_mlevel){
      water(water_trigger_ontime);
      repeated++;
      char message[20];
      sprintf(message, "%d/%d: %d.%d < %d.%d",repeated,maxrepeats, moisture/10, moisture%10, repeat_mlevel/10, repeat_mlevel%10); 
      RFduinoBLE.send(message,20);
      return;
      
    }
    repeated=0;
    water_trigger_state=0;  
  }  
  digitalWrite(water_switch, HIGH); //is off!! 
  water_duration=0;
}
void setup() {
  // put your setup code here, to run once:
  RFduinoBLE.deviceName = "SmartBalkon"; 
  RFduinoBLE.advertisementData = "42";
  // start the BLE stack
  RFduinoBLE.begin();
  //dht.begin();
  pinMode(water_switch, OUTPUT);
  digitalWrite(water_switch, HIGH);
  //pinMode(flow_sensor, INPUT);
  //digitalWrite(flow_sensor, HIGH);//pullup
  //RFduino_pinWake(flow_sensor, LOW);

  pinMode(button, INPUT_PULLDOWN);
  digitalWrite(button, LOW);//pulldown

  RFduino_pinWake(button, HIGH);
  button_state=LOW; //defines not pressed state
  updateSensors(0);
  log_temp[0]=temp1;
  log_water[0]=0;
  //log_time[0]=0;
  log_moisture[0]=moisture;

}
void updateSensors(byte v){
    temp1 = RFduino_temperature(CELSIUS)*10;
    int moisture_read=analogRead(moisture_sensor);
    moisture = ((1000.0 - moisture_read / 1.023) -200)/.4;
    
    if(moisture < 0){  
      moisture=0;
    }
    if(moisture>1000){
      moisture=1000;
    }
    //float humidity = dht.readHumidity();
    //float temp2 = dht.readTemperature();
    // Compute heat index in Celsius (isFahreheit = false)
    //float hic = dht.computeHeatIndex(temp2, humidity, false);
    //int water_cl=water_counter/4.5;
    if(v){    
      char message [21];
      sprintf(message,"%3d.%01d*;%2d.%01d%%;%2d:%02dmin",
          temp1/10,abs(temp1%10),
          moisture/10,abs(moisture%10),
          water_counter_sec/60, abs(water_counter_sec%60));
      //char message2 [10];
      //sprintf(message2,"Raw: %4d",moisture_read);
    
      RFduinoBLE.send(message,21);
    }

}
void printCountdown(){
    int water_runtime=0;
    if (water_duration){
      water_runtime=water_duration-(millis()-water_ts)/1000;
    }
    char message[20];
    
    sprintf(message,"water %3d sec", water_runtime);
    RFduinoBLE.send(message,13);
}
void printInterval(){
    if(water_trigger_interval){
      unsigned int diff_min=(HOURS(water_trigger_interval)-(millis()-water_trigger_ts))/1000/60;
      char message [20];
      sprintf(message,"%2d:%02dh|%3ds|%2dh", diff_min/60,diff_min%60,water_trigger_ontime,water_trigger_interval);
      RFduinoBLE.send(message,15);
    }else{
      RFduinoBLE.send("no interval set",15);
    }
}
void printLog(unsigned int when){
    if(when>log_counter){
      RFduinoBLE.send("this is future",14);
    }else if(log_counter-when>=log_len){
      RFduinoBLE.send("this is past",14);
    }else{
      unsigned int pt=when%log_len;
      char message[20];
      sprintf(message,"%3d.%01d*;%2d.%01d%%;%2d:%02dm  ",
          log_temp[pt]/10,abs(log_temp[pt]%10),
          log_moisture[pt]/10,abs(log_moisture[pt]%10),
          log_water[pt]/60, abs(log_water[pt]%60));

      RFduinoBLE.send(message,20);

    }
}
void printRepeats(){
  if(maxrepeats>0 && repeat_mlevel>0){
    char message[20];
    sprintf(message, "until %2d.%01d%%, max %1dx",repeat_mlevel/10, repeat_mlevel%10, maxrepeats);
     RFduinoBLE.send(message,20);
  }else{
     RFduinoBLE.send("no moisture level set",20);
  }
}
void printOptions(){
    RFduinoBLE.send("00:          Options",20);  
    RFduinoBLE.send("01 <TP>: sensor data",20);  
    RFduinoBLE.send("02:   print interval",20);  
    RFduinoBLE.send("03:    print runtime",20);  
    RFduinoBLE.send("04:current watertime",20);  
    RFduinoBLE.send("08: <%/2> <n> repeat",20);  
    RFduinoBLE.send("09:<h/6><o> interval",20);  

}
void printRuntime(){
  unsigned int sec=(millis()-log_ts)/1000;
  char message[20];
  sprintf(message, "%d days %2d:%02d:%02d h  ",log_counter/24, log_counter%24, sec/60, sec%60);
  RFduinoBLE.send(message,20);  
}
void BLE_request(){
  if(buf[0]==0){
    printOptions();
  }else if(buf[0]==1){
    if(buflen==1){
      updateSensors(1);
    }else{
      int when=buf[1];
      if(buflen>2)
        when=when*2^8+buf[2];
      char message[20];
      sprintf(message, "sensor history %d    ",when);
      printLog(when);
    }
  }else if(buf[0]==2){
    printInterval();
  }else if(buf[0]==3){
    printRuntime();
  }else if(buf[0]==4){
    printCountdown();
  }else if(buf[0]==8){//->8 / moisture level / number repeats
    if(buflen<3){
      printRepeats();
    }else{
      repeat_mlevel=buf[1]*5;
      maxrepeats=buf[2];
      printRepeats();
    }
  }else if(buf[0]==9){
    if(buflen==1) {
      if(water_duration>0){
        RFduinoBLE.send("BLE interrupt",13);
        stop_water();
      }
    }else if(buflen==2){ // -> 9/sec water/interval(h)/offset(10 min)
      char message[20];
      sprintf(message, "BLE water %3d sec",buf[1]);
      RFduinoBLE.send(message,17);
      water(buf[1]);
    }else{//set autostart 
      if(buf[1]==0 || buf[2]==0){
        RFduinoBLE.send("Disable trigger",15);
        water_trigger_ontime=0;
        water_trigger_interval=0;
      }else{
        RFduinoBLE.send("Enable trigger",14);
        int offset= (buflen>3?buf[3]:0);
        water_trigger_interval=buf[2];
        water_trigger_ontime=buf[1];    
        water_trigger_ts=millis()-HOURS(water_trigger_interval)+MINUTES(offset*10);            
      }
    }
   }else{
    RFduinoBLE.send("invalid option",19);
   }
}

void loop() { 
    if(RFduino_pinWoke(button)){
      delay(debounce_time);//ignore short button press
      RFduino_resetPinWake(button);      
      if(digitalRead(button) != button_state){
        RFduino_pinWake(button, button_state);
        button_state= ! button_state;
        if(button_state==HIGH){//button pressed
          button_ts=millis();
          RFduinoBLE.send("Button Pressed",15);
          water(10);
        }else if(millis()-button_ts> button_reset_time){
          stop_water();
          RFduinoBLE.send("Button Interrupt",16);//button Released
        }
      }
    }
    //if(RFduino_pinWoke(flow_sensor)){
    //  ++water_counter;
    //  RFduino_resetPinWake(flow_sensor);      
    //}else{

    //if(button_state==HIGH)
    //  updateSensors();//only if button is not currently pressed
    
    unsigned long now=millis(); //to avoid "negative" pausetime
    if( water_trigger_interval>0 && 
        now-water_trigger_ts>HOURS(water_trigger_interval)){
      //trigger autostart      
      water_trigger_ts=now;
      RFduinoBLE.send("trigger autostart",17);
      water_trigger_state=1;
      water(water_trigger_ontime);
    }else if(water_duration >0 && now-water_ts>SECONDS(water_duration)){
      RFduinoBLE.send("water timeout",13);
      stop_water();
    }
    if(now-log_ts>MINUTES(log_interval)){
      log_counter++;
      updateSensors(0);
      log_ts=now;
      //requires copy?
      log_temp[log_counter%log_len]=temp1;
      log_water[log_counter%log_len]=water_counter_sec;
      //log_time[log_counter%log_len]=log_counter;
      log_moisture[log_counter%log_len]=moisture;
    }
     
    unsigned long pausetime=MINUTES(log_interval)-(now-log_ts); //log interval
    if(water_trigger_interval>0)
      pausetime=min(pausetime, HOURS(water_trigger_interval)-(now-water_trigger_ts) );
    if(water_duration>0)
      pausetime=min(pausetime, SECONDS(water_duration)-(now-water_ts));
    char message [20];
    sprintf(message,"[sleep %d sec]      ",pausetime/1000);
    RFduinoBLE.send(message,16);
    RFduino_ULPDelay(pausetime);    
    if(Lazarus.lazarusArising()){
      //RFduinoBLE.send("[Lazerus]",10);
      BLE_request();
    }  
}
void RFduinoBLE_onReceive(char *data, int len)
{
  buf=data;
  buflen=len;
    //RFduinoBLE.send("call lazarus",13);

   Lazarus.ariseLazarus(); 
  // if the first byte 
  // 1 --> print sensor data
  // 2 --> print hours to next autostart, interval time and duration
  // 8 --> set repeat moisture level and maxrepeats
  // 9 --> set up interval
}
