#ifndef _LCD_H
#define _LCD_H

#include <string.h>
#include "hal_board.h"

#define 	DATA    	1
#define 	COMMAND		0


#define		K_UP		0X20
#define		K_DOWN		0X21
#define		K_LEFT		0X22
#define		K_RIGHT		0X23
#define		K_CANCEL	0X24
#define		K_OK		0X25
#define		NO_1		0x01
#define 	NO_2		0x02

#ifdef PLUG_P1_3
#define	LCD_595_LD NULL	//P1_3debug by lihao 20160226
#else
#define	LCD_595_LD 	P1_3
#endif

#define	LCD_595_CK 	P1_5
#define	LCD_595_DAT	P2_0

#define	LCD_BK   	P1_2
#define	LCD_RS		P1_7
#define	LCD_RW		P0_1
#define	LCD_CS1		P1_4
#define	LCD_E		P1_6

extern void InitDisplay(void);
extern void HalLcdInit(void);
//extern void LoadICO(uint8 y , uint8 x , uint8 n);
extern void ClearScreen(void);
extern void Printn(uint8 xx ,uint8 yy , uint32 no,uint8 yn,uint8 le);
extern void Printn8(uint8 xx ,uint8 yy , uint32 no,uint8 yn,uint8 le);
extern void Print6(uint8 xx, uint8 yy, uint8 ch1[], uint8 yn);
extern void Print8(uint16 y,uint16 x, uint8 ch[],uint16 yn);
extern void Print16(uint16 y,uint16 x,uint8 ch[],uint16 yn);
extern void Print(uint8 y, uint8 x, uint8 ch[], uint16 yn);
extern void ClearCol(uint8 Begin , uint8 End);
//extern void Rectangle(uint8 x1,uint8 y1,uint8 x2,uint8 y2);
extern void DoSetContrast(void);
extern void SetContrast(uint8 Gain, uint8 Step);
extern void SetRamAddr (uint8 Page, uint8 Col);
extern void Lcdwritedata(uint8 dat);
extern void LoadICO(void);
extern void TurnOnDisp(void);
//void MenuMenuDisp(void);


#endif

