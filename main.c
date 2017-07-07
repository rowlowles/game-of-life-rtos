#include <RTL.h>
#include "LPC17xx.H"                    /* LPC17xx definitions               */
#include "GLCD.h"
#include "LED.h"
#include "KBD.h"
#include "ADC.h"
#include "stdbool.h"

#define __FI        1                   /* Font index 16x24                  */
#define LG LightGrey
#define WT White
#define BK Black

OS_TID t_led;                           /* assigned task id of task: led */
OS_TID t_adc;                           /* assigned task id of task: adc */
OS_TID t_kbd;                           /* assigned task id of task: keyread */
OS_TID t_jst   ;                        /* assigned task id of task: joystick */
OS_TID t_clock;                         /* assigned task id of task: clock   */
OS_TID t_lcd;                           /* assigned task id of task: lcd     */
OS_TID t_draw;
OS_TID t_next;
OS_TID t_paint;


OS_MUT mut_GLCD;                        /* Mutex to controll GLCD access     */
OS_SEM sem_update;
OS_SEM sem_draw;
OS_SEM sem_LED;
OS_SEM sem_not_pause;
OS_SEM sem_newGen;
//OS_SEM readyToEvolve;

//must make the LED function into a task with a semaphore to protect the array
//must create the pause function
//must create fourth seed structure, add a random aspect to the placement/selection



 unsigned int ADCStat = 0;
 unsigned int ADCValue = 0;
 int orgLife[32][24] , newLife[32][24] ;
 uint32_t sleep;
volatile BOOL ADCRead = false;
int sleep_flag = 0;

//This is used to create an 8x8 white pixel with a 1 pixel wide grey border. 
 unsigned short deadCell[] = { LG, LG, LG, LG, LG, LG, LG, LG, LG, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,
				LG, WT, WT, WT, WT, WT, WT, WT, WT, LG,	
				LG, LG, LG, LG, LG, LG, LG, LG, LG, LG};

//This is used to make an 8x8 dead picel with a 1 pixel wide grey border.
	unsigned short livingCell[] = { LG, LG, LG, LG, LG, LG, LG, LG, LG, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,
					LG, BK, BK, BK, BK, BK, BK, BK, BK, LG,	
					LG, LG, LG, LG, LG, LG, LG, LG, LG, LG};
 
 
 
 

void emptyRealities(){
	int i, j;
	for (i = 0; i< 32; i++){
		for (j = 0; j<24; j++){
			orgLife[i][j] = 0;
			newLife[i][j] = 0;
		}
	}
}

/*----------------------------------------------------------------------------
  light up the LEDs to count the number of cells
----------------------------------------------------------------------------*/

void LEDCount (void){
	
	int i,j,num = 0, index = 0;
	unsigned char o,c;
	
	os_sem_wait(sem_LED,0xffff); //FOR ORG-----------------------
	
	for(o = 0; o < 7; o++){
		LED_Off(o);
	}

	
	for (i = 0; i< 32; i++){
		for (j = 0; j<24; j++){
			if(orgLife[i][j] == 1){
				num++; 
			}
		}
	}
	
	while ((1 << index) <= num){
		c = index;
		LED_On(c-1);
		index++;
	}
	
	
	
	
	
	os_sem_send(sem_update);
}

void detect_press (void){
	
	while(sleep_flag == 0){
		if(INT0_Get() == 0 && sleep_flag == 0){
			while(INT0_Get() == 0){
			}
			sleep_flag == 1;
			return;
		}
		os_sem_send(sem_newGen);
	}
	
	while(sleep_flag == 1){
		if(INT0_Get() == 0 && sleep_flag == 1){
			while(INT0_Get() == 0){
			}
			sleep_flag == 0;
			return;
		}
	}
}

/*----------------------------------------------------------------------------
  switch LED on
 *---------------------------------------------------------------------------*/
void LED_on  (unsigned char led) {
  LED_On (led); //turn on the physical LED
  os_mut_wait(mut_GLCD, 0xffff); //Rest of code updates graphics on the LCD
  GLCD_SetBackColor(White);
  GLCD_SetTextColor(Green);
  GLCD_DisplayChar(4, 5+led, __FI, 0x80+1); /* Circle Full                   */
  os_mut_release(mut_GLCD);
}

/*----------------------------------------------------------------------------
  switch LED off
 *---------------------------------------------------------------------------*/
void LED_off (unsigned char led) {
  LED_Off(led); //turn off the physical LED
  os_mut_wait(mut_GLCD, 0xffff); //Rest of code updates graphics on the LCD
  GLCD_SetBackColor(White);
  GLCD_SetTextColor(Green);
  GLCD_DisplayChar(4, 5+led, __FI, 0x80+0);  /* Circle Empty                 */
  os_mut_release(mut_GLCD);
}


