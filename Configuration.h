// Hardware-dependent and other configuration parameters to change only with forethought

// Pin assignments
// IR receiver
#define IR_PIN          2   // Requires having run code to enable GPIO on D2

// Quadrature rotary encoder with pushbutton
#define ENCODER_BUTTON  5   
#define ENCODER_A       10
#define ENCODER_B       11

// Amp controls: power relay and ICEPower EN pins
#define AMP_ENABLE_PIN  13
#define RELAY_PIN       12

// Buttons on the Featherwing OLED
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5

// USB host shield 
// NOTE: At present, these are specified directly in the UHS library, in Usbcore.h line 57
//       Usbcore.h should use these
#define UHS_SS  1   // Labeled RX
#define UHS_INT 0   // Labeled TX


