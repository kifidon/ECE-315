// Include FreeRTOS Libraries
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Include xilinx Libraries
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "semphr.h"

// Other miscellaneous libraries
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "pmodkypd.h"
#include "sleep.h"
#include "PmodOLED.h"
#include "OLEDControllerCustom.h"


#define BTN_DEVICE_ID  XPAR_INPUTS_DEVICE_ID
#define BTN_CHANNEL    1
#define KYPD_DEVICE_ID XPAR_KEYPAD_DEVICE_ID
#define KYPD_BASE_ADDR XPAR_KEYPAD_BASEADDR


#define FRAME_DELAY 50000

// keypad key table
#define DEFAULT_KEYTABLE 	"0FED789C456B123A"
#define XLENGTH 15
#define YLENGTH 10

typedef struct{
	u8 xCord;
	u8 yCord;
	u8 collision;
	int fillRect;
} GameObject;

GameObject enemies[3];
GameObject *player;
GameObject attack[3];

xSemaphoreHandle enemyMutex; //Potentially sub out for priotiy inversion mutexes 
xSemaphoreHandle attackMutex; 

// Declaring the devices
XGpio       btnInst;
PmodOLED    oledDevice;
PmodKYPD 	KYPDInst;

// Function prototypes
void initializeScreen();
static void movePlayer( void *pvParameters );
static void moveEnemies( void *pvParameters );
static void moveAttack(void *pvParameters);
static void generateEnemies( void *pvParameters );
static void generateAttack( void *pvParameters );
static void updateScreen( void *pvParameters );

// To change between PmodOLED and OnBoardOLED is to change Orientation
const u8 orientation = 0x0; // Set up for Normal PmodOLED(false) vs normal
                            // Onboard OLED(true)
const u8 invert = 0x0; // true = whitebackground/black letters
                       // false = black background /white letters
u8 keypad_val = 'x';


int main()
{
	int status = 0;
	// Initialize Devices
	initializeScreen();

	// Buttons
	status = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}
	XGpio_SetDataDirection (&btnInst, BTN_CHANNEL, 0x0f);


	xil_printf("Initialization Complete, System Ready!\n");

	enemyMutex = xSemaphoreCreateMutex();
	xTaskCreate( movePlayer						/* The function that implements the task. */
			   , "Move Player"					/* Text name for the task, provided to assist debugging only. */
			   , configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			   , NULL							/* The task parameter is not used, so set to NULL. */
			   , tskIDLE_PRIORITY + 3			/* The task runs at the idle priority. */
			   , NULL
			   );
	xTaskCreate( moveEnemies 					/* The function that implements the task. */
				   , "MoveEnemies"				/* Text name for the task, provided to assist debugging only. */
				   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
				   , NULL						/* The task parameter is not used, so set to NULL. */
				   , tskIDLE_PRIORITY + 2		/* The task runs at the idle priority. */
				   , NULL
				   );
	xTaskCreate( generateEnemies 				/* The function that implements the task. */
				   , "generateEnemies"			/* Text name for the task, provided to assist debugging only. */
				   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
				   , NULL						/* The task parameter is not used, so set to NULL. */
				   , tskIDLE_PRIORITY + 1		/* The task runs at the idle priority. */
				   , NULL
				   );
	xTaskCreate( generateAttack 				/* The function that implements the task. */
					, "generateAttack"			/* Text name for the task, provided to assist debugging only. */
					, configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
					, NULL						/* The task parameter is not used, so set to NULL. */
					, tskIDLE_PRIORITY + 2		/* The task runs at the idle priority. */
					, NULL
					);
	xTaskCreate( moveAttack 					/* The function that implements the task. */
				   , "moveAttack"				/* Text name for the task, provided to assist debugging only. */
				   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
				   , NULL						/* The task parameter is not used, so set to NULL. */
				   , tskIDLE_PRIORITY   		/* The task runs at the idle priority. */
				   , NULL
				   );
	xTaskCreate( updateScreen 					/* The function that implements the task. */
				   , "updateScreen"				/* Text name for the task, provided to assist debugging only. */
				   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
				   , NULL						/* The task parameter is not used, so set to NULL. */
				   , tskIDLE_PRIORITY 			/* The task runs at the idle priority. */
				   , NULL
				   );

	vTaskStartScheduler();


   while(1);

   return 0;
}

void initializeScreen()
{
   OLED_Begin(&oledDevice, XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR,
         XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR, orientation, invert);
}

void InitializeKeypad()
{
   KYPD_begin(&KYPDInst, KYPD_BASE_ADDR);
   KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
}



void moveGameObject(GameObject *gamer, int direction){


	if(direction == 0 && gamer->yCord != 0){
		gamer->yCord -=YLENGTH;
	}
	if(direction == 1 && gamer->yCord >= crowOledMax){
		gamer->yCord -=YLENGTH;
	}
	if(direction == 2){
		gamer->xCord -=XLENGTH;
	}
	

	// OLED_MoveTo(&oledDevice,gamer->xCord, player->yCord);

	// OLED_DrawRect(&oledDevice, gamer->xCord+XLENGTH, gamer->yCord+YLENGTH);
	// if(gamer->fillRect){
	// 	OLED_FillRect(&oledDevice, gamer->xCord+XLENGTH, gamer->yCord+YLENGTH);
	// }

	// OLED_Update(&oledDevice);
	// usleep(FRAME_DELAY);
	// OLED_ClearBuffer(&oledDevice);
}