/*----------------------------------------------------------------------------
  Task 'LEDs': Cycle through LEDs
 *---------------------------------------------------------------------------*/
__task void led (void) {
	int num_led = 3; //the number of LEDs to cycle through
	int on = 0;
	int off = 0;
	
	while(1)
	{
		for( on = 0; on <  num_led; on++)
		{
			
			off = on - 1; //Figure out which LED to turn off, wrap around if needed
			if(off == -1)
			{ 
				off = num_led -1;
			}
			LED_off(off);			
			LED_on (on);
			os_dly_wait (50);                      /* delay 50 clock ticks             */
		}
	}
}

/*----------------------------------------------------------------------------
  Task 'keyread': process key stroke from int0 push button
 *---------------------------------------------------------------------------*/
__task void keyread (void) {

  for(;;) { 
		detect_press();
		sleep = os_suspend();
		detect_press();
		os_resume(sleep);
		}   
	}	
	

/*----------------------------------------------------------------------------
  Task 'ADC': Read potentiometer
 *---------------------------------------------------------------------------*/

__task void adc (void) {
  for (;;) {
		ADC_ConversionStart();
		}
		//As mentioned in the readme file, this is one of the functions
		//that is integrated in a seperate file not included here.
  }




/*----------------------------------------------------------------------------
  Task 'lcd': LCD Control task
 *---------------------------------------------------------------------------*/
__task void lcd (void) {

  for (;;) {
    os_mut_wait(mut_GLCD, 0xffff);
    GLCD_SetBackColor(Blue);
    GLCD_SetTextColor(White);
    GLCD_DisplayString(0, 0, __FI, "      MTE 241        ");
    GLCD_DisplayString(1, 0, __FI, "      RTX            ");
    GLCD_DisplayString(2, 0, __FI, "  Project 4 Demo   ");
    os_mut_release(mut_GLCD);
    os_dly_wait (400);

    os_mut_wait(mut_GLCD, 0xffff);
    GLCD_SetBackColor(Blue);
    GLCD_SetTextColor(Red);
    GLCD_DisplayString(0, 0, __FI, "      MTE 241        ");
    GLCD_DisplayString(1, 0, __FI, "      Other text     ");
    GLCD_DisplayString(2, 0, __FI, "    More text        ");
    os_mut_release(mut_GLCD);
    os_dly_wait (400);
  }
}




/*----------------------------------------------------------------------------
Draw Life
----------------------------------------------------------------------------*/
void drawNextGen(){
	
	int i, j, delayTime;
  os_sem_wait(sem_draw,0xffff);

	delayTime = (ADCValue/10);
	
	
	
	os_dly_wait(delayTime);
	
	for (i = 0; i< 32; i++){
		for (j = 0; j<24; j++){
			//go through elements of newLife
			if (newLife[i][j] ==1)
			{
				GLCD_Bitmap ((i*10), (j*10), 10, 10, (unsigned char*)livingCell);
			}
			else
			{
				GLCD_Bitmap ((i*10), (j*10), 10, 10, (unsigned char*)deadCell);
			}
			//Overwrite new board
			orgLife[i][j] = newLife[i][j];
		}
	}

	os_sem_send(sem_LED);
	return;
	
}


