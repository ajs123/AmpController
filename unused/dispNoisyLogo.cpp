// const int pctNoise = 10;
// const int pctCentralNoise = 0;
// const int pctChange = 20;
// const int pctCentralAnim = -60;
// const int animateFrames = 60;
// NoisyLogo noisyLogo;
// const unsigned char * noisyLogoBits = noisyLogo.getNoisyLogo();
// void animateLogo() {
//   display.clear();
//   noisyLogo.makeNoise(pctNoise, pctCentralNoise);
//   display.drawXBMP(0, (64 - logo_height) / 2, logo_width, logo_height, noisyLogoBits);
//   display.updateDisplay();
//   //delay(2000);
//   for (int i = 0; i < animateFrames; i++) {
//     //delay(10);
//     noisyLogo.doStep(pctChange, pctCentralAnim);
//     display.drawXBMP(0, (64 - logo_height) / 2, logo_width, logo_height, noisyLogoBits);
//     display.updateDisplay();
//   }
//   display.drawXBMP(0, (64 - logo_height) / 2, logo_width, logo_height, logo_bits);
//   delay(5000);
// }
