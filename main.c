#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>

#include <math.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define DEVICE_ADDRESS 0x27

#include <geniePi.h>  //the ViSi-Genie-RaspPi library
#include <wiringPi.h>

/********fuctions definitions***/
int check_sensor(int file, char *fichero);
void read_request(int file, unsigned char buf[10]);
unsigned char *sensor_data(int file, unsigned char buf[10] );
float get_humidity(unsigned char seis_h, unsigned char ocho_h);
float get_temperature(unsigned char ocho_t, unsigned char seis_t);
void close_connection(int file, char *fichero);

//variables globales
unsigned int threshold_temp = 27;

/*******functions*****/

int check_sensor(int file, char *fichero){

    if ((file = open(fichero, O_RDWR)) < 0)
        {
            /*ERROR*/
            perror("Failed to open i2c bus!\n");
            exit(1);
        }
    else{
            printf("Succesful bus access.\n");
        }

    if (ioctl(file, I2C_SLAVE, DEVICE_ADDRESS) < 0){
            perror("Failed to acquire bus access and talk to slave! \n");
            exit(1);
        }
        else{
            printf("Succesful file access. \n");
        }

    return file;
}

void read_request(int file, unsigned char buf[10] ){
    if ((write(file, buf, 1)) != 1) {							// Send register we want to read from
            printf("Error writing to i2c slave\n");
            exit(1);
        }
}

unsigned char *sensor_data(int file,unsigned char buf[10] ){
    if (read(file, buf, 4) != 4) {						// Read back data into buf[]
            printf("Unable to read from slave\n");
            exit(1);
        }

        return buf;
    }

float get_humidity(unsigned char seis_h, unsigned char ocho_h){
    //unsigned int result =( (seis_h <<26)>>18) + ocho_h;			// Calculate bearing as a word value
    unsigned int aux = 0;
    aux = aux + seis_h;
    aux = (aux << 8) + ocho_h;
    unsigned int result = (aux & 0x3FFF); 			// Calculate bearing as a word valu/e

    printf("Raw humidity: %u \n",result);
    //int val=(result / ((2^14)-2) * 100);
    float val = (result / (pow(2.0,14) -2)) *100;
    printf("Humitidy : %f \n", val);

    return val;

}

float get_temperature(unsigned char ocho_t, unsigned char seis_t){
    unsigned int result =( (ocho_t <<8) + seis_t) >> 2;			// Calculate bearing as a word value)


    printf("Raw temp: %u \n",result);
    // int val=(result / ((2^14)-2) * 165)-40;
    float val = ((result / (pow(2.0,14) -2))*165)-40;
    printf("Temp : %f \n\n", val);

    return val;
}

void close_connection(int file, char *fichero){

    if (close(file)<0){
        printf("Unable to close slave\n");
        exit(1);
    }

}


//ESTE ES EL HILO QUE SE USA PARA ESCRIBIR LA HORA CONSTANTEMENTE

static void *handleDigitsClock(void *data)
{
  int digitClock_hour = 1;
  int digitClcok_min = 2;
  int digitClock_seg = 3;

  time_t rawtime;
  struct tm * timeinfo;


  for(;;)             //infinite loop
  {
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    digitClock_hour = timeinfo->tm_hour;
    digitClcok_min = timeinfo->tm_min;
    digitClock_seg = timeinfo->tm_sec;

    //escribe en el reloj
    genieWriteObj(GENIE_OBJ_LED_DIGITS, 0x01, digitClock_hour);
    genieWriteObj(GENIE_OBJ_LED_DIGITS, 0x02, digitClcok_min);
    genieWriteObj(GENIE_OBJ_LED_DIGITS, 0x03, digitClock_seg);

    //usleep(10000);
  }
  return NULL;
}


//This is the event handler. Messages received from the display are processed here.
void handleGenieEvent(struct genieReplyStruct * reply)
{
  if(reply->cmd == GENIE_REPORT_EVENT)    //check if the cmd byte is a report event
  {
    if(reply->object == GENIE_OBJ_KNOB) //check if the object byte is that of a knob
      {
        if(reply->index == 0)		  //check if the index byte is that of knob0
          //write to the LED digits object
          //genieWriteObj(GENIE_OBJ_LED_DIGITS, 0x02, reply->data);
          threshold_temp = reply->data;

      }
  }

  //if the received message is not a report event, print a message on the terminal window
  else
    printf("Unhandled event: command: %2d, object: %2d, index: %d, data: %d \r\n", reply->cmd, reply->object, reply->index, reply->data);
}

