#include "TFTs.h"
#include <LITTLEFS.h> 

void TFTs::begin() {
  // Start with all displays selected.
  chip_select.begin();
  chip_select.setAll();

  // Turn power on to displays.
  pinMode(TFT_ENABLE_PIN, OUTPUT);
  enableAllDisplays();

  // Initialize the super class.
  init();  
}

void TFTs::beginJpg()
{
  root = LITTLEFS.open("/");  
}

int TFTs::EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int TFTs::EndsWithJPG(const char *str) { return EndsWith(str, ".jpg"); }

void TFTs::showSlice( char * filename, int devicenum )
{
    chip_select.setDigit( devicenum );
    drawSdJpeg( filename, 0, 0);
}


void TFTs::showNextJpg()
{
    fs::File file = root.openNextFile();
    if (! file)
    {
      root = LITTLEFS.open("/"); 
    }
    else
    {
        if( file.isDirectory())
        {
          Serial.print( "Skipping directory " );
          Serial.println( file.name() );
        }
        else
        {
          if ( EndsWithJPG( file.name() ) )
          {
            //Serial.print( "Ends with JPG " );
            //Serial.println( file.name() );

            chooseRandomDisplay();
            drawSdJpeg( file.name() , 0, 0);     // This draws a jpeg pulled off the SD Card
          }
          else
          {
            //Serial.print( "Does not end with JPG " );
            //Serial.println( file.name() );
          }          
        }
    }
}

void TFTs::chooseRandomDisplay()
{
  int i = random(6);
  
  Serial.print( "display " );
  Serial.println( i );

  chip_select.setDigit(i);
}

// Modified code from TFT_eSPI examples, combined ESP32_SDcard_JPEG and TFT_SPIFFS_BMP

//####################################################################################################
// Draw a JPEG on the TFT pulled from SD Card
//####################################################################################################
// xpos, ypos is top left corner of plotted image
void TFTs::drawSdJpeg(const char *filename, int xpos, int ypos) {

  // Open the named file (the Jpeg decoder library will close it)
  fs::File jpegFile = LITTLEFS.open( filename, FILE_READ);  // or, file handle reference for SD library
 
  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  Serial.println("===========================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("===========================");

  // Use one of the following methods to initialise the decoder:
  boolean decoded = JpegDec.decodeSdFile(jpegFile);  // Pass the SD file handle to the decoder,
  //boolean decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)

  if (decoded) {
    // print information about the image to the serial port
    jpegInfo();
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported");
  }
}

//####################################################################################################
// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//####################################################################################################
// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void TFTs::jpegRender(int xpos, int ypos) {

  //jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = getSwapBytes();
  setSwapBytes(true);
  
  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {    // While there is more data in the file
    pImg = JpegDec.pImage ;   // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= width() && ( mcu_y + win_h ) <= height())
      pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ( (mcu_y + win_h) >= height())
      JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }

  setSwapBytes(swapBytes);

  showTime(millis() - drawTime); // These lines are for sketch testing only
}

//####################################################################################################
// Print image information to the serial port (optional)
//####################################################################################################
// JpegDec.decodeFile(...) or JpegDec.decodeArray(...) must be called before this info is available!
void TFTs::jpegInfo() {

  // Print information extracted from the JPEG file
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

//####################################################################################################
// Show the execution time (optional)
//####################################################################################################
// WARNING: for UNO/AVR legacy reasons printing text to the screen with the Mega might not work for
// sketch sizes greater than ~70KBytes because 16 bit address pointers are used in some libraries.

// The Due will work fine with the HX8357_Due library.

void TFTs::showTime(uint32_t msTime) {
  //tft.setCursor(0, 0);
  //tft.setTextFont(1);
  //tft.setTextSize(2);
  //tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.print(F(" JPEG drawn in "));
  //tft.print(msTime);
  //tft.println(F(" ms "));
  Serial.print(F(" JPEG drawn in "));
  Serial.print(msTime);
  Serial.println(F(" ms "));
}

void TFTs::setDigit(uint8_t digit, uint8_t value, show_t show) {
  uint8_t old_value = digits[digit];
  digits[digit] = value;
  
  if (show != no && (old_value != value || show == force)) {
    showDigit(digit);
  }
}

/* 
 * Where the rubber meets the road.  Displays the bitmap for the value to the given digit. 
 */
void TFTs::showDigit(uint8_t digit) {
  chip_select.setDigit(digit);

  // Filenames are no bigger than "255.bmp\0"
  char file_name[10];
  sprintf(file_name, "/%d.bmp", digits[digit]);
  drawBmp(file_name, 0, 0);
}


// These BMP functions are stolen directly from the TFT_SPIFFS_BMP example in the TFT_eSPI library.
// Unfortunately, they aren't part of the library itself, so I had to copy them.

//// BEGIN STOLEN CODE
bool TFTs::drawBmp(const char *filename, int16_t x, int16_t y) {

  // Nothing to do.
  if ((x >= width()) || (y >= height())) return(true);

  fs::File bmpFS;

  // Open requested file on SD card
  bmpFS = LITTLEFS.open(filename, "r");

  if (!bmpFS)
  {
    Serial.print("File not found: ");
    Serial.println( filename );
    return(false);
  }

  uint32_t seekOffset;
  uint16_t w, h, row, col;
  uint8_t  r, g, b;

  uint32_t startTime = millis();

  uint16_t magic = read16(bmpFS);
  if (magic == 0xFFFF) {
    Serial.print( "SPIFFS image not found: " );
    Serial.println( filename );
    return(false);
  }
  
  if (magic == 0x4D42)
  {
    read32(bmpFS);
    read32(bmpFS);
    seekOffset = read32(bmpFS);
    read32(bmpFS);
    w = read32(bmpFS);
    h = read32(bmpFS);

    if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
    {
      y += h - 1;

      bool oldSwapBytes = getSwapBytes();
      setSwapBytes(true);
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++) {

        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t*  bptr = lineBuffer;
        uint16_t* tptr = (uint16_t*)lineBuffer;
        // Convert 24 to 16 bit colours
        for (uint16_t col = 0; col < w; col++)
        {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        // Push the pixel row to screen, pushImage will crop the line if needed
        // y is decremented as the BMP image is drawn bottom up
        pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
      }
      setSwapBytes(oldSwapBytes);
      //Serial.print("Loaded in ");
      //Serial.print(millis() - startTime);
      //Serial.println(" ms");
    }
    else {
      Serial.println("BMP format not recognized.");
      bmpFS.close();
      return(false);
    }
  }
  else {
    Serial.print("File not a BMP. Magic: ");
    Serial.println(magic);
    bmpFS.close();
    return(false);
  }
  bmpFS.close();
  return(true);
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t TFTs::read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t TFTs::read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
//// END STOLEN CODE
