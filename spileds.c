#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdlib.h>

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

#define DD_MOSI    PD7
#define DD_SCK     PD6 // So we can see the activity on the LED
#define DDR_SPI    DDRD
#define PORT_SPI   PORTD

#define NLEDS 50

void softspi_send(unsigned char data)
{
	for (unsigned char bit=0x80; bit; bit >>= 1) {
        PORT_SPI &= ~_BV(DD_SCK);
		//_delay_us(10);
        if (data & bit) {
          PORT_SPI |= _BV(DD_MOSI);
        } else {
          PORT_SPI &= ~_BV(DD_MOSI);
        }
        PORT_SPI |= _BV(DD_SCK);
		//_delay_us(10);
      }
}

uint8_t buffer[NLEDS*3];

void draw(uint8_t* buffer, uint16_t N)
{
	for (int i=0;i<N*3;i++)
	{
		softspi_send(buffer[i]);
	}
	PORT_SPI &= ~_BV(DD_SCK);
	_delay_ms(1);
}


void h2rgb(float H, float S, uint8_t *r, uint8_t *g, uint8_t *b)
// NOTE: 8 bits per channel!!
{
int var_i;
  float V=1,var_1, var_2, var_3, var_h, var_r, var_g, var_b;

  if ( S == 0 )                       //HSV values = 0 ÷ 1
  {
    *r = V * 1023;
    *g = V * 1023;
    *b = V * 1023;
  }
  else
  {
    var_h = H * 6;
    if ( var_h == 6 ) var_h = 0;      //H must be < 1
    var_i = (int) var_h ;            //Or ... var_i = floor( var_h )
    var_1 = V * ( 1 - S );
    var_2 = V * ( 1 - S * ( var_h - var_i ) );
    var_3 = V * ( 1 - S * ( 1 - ( var_h - var_i ) ) );

    if      ( var_i == 0 ) {
      var_r = V     ;
      var_g = var_3 ;
      var_b = var_1 ;
    }
    else if ( var_i == 1 ) {
      var_r = var_2 ;
      var_g = V     ;
      var_b = var_1 ;
    }
    else if ( var_i == 2 ) {
      var_r = var_1 ;
      var_g = V     ;
      var_b = var_3 ;
    }
    else if ( var_i == 3 ) {
      var_r = var_1 ;
      var_g = var_2 ;
      var_b = V     ;
    }
    else if ( var_i == 4 ) {
      var_r = var_3 ;
      var_g = var_1 ;
      var_b = V     ;
    }
    else                   {
      var_r = V     ;
      var_g = var_1 ;
      var_b = var_2 ;
    }

    *r = (1-var_r) * 255;                  
    *g = (1-var_g) * 255;
    *b = (1-var_b) * 255;
  }
}



void smear(unsigned long t, uint8_t* buffer, uint16_t N, float factor)
{
	if ((t % 3) == 0)
	{
		static uint8_t* temp =0 ;
		if (temp == 0) temp = (uint8_t*)malloc(N*3);
		
		memcpy(temp,buffer,N*3);
		// LED0
		temp[0] = buffer[0] * (1-factor) + factor* buffer[3];
		temp[1] = buffer[1] * (1-factor) + factor* buffer[4];
		temp[2] = buffer[2] * (1-factor) + factor* buffer[5];
		for (int i=1;i<N-1;++i)
		{
			temp[i*3] = buffer[i*3] * (1-2*factor) + factor* buffer[i*3-3] + factor* buffer[i*3+3];
			temp[i*3+1] = buffer[i*3+1] * (1-2*factor) + factor* buffer[i*3-3+1] + factor* buffer[i*3+3+1];
			temp[i*3+2] = buffer[i*3+2] * (1-2*factor) + factor* buffer[i*3-3+2] + factor* buffer[i*3+3+2];		
		}
		// LED N-1
		temp[(N-1)*3] = buffer[(N-1)*3] * (1-factor) + factor* buffer[(N-1)-3];
		temp[(N-1)*3+1] = buffer[(N-1)*3+1] * (1-factor) + factor* buffer[(N-1)-3+1];
		temp[(N-1)*3+2] = buffer[(N-1)*3+2] * (1-factor) + factor* buffer[(N-1)-3+2];
		memcpy(buffer,temp,N*3);
	}
}

void darken(unsigned long t, uint8_t* buffer, uint16_t N, int amount)
{	
	if ((t % 3) == 0)
	{
		for (int i=0;i<N*3;++i)
		{
			if (buffer[i] > amount)
				buffer[i] = buffer[i] - amount;
			else
				buffer[i] = 0;
		}
	}
}



void random_flicker_random_hue(unsigned long t, uint8_t* buffer, uint16_t N)
{
	static unsigned long last = 0;
	if (t-last > (rand() % 10 + 5))
	{
		last = t;
		int i = rand() % (N);
		float hue = (float)(rand() & 0xff)/ (float)255.0;
		h2rgb(hue,1.0,&buffer[i*3],&buffer[i*3+1],&buffer[i*3+2]);
	}
}

void random_flicker_white(unsigned long t, uint8_t* buffer, uint16_t N)
{
	static unsigned long last = 0;
	if (t-last > (rand() % 400 + 1200))
	{
		last = t;
		int i = rand() % (N-2) + 1;
		buffer[i*3] = 0xff;
		buffer[i*3+1] = 0xff;
		buffer[i*3+2] = 0xff;
	}	
}


void update_buffer(unsigned long t, uint8_t* buffer, uint16_t N)	
{
	random_flicker_random_hue(t,buffer, N);
	smear(t,buffer, N, 0.05);
	darken(t,buffer, N, 1);	
}

int main(void)
{	
	uint8_t val;
	// set for 16 MHz clock, and make sure the LED is off
	CPU_PRESCALE(0);

	// Set MOSI and SCK output, all others input
	DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK);
	
	val = 0x30;
	for (int i=0;i<NLEDS;++i)
	{
		buffer[i*3] = 0;//i*10;
		buffer[i*3+1] = 0;
		buffer[i*3+2] = 0;//255-i*10;
	}

	unsigned long t = 0;
		
	while (1) {
		update_buffer(t,buffer,NLEDS);
		draw(buffer,NLEDS);
		PORT_SPI &= ~_BV(DD_SCK);
		_delay_ms(15);
		t ++;
	}
}