int main()
{
    pthread_t myThread;              //declare a thread
    struct genieReplyStruct reply ;  //declare a genieReplyStruct type structure

    //print some information on the terminal window
    printf("\n\n");
    printf("Visi-Genie-Raspberry-Pi\n");
    printf("==================================\n");
    printf("Program is running. Press Ctrl + C to close.\n");

    //open the Raspberry Pi's onboard serial port, baud rate is 115200
    //make sure that the display module has the same baud rate
    genieSetup("/dev/ttyAMA0", 115200);
    genieWriteObj(GENIE_OBJ_KNOB,0x00, 27);
    genieWriteObj(GENIE_OBJ_LED_DIGITS,0x00, 27);

    //start the thread for writing to the CLOCK
    (void)pthread_create (&myThread,  NULL, handleDigitsClock, NULL);

    //vars defintion
    char *fichero= "/dev/i2c-1";
    int fd =0;
    unsigned char buf[10];
    float temp=0.0;
    float hum=0.0;
    int aux_hum, aux_temp;
    char* aux_str = "Iniciando medicion";

    wiringPiSetup();
    pinMode(0,OUTPUT);
    pinMode(1,OUTPUT);
    pinMode(2,OUTPUT);
    pinMode(3,OUTPUT);

    softPwmCreate(0,0,100);
    softPwmCreate(1,0,100);
    softPwmCreate(2,0,100);


    //start device checking
    fd=check_sensor(fd, fichero);

    for(;;)
    {
        //ask for data

        buf[0]=0;
        read_request(fd,buf);

        sensor_data(fd,buf);

        //Check data
        unsigned int read_status = buf[0]>>6;

        if (read_status >1)
        {
            printf("Status is : %i", read_status);
        }
        else
        {
            if (read_status == 1)
            {
                printf("Status is : %i so it has been already fetched\n", read_status);
            }

            hum=get_humidity(buf[0],buf[1]);
            aux_hum = (int)hum;
            genieWriteObj(GENIE_OBJ_METER,0x00,aux_hum);

            temp=get_temperature(buf[2],buf[3]);
            aux_temp = (int)temp;
            genieWriteObj(GENIE_OBJ_ANGULAR_METER,0x00,aux_temp);

            asprintf(&aux_str,"Iniciando mediciones...\n\nTemperatura: %d (oC)\nHumedad: %d (%)\n\n",aux_temp,aux_hum);
            genieWriteStr(0x00, aux_str);

            //codigo para prender el led RGB
            if(hum < 40.0)
            {
                //rojo
                softPwmWrite(1,0);
                softPwmWrite(2,0);
                softPwmWrite(0,100);
            }

            if((hum > 40.0)&&(hum < 70.0))
            {
                //verde
                softPwmWrite(0,0);
                softPwmWrite(2,0);
                softPwmWrite(1,100);
            }

            if(hum > 70.0)
            {
                //azul
                softPwmWrite(0,0);
                softPwmWrite(1,0);
                softPwmWrite(2,100);
            }

            // codigo para prender 1 led
            if(temp > threshold_temp)
            {
                digitalWrite(3,HIGH);
                genieWriteObj(GENIE_OBJ_LED,0x00,1);
            }
            else
            {
                digitalWrite(3,LOW);
                genieWriteObj(GENIE_OBJ_LED,0x00,0);
            }

        }//llave del else

        while(genieReplyAvail())      //check if a message is available
        {
          genieGetReply(&reply);      //take out a message from the events buffer
          handleGenieEvent(&reply);   //call the event handler to process the message
        }
        sleep(1);
        //softPwmWrite(0,0);
        //softPwmWrite(1,0);
        //softPwmWrite(2,0);
        //digitalWrite(3,LOW);

    }//llave dEL FOR
    return 0;
}





