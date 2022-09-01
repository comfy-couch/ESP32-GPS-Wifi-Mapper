// This could also be defined as display.color(255,0,0) but these defines
// are meant to work for adafruit_gfx backends that are lacking color()
#define BLACK    0

// in RGB 5/6/5 red and blue get 5 bits, green gets 6
#define RED_VERYLOW   (3 <<  11)  // 0001 1000 0000 0000
#define RED_LOW       (7 <<  11)    // 0011 1000 0000 0000
#define RED_MEDIUM    (15 << 11)
#define RED_HIGH      (31 << 11)    // 1111 1000 0000 0000

#define GREEN_VERYLOW (1 <<  5)   // 0000 0000 0010 0000
#define GREEN_LOW     (15 << 5)  
#define GREEN_MEDIUM  (31 << 5)   // 0000 0011 1110 0000
#define GREEN_HIGH    (63 << 5)   // 0000 0111 1110 0000

#define BLUE_VERYLOW  3           // 0000 0000 0000 0011
#define BLUE_LOW      7
#define BLUE_MEDIUM   15
#define BLUE_HIGH     31          // 0000 0000 0001 1111

#define ORANGE_VERYLOW  (RED_VERYLOW + GREEN_VERYLOW)
#define ORANGE_LOW      (RED_LOW     + GREEN_LOW)
#define ORANGE_MEDIUM   (RED_MEDIUM  + GREEN_MEDIUM)    // 0111 1011 1110 0000
#define ORANGE_HIGH     (RED_HIGH    + GREEN_HIGH)

#define PURPLE_VERYLOW  (RED_VERYLOW + BLUE_VERYLOW)
#define PURPLE_LOW      (RED_LOW     + BLUE_LOW)
#define PURPLE_MEDIUM   (RED_MEDIUM  + BLUE_MEDIUM)
#define PURPLE_HIGH    (RED_HIGH    + BLUE_HIGH)

#define CYAN_VERYLOW  (GREEN_VERYLOW + BLUE_VERYLOW)
#define CYAN_LOW      (GREEN_LOW     + BLUE_LOW)
#define CYAN_MEDIUM   (GREEN_MEDIUM  + BLUE_MEDIUM)
#define CYAN_HIGH     (GREEN_HIGH    + BLUE_HIGH)

#define WHITE_VERYLOW (RED_VERYLOW + GREEN_VERYLOW + BLUE_VERYLOW)
#define WHITE_LOW     (RED_LOW     + GREEN_LOW     + BLUE_LOW)
#define WHITE_MEDIUM  (RED_MEDIUM  + GREEN_MEDIUM  + BLUE_MEDIUM)
#define WHITE_HIGH    (RED_HIGH    + GREEN_HIGH    + BLUE_HIGH)