int restartGame(){
	for(int i = 0; i < 3; i++){
		enemies[i].collision = 0;
		attack[i].collision = 0;
	}
	player->Ycord = YLENGTH;
	return 0;
}

static void generateEnemies(void *pvparameters){
	int enemieIndex;
//	int collumnOffset;
	while(1){
		if (xSemaphoreTake(enemyMutex, portMAX_DELAY) == pdTRUE) {
			enemieIndex = rand() % 3;
			if(enemies[enemieIndex].collision == 0){
				enemies[enemieIndex].xCord = ccolOledMax;
				enemies[enemieIndex].yCord = (YLENGTH)*enemieIndex;
				enemies[enemieIndex].collision = 1;
			}
			xSemaphoreGive(enemyMutex);
		}
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
}

static void generateAttack(void *pvparameters)
{
	int attackIndex;
	//	int collumnOffset;
	while (1)
	{
		if (xSemaphoreTake(attackMutex, portMAX_DELAY) == pdTRUE)
		{
			attackIndex = player->yCord % YLENGTH;
			if (attack[attackIndex].collision == 0)
			{ // initial position of attacks 
				attack[attackIndex].xCord = XLENGTH + 5;
				attack[attackIndex].yCord = (attackIndex * YLENGTH) + 5;
				attack[attackIndex].collision = 1;
			}
			xSemaphoreGive(attackMutex);
		}
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
}

static void moveAttack(void *pvparameters)
{
	OLED_SetDrawMode(&oledDevice, 0);
	while (1)
	{
		if (xSemaphoreTake(attackMutex, portMAX_DELAY) == pdTRUE)
		{
			for (int i = 0; i < 3; i++)
			{
				if (attack[i].collision == 1)
				{
					attack[i].xCord += 2;
					// moveGameObject(&enemies[i], 2); // 2 moves down
				}
			}
			xSemaphoreGive(attackMutex);
			taskYIELD();
			for (int i = 0; i < 3; i++)
			{
				if (attack[i].xCord >= (enemies[i].xCord + XLENGTH/2) && attack[i].collision && enemies[i].collision)
					attack[i].collision = 0;
					enemies[i].collision = 0;
			}
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

static void moveEnemies(void *pvparameters){
	u8 gameOverPosition=8;
	OLED_SetDrawMode(&oledDevice, 0);
	int gameOver = 0;
	while(1){
		if(!gameOver){
			if (xSemaphoreTake(enemyMutex, portMAX_DELAY) == pdTRUE) {
				for(int i = 0; i < 3; i++){
					if(enemies[i].collision == 1){
						moveGameObject(&enemies[i], 2); // 2 moves down
					}
				}
				xSemaphoreGive(enemyMutex);
				taskYIELD();
				for(int i = 0; i < 3; i++){
					if(enemies[i].xCord <= gameOverPosition && enemies[i].collision)
						gameOver =1;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
		else{
			OLED_ClearBuffer(&oledDevice);
			OLED_SetCursor(&oledDevice, 0, 1);
			OLED_PutString(&oledDevice, "Game Over");
			OLED_Update(&oledDevice);
			vTaskDelay(pdMS_TO_TICKS(3000));
			OLED_ClearBuffer(&oledDevice);
			gameOver = restartGame();
		}
	}
}

static void movePlayer( void *pvParameters )
{
	player->collision = 1;
	player->xCord = 0;
	player->yCord = crowOledMax/3;
	player->fillRect = 1;
	u8 buttonVal=0;
	u8 previousButtonVal=0;
	OLED_SetDrawMode(&oledDevice, 0);
	// Turn automatic updating off
	OLED_SetCharUpdate(&oledDevice, 0);

	OLED_MoveTo(&oledDevice,0, player->yCord);
	OLED_DrawRect(&oledDevice, player->xCord+XLENGTH, player->yCord+YLENGTH);
	OLED_Update(&oledDevice);
	usleep(FRAME_DELAY);
	while(1){
		previousButtonVal = buttonVal;
		buttonVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);

		if (buttonVal == 1 && previousButtonVal == 0){
			moveGameObject(player, 0); //  moves Left
		} else  if (buttonVal == 2 && previousButtonVal == 0){
			moveGameObject(player, 1); // move right
		} else{
			vTaskDelay(pdMS_TO_TICKS(500));
		}
	}
}

static void updateScreen(void *pvParameters){
	while(1){
		OLED_ClearBuffer(&oledDevice);
		// draw player
		OLED_MoveTo(&oledDevice, player->xCord, player->yCord);
		OLED_DrawRect(&oledDevice, player->xCord+XLENGTH, player->yCord+YLENGTH);
		OLED_FillRect(&oledDevice, gamer->xCord + XLENGTH, gamer->yCord + YLENGTH);
		// draw enemies
		for(int i = 0; i < 3; i++){
			if(enemies[i].collision == 1){
				OLED_MoveTo(&oledDevice, enemies[i].xCord, enemies[i].yCord);
				OLED_DrawRect(&oledDevice, enemies[i].xCord+XLENGTH, enemies[i].yCord+YLENGTH);
			}
			if(attack[i].collision == 1){
				OLED_MoveTo(&oledDevice, attack[i].xCord, attack[i].yCord);
				OLED_DrawLine(&oledDevice, attack[i].xCord+(XLENGTH/2), attack[i].yCord);
			}
		}
		OLED_Update(&oledDevice);
		usleep(FRAME_DELAY);
		// vTaskDelay(pdMS_TO_TICKS(100));
	}
}