void nextGen(){
	
	int livingNeighbours, i , j, iCheck,jCheck;
    //Assume alive = 1, dead = 0
    

		os_sem_wait(sem_newGen,0xffff);
    os_sem_wait(sem_update,0xffff);
    for (i = 0; i<32; i++){
    	for (j = 0; j<24; j++){
    		//Loop through all the squares
    		livingNeighbours = 0;
    		//check each neighbouring square
    		
				for (iCheck = -1; iCheck<=1;iCheck++){
					for(jCheck = -1; jCheck <=1; jCheck++){
						
						if((i+iCheck)<0){
							//Check the corner case where we go off the negative end for i
							//First two statements should check the literal corner cases
							//Else statement checks the edge case
							if ((j+jCheck)<0){
								//Check the corner case where we go off the negative end for j
								livingNeighbours = livingNeighbours + orgLife[31][23];
							}
							else if((j+jCheck)>23){
								//check the corner case where we go off the positive end for j
								livingNeighbours = livingNeighbours + orgLife[31][0];
							}
							
							else{
								//This just checks if we go off the negative side of i and are otherwise fine with j
								livingNeighbours = livingNeighbours + orgLife[31][j+jCheck];
							}
						}
						
						else if((i+iCheck)>31){
							//Check the corner cases where we go off the positive end for i
							//First two statements should take care of cases when we also go out of bounds for j
							//Else statement should do the job for all other cases
							
							if((j+jCheck)<0){
							//Same behaviour as the cases above
								livingNeighbours = livingNeighbours + orgLife[0][23];
							}
							else if((j+jCheck)>23){
								livingNeighbours = livingNeighbours + orgLife[0][0];
							}
							else{
								livingNeighbours = livingNeighbours + orgLife[0][j+jCheck];
							}
							
						}
						
						//The above only accounted for going out of bounds in the corners, and the i edges
						//We need to account for the bounds where we skew off the j edges
						else if((j+jCheck)<0){
							//skew into the negatives
							livingNeighbours = livingNeighbours + orgLife[i+iCheck][23];
						}
						else if((j+jCheck)>23){
							//skew into the high positives
							livingNeighbours = livingNeighbours + orgLife[i+iCheck][0];
						}
						
						else{
							//If a cell is alive, add the state to the number of living neighbours
							//This is valid because alive = 1
							livingNeighbours = livingNeighbours + orgLife[i+iCheck][j+jCheck];
							//Add boundary conditions here
							if(iCheck == 0 && jCheck == 0){
								livingNeighbours = livingNeighbours - orgLife[i][j];
								//If base cell is dead, this has no effect
								//If base cell is alive, this removes their influence from deciding on the next generation
							}	
						}
					
				}
			}
			
			//Cell dies by overpopulation of underpopulation
			if (livingNeighbours < 2 || livingNeighbours > 3){
				newLife[i][j] = 0;
			}
			//If a cell has 3 living neighbours it is alive next generation no matter what
			//If a cell has 2 living neighbours it is only alive if it is already alive
			if(   (livingNeighbours ==2 && orgLife[i][j]==1)  ||livingNeighbours ==3){
				newLife[i][j] = 1;
			}
    	}
    }
		//os_sem_send(readyToEvolve);
		os_sem_send(&sem_draw);//MUTEX FOR ORG-----------------------
		return;
}

__task void evolve(void){
	
	while(1){
		nextGen(); 
		drawNextGen();
		}
	
	os_tsk_delete_self ();
}
/*----------------------------------------------------------------------------
	Update the number of cells
------------------------------------------------------------------------------*/
__task void LEDc (void){
	while (1)
	{
		LEDCount();
	}
}

/*----------------------------------------------------------------------------
  Task 'init': Initialize
 *---------------------------------------------------------------------------*/
__task void init (void) {

  os_mut_init(mut_GLCD);
	os_sem_init(sem_draw,0);
	os_sem_init(sem_LED,0);
	os_sem_init(sem_update,0);
	os_sem_init(sem_newGen,0); 
	os_sem_send(sem_update); 
	
	GLCD_SetBackColor(Black);
	GLCD_SetTextColor(Green);
	GLCD_DisplayString(4,6,__FI,"Conway's");
	GLCD_DisplayString(5,4,__FI,"Game of Life");
	os_dly_wait(100);
	
	os_tsk_create (adc, 0);		 /* start the adc task               */
	
	
	//Acorn
	orgLife[10][10] = 1;
	orgLife[11][12] = 1;
	orgLife[12][9] = 1;
	orgLife[12][10] = 1;
	orgLife[12][13] = 1;
	orgLife[12][14] = 1;
	orgLife[12][15] = 1;
	//Blinker
	orgLife[18][9] = 1;
	orgLife[18][10] = 1;
	orgLife[18][11] = 1;
	//Die Hard
	orgLife[28][9] = 1;
	orgLife[28][10] = 1;
	orgLife[29][10] = 1;
	orgLife[28][14] = 1;
	orgLife[28][15] = 1;
	orgLife[28][16] = 1;
	orgLife[27][15] = 1;
	
	
	os_dly_wait (10);
	
	os_tsk_create(evolve,0);
	os_tsk_create(LEDc,0);
	os_tsk_create(keyread, 0);
	
  os_tsk_delete_self ();
}

/*----------------------------------------------------------------------------
  Main: Initialize and start RTX Kernel
 *---------------------------------------------------------------------------*/
int main (void) {
	
	NVIC_EnableIRQ( ADC_IRQn ); 							/* Enable ADC interrupt handler  */
	
	//emptyRealities();
	
  LED_Init ();                              /* Initialize the LEDs           */
  GLCD_Init();                              /* Initialize the GLCD           */
	KBD_Init ();                              /* initialize Push Button        */
	ADC_Init ();															/* initialize the ADC            */

	
	GLCD_Clear(Black);
	
  os_sys_init(init);                        /* Initialize RTX and start init */
}
